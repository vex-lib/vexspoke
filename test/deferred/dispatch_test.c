#include "vex.h"
#include "deferred/dispatch.h"

#include <pthread.h>
#include <time.h>
#include <unistd.h>

;;OVERVIEW
/**
 * DispatchTest — L3 headless verification. Real job identities check FIFO,
 * producer-local order, consumer affinity and exactly-once delivery. Tests
 * callback-post deferral, nested drain refusal, reusable buffers and failures.
 * Job is a private behaviorless record: queue, identity, shared sequence cursor,
 * per-job delivery count, producer done flag and expected consumer thread.
 * Functions: record, repost, producer, main. No payload ownership transfer.
 */
#define REQUIRE(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); exit(1); \
} } while (0)

typedef struct Job {
    Dispatch *queue;
    int id;
    int *next;
    int deliveries;
    atomic_bool done;
    pthread_t consumer;
} Job;

// Records one delivery and asserts FIFO order and consumer-thread affinity.
static void record(any context) {
    Job *job = (Job*) context;
    REQUIRE(pthread_equal(pthread_self(), (*job).consumer));
    REQUIRE((*job).id == *(*job).next);
    (*(*job).next)++;
    (*job).deliveries++;
    REQUIRE((*job).deliveries == 1);
}

// Posts a follow-up job and proves draining/freeing are refused while draining.
static void repost(any context) {
    Job *job = (Job*) context;
    REQUIRE(Dispatch_post((*job).queue, record, job));
    REQUIRE(!Dispatch_drain((*job).queue));
    REQUIRE(!Dispatch_free((*job).queue));
    REQUIRE((*job).deliveries == 0);
}

// Produces one ordered row of jobs, retrying bounded by the process watchdog.
static any producer(any context) {
    Job *jobs = (Job*) context;
    const struct timespec pause = {0, 100000L};
    for (int i = 0; i < 250; i++) {
        Job *job = &jobs[i];
        while (!Dispatch_post((*job).queue, record, job))
            nanosleep(&pause, nullptr); // test-only retry; process watchdog bounds it
    }
    Job *first = &jobs[0];
    atomic_store_explicit(&(*first).done, true, memory_order_release);
    return nullptr;
}

// Exercises dispatch ordering, deferral, reuse, lock contention, and producers.
int main(void) {
    alarm(20); // A broken queue fails rather than hanging CI or an unbounded join.
    REQUIRE(!Dispatch_post(nullptr, record, nullptr));
    REQUIRE(!Dispatch_drain(nullptr));
    REQUIRE(!Dispatch_free(nullptr));
    REQUIRE(Dispatch_pending(nullptr) == 0 && Dispatch_getDrainedCount(nullptr) == 0);
    Dispatch *queue = Dispatch();
    REQUIRE(queue);
    REQUIRE(!Dispatch_post(queue, nullptr, nullptr));
    REQUIRE(Dispatch_drain(queue));
    int next = 0;
    Job jobs[40] = {0};
    for (int i = 0; i < 40; i++) {
        Job *job = &jobs[i];
        (*job).id = i;
        (*job).next = &next;
        (*job).consumer = pthread_self();
        REQUIRE(Dispatch_post(queue, record, job));
    }
    REQUIRE(Dispatch_pending(queue) == 40);
    REQUIRE(Dispatch_drain(queue));
    REQUIRE(next == 40 && Dispatch_getDrainedCount(queue) == 40);
    REQUIRE(Dispatch_pending(queue) == 0);

    next = 0;
    Job deferred = {.queue = queue, .id = 0, .next = &next, .consumer = pthread_self()};
    REQUIRE(Dispatch_post(queue, repost, &deferred));
    REQUIRE(Dispatch_drain(queue));
    REQUIRE(deferred.deliveries == 0 && Dispatch_pending(queue) == 1);
    REQUIRE(Dispatch_drain(queue));
    REQUIRE(deferred.deliveries == 1 && next == 1);
    // Both buffers have warmed: each new pass reuses one of these allocations.
    DispatchItem *a = (*queue).items;
    DispatchItem *b = (*queue).batch;
    REQUIRE(a && b);
    for (int i = 0; i < 10; i++) {
        next = 0;
        deferred.deliveries = 0;
        REQUIRE(Dispatch_post(queue, record, &deferred));
        REQUIRE(Dispatch_drain(queue));
        REQUIRE(((*queue).items == a || (*queue).items == b)
                && ((*queue).batch == a || (*queue).batch == b));
    }
    // White-box contention: a failed post/drain neither blocks nor loses state.
    REQUIRE(SpinLock_tryLock(&(*queue).lock));
    REQUIRE(!Dispatch_post(queue, record, nullptr));
    REQUIRE(!Dispatch_drain(queue));
    SpinLock_unlock(&(*queue).lock);
    REQUIRE(Dispatch_pending(queue) == 0 && Dispatch_drain(queue));
    REQUIRE(Dispatch_free(queue));

    queue = Dispatch();
    REQUIRE(queue);
    Job concurrent[4][250] = {0};
    pthread_t threads[4];
    int cursors[4] = {0};
    for (int t = 0; t < 4; t++) {
        Job *row = concurrent[t];
        for (int i = 0; i < 250; i++) {
            Job *job = &row[i];
            (*job).queue = queue;
            (*job).id = i;
            (*job).next = &cursors[t];
            (*job).consumer = pthread_self();
            atomic_init(&(*job).done, false);
        }
        REQUIRE(pthread_create(&threads[t], nullptr, producer, row) == 0);
    }
    for (;;) {
        (void) Dispatch_drain(queue);
        bool done = true;
        for (int t = 0; t < 4; t++) {
            Job *first = concurrent[t];
            if (!atomic_load_explicit(&(*first).done, memory_order_acquire))
                done = false;
        }
        if (done && Dispatch_pending(queue) == 0)
            break;
    }
    for (int t = 0; t < 4; t++) {
        REQUIRE(pthread_join(threads[t], nullptr) == 0);
        REQUIRE(cursors[t] == 250);
    }
    REQUIRE(Dispatch_getDrainedCount(queue) == 1000);
    REQUIRE(Dispatch_free(queue));
    queue = Dispatch();
    REQUIRE(queue && Dispatch_post(queue, record, nullptr));
    REQUIRE(Dispatch_free(queue)); // dropped: callback is deliberately NOT invoked
    alarm(0);
    puts("Dispatch tests: PASS (FIFO, deferral, reuse, contention, 1000 concurrent jobs)");
    return 0;
}
