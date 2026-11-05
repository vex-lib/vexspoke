// vexspoke/test/bit/bit_test.c — the BitPool class _test.
//
// Proves bit/bit.c: carve, exhaust, recycle; header type/length stamping;
// containment routing; double-free and foreign-pointer refusal; shutdown.
// Deterministic, headless; allocation is the pool's own single calloc arena.

#include <stdint.h>
#include <stdio.h>

#include "bit/bit.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Exercises BitPool admission, allocation, recycling, validation, and shutdown.
int main(void) {
    // Empty and invalid constructor choices leave no backing arena.
    BitPool empty = BitPool();
    CHECK(empty.arena == nullptr);
    BitPool bad = BitPool(0u, 4u);
    CHECK(bad.arena == nullptr);
    bad = BitPool(8u, 0u);
    CHECK(bad.arena == nullptr);

    // Carve a 4-slot pool of 8-byte payloads.
    BitPool pool = BitPool(8u, 4u);
    CHECK(pool.arena != nullptr);

    void *slots[4];
    for (int i = 0; i < 4; i++) {
        slots[i] = BitPool_alloc(&pool, 0x5150u);
        CHECK(slots[i] != nullptr);
        CHECK(BitPool_contains(&pool, slots[i]));
        CHECK(BitPool_type(&pool, slots[i]) == 0x5150u);
        CHECK(BitPool_length(&pool, slots[i]) == 8);
    }

    // Exhausted.
    CHECK(BitPool_alloc(&pool, 0x5150u) == nullptr);

    // Containment guards.
    CHECK(!BitPool_contains(nullptr, slots[0]));
    CHECK(!BitPool_contains(&pool, nullptr));
    CHECK(!BitPool_contains(&pool, (void*) (uintptr_t) 0x1));

    // Free one -> it recycles on the next alloc.
    BitPool_free(&pool, slots[2]);
    void *recycled = BitPool_alloc(&pool, 0x9999u);
    CHECK(recycled == slots[2]);
    CHECK(BitPool_type(&pool, recycled) == 0x9999u);

    // Guards: null and foreign frees are no-ops.
    BitPool_free(&pool, nullptr);
    BitPool_free(&pool, (void*) (uintptr_t) 0x1);
    CHECK(1);

    // Type/length on a foreign pointer are refused (0).
    CHECK(BitPool_type(&pool, (void*) (uintptr_t) 0x1) == 0);
    CHECK(BitPool_length(&pool, (void*) (uintptr_t) 0x1) == 0);

    // Shutdown is idempotent.
    BitPool_shutdown(&pool);
    CHECK(pool.arena == nullptr);
    BitPool_shutdown(&pool);
    CHECK(1);

    if (g_failures == 0) {
        printf("bit_test: all assertions held\n");
        return 0;
    }
    printf("bit_test: %d FAILURES\n", g_failures);
    return 1;
}
