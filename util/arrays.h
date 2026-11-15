#ifndef UTIL_ARRAYS_H
#define UTIL_ARRAYS_H

#include "vex.h"

// util/arrays.h — the Arrays utility, ported from util/Arrays.java.
//
// Draft-level off-heap array operations: quicksort, binary search, fill, copy.
// In C these operate on plain int64/int32 buffers owned by an Array/List.

// In-place quicksort of an int32 buffer.
fn Arrays_sortInt(i32 *data, bytes length);

// In-place quicksort of an int64 buffer.
fn Arrays_sortLong(i64 *data, bytes length);

// Binary search over a sorted int32 buffer. Returns the index, or
// -(insertion point + 1) when not found (legacy semantics).
pointer Arrays_binarySearchInt(const i32 *data, bytes length, i32 key);

// Binary search over a sorted int64 buffer. Returns the index, or
// -(insertion point + 1) when not found.
pointer Arrays_binarySearchLong(const i64 *data, bytes length, i64 key);

// Fill a byte block with value.
fn Arrays_fill(u8 *data, bytes length, u8 value);

// Copy a byte block (non-overlapping, like memcpy).
fn Arrays_copy(const u8 *src, u8 *dest, bytes Bytes);

#endif