// vexspoke/test/struct/minheap_test.c — the MinHeap class _test.
//
// A fixed-capacity binary min-heap (1-based). Proves heap order (popItem yields
// ascending priorities), the capacity-full rejection, and null-safety.

#include <stdint.h>
#include <stdio.h>

#include "struct/minheap.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Verifies minimum-heap ordering through insertion, peek, removal, and empty cases.
int main(void) {
    MinHeap *h = MinHeap_1(8);
    CHECK(h != nullptr);
    CHECK(MinHeap_isEmpty(h));
    CHECK(MinHeap_size(h) == 0);
    // The Exhaustion Loudness Law accessor: a fresh owner has no refusals.
    CHECK(MinHeap_exhaustionCount(h) == 0);
    CHECK(MinHeap_capacity(h) >= 8);

    // Push out of order; popItem yields ascending priority.
    CHECK(MinHeap_push(h, 30, 3.0f) == 1);
    CHECK(MinHeap_push(h, 10, 1.0f) == 1);
    CHECK(MinHeap_push(h, 20, 2.0f) == 1);
    CHECK(MinHeap_size(h) == 3);
    CHECK(MinHeap_peekItem(h) == 10);
    CHECK(MinHeap_popItem(h) == 10);
    CHECK(MinHeap_popItem(h) == 20);
    CHECK(MinHeap_popItem(h) == 30);
    CHECK(MinHeap_popItem(h) == 0);   // empty
    CHECK(MinHeap_peekItem(h) == 0);

    // Volume within capacity: 2048 pushed in reverse priority, popped ascending.
    MinHeap *big = MinHeap_1(2048);
    CHECK(big != nullptr);
    for (int i = 2047; i >= 0; i--)
        CHECK(MinHeap_push(big, i, (float) i) == 1);
    CHECK(MinHeap_size(big) == 2048);
    // A full fixed-capacity heap is a refusal, not a silent no-op: the push is
    // rejected and the refusal is COUNTED (the Exhaustion Loudness Law).
    CHECK(MinHeap_exhaustionCount(big) == 0);
    CHECK(MinHeap_push(big, 99, 99.0f) == 0);
    CHECK(MinHeap_exhaustionCount(big) == 1);
    CHECK(MinHeap_push(big, 98, 98.0f) == 0);
    CHECK(MinHeap_exhaustionCount(big) == 2);
    CHECK(MinHeap_size(big) == 2048);   // unchanged by a refused push
    int prev = -1;
    int ok = 1;
    for (int i = 0; i < 2048; i++) {
        int item = MinHeap_popItem(big);
        if (item <= prev)
            ok = 0;
        prev = item;
    }
    CHECK(ok); // strictly ascending

    // Capacity-full rejection (fixed capacity).
    MinHeap *full = MinHeap_1(2);
    CHECK(MinHeap_push(full, 1, 1.0f) == 1);
    CHECK(MinHeap_push(full, 2, 2.0f) == 1);
    CHECK(MinHeap_push(full, 3, 3.0f) == 0); // full -> refused

    // Null-safety.
    CHECK(MinHeap_push(nullptr, 1, 1.0f) == 0);
    CHECK(MinHeap_popItem(nullptr) == 0);
    CHECK(MinHeap_peekItem(nullptr) == 0);
    CHECK(MinHeap_size(nullptr) == 0);
    CHECK(MinHeap_isEmpty(nullptr));
    MinHeap_free(nullptr);

    MinHeap_free(full);
    MinHeap_free(big);
    MinHeap_free(h);

    if (g_failures == 0) {
        printf("minheap_test: all assertions held\n");
        return 0;
    }
    printf("minheap_test: %d FAILURES\n", g_failures);
    return 1;
}
