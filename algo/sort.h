#ifndef ALGO_SORT_H
#define ALGO_SORT_H

#include "vex.h"

// algo/sort.h — in-place comparison sorts and unsigned radix sorts.
//
// Comparison sorts operate on caller-owned arrays and require a comparator
// whose ordering remains consistent for the duration of the call. They do not
// allocate. Sort_mergeSort requires a non-overlapping caller-owned scratch buffer of
// count * elementSize bytes. Radix sorts use the Relational Engine Memory ABI.

typedef int (*SortCompareFn)(const_any left, const_any right);

bool Sort_quickSort(any values, bytes count, bytes elementSize,
                    SortCompareFn compare);
bool Sort_insertionSort(any values, bytes count, bytes elementSize,
                        SortCompareFn compare);
bool Sort_selectionSort(any values, bytes count, bytes elementSize,
                        SortCompareFn compare);
bool Sort_bubbleSort(any values, bytes count, bytes elementSize,
                     SortCompareFn compare);
bool Sort_heapSort(any values, bytes count, bytes elementSize,
                   SortCompareFn compare);
bool Sort_mergeSort(any values, bytes count, bytes elementSize,
                    SortCompareFn compare, any scratch);

// Two-way partition: [0, boundary) is below pivot; the remainder is greater
// than or equal to pivot. This operation is not stable. Returns zero for
// nullptr or an empty range.
bytes Sort_partitionInt32(i32 *values, bytes count, i32 pivot);

// Stable unsigned LSD radix sorts. Return false for invalid input or allocation
// failure; on allocation failure the input arrays remain unchanged.
bool Sort_radixSortUint32(u32 *values, bytes count);
bool Sort_radixSortUint64(u64 *values, bytes count);
bool Sort_radixSortPairsUint64(u64 *keys, any *values, bytes count);

#endif
