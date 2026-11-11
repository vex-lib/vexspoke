#ifndef OBJECTS_PROBABLE_OBJECTS_H
#define OBJECTS_PROBABLE_OBJECTS_H

#include <stddef.h>
#include <stdint.h>
#include <stddef.h>
#include "lang/constructor.h"

#include "objects/probable.h"

// objects/probable_objects.h — the ProbableObjects class, ported from
// objects/ProbableObjects.java.
//
// A weighted pool: capacity choices, each with its own weight. The total weight
// is tracked, each slot stores a running cumulative weight, and a draw picks a
// random value in [0, total) then binary-searches the cumulative line.

typedef struct ProbableObjects {
    u32 count;         // active choices
    u32 totalWeight;   // sum of all weights
    u32 capacity;      // maximum choices
    u32 pad;
    // capacity slots of 16 Bytes: object(8) + cumulativeWeight(4) + weight(4)
    u8 slots[];
} ProbableObjects;

// Allocate an empty pool with room for capacity choices. Rejects capacities
// that cannot fit the allocator's 32-bit payload length.
ProbableObjects *ProbableObjects_1(bytes capacity);


// Clone one populated pool (including its slots), or create count empty
// zero-capacity headers. Multiple populated pools cannot be represented as a
// contiguous C array because each pool ends in a flexible array; rejected.
ProbableObjects *ProbableObjects_2(const ProbableObjects *init, bytes count);

#define ProbableObjects(...) CONSTRUCTOR_DISPATCH(ProbableObjects, __VA_ARGS__)

void ProbableObjects_free(ProbableObjects *po);

bytes ProbableObjects_size(ProbableObjects *po);
bytes ProbableObjects_capacity(ProbableObjects *po);
u32 ProbableObjects_totalWeight(ProbableObjects *po);

// Append a choice. Returns 0 without changing the pool if full or if the
// cumulative u32 weight would overflow.
int ProbableObjects_add(ProbableObjects *po, uintptr_t object, u32 weight);

// Append the object/weight of an existing Probable.
int ProbableObjects_addProbable(ProbableObjects *po, const Probable *probable);

// Slot accessors (used by Random.probablePool's binary search).
uintptr_t ProbableObjects_objectAt(ProbableObjects *po, bytes index);
u32 ProbableObjects_cumulativeAt(ProbableObjects *po, bytes index);
u32 ProbableObjects_weightAt(ProbableObjects *po, bytes index);

// Draw a weighted object from the pool using the system RNG.
uintptr_t ProbableObjects_get(ProbableObjects *po);

#endif
