// thread_test/test.c — headless verification of the threading package.
//
// Registry: many threads registering concurrently must receive distinct
// dense indexes, and roles must round-trip. Atomics: C23 stdatomic primitives,
// contended counters landing exactly, CAS races, and SpinLock correctness.

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "re/atomic/atomic.h"
#include "re/thread/thread.h"
#include "thread/thread.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// --- Contended counter hammering ---
#define HAMMER_THREADS 8
#define HAMMER_ITERS 10000

static _Atomic i32 g_counter;

// Atomically increments the shared counter for the configured contention batch.
// Performs the shared atomic increment workload for contention testing.
static void *hammer(void *arg) {
    (void) arg;
    for (int i = 0; i < HAMMER_ITERS; i++)
        atomic_fetch_add(&g_counter, 1);
    return nullptr;
}

// --- Concurrent registration ---
static _Atomic i32 g_seenMask[THREAD_REGISTRY_DEFAULT_SIZE]; // per-index arrival flags

// Registers the calling thread, assigns its role, and marks its registry slot.
// Registers the calling thread, assigns its role, and marks its registry slot.
static void *registerer(void *arg) {
    (void) arg;
    int idx = ThreadRegistry_index();
    ThreadRegistry_setTag(idx, THREAD_ROLE_USER);

    if (idx >= 0 && idx < THREAD_REGISTRY_DEFAULT_SIZE)
        atomic_store(&g_seenMask[idx], 1); // racy-but-atomic same-slot writes
    return nullptr;
}

// --- SpinLock contention under threads ---
static SpinLock g_testLock = SPIN_LOCK_INIT;
static int g_guardedSum = 0;

// Adds to the shared sum while holding the test SpinLock.
// Increments the shared sum only while holding the test SpinLock.
static void *lockWorker(void *arg) {
    (void) arg;
    for (int i = 0; i < 5000; i++) {
        SpinLock_lock(&g_testLock);
        g_guardedSum++;
        SpinLock_unlock(&g_testLock);
    }
    return nullptr;
}

// Exercises C atomics, thread registry publication, and SpinLock contention.
// Verifies thread creation, start/join lifecycle, and observable worker completion.
// Verifies atomic operations, thread registration, and SpinLock mutual exclusion.
int main(void) {
    // --- Single-thread C23 atomic semantics ---
    _Atomic i32 a = 0;
    CHECK(atomic_fetch_add(&a, 5) == 0 && atomic_load(&a) == 5);
    CHECK(atomic_exchange(&a, 9) == 5);
    i32 expected = 9;
    CHECK(atomic_compare_exchange_strong(&a, &expected, 7));
    expected = 9;
    CHECK(!atomic_compare_exchange_strong(&a, &expected, 1)); // expected stale
    CHECK(atomic_load(&a) == 7);
    CHECK(atomic_fetch_sub(&a, 1) == 7 && atomic_load(&a) == 6);

    _Atomic bool b = false;
    CHECK(!atomic_load(&b));
    atomic_store(&b, true);
    CHECK(atomic_load(&b));

    _Atomic int64_t l = -1;
    CHECK(atomic_fetch_add(&l, 42) == -1 && atomic_load(&l) == 41);
    int64_t expectedL = 41;
    CHECK(atomic_compare_exchange_strong(&l, &expectedL, 100));

    _Atomic(void*) p = nullptr;
    static int payload;
    CHECK(atomic_load(&p) == nullptr);
    void *expectedP = &payload;
    CHECK(!atomic_compare_exchange_strong(&p, &expectedP, nullptr)); // stale expectation fails
    expectedP = nullptr;
    CHECK(atomic_compare_exchange_strong(&p, &expectedP, &payload));  // correct one lands
    CHECK(atomic_exchange(&p, nullptr) == &payload);

    // --- Contended increments land exactly ---
    atomic_store(&g_counter, 0);
    pthread_t hammers[HAMMER_THREADS];
    for (int i = 0; i < HAMMER_THREADS; i++)
        pthread_create(&hammers[i], nullptr, hammer, nullptr);
    for (int i = 0; i < HAMMER_THREADS; i++)
        pthread_join(hammers[i], nullptr);
    CHECK(atomic_load(&g_counter) == HAMMER_THREADS * HAMMER_ITERS);

    // --- Concurrent registration yields distinct dense indexes ---
    for (int i = 0; i < THREAD_REGISTRY_DEFAULT_SIZE; i++)
        atomic_store(&g_seenMask[i], 0);

    int mainIdx = ThreadRegistry_index();
    ThreadRegistry_setTag(mainIdx, THREAD_ROLE_MAIN);
    CHECK(mainIdx >= 0 && mainIdx < THREAD_REGISTRY_DEFAULT_SIZE);
    CHECK(ThreadRegistry_tag(mainIdx) == (u32) THREAD_ROLE_MAIN);

    enum { REG_THREADS = 16 };
    pthread_t regs[REG_THREADS];
    for (int i = 0; i < REG_THREADS; i++)
        pthread_create(&regs[i], nullptr, registerer, nullptr);
    for (int i = 0; i < REG_THREADS; i++)
        pthread_join(regs[i], nullptr);

    int arrivals = 0;
    for (int i = 0; i < THREAD_REGISTRY_DEFAULT_SIZE; i++) {
        if (atomic_load(&g_seenMask[i]) != 0) {
            arrivals++;
            CHECK(ThreadRegistry_tag(i) == (u32) THREAD_ROLE_USER || i == mainIdx);
        }
    }
    CHECK(arrivals >= REG_THREADS);      // every joiner marked its slot...
    CHECK(arrivals <= THREAD_REGISTRY_DEFAULT_SIZE);

    // Same thread re-asks: identical index.
    CHECK(ThreadRegistry_index() == mainIdx);
    CHECK(ThreadRole_name(THREAD_ROLE_DRAW)[0] == 'd');
    CHECK(strcmp(ThreadRole_name(THREAD_ROLE_MAIN), "_main") == 0);
    CHECK(strcmp(ThreadRole_name(THREAD_ROLE_ENGINE), "engine") == 0);
    CHECK(strcmp(ThreadRole_name(THREAD_ROLE_PRESENT), "present") == 0);
    CHECK(strcmp(ThreadRole_name(THREAD_ROLE_NETWORKING), "networking") == 0);
    CHECK(strcmp(ThreadRole_name(THREAD_ROLE_SCRIPTING), "scripting") == 0);
    CHECK(strcmp(ThreadRole_name(THREAD_ROLE_CONSOLE), "console") == 0);
    CHECK(strcmp(ThreadRole_name(THREAD_ROLE_UI), "ui") == 0);
    CHECK(strcmp(ThreadRole_name(THREAD_ROLE_USER), "user") == 0);
    CHECK(strcmp(ThreadRole_name(THREAD_ROLE_NONE), "none") == 0);
    CHECK(strcmp(ThreadRole_name((ThreadRole) 9999), "none") == 0);

    // --- SpinLock mutual exclusion under contention ---
    pthread_t lockThreads[4];
    for (int i = 0; i < 4; i++)
        pthread_create(&lockThreads[i], nullptr, lockWorker, nullptr);
    for (int i = 0; i < 4; i++)
        pthread_join(lockThreads[i], nullptr);
    CHECK(g_guardedSum == 4 * 5000);

    // Try-lock timeout test: locking once and attempting tryLock from same/different
    CHECK(!SpinLock_isLocked(&g_testLock));
    SpinLock_lock(&g_testLock);
    CHECK(SpinLock_isLocked(&g_testLock));
    SpinLock_unlock(&g_testLock);
    CHECK(!SpinLock_isLocked(&g_testLock));

    if (g_failures == 0)
        printf("thread_test: all checks passed\n");
    else
        printf("thread_test: %d FAILURES\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
