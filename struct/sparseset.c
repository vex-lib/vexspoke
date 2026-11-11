#include "struct/sparseset.h"

#include <string.h>

#include "nio/mem.h"
#include "oop/type.h"
#include "util/annotation.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Sparseset
 * ============================================================================
 * Sparse set (the classic dense/sparse pair): dense[i] holds entity ids in
 * insertion order, sparse[entity] maps id to dense index (-1 absent), and an
 * optional stride-strided data buffer carries per-entity components. add
 * returns the component slot (or a non-nullptr presence sentinel when stride
 * is 0); remove is swap-remove, moving the last dense entry into the hole in
 * O(1). All three arenas are allocated at construction and freed together —
 * no steady-state allocation on the hot add/remove/contains path.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Sparseset (struct/sparseset.c)
 * ============================================================================
 * the SparseSet class, ported from struct/SparseSet.java.
 *
 * STRUCT FIELDS (Mirroring struct/sparseset.h):
 * ----------------------------------------------------------------------------
 *   SparseSet {
 *     i32 capacity; // dense/data capacity
 *     i32 maxEntities; // sparse array length
 *     i32 count; // live entries
 *     i32 stride; // component stride (0 = set only)
 *     i32 *dense; // dense[i] = entity id
 *     i32 *sparse; // sparse[entity] = dense index, -1 = absent
 *     u8 *data; // component data, capacity * stride Bytes
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - SparseSet_3(capacity, maxEntities, stride)
 *
 * Core Functions:
 *   - SparseSet_free(set)
 *   - SparseSet_count(set)
 *   - SparseSet_capacity(set)
 *   - SparseSet_maxEntities(set)
 *   - SparseSet_contains(set, entityId)
 *   - SparseSet_add(set, entityId)
 *   - SparseSet_remove(set, entityId)
 *   - SparseSet_denseEntities(set)
 *   - SparseSet_denseData(set)
 *
 * Getters:
 *   - SparseSet_get(set, entityId)
 * ============================================================================
 */


// sparseset.c — SparseSet port (Legacy: struct/SparseSet.java).

static i32 *allocateInts(bytes count) {
    return (i32*) Memory_alloc(TYPE_INT_ARRAY, count * sizeof(i32));
}

SparseSet *SparseSet_3(bytes capacity, bytes maxEntities, bytes stride) {
    SparseSet *set = (SparseSet*) Memory_alloc(Type_make(PROJ_VEXSPOKE, FORM_SINGLETON, ID_SPARSE_SET), sizeof(SparseSet));
    if (!set) {
        (void) Struct_reportConstructionExhaustion("sparseset", sizeof(SparseSet));
        return nullptr;
    }

    (*set).capacity = (i32)capacity;
    (*set).maxEntities = (i32)maxEntities;
    (*set).count = 0;
    (*set).stride = (i32)stride;
    (*set).exhaustionCount = 0;
    (*set).exhaustionReported = false;

    (*set).dense = allocateInts(capacity);
    if (!(*set).dense) {
        (void) Struct_reportConstructionExhaustion("sparseset", capacity * sizeof(i32));
        Memory_free(set);
        return nullptr;
    }

    (*set).sparse = allocateInts(maxEntities);
    if (!(*set).sparse) {
        (void) Struct_reportConstructionExhaustion("sparseset", maxEntities * sizeof(i32));
        Memory_free((*set).dense);
        Memory_free(set);
        return nullptr;
    }
    for (bytes i = 0; i < maxEntities; i++)
        (*set).sparse[i] = -1;

    if (stride > 0) {
        bytes Bytes = capacity * stride;
        u64 bufType = Type_make(PROJ_VEXSPOKE, FORM_ARRAY, ID_SPARSE_SET);
        (*set).data = (u8*) Memory_alloc(bufType, Bytes);
        if (!(*set).data) {
            (void) Struct_reportConstructionExhaustion("sparseset", Bytes);
            Memory_free((*set).dense);
            Memory_free((*set).sparse);
            Memory_free(set);
            return nullptr;
        }
    } else {
        (*set).data = nullptr;
    }
    return set;
}

void SparseSet_free(SparseSet *set) {
    if (!set) return;
    Memory_free((*set).dense);
    Memory_free((*set).sparse);
    Memory_free((*set).data);
    Memory_free(set);
}

bytes SparseSet_count(SparseSet *set) {
    if (!set) return 0;
    return (bytes)(*set).count;
}

;;TEST
u64 SparseSet_exhaustionCount(const SparseSet *set) {
    return set ? Struct_exhaustionCount(&(*set).exhaustionCount) : 0;
}

bytes SparseSet_capacity(SparseSet *set) {
    if (!set) return 0;
    return (bytes)(*set).capacity;
}

bytes SparseSet_maxEntities(SparseSet *set) {
    if (!set) return 0;
    return (bytes)(*set).maxEntities;
}

bool SparseSet_contains(SparseSet *set, i32 entityId) {
    if (!set) return false;
    if (entityId < 0 || entityId >= (*set).maxEntities)
        return false;
    return (*set).sparse[entityId] != -1;
}

u8 *SparseSet_add(SparseSet *set, i32 entityId) {
    if (!set) return nullptr;
    if (entityId < 0 || entityId >= (*set).maxEntities)
        return nullptr;

    i32 denseIndex = (*set).sparse[entityId];
    if (denseIndex != -1) {
        if ((*set).stride > 0)
            return (*set).data + (bytes)denseIndex * (bytes)(*set).stride;
        return (*set).data;
    }

    if ((*set).count >= (*set).capacity) {
        // The dense capacity is full: a refusal, reported (once per epoch).
        (void) Struct_reportExhaustion(&(*set).exhaustionCount, &(*set).exhaustionReported,
                                       "sparseset", (bytes) (*set).capacity, (bytes) (*set).capacity);
        return nullptr;
    }

    i32 count = (*set).count;
    (*set).dense[count] = entityId;
    (*set).sparse[entityId] = count;
    (*set).count = count + 1;

    if ((*set).stride > 0)
        return (*set).data + (bytes)count * (bytes)(*set).stride;
    return (*set).data;
}

void SparseSet_remove(SparseSet *set, i32 entityId) {
    if (!set) return;
    if (entityId < 0 || entityId >= (*set).maxEntities)
        return;

    i32 denseIndex = (*set).sparse[entityId];
    if (denseIndex == -1)
        return;

    i32 count = (*set).count - 1;
    i32 lastEntity = (*set).dense[count];

    if (denseIndex != count) {
        (*set).dense[denseIndex] = lastEntity;
        (*set).sparse[lastEntity] = denseIndex;
        if ((*set).stride > 0) {
            u8 *dest = (*set).data + (bytes)denseIndex * (bytes)(*set).stride;
            u8 *src = (*set).data + (bytes)count * (bytes)(*set).stride;
            memcpy(dest, src, (bytes)(*set).stride);
        }
    }

    (*set).sparse[entityId] = -1;
    (*set).count = count;
}

u8 *SparseSet_get(SparseSet *set, i32 entityId) {
    if (!set) return nullptr;
    if (entityId < 0 || entityId >= (*set).maxEntities)
        return nullptr;

    i32 denseIndex = (*set).sparse[entityId];
    if (denseIndex == -1)
        return nullptr;
    if ((*set).stride == 0)
        return (u8*) set; // non-nullptr presence sentinel (legacy returns ptr)

    return (*set).data + (bytes)denseIndex * (bytes)(*set).stride;
}

const i32 *SparseSet_denseEntities(SparseSet *set) {
    if (!set) return nullptr;
    return (*set).dense;
}

const u8 *SparseSet_denseData(SparseSet *set) {
    if (!set) return nullptr;
    return (*set).data;
}
