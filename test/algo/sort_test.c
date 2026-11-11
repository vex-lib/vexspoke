// vexspoke/test/algo/sort_test.c — owner tests for the Sort algorithms.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "algo/sort.h"

static int g_failures = 0;

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #condition);      \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

static int compareUint64(const void *left, const void *right) {
    u64 a = *(const u64*) left;
    u64 b = *(const u64*) right;
    return (a > b) - (a < b);
}

static int compareInt32(const void *left, const void *right) {
    i32 a = *(const i32*) left;
    i32 b = *(const i32*) right;
    return (a > b) - (a < b);
}

typedef struct StableItem {
    u32 key;
    u32 sequence;
} StableItem;

static int compareStableItem(const void *left, const void *right) {
    const StableItem *a = left;
    const StableItem *b = right;
    return ((*a).key > (*b).key) - ((*a).key < (*b).key);
}

typedef bool (*SortFunction)(void *, bytes, bytes, SortCompareFn);

static bool isSortedUint64(const u64 *values, bytes count) {
    for (bytes i = 1u; i < count; ++i)
        if (values[i - 1u] > values[i])
            return false;
    return true;
}

static void checkSort(SortFunction sort, const u64 *input, bytes count) {
    u64 actual[128];
    u64 expected[128];
    CHECK(count <= 128u);
    if (count > 128u)
        return;
    memcpy(actual, input, count * sizeof(u64));
    memcpy(expected, input, count * sizeof(u64));
    qsort(expected, count, sizeof(u64), compareUint64);
    CHECK(sort(actual, count, sizeof(u64), compareUint64));
    CHECK(isSortedUint64(actual, count));
    CHECK(memcmp(actual, expected, count * sizeof(u64)) == 0);
}

static void testComparisonSorts(void) {
    const u64 values[] = {
        UINT64_MAX, 7u, 2u, 7u, 0u, 0x8000000000000000ull, 1u, 5u
    };
    const bytes count = sizeof values / sizeof values[0];
    checkSort(Sort_quickSort, values, count);
    checkSort(Sort_insertionSort, values, count);
    checkSort(Sort_selectionSort, values, count);
    checkSort(Sort_bubbleSort, values, count);
    checkSort(Sort_heapSort, values, count);

    u64 actual[128];
    u64 expected[128];
    for (bytes i = 0u; i < 128u; ++i) {
        u64 seed = (u64) i * 6364136223846793005ull + 1442695040888963407ull;
        actual[i] = seed ^ (seed >> 13u);
        expected[i] = actual[i];
    }
    qsort(expected, 128u, sizeof(u64), compareUint64);
    CHECK(Sort_quickSort(actual, 128u, sizeof(u64), compareUint64));
    CHECK(memcmp(actual, expected, sizeof actual) == 0);

    for (bytes i = 0u; i < 128u; ++i)
        actual[i] = (u64) i;
    CHECK(Sort_quickSort(actual, 128u, sizeof(u64), compareUint64));
    CHECK(isSortedUint64(actual, 128u));
    for (bytes i = 0u; i < 128u; ++i)
        actual[i] = 7u;
    CHECK(Sort_quickSort(actual, 128u, sizeof(u64), compareUint64));
    CHECK(isSortedUint64(actual, 128u));
}

static bool stableOrder(const StableItem *values, bytes count) {
    for (bytes i = 1u; i < count; ++i) {
        if (values[i - 1u].key == values[i].key
            && values[i - 1u].sequence > values[i].sequence)
            return false;
    }
    return true;
}

static void makeStableItems(StableItem *values) {
    values[0].key = 2u; values[0].sequence = 0u;
    values[1].key = 1u; values[1].sequence = 1u;
    values[2].key = 2u; values[2].sequence = 2u;
    values[3].key = 1u; values[3].sequence = 3u;
    values[4].key = 2u; values[4].sequence = 4u;
}

static void testStableSorts(void) {
    StableItem insertion[5];
    StableItem bubble[5];
    StableItem merge[5];
    StableItem scratch[5];
    makeStableItems(insertion);
    makeStableItems(bubble);
    makeStableItems(merge);
    CHECK(Sort_insertionSort(insertion, 5u, sizeof(StableItem), compareStableItem));
    CHECK(Sort_bubbleSort(bubble, 5u, sizeof(StableItem), compareStableItem));
    CHECK(Sort_mergeSort(merge, 5u, sizeof(StableItem), compareStableItem, scratch));
    CHECK(stableOrder(insertion, 5u));
    CHECK(stableOrder(bubble, 5u));
    CHECK(stableOrder(merge, 5u));
    CHECK(insertion[0].sequence == 1u && insertion[1].sequence == 3u);
    CHECK(merge[2].sequence == 0u && merge[3].sequence == 2u);
}

static void testMergeBoundaries(void) {
    i32 values[] = { 4, 1, 3, 2 };
    i32 scratch[4];
    CHECK(!Sort_mergeSort(values, 4u, sizeof(i32), compareInt32, nullptr));
    CHECK(values[0] == 4 && values[1] == 1 && values[2] == 3 && values[3] == 2);
    CHECK(!Sort_mergeSort(values, 4u, sizeof(i32), compareInt32, values));
    CHECK(Sort_mergeSort(values, 4u, sizeof(i32), compareInt32, scratch));
    CHECK(values[0] == 1 && values[1] == 2 && values[2] == 3 && values[3] == 4);
}

static void testInvalidRangesAndPartition(void) {
    i32 value = 7;
    CHECK(Sort_quickSort(nullptr, 1u, sizeof value, compareInt32) == false);
    CHECK(Sort_quickSort(&value, 1u, 0u, compareInt32) == false);
    CHECK(Sort_quickSort(&value, SIZE_MAX, sizeof value, compareInt32) == false);
    CHECK(Sort_quickSort(nullptr, 0u, 0u, nullptr));
    CHECK(Sort_insertionSort(nullptr, 0u, 0u, nullptr));
    CHECK(Sort_selectionSort(nullptr, 0u, 0u, nullptr));
    CHECK(Sort_bubbleSort(nullptr, 0u, 0u, nullptr));
    CHECK(Sort_heapSort(nullptr, 0u, 0u, nullptr));
    CHECK(Sort_mergeSort(nullptr, 0u, 0u, nullptr, nullptr));

    i32 keys[] = { 3, 1, 2, 5, 4, -1, 3 };
    bytes boundary = Sort_partitionInt32(keys, 7u, 3);
    CHECK(boundary == 3u);
    for (bytes i = 0u; i < boundary; ++i)
        CHECK(keys[i] < 3);
    for (bytes i = boundary; i < 7u; ++i)
        CHECK(keys[i] >= 3);
    CHECK(Sort_partitionInt32(nullptr, 5u, 0) == 0u);
    CHECK(Sort_partitionInt32(keys, 0u, 0) == 0u);
}

int main(void) {
    testComparisonSorts();
    testStableSorts();
    testMergeBoundaries();
    testInvalidRangesAndPartition();
    if (g_failures == 0) {
        puts("sort_test: all assertions held");
        return 0;
    }
    printf("sort_test: %d failures\n", g_failures);
    return 1;
}
