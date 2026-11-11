#ifndef UTIL_ARRAYS_H
#define UTIL_ARRAYS_H

#include <stddef.h>
#include <stdint.h>

// util/arrays.h — the Arrays utility, ported from util/Arrays.java.
//
// Draft-level off-heap array operations: quicksort, binary search, fill, copy.
// In C these operate on plain int64/int32 buffers owned by an Array/List.

// In-place quicksort of an int32 buffer.
void Arrays_sortInt(i32 *data, bytes length);

// In-place quicksort of an int64 buffer.
void Arrays_sortLong(int64_t *data, bytes length);

// Binary search over a sorted int32 buffer. Returns the index, or
// -(insertion point + 1) when not found (legacy semantics).
intptr_t Arrays_binarySearchInt(const i32 *data, bytes length, i32 key);

// Binary search over a sorted int64 buffer. Returns the index, or
// -(insertion point + 1) when not found.
intptr_t Arrays_binarySearchLong(const int64_t *data, bytes length, int64_t key);

// Fill a byte block with value.
void Arrays_fill(u8 *data, bytes length, u8 value);

// Copy a byte block (non-overlapping, like memcpy).
void Arrays_copy(const u8 *src, u8 *dest, bytes Bytes);

#endif