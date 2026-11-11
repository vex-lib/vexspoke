#ifndef STRUCT_SPARSESET_H
#define STRUCT_SPARSESET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lang/constructor.h"

#include "struct/exhaustion.h"

// struct/sparseset.h — the SparseSet class, ported from struct/SparseSet.java.
//
// ECS sparse set: maps sparse entity ids (int) to a tightly packed dense index.
// Optionally carries a contiguous component data block (stride Bytes per entry)
// so each add returns the data pointer to write into.

typedef struct SparseSet {
    i32 capacity;      // dense/data capacity
    i32 maxEntities;   // sparse array length
    i32 count;         // live entries
    i32 stride;        // component stride (0 = set only)
    i32 *dense;        // dense[i] = entity id
    i32 *sparse;       // sparse[entity] = dense index, -1 = absent
    u8 *data;         // component data, capacity * stride Bytes
    // Exhaustion observability (the Exhaustion Loudness Law).
    u64 exhaustionCount;
    bool exhaustionReported;
} SparseSet;

// Set over entity ids [0, maxEntities) with optional component stride.
SparseSet *SparseSet_3(bytes capacity, bytes maxEntities, bytes stride);

void SparseSet_free(SparseSet *set);

bytes SparseSet_count(SparseSet *set);
// Refused-insertion count since construction (the Exhaustion Loudness Law).
;;TEST
u64 SparseSet_exhaustionCount(const SparseSet *set);
bytes SparseSet_capacity(SparseSet *set);
bytes SparseSet_maxEntities(SparseSet *set);

bool SparseSet_contains(SparseSet *set, i32 entityId);

// Add entity (if absent) and return a pointer to its component data (or nullptr
// when the set carries no data block).
u8 *SparseSet_add(SparseSet *set, i32 entityId);

// Remove the entity if present; data is swap-removed to keep density.
void SparseSet_remove(SparseSet *set, i32 entityId);

// Component data pointer for the entity, or nullptr if absent (or stride == 0,
// in which case a non-nullptr sentinel signals presence).
u8 *SparseSet_get(SparseSet *set, i32 entityId);

// Tightly packed entity ids, dense[0..count).
const i32 *SparseSet_denseEntities(SparseSet *set);

// Tightly packed component data, first count * stride Bytes are live.
const u8 *SparseSet_denseData(SparseSet *set);


#define SparseSet(...) CONSTRUCTOR_DISPATCH(SparseSet, __VA_ARGS__)

#endif