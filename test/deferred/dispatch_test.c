#include "vex.h"
#include <stdlib.h>
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

;;GETTER
static inline DispatchItem * Dispatch_batch(const Dispatch *s) { return (*s).batch; }
;;GETTER
static inline DispatchItem * *Dispatch_batchMut(Dispatch *s) { return &(*s).batch; }
;;SETTER
static inline fn Dispatch_setBatch(Dispatch *s, DispatchItem * v) { (*s).batch = v; }
;;GETTER
static inline DispatchItem * Dispatch_items(const Dispatch *s) { return (*s).items; }
;;GETTER
static inline DispatchItem * *Dispatch_itemsMut(Dispatch *s) { return &(*s).items; }
;;SETTER
static inline fn Dispatch_setItems(Dispatch *s, DispatchItem * v) { (*s).items = v; }
;;GETTER
static inline SpinLock Dispatch_lock(const Dispatch *s) { return (*s).lock; }
;;GETTER
static inline SpinLock *Dispatch_lockMut(Dispatch *s) { return &(*s).lock; }
;;SETTER
static inline fn Dispatch_setLock(Dispatch *s, SpinLock v) { (*s).lock = v; }

    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); exit(1); \
} } while (0)

typedef struct Job {
    Dispatch *queue;
    i32 id;
    i32 *next;
    i32 deliveries;
    atomic_bool done;
    pthread_t consumer;
} Job;

;;GETTER
static inline pthread_t Job_consumer(const Job *s) { return (*s).consumer; }
;;GETTER
static inline pthread_t *Job_consumerMut(Job *s) { return &(*s).consumer; }
;;SETTER
static inline fn Job_setConsumer(Job *s, pthread_t v) { (*s).consumer = v; }
;;GETTER
static inline i32 Job_deliveries(const Job *s) { return (*s).deliveries; }
;;GETTER
static inline i32 *Job_deliveriesMut(Job *s) { return &(*s).deliveries; }
;;SETTER
static inline fn Job_setDeliveries(Job *s, i32 v) { (*s).deliveries = v; }
;;GETTER
static inline atomic_bool Job_done(const Job *s) { return (*s).done; }
;;GETTER
static inline atomic_bool *Job_doneMut(Job *s) { return &(*s).done; }
;;SETTER
static inline fn Job_setDone(Job *s, atomic_bool v) { (*s).done = v; }
;;GETTER
static inline i32 Job_id(const Job *s) { return (*s).id; }
;;GETTER
static inline i32 *Job_idMut(Job *s) { return &(*s).id; }
;;SETTER
static inline fn Job_setId(Job *s, i32 v) { (*s).id = v; }
;;GETTER
static inline i32 * Job_next(const Job *s) { return (*s).next; }
;;GETTER
static inline i32 * *Job_nextMut(Job *s) { return &(*s).next; }
;;SETTER
static inline fn Job_setNext(Job *s, i32 * v) { (*s).next = v; }
;;GETTER
static inline Dispatch * Job_queue(const Job *s) { return (*s).queue; }
;;GETTER
static inline Dispatch * *Job_queueMut(Job *s) { return &(*s).queue; }
;;SETTER
static inline fn Job_setQueue(Job *s, Dispatch * v) { (*s).queue = v; }


// Records one delivery and asserts FIFO order and consumer-thread affinity.
static fn record(any context) {
    Job *job = (Job*) context;
    REQUIRE(pthread_equal(pthread_self(), Job_consumer(job)));
    REQUIRE(Job_id(job) == *Job_next(job));
    (*Job_next(job))++;
    Job_setDeliveries(job, Job_deliveries(job) + 1u);
    REQUIRE(Job_deliveries(job) == 1);
}

// Posts a follow-up job and proves draining/freeing are refused while draining.
static fn repost(any context) {
    Job *job = (Job*) context;
    REQUIRE(Dispatch_post(Job_queue(job), record, job));
    REQUIRE(!Dispatch_drain(Job_queue(job)));
    REQUIRE(!Dispatch_free(Job_queue(job)));
    REQUIRE(Job_deliveries(job) == 0);
}

// Produces one ordered row of jobs, retrying bounded by the process watchdog.
static any producer(any context) {
    Job *jobs = (Job*) context;
    const struct timespec pause = {0, 100000L};
    for (i32 i = 0; i < 250; i++) {
        Job *job = &jobs[i];
        while (!Dispatch_post(Job_queue(job), record, job))
            nanosleep(&pause, nullptr); // test-only retry; process watchdog bounds it
    }
    Job *first = &jobs[0];
    atomic_store_explicit(Job_doneMut(first), true, memory_order_release);
    return nullptr;
}

// Exercises dispatch ordering, deferral, reuse, lock contention, and producers.
i32 main() {
    alarm(20); // A broken queue fails rather than hanging CI or an unbounded join.
    REQUIRE(!Dispatch_post(nullptr, record, nullptr));
    REQUIRE(!Dispatch_drain(nullptr));
    REQUIRE(!Dispatch_free(nullptr));
    REQUIRE(Dispatch_pending(nullptr) == 0 && Dispatch_getDrainedCount(nullptr) == 0);
    Dispatch *queue = Dispatch();
    REQUIRE(queue);
    REQUIRE(!Dispatch_post(queue, nullptr, nullptr));
    REQUIRE(Dispatch_drain(queue));
    i32 next = 0;
    Job jobs[40] = {0};
    for (i32 i = 0; i < 40; i++) {
        Job *job = &jobs[i];
        Job_setId(job, i);
        Job_setNext(job, &next);
        Job_setConsumer(job, pthread_self());
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
    DispatchItem *a = Dispatch_items(queue);
    DispatchItem *b = Dispatch_batch(queue);
    REQUIRE(a && b);
    for (i32 i = 0; i < 10; i++) {
        next = 0;
        deferred.deliveries = 0;
        REQUIRE(Dispatch_post(queue, record, &deferred));
        REQUIRE(Dispatch_drain(queue));
        REQUIRE((Dispatch_items(queue) == a || Dispatch_items(queue) == b)
                && (Dispatch_batch(queue) == a || Dispatch_batch(queue) == b));
    }
    // White-box contention: a failed post/drain neither blocks nor loses state.
    REQUIRE(SpinLock_tryLock(Dispatch_lockMut(queue)));
    REQUIRE(!Dispatch_post(queue, record, nullptr));
    REQUIRE(!Dispatch_drain(queue));
    SpinLock_unlock(Dispatch_lockMut(queue));
    REQUIRE(Dispatch_pending(queue) == 0 && Dispatch_drain(queue));
    REQUIRE(Dispatch_free(queue));

    queue = Dispatch();
    REQUIRE(queue);
    Job concurrent[4][250] = {0};
    pthread_t threads[4];
    i32 cursors[4] = {0};
    for (i32 t = 0; t < 4; t++) {
        Job *row = concurrent[t];
        for (i32 i = 0; i < 250; i++) {
            Job *job = &row[i];
            Job_setQueue(job, queue);
            Job_setId(job, i);
            Job_setNext(job, &cursors[t]);
            Job_setConsumer(job, pthread_self());
            atomic_init(Job_doneMut(job), false);
        }
        REQUIRE(pthread_create(&threads[t], nullptr, producer, row) == 0);
    }
    for (;;) {
        (discard) Dispatch_drain(queue);
        bool done = true;
        for (i32 t = 0; t < 4; t++) {
            Job *first = concurrent[t];
            if (!atomic_load_explicit(Job_doneMut(first), memory_order_acquire))
                done = false;
        }
        if (done && Dispatch_pending(queue) == 0)
            break;
    }
    for (i32 t = 0; t < 4; t++) {
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
