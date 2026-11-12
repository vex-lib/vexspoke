#include "vex.h"
#include "re.h"
#include "objects/probable_objects.h"
#include "oop/vexspoke_type.h"
#include "util/random.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Probable_objects
 * ============================================================================
 * Weighted random-selection pool: a flat arena of 16-byte slot records
 * (object pointer, running cumulative weight, per-slot weight) behind a
 * fixed header of count/totalWeight/capacity. add() appends and maintains
 * the cumulative running total so Random_probablePool can binary-search the
 * cumulative column for a uniform draw; totalWeight 0 falls back to uniform
 * index selection. The pool is capacity-bounded at construction and never
 * reallocates on the hot draw path.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Probable_objects (objects/probable_objects.c)
 * ============================================================================
 * the ProbableObjects class, ported from
 *
 * STRUCT FIELDS (Mirroring objects/probable_objects.h):
 * ----------------------------------------------------------------------------
 *   ProbableObjects {
 *     u32 count; // active choices
 *     u32 totalWeight; // sum of all weights
 *     u32 capacity; // maximum choices
 *     u32 pad; // alignment padding
 *     u8 slots[]; // backing store
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - ProbableObjects_1(capacity)
 *   - ProbableObjects_2(init, count)
 *
 * Core Functions:
 *   - ProbableObjects_free(po)
 *   - ProbableObjects_size(po)
 *   - ProbableObjects_capacity(po)
 *   - ProbableObjects_totalWeight(po)
 *   - ProbableObjects_add(po, object, weight)
 *   - ProbableObjects_addProbable(po, probable)
 *   - ProbableObjects_objectAt(po, index)
 *   - ProbableObjects_cumulativeAt(po, index)
 *   - ProbableObjects_weightAt(po, index)
 *
 * Getters:
 *   - ProbableObjects_get(po)
 * ============================================================================
 */


// probable_objects.c — ProbableObjects port (Legacy: objects/ProbableObjects.java).

static const bytes SLOT_SIZE = 16;

ProbableObjects *ProbableObjects_1(bytes capacity) {
    // The allocator records payload lengths in 32 bits, as does this pool's
    // capacity. Reject before computing Bytes or narrowing the capacity.
    if (capacity > (UINT32_MAX - sizeof(ProbableObjects)) / SLOT_SIZE) {
        THROW("probable objects capacity is not representable");
        return nullptr;
    }
    bytes Bytes = sizeof(ProbableObjects) + capacity * SLOT_SIZE;
    ProbableObjects *po = (ProbableObjects*) Memory_alloc(TYPE_PROBABLE_OBJECTS, Bytes);
    if (!po) return nullptr;
    memset(po, 0, Bytes);
    (*po).capacity = (u32)capacity;
    return po;
}


ProbableObjects *ProbableObjects_2(const ProbableObjects *init, bytes count) {
    if (count == 0) return nullptr;
    // C arrays stride by sizeof(ProbableObjects), not by a flexible slot
    // payload. Only a single element can own populated flexible storage.
    if (init && (*init).capacity && count != 1) {
        THROW("probable objects array cannot copy flexible slots");
        return nullptr;
    }
    if (count > UINT32_MAX / sizeof(ProbableObjects)) {
        THROW("probable objects array size is not representable");
        return nullptr;
    }
    bytes Bytes = sizeof(ProbableObjects) * count;
    if (init && (*init).capacity)
        Bytes += (bytes) (*init).capacity * SLOT_SIZE;
    ProbableObjects *p = (ProbableObjects*) Memory_alloc(TYPE_PROBABLE_OBJECTS_ARRAY, Bytes);
    if (!p) return nullptr;
    if (init && (*init).capacity)
        memcpy(p, init, Bytes);
    else
        memset(p, 0, Bytes);
    return p;
}
void ProbableObjects_free(ProbableObjects *po) {
    Memory_free(po);
}

bytes ProbableObjects_size(ProbableObjects *po) {
    if (!po) return 0;
    return (*po).count;
}

bytes ProbableObjects_capacity(ProbableObjects *po) {
    if (!po) return 0;
    return (*po).capacity;
}

u32 ProbableObjects_totalWeight(ProbableObjects *po) {
    if (!po) return 0;
    return (*po).totalWeight;
}

static u8 *slotAt(ProbableObjects *po, bytes index) {
    return (*po).slots + index * SLOT_SIZE;
}

int ProbableObjects_add(ProbableObjects *po, pointer object, u32 weight) {
    if (!po) return 0;
    if ((*po).count >= (*po).capacity) {
        THROW("probable objects pool is full");
        return 0;
    }
    if (weight > UINT32_MAX - (*po).totalWeight) {
        THROW("probable objects total weight overflow");
        return 0;
    }

    u8 *slot = slotAt(po, (*po).count);
    *(pointer*) slot = object;

    u32 running = (*po).totalWeight + weight;
    *(u32*) (slot + 8) = running;
    *(u32*) (slot + 12) = weight;

    (*po).totalWeight = running;
    (*po).count++;
    return 1;
}

int ProbableObjects_addProbable(ProbableObjects *po, const Probable *probable) {
    if (!probable) return 0;
    return ProbableObjects_add(po, (*probable).object, (*probable).weight);
}

pointer ProbableObjects_objectAt(ProbableObjects *po, bytes index) {
    if (!po || index >= (*po).count) return 0;
    return *(pointer*) slotAt(po, index);
}

u32 ProbableObjects_cumulativeAt(ProbableObjects *po, bytes index) {
    if (!po || index >= (*po).count) return 0;
    return *(u32*) (slotAt(po, index) + 8);
}

u32 ProbableObjects_weightAt(ProbableObjects *po, bytes index) {
    if (!po || index >= (*po).count) return 0;
    return *(u32*) (slotAt(po, index) + 12);
}

pointer ProbableObjects_get(ProbableObjects *po) {
    return Random_probablePool(Random_system(), po);
}
