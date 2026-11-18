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



;;GETTER
static inline u32 ProbableObjects_capacityRaw(const ProbableObjects *s) { return (*s).capacity; }
;;SETTER
static inline fn ProbableObjects_setCapacity(ProbableObjects *s, u32 v) { (*s).capacity = v; }
;;GETTER
static inline u32 ProbableObjects_count(const ProbableObjects *s) { return (*s).count; }
;;SETTER
static inline fn ProbableObjects_setCount(ProbableObjects *s, u32 v) { (*s).count = v; }
;;GETTER
static inline u8 *ProbableObjects_slotsMut(ProbableObjects *s) { return (*s).slots; }
;;GETTER
static inline u32 ProbableObjects_totalWeightRaw(const ProbableObjects *s) { return (*s).totalWeight; }
;;SETTER
static inline fn ProbableObjects_setTotalWeight(ProbableObjects *s, u32 v) { (*s).totalWeight = v; }

;;GETTER
static inline pointer Probable_objectRaw(const Probable *s) { return (*s).object; }
;;GETTER
static inline u32 Probable_weightRaw(const Probable *s) { return (*s).weight; }

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
    ProbableObjects_setCapacity(po, (u32)capacity);
    return po;
}


ProbableObjects *ProbableObjects_2(const ProbableObjects *init, bytes count) {
    if (count == 0) return nullptr;
    // C arrays stride by sizeof(ProbableObjects), not by a flexible slot
    // payload. Only a single element can own populated flexible storage.
    if (init && ProbableObjects_capacityRaw(init) && count != 1) {
        THROW("probable objects array cannot copy flexible slots");
        return nullptr;
    }
    if (count > UINT32_MAX / sizeof(ProbableObjects)) {
        THROW("probable objects array size is not representable");
        return nullptr;
    }
    bytes Bytes = sizeof(ProbableObjects) * count;
    if (init && ProbableObjects_capacityRaw(init))
        Bytes += (bytes) ProbableObjects_capacityRaw(init) * SLOT_SIZE;
    ProbableObjects *p = (ProbableObjects*) Memory_alloc(TYPE_PROBABLE_OBJECTS_ARRAY, Bytes);
    if (!p) return nullptr;
    if (init && ProbableObjects_capacityRaw(init))
        memcpy(p, init, Bytes);
    else
        memset(p, 0, Bytes);
    return p;
}
fn ProbableObjects_free(ProbableObjects *po) {
    Memory_free(po);
}

bytes ProbableObjects_size(ProbableObjects *po) {
    if (!po) return 0;
    return ProbableObjects_count(po);
}

bytes ProbableObjects_capacity(ProbableObjects *po) {
    if (!po) return 0;
    return ProbableObjects_capacityRaw(po);
}

u32 ProbableObjects_totalWeight(ProbableObjects *po) {
    if (!po) return 0;
    return ProbableObjects_totalWeightRaw(po);
}

static u8 *slotAt(ProbableObjects *po, bytes index) {
    return ProbableObjects_slotsMut(po) + index * SLOT_SIZE;
}

i32 ProbableObjects_add(ProbableObjects *po, pointer object, u32 weight) {
    if (!po) return 0;
    if (ProbableObjects_count(po) >= ProbableObjects_capacityRaw(po)) {
        THROW("probable objects pool is full");
        return 0;
    }
    if (weight > UINT32_MAX - ProbableObjects_totalWeightRaw(po)) {
        THROW("probable objects total weight overflow");
        return 0;
    }

    u8 *slot = slotAt(po, ProbableObjects_count(po));
    *(pointer*) slot = object;

    u32 running = ProbableObjects_totalWeightRaw(po) + weight;
    *(u32*) (slot + 8) = running;
    *(u32*) (slot + 12) = weight;

    ProbableObjects_setTotalWeight(po, running);
    ProbableObjects_setCount(po, ProbableObjects_count(po) + 1u);
    return 1;
}

i32 ProbableObjects_addProbable(ProbableObjects *po, const Probable *probable) {
    if (!probable) return 0;
    return ProbableObjects_add(po, Probable_objectRaw(probable), Probable_weightRaw(probable));
}

pointer ProbableObjects_objectAt(ProbableObjects *po, bytes index) {
    if (!po || index >= ProbableObjects_count(po)) return 0;
    return *(pointer*) slotAt(po, index);
}

u32 ProbableObjects_cumulativeAt(ProbableObjects *po, bytes index) {
    if (!po || index >= ProbableObjects_count(po)) return 0;
    return *(u32*) (slotAt(po, index) + 8);
}

u32 ProbableObjects_weightAt(ProbableObjects *po, bytes index) {
    if (!po || index >= ProbableObjects_count(po)) return 0;
    return *(u32*) (slotAt(po, index) + 12);
}

pointer ProbableObjects_get(ProbableObjects *po) {
    return Random_probablePool(Random_system(), po);
}
