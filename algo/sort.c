#include "vex.h"
#include "algo/sort.h"

#include "oop/vexspoke_type.h"
#include "nio/mem.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Sort
 * ============================================================================
 * Stateless sorting algorithms over caller-owned arrays. Comparison sorts
 * use the caller's comparator and never allocate. Merge sort receives its
 * scratch space from the caller. Unsigned radix sorts use temporary storage
 * from the Relational Engine Memory ABI and report allocation failure without
 * partially reordering the input.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Sort (algo/sort.c)
 * ============================================================================
 * In-place comparison sorts, stable merge sort, integer radix sorts and a
 * signed 32-bit partition helper.
 *
 * STRUCT FIELDS: none — procedural algorithms over caller-owned arrays
 *
 * PRIVATE HELPERS:
 *   swapElements(left, right, size)       // swap fixed-width records
 *   insertionSortRange(values, begin, end, size, compare) // stable small sort
 *   siftDown(values, root, count, size, compare) // restore max heap
 *   heapSortRange(values, begin, end, size, compare) // in-place heap sort
 *   partitionRange(values, begin, end, size, compare) // pivot partition
 *   quickSortRange(values, begin, end, size, compare, depth) // introspective quicksort
 *   unsignedKeyAt(values, index, size)    // read a u32/u64 radix key
 *   radixSortUnsigned(values, count, size) // stable byte-wise radix passes
 *
 * FUNCTION REGISTRY:
 *   Quick sort, insertion sort, selection sort, bubble sort, heap sort,
 *   merge sort, int32 partition, uint32/uint64 radix sort and paired-u64 radix.
 * ============================================================================
 */

static unsigned char *elementAt(any values, bytes index, bytes elementSize) {
    return (unsigned char*) values + index * elementSize;
}

static const unsigned char *constElementAt(const_any values, bytes index,
                                            bytes elementSize) {
    return (const unsigned char*) values + index * elementSize;
}

static fn swapElements(any left, any right, bytes elementSize) {
    if (left == right)
        return;
    unsigned char *a = left;
    unsigned char *b = right;
    for (bytes i = 0u; i < elementSize; ++i) {
        unsigned char byte = a[i];
        a[i] = b[i];
        b[i] = byte;
    }
}

static bool validComparisonRange(const_any values, bytes count,
                                 bytes elementSize, SortCompareFn compare) {
    if (count == 0u)
        return true;
    return values != nullptr && elementSize > 0u && compare != nullptr
        && count <= SIZE_MAX / elementSize;
}

static fn insertionSortRange(any values, bytes begin, bytes end,
                               bytes elementSize, SortCompareFn compare) {
    for (bytes i = begin + (begin < end); i < end; ++i) {
        bytes current = i;
        while (current > begin
               && compare(constElementAt(values, current, elementSize),
                          constElementAt(values, current - 1u, elementSize)) < 0) {
            swapElements(elementAt(values, current, elementSize),
                         elementAt(values, current - 1u, elementSize), elementSize);
            --current;
        }
    }
}

bool Sort_insertionSort(any values, bytes count, bytes elementSize,
                        SortCompareFn compare) {
    if (!validComparisonRange(values, count, elementSize, compare))
        return false;
    insertionSortRange(values, 0u, count, elementSize, compare);
    return true;
}

bool Sort_selectionSort(any values, bytes count, bytes elementSize,
                        SortCompareFn compare) {
    if (!validComparisonRange(values, count, elementSize, compare))
        return false;
    for (bytes first = 0u; first < count; ++first) {
        bytes minimum = first;
        for (bytes candidate = first + 1u; candidate < count; ++candidate) {
            if (compare(constElementAt(values, candidate, elementSize),
                        constElementAt(values, minimum, elementSize)) < 0)
                minimum = candidate;
        }
        swapElements(elementAt(values, first, elementSize),
                     elementAt(values, minimum, elementSize), elementSize);
    }
    return true;
}

bool Sort_bubbleSort(any values, bytes count, bytes elementSize,
                     SortCompareFn compare) {
    if (!validComparisonRange(values, count, elementSize, compare))
        return false;
    for (bytes unsorted = count; unsorted > 1u; --unsorted) {
        bool changed = false;
        for (bytes i = 1u; i < unsorted; ++i) {
            if (compare(constElementAt(values, i, elementSize),
                        constElementAt(values, i - 1u, elementSize)) < 0) {
                swapElements(elementAt(values, i, elementSize),
                             elementAt(values, i - 1u, elementSize), elementSize);
                changed = true;
            }
        }
        if (!changed)
            break;
    }
    return true;
}

static fn siftDown(any values, bytes root, bytes count,
                     bytes elementSize, SortCompareFn compare) {
    while (root < count / 2u) {
        bytes child = root * 2u + 1u;
        if (child + 1u < count
            && compare(constElementAt(values, child, elementSize),
                       constElementAt(values, child + 1u, elementSize)) < 0)
            ++child;
        if (compare(constElementAt(values, root, elementSize),
                    constElementAt(values, child, elementSize)) >= 0)
            return;
        swapElements(elementAt(values, root, elementSize),
                     elementAt(values, child, elementSize), elementSize);
        root = child;
    }
}

static fn heapSortRange(any values, bytes begin, bytes end,
                          bytes elementSize, SortCompareFn compare) {
    bytes count = end - begin;
    if (count < 2u)
        return;
    unsigned char *range = elementAt(values, begin, elementSize);
    for (bytes parent = count / 2u; parent > 0u; --parent)
        siftDown(range, parent - 1u, count, elementSize, compare);
    for (bytes remaining = count; remaining > 1u; --remaining) {
        swapElements(elementAt(range, 0u, elementSize),
                     elementAt(range, remaining - 1u, elementSize), elementSize);
        siftDown(range, 0u, remaining - 1u, elementSize, compare);
    }
}

bool Sort_heapSort(any values, bytes count, bytes elementSize,
                   SortCompareFn compare) {
    if (!validComparisonRange(values, count, elementSize, compare))
        return false;
    heapSortRange(values, 0u, count, elementSize, compare);
    return true;
}

static bytes partitionRange(any values, bytes begin, bytes end,
                             bytes elementSize, SortCompareFn compare) {
    bytes last = end - 1u;
    bytes middle = begin + (last - begin) / 2u;
    if (compare(constElementAt(values, begin, elementSize),
                constElementAt(values, middle, elementSize)) > 0)
        swapElements(elementAt(values, begin, elementSize),
                     elementAt(values, middle, elementSize), elementSize);
    if (compare(constElementAt(values, middle, elementSize),
                constElementAt(values, last, elementSize)) > 0)
        swapElements(elementAt(values, middle, elementSize),
                     elementAt(values, last, elementSize), elementSize);
    if (compare(constElementAt(values, begin, elementSize),
                constElementAt(values, middle, elementSize)) > 0)
        swapElements(elementAt(values, begin, elementSize),
                     elementAt(values, middle, elementSize), elementSize);
    swapElements(elementAt(values, middle, elementSize),
                 elementAt(values, last, elementSize), elementSize);

    bytes boundary = begin;
    for (bytes candidate = begin; candidate < last; ++candidate) {
        if (compare(constElementAt(values, candidate, elementSize),
                    constElementAt(values, last, elementSize)) < 0) {
            swapElements(elementAt(values, boundary, elementSize),
                         elementAt(values, candidate, elementSize), elementSize);
            ++boundary;
        }
    }
    swapElements(elementAt(values, boundary, elementSize),
                 elementAt(values, last, elementSize), elementSize);
    return boundary;
}

static fn quickSortRange(any values, bytes begin, bytes end,
                           bytes elementSize, SortCompareFn compare,
                           bytes depthLimit) {
    while (end - begin > 16u) {
        if (depthLimit == 0u) {
            heapSortRange(values, begin, end, elementSize, compare);
            return;
        }
        --depthLimit;
        bytes pivot = partitionRange(values, begin, end, elementSize, compare);
        bytes leftCount = pivot - begin;
        bytes rightBegin = pivot + 1u;
        bytes rightCount = end - rightBegin;
        if (leftCount < rightCount) {
            quickSortRange(values, begin, pivot, elementSize, compare, depthLimit);
            begin = rightBegin;
        } else {
            quickSortRange(values, rightBegin, end, elementSize, compare, depthLimit);
            end = pivot;
        }
    }
    insertionSortRange(values, begin, end, elementSize, compare);
}

bool Sort_quickSort(any values, bytes count, bytes elementSize,
                    SortCompareFn compare) {
    if (!validComparisonRange(values, count, elementSize, compare))
        return false;
    bytes depthLimit = 0u;
    for (bytes remaining = count; remaining > 1u; remaining >>= 1u)
        depthLimit += 2u;
    quickSortRange(values, 0u, count, elementSize, compare, depthLimit);
    return true;
}

bool Sort_mergeSort(any values, bytes count, bytes elementSize,
                    SortCompareFn compare, any scratch) {
    if (!validComparisonRange(values, count, elementSize, compare))
        return false;
    if (count <= 1u)
        return true;
    if (scratch == nullptr)
        return false;

    bytes byteCount = count * elementSize;
    pointer valuesStart = (pointer) values;
    pointer scratchStart = (pointer) scratch;
    if (valuesStart > UINTPTR_MAX - byteCount
        || scratchStart > UINTPTR_MAX - byteCount
        || (valuesStart < scratchStart + byteCount
            && scratchStart < valuesStart + byteCount))
        return false;
    any source = values;
    any destination = scratch;
    for (bytes run = 1u; run < count;) {
        bytes left = 0u;
        while (left < count) {
            bytes middle = left + (run < count - left ? run : count - left);
            bytes right = middle + (run < count - middle ? run : count - middle);
            bytes a = left;
            bytes b = middle;
            bytes out = left;
            while (a < middle && b < right) {
                if (compare(constElementAt(source, a, elementSize),
                            constElementAt(source, b, elementSize)) <= 0) {
                    memcpy(elementAt(destination, out++, elementSize),
                           constElementAt(source, a++, elementSize), elementSize);
                } else {
                    memcpy(elementAt(destination, out++, elementSize),
                           constElementAt(source, b++, elementSize), elementSize);
                }
            }
            while (a < middle)
                memcpy(elementAt(destination, out++, elementSize),
                       constElementAt(source, a++, elementSize), elementSize);
            while (b < right)
                memcpy(elementAt(destination, out++, elementSize),
                       constElementAt(source, b++, elementSize), elementSize);
            left = right;
        }
        any swap = source;
        source = destination;
        destination = swap;
        run = run > count / 2u ? count : run * 2u;
    }
    if (source != values)
        memcpy(values, source, byteCount);
    return true;
}

bytes Sort_partitionInt32(i32 *values, bytes count, i32 pivot) {
    if (values == nullptr || count == 0u)
        return 0u;
    bytes boundary = 0u;
    for (bytes i = 0u; i < count; ++i) {
        if (values[i] < pivot) {
            i32 value = values[boundary];
            values[boundary] = values[i];
            values[i] = value;
            ++boundary;
        }
    }
    return boundary;
}

static u64 unsignedKeyAt(const_any values, bytes index,
                              bytes keySize) {
    if (keySize == sizeof(u32))
        return ((const u32*) values)[index];
    return ((const u64*) values)[index];
}

static bool radixSortUnsigned(any values, bytes count, bytes keySize) {
    if (count <= 1u)
        return true;
    if (values == nullptr || count > SIZE_MAX / keySize)
        return false;
    bytes byteCount = count * keySize;
    any temporary = Memory_alloc(TYPE_BYTE_ARRAY, byteCount);
    if (temporary == nullptr)
        return false;

    any source = values;
    any destination = temporary;
    unsigned passes = (unsigned) keySize;
    for (unsigned pass = 0u; pass < passes; ++pass) {
        bytes bucketCounts[256] = {0};
        bytes bucketOffsets[256] = {0};
        unsigned shift = pass * 8u;
        for (bytes i = 0u; i < count; ++i) {
            u8 bucket = (u8) ((unsignedKeyAt(source, i, keySize) >> shift) & 0xFFu);
            ++bucketCounts[bucket];
        }
        bytes offset = 0u;
        for (bytes bucket = 0u; bucket < 256u; ++bucket) {
            bucketOffsets[bucket] = offset;
            offset += bucketCounts[bucket];
        }
        for (bytes i = 0u; i < count; ++i) {
            u64 key = unsignedKeyAt(source, i, keySize);
            u8 bucket = (u8) ((key >> shift) & 0xFFu);
            bytes target = bucketOffsets[bucket]++;
            memcpy(elementAt(destination, target, keySize),
                   constElementAt(source, i, keySize), keySize);
        }
        any swap = source;
        source = destination;
        destination = swap;
    }
    if (source != values)
        memcpy(values, source, byteCount);
    Memory_free(temporary);
    return true;
}

bool Sort_radixSortUint32(u32 *values, bytes count) {
    return radixSortUnsigned(values, count, sizeof(u32));
}

bool Sort_radixSortUint64(u64 *values, bytes count) {
    return radixSortUnsigned(values, count, sizeof(u64));
}

bool Sort_radixSortPairsUint64(u64 *keys, any *values, bytes count) {
    if (count <= 1u)
        return true;
    if (keys == nullptr || values == nullptr
        || count > SIZE_MAX / sizeof(u64)
        || count > SIZE_MAX / sizeof(any))
        return false;
    bytes keyBytes = count * sizeof(u64);
    bytes valueBytes = count * sizeof(any);
    u64 *temporaryKeys = (u64*) Memory_alloc(TYPE_BYTE_ARRAY, keyBytes);
    any *temporaryValues = (any*) Memory_alloc(TYPE_BYTE_ARRAY, valueBytes);
    if (temporaryKeys == nullptr || temporaryValues == nullptr) {
        if (temporaryKeys != nullptr)
            Memory_free(temporaryKeys);
        if (temporaryValues != nullptr)
            Memory_free(temporaryValues);
        return false;
    }

    u64 *sourceKeys = keys;
    u64 *destinationKeys = temporaryKeys;
    any *sourceValues = values;
    any *destinationValues = temporaryValues;
    for (unsigned pass = 0u; pass < 8u; ++pass) {
        bytes bucketCounts[256] = {0};
        bytes bucketOffsets[256] = {0};
        unsigned shift = pass * 8u;
        for (bytes i = 0u; i < count; ++i) {
            u8 bucket = (u8) ((sourceKeys[i] >> shift) & 0xFFu);
            ++bucketCounts[bucket];
        }
        bytes offset = 0u;
        for (bytes bucket = 0u; bucket < 256u; ++bucket) {
            bucketOffsets[bucket] = offset;
            offset += bucketCounts[bucket];
        }
        for (bytes i = 0u; i < count; ++i) {
            u8 bucket = (u8) ((sourceKeys[i] >> shift) & 0xFFu);
            bytes target = bucketOffsets[bucket]++;
            destinationKeys[target] = sourceKeys[i];
            destinationValues[target] = sourceValues[i];
        }
        u64 *keySwap = sourceKeys;
        sourceKeys = destinationKeys;
        destinationKeys = keySwap;
        any *valueSwap = sourceValues;
        sourceValues = destinationValues;
        destinationValues = valueSwap;
    }
    if (sourceKeys != keys) {
        memcpy(keys, sourceKeys, keyBytes);
        memcpy(values, sourceValues, valueBytes);
    }
    Memory_free(temporaryKeys);
    Memory_free(temporaryValues);
    return true;
}
