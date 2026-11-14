// vexspoke/test/util/arrays_test.c — the Arrays class _test.
//
// Proves util/arrays.c: in-place quicksort (int32/int64), binary search with
// the legacy -(insertion+1) miss encoding, and byte fill/copy. Deterministic,
// headless, no allocation.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "util/arrays.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Returns whether the supplied signed integer slice is in nondecreasing order.
// Returns whether the supplied integer span is in nondecreasing order.
// Returns whether the supplied int32 sequence is in nondecreasing order.
static bool intSorted(const i32 *data, bytes n) {
    for (bytes i = 1; i < n; i++)
        if (data[i - 1] > data[i])
            return false;
    return true;
}

// Exercises in-place sorting, binary-search miss encoding, and byte utilities.
// Exercises array utilities and checks their ordering and boundary behavior.
// Verifies in-place sorting, binary-search insertion encoding, and byte operations.
int main() {
    // sortInt: a scrambled buffer becomes ascending and keeps its multiset.
    i32 a[] = { 5, 3, 9, 1, 3, 7, 0, -2, 8, 3 };
    bytes n = sizeof a / sizeof a[0];
    Arrays_sortInt(a, n);
    CHECK(intSorted(a, n));
    CHECK(a[0] == -2 && a[n - 1] == 9);
    CHECK(a[2] == 1 && a[3] == 3 && a[4] == 3);

    // Empty and single-element sorts are no-ops.
    Arrays_sortInt(a, 0);
    Arrays_sortInt(a, 1);
    CHECK(1);

    // sortLong.
    int64_t b[] = { 40, -5, 12, 7, -100, 7 };
    bytes nb = sizeof b / sizeof b[0];
    Arrays_sortLong(b, nb);
    CHECK(b[0] == -100 && b[nb - 1] == 40);
    CHECK(b[1] == -5 && b[2] == 7 && b[3] == 7);

    // binarySearchInt: found index; miss encodes -(insertion + 1).
    i32 sorted[] = { 1, 3, 5, 7, 9 };
    CHECK(Arrays_binarySearchInt(sorted, 5, 1) == 0);
    CHECK(Arrays_binarySearchInt(sorted, 5, 9) == 4);
    CHECK(Arrays_binarySearchInt(sorted, 5, 5) == 2);
    CHECK(Arrays_binarySearchInt(sorted, 5, 0) == -1);   // insertion 0
    CHECK(Arrays_binarySearchInt(sorted, 5, 4) == -3);   // insertion 2
    CHECK(Arrays_binarySearchInt(sorted, 5, 100) == -6); // insertion 5

    // binarySearchLong.
    int64_t sortedL[] = { 10, 20, 30 };
    CHECK(Arrays_binarySearchLong(sortedL, 3, 10) == 0);
    CHECK(Arrays_binarySearchLong(sortedL, 3, 30) == 2);
    CHECK(Arrays_binarySearchLong(sortedL, 3, 25) == -3);

    // fill + copy.
    u8 buf[16];
    Arrays_fill(buf, sizeof buf, 0xAB);
    for (bytes i = 0; i < sizeof buf; i++)
        CHECK(buf[i] == 0xAB);
    u8 dst[16];
    memset(dst, 0, sizeof dst);
    Arrays_copy(buf, dst, sizeof buf);
    CHECK(memcmp(buf, dst, sizeof buf) == 0);

    if (g_failures == 0) {
        printf("arrays_test: all assertions held\n");
        return 0;
    }
    printf("arrays_test: %d FAILURES\n", g_failures);
    return 1;
}
