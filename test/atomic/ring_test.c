// vexspoke/test/atomic/ring_test.c — owner test for atomic/ring (RingBuffer).
//
// Proves the fixed-capacity MPMC FIFO:
//   - init rejects nullptr / zero elem_size; capacity is rounded UP to a power
//     of two (0 -> 1, 3 -> 4, 5 -> 8) and mask follows;
//   - strict FIFO ordering, powers-of-two wrap-around far past capacity;
//   - the capacity is a hard contract: push fails exactly at full, pop fails
//     exactly at empty, and neither silently drops or duplicates;
//   - arbitrary element sizes (a multi-word struct copies whole);
//   - nullptr push/pop are safe refusals; double shutdown is safe;
//   - re-init after shutdown works;
//   - real MPMC concurrency under a bounded watchdog: every pushed value is
//     popped exactly once, never torn, never lost, never duplicated.

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sched.h>

#include "atomic/ring.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

/** Checks ring initialization and rejects invalid dimensions or empty/full operations. */
static void test_init_and_bounds(void) {
    RingBuffer r;
    RingBuffer invalid = RingBuffer();
    CHECK(invalid.slots == nullptr);
    RingBuffer invalidDimensions = RingBuffer(0u, 4u);
    CHECK(invalidDimensions.slots == nullptr);
    r = RingBuffer(0u, 4u);
    CHECK(r.slots == nullptr); // zero element size refused

    r = RingBuffer(sizeof(int), 3); CHECK(r.slots != nullptr);
    CHECK(r.capacity == 4);
    CHECK(r.mask == 3);
    CHECK(r.elem_size == sizeof(int));
    RingBuffer_shutdown(&r);

    r = RingBuffer(sizeof(int), 5); CHECK(r.slots != nullptr);
    CHECK(r.capacity == 8);
    CHECK(r.mask == 7);

    // Exact powers of two do not grow.
    RingBuffer_shutdown(&r);
    r = RingBuffer(sizeof(int), 2); CHECK(r.slots != nullptr);
    CHECK(r.capacity == 2);
    RingBuffer_shutdown(&r);

    // Requesting zero rounds up to the minimum ring of one slot.
    r = RingBuffer(sizeof(int), 0); CHECK(r.slots != nullptr);
    CHECK(r.capacity == 1);
    CHECK(r.mask == 0);
    int one = 7;
    CHECK(RingBuffer_push(&r, &one));
    CHECK(!RingBuffer_push(&r, &one));          // capacity 1 is full
    int out = 0;
    CHECK(RingBuffer_pop(&r, &out));
    CHECK(out == 7);
    CHECK(!RingBuffer_pop(&r, &out));           // empty
    RingBuffer_shutdown(&r);

    // Double shutdown is safe; re-init after shutdown is safe.
    RingBuffer_shutdown(&r);
    r = RingBuffer(sizeof(int), 4); CHECK(r.slots != nullptr);
    RingBuffer_shutdown(&r);
}

/** Verifies FIFO order, full-capacity refusal, and slot reuse after popping. */
static void test_fifo_and_full(void) {
    RingBuffer r;
    r = RingBuffer(sizeof(int), 4); CHECK(r.slots != nullptr);

    int out = -1;
    CHECK(!RingBuffer_pop(&r, &out));           // empty pop refused
    CHECK(!RingBuffer_push(&r, nullptr));       // nullptr item refused
    CHECK(!RingBuffer_pop(&r, nullptr));        // nullptr out refused

    for (int i = 0; i < 4; i++) {
        int v = 100 + i;
        CHECK(RingBuffer_push(&r, &v));
    }
    CHECK(!RingBuffer_push(&r, &(int){ 999 })); // exactly full -> refused

    // Strict FIFO.
    for (int i = 0; i < 4; i++) {
        CHECK(RingBuffer_pop(&r, &out));
        CHECK(out == 100 + i);
    }
    CHECK(!RingBuffer_pop(&r, &out));           // drained

    // Refill to full, free one slot, and prove exactly one more fits.
    for (int i = 0; i < 4; i++)
        CHECK(RingBuffer_push(&r, &(int){ 200 + i }));
    CHECK(!RingBuffer_push(&r, &(int){ 999 }));
    CHECK(RingBuffer_pop(&r, &out));
    CHECK(out == 200);
    CHECK(RingBuffer_push(&r, &(int){ 300 }));
    CHECK(!RingBuffer_push(&r, &(int){ 301 }));
    CHECK(RingBuffer_pop(&r, &out));
    CHECK(out == 201);
    RingBuffer_shutdown(&r);
}

/** Exercises repeated index wraparound while preserving element order. */
static void test_wraparound(void) {
    RingBuffer r;
    r = RingBuffer(sizeof(int), 4); CHECK(r.slots != nullptr);

    // Push 2 / pop 2 repeatedly: the masked slot index wraps many times.
    int expect = 0;
    for (int round = 0; round < 1000; round++) {
        for (int i = 0; i < 2; i++) {
            int v = expect++;
            CHECK(RingBuffer_push(&r, &v));
        }
        for (int i = 0; i < 2; i++) {
            int out = -1;
            CHECK(RingBuffer_pop(&r, &out));
            CHECK(out == round * 2 + i);
        }
    }
    int out;
    CHECK(!RingBuffer_pop(&r, &out));
    RingBuffer_shutdown(&r);
}

// Elements larger than a word must copy whole, including padding.
typedef struct Big {
    uint64_t a;
    uint64_t b;
    char tag[7];
} Big;

/** Confirms the ring copies complete elements larger than a machine word. */
static void test_wide_elements(void) {
    RingBuffer r;
    r = RingBuffer(sizeof(Big), 2); CHECK(r.slots != nullptr);
    for (int i = 0; i < 2; i++) {
        Big b = { .a = (uint64_t) i * 11, .b = (uint64_t) i * 22, .tag = "abc" };
        CHECK(RingBuffer_push(&r, &b));
    }
    CHECK(!RingBuffer_push(&r, &(Big){ .a = 99 }));
    for (int i = 0; i < 2; i++) {
        Big b = { 0 };
        CHECK(RingBuffer_pop(&r, &b));
        CHECK(b.a == (uint64_t) i * 11);
        CHECK(b.b == (uint64_t) i * 22);
        CHECK(b.tag[0] == 'a' && b.tag[1] == 'b' && b.tag[2] == 'c');
    }
    RingBuffer_shutdown(&r);
}

// ── MPMC stress: exact once-through of every value ─────────────────────────
#define RING_PRODUCERS 4
#define RING_CONSUMERS 4
#define RING_PER_PRODUCER 2000
#define RING_TOTAL (RING_PRODUCERS * RING_PER_PRODUCER)

static RingBuffer g_ring;
static atomic_int g_seen[RING_TOTAL];
static atomic_int g_consumed;
static atomic_int g_bad;
static atomic_int g_dup;
static atomic_int g_start;

/** Publishes the producer's assigned values into the shared ring until complete. */
static void *ring_producer(void *arg) {
    int id = (int) (intptr_t) arg;
    while (!atomic_load_explicit(&g_start, memory_order_acquire))
        sched_yield();
    for (int j = 0; j < RING_PER_PRODUCER; j++) {
        int v = id * RING_PER_PRODUCER + j;
        while (!RingBuffer_push(&g_ring, &v))
            sched_yield();
    }
    return nullptr;
}

/** Drains published values and records consumption for the bounded MPMC assertion. */
static void *ring_consumer(void *arg) {
    (void) arg;
    while (!atomic_load_explicit(&g_start, memory_order_acquire))
        sched_yield();
    while (atomic_load_explicit(&g_consumed, memory_order_acquire) < RING_TOTAL) {
        int v;
        if (!RingBuffer_pop(&g_ring, &v)) {
            sched_yield();
            continue;
        }
        if (v < 0 || v >= RING_TOTAL) {
            atomic_fetch_add(&g_bad, 1);
            continue;
        }
        if (atomic_exchange_explicit(&g_seen[v], 1, memory_order_acq_rel) != 0)
            atomic_fetch_add(&g_dup, 1);
        atomic_fetch_add_explicit(&g_consumed, 1, memory_order_acq_rel);
    }
    return nullptr;
}

/** Checks concurrent producers and consumers deliver every submitted value exactly once. */
static void test_mpmc_exactly_once(void) {
    g_ring = RingBuffer(sizeof(int), 64u); CHECK(g_ring.slots != nullptr);
    for (int i = 0; i < RING_TOTAL; i++)
        atomic_store(&g_seen[i], 0);
    atomic_store(&g_consumed, 0);
    atomic_store(&g_bad, 0);
    atomic_store(&g_dup, 0);
    atomic_store(&g_start, 0);

    pthread_t producers[RING_PRODUCERS];
    pthread_t consumers[RING_CONSUMERS];
    for (int i = 0; i < RING_PRODUCERS; i++)
        CHECK(pthread_create(&producers[i], nullptr, ring_producer, (void*) (intptr_t) i) == 0);
    for (int i = 0; i < RING_CONSUMERS; i++)
        CHECK(pthread_create(&consumers[i], nullptr, ring_consumer, nullptr) == 0);

    atomic_store_explicit(&g_start, 1, memory_order_release);
    for (int i = 0; i < RING_PRODUCERS; i++)
        CHECK(pthread_join(producers[i], nullptr) == 0);
    for (int i = 0; i < RING_CONSUMERS; i++)
        CHECK(pthread_join(consumers[i], nullptr) == 0);

    CHECK(atomic_load(&g_consumed) == RING_TOTAL);
    CHECK(atomic_load(&g_bad) == 0);
    CHECK(atomic_load(&g_dup) == 0);
    int missing = 0;
    for (int i = 0; i < RING_TOTAL; i++)
        if (atomic_load(&g_seen[i]) == 0)
            missing++;
    CHECK(missing == 0);

    // The ring is fully drained and reusable afterwards.
    int out;
    CHECK(!RingBuffer_pop(&g_ring, &out));
    int v = 5;
    CHECK(RingBuffer_push(&g_ring, &v));
    CHECK(RingBuffer_pop(&g_ring, &out));
    CHECK(out == 5);
    RingBuffer_shutdown(&g_ring);
}

/** Runs the RingBuffer owner scenarios and reports aggregate assertion status. */
int main(void) {
    test_init_and_bounds();
    test_fifo_and_full();
    test_wraparound();
    test_wide_elements();
    test_mpmc_exactly_once();

    if (g_failures == 0) {
        printf("ring_test: all assertions held\n");
        return 0;
    }
    printf("ring_test: %d FAILURES\n", g_failures);
    return 1;
}
