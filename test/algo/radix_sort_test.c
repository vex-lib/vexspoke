// vexspoke/test/algo/radix_sort_test.c — owner tests for Sort radix methods.
//
// LSD radix sort is verified against a qsort ORACLE over deterministic
// pseudo-random arrays, plus the boundary rows:
//   - count 0 / count 1 are successful no-ops; nullptr with count > 1 is
//     rejected without touching input;
//   - value boundaries: 0, 1, UINT32_MAX/UINT64_MAX, and values that differ
//     only in the top byte (all 4/8 passes must run);
//   - duplicates are preserved as a multiset, and the result is fully ordered;
//   - keys and values in pairsU64 stay paired through the sort, with stable
//     ordering for equal keys (LSD radix is stable);
//   - nullptr keys or values in pairsU64 is rejected without touching keys.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "algo/sort.h"
#include "nio/mem.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

/** Returns whether the supplied u32 prefix is in nondecreasing order. */
static bool sorted_u32(const u32 *a, bytes n) {
    for (bytes i = 1; i < n; i++)
        if (a[i - 1] > a[i])
            return false;
    return true;
}

/** Returns whether the supplied u64 prefix is in nondecreasing order. */
static bool sorted_u64(const u64 *a, bytes n) {
    for (bytes i = 1; i < n; i++)
        if (a[i - 1] > a[i])
            return false;
    return true;
}

/** qsort comparator used as an independent oracle for u32 sorting. */
static int cmp_u32(const void *a, const void *b) {
    u32 x = *(const u32*) a, y = *(const u32*) b;
    return (x > y) - (x < y);
}

/** qsort comparator used as an independent oracle for u64 sorting. */
static int cmp_u64(const void *a, const void *b) {
    u64 x = *(const u64*) a, y = *(const u64*) b;
    return (x > y) - (x < y);
}

/** Checks empty, singleton, and already ordered input cases. */
static void test_trivial(void) {
    CHECK(!Sort_radixSortUint32(nullptr, 16u));
    CHECK(!Sort_radixSortUint64(nullptr, 16u));
    CHECK(!Sort_radixSortPairsUint64(nullptr, nullptr, 16u));

    u32 one[1] = { 7 };
    CHECK(Sort_radixSortUint32(one, 1u));
    CHECK(one[0] == 7);
    CHECK(Sort_radixSortUint32(one, 0u));
    CHECK(one[0] == 7);

    u64 big[1] = { 0x123456789abcdefull };
    CHECK(Sort_radixSortUint64(big, 1u));
    CHECK(big[0] == 0x123456789abcdefull);

    u64 keys[1] = { 5 };
    void *vals[1] = { (void*) (pointer) 9 };
    CHECK(Sort_radixSortPairsUint64(keys, vals, 1u));
    CHECK(keys[0] == 5 && (pointer) vals[0] == 9);

    // pairsU64 with a null side is a no-op, not a crash.
    u64 k2[2] = { 2, 1 };
    CHECK(!Sort_radixSortPairsUint64(k2, nullptr, 2u));
    CHECK(k2[0] == 2 && k2[1] == 1);
    CHECK(!Sort_radixSortPairsUint64(nullptr, vals, 2u));
}

/** Exercises unsigned value extremes and duplicate values for radix sorting. */
static void test_value_boundaries(void) {
    u32 a[] = { UINT32_MAX, 0, 1, 0x01000000u, 0x00FFFFFFu, 0, UINT32_MAX };
    bytes n = sizeof(a) / sizeof(a[0]);
    CHECK(Sort_radixSortUint32(a, n));
    CHECK(sorted_u32(a, n));
    CHECK(a[0] == 0 && a[1] == 0);
    CHECK(a[n - 1] == UINT32_MAX && a[n - 2] == UINT32_MAX);
    CHECK(a[2] == 1);

    u64 b[] = { UINT64_MAX, 0, 1, 0x0100000000000000ull, 0x00FFFFFFFFFFFFFFull };
    bytes m = sizeof(b) / sizeof(b[0]);
    CHECK(Sort_radixSortUint64(b, m));
    CHECK(sorted_u64(b, m));
    CHECK(b[0] == 0 && b[1] == 1);
    CHECK(b[m - 1] == UINT64_MAX);
}

/** Compares deterministic randomized u32 results against the standard-library oracle. */
static void test_oracle_random(void) {
    enum { N = 4096 };
    u32 a[N], ref[N];
    u64 seed = 0x12345678u;
    for (bytes i = 0; i < N; i++) {
        seed = seed * 6364136223846793005ull + 1442695040888963407ull;
        a[i] = (u32) (seed >> 32);
        ref[i] = a[i];
    }
    CHECK(Sort_radixSortUint32(a, N));
    qsort(ref, N, sizeof(u32), cmp_u32);
    CHECK(sorted_u32(a, N));
    CHECK(memcmp(a, ref, sizeof(a)) == 0);
}

/** Compares deterministic randomized u64 results against the standard-library oracle. */
static void test_oracle_random_u64(void) {
    enum { N = 2048 };
    u64 a[N], ref[N];
    u64 seed = 0xfeedfacecafebeefull;
    for (bytes i = 0; i < N; i++) {
        seed = seed * 6364136223846793005ull + 1442695040888963407ull;
        a[i] = seed ^ (seed << 17);
        ref[i] = a[i];
    }
    CHECK(Sort_radixSortUint64(a, N));
    qsort(ref, N, sizeof(u64), cmp_u64);
    CHECK(sorted_u64(a, N));
    CHECK(memcmp(a, ref, sizeof(a)) == 0);
}

/** Verifies key/value pairs remain associated and ordered by key after sorting. */
static void test_pairs(void) {
    // Keys with duplicates; the values are distinct tags. Stability means the
    // tags of equal keys keep their original relative order.
    u64 keys[]      = { 5, 1, 5, 3, 1, 5, 0, 3 };
    pointer tags[]     = { 50, 11, 51, 31, 12, 52, 70, 32 };
    void *vals[8];
    for (int i = 0; i < 8; i++)
        vals[i] = (void*) tags[i];

    CHECK(Sort_radixSortPairsUint64(keys, vals, 8u));
    CHECK(sorted_u64(keys, 8));

    // Reconstruct the (key, tag) multiset and check stability per key.
    u64 expectKeys[] = { 0, 1, 1, 3, 3, 5, 5, 5 };
    pointer expectTags[] = { 70, 11, 12, 31, 32, 50, 51, 52 };
    for (int i = 0; i < 8; i++) {
        CHECK(keys[i] == expectKeys[i]);
        CHECK((pointer) vals[i] == expectTags[i]);
    }
}

/** Runs the Sort radix owner scenarios and reports aggregate assertion status. */
int main(void) {
    CHECK(Memory_init(0));

    test_trivial();
    test_value_boundaries();
    test_oracle_random();
    test_oracle_random_u64();
    test_pairs();
    Memory_freeAll();

    if (g_failures == 0) {
        printf("sort_radix_test: all assertions held\n");
        return 0;
    }
    printf("sort_radix_test: %d FAILURES\n", g_failures);
    return 1;
}
