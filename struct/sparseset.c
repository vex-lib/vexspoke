#include "vex.h"
#include "nio/mem.h"

#include "struct/sparseset.h"
#include "oop/vexspoke_type.h"

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



;;GETTER
static inline i32 SparseSet_capacityRaw(const SparseSet *s) { return (*s).capacity; }
;;SETTER
static inline fn SparseSet_setCapacity(SparseSet *s, i32 v) { (*s).capacity = v; }
;;GETTER
static inline i32 SparseSet_countRaw(const SparseSet *s) { return (*s).count; }
;;SETTER
static inline fn SparseSet_setCount(SparseSet *s, i32 v) { (*s).count = v; }
;;GETTER
static inline u8 * SparseSet_data(const SparseSet *s) { return (*s).data; }
;;SETTER
static inline fn SparseSet_setData(SparseSet *s, u8 * v) { (*s).data = v; }
;;GETTER
static inline i32 * SparseSet_dense(const SparseSet *s) { return (*s).dense; }
;;SETTER
static inline fn SparseSet_setDense(SparseSet *s, i32 * v) { (*s).dense = v; }
;;GETTER
static inline const u64 *SparseSet_exhaustionCountPtr(const SparseSet *s) { return &(*s).exhaustionCount; }
;;GETTER
static inline u64 *SparseSet_exhaustionCountMut(SparseSet *s) { return &(*s).exhaustionCount; }
;;SETTER
static inline fn SparseSet_setExhaustionCount(SparseSet *s, u64 v) { (*s).exhaustionCount = v; }
;;GETTER
static inline bool *SparseSet_exhaustionReportedMut(SparseSet *s) { return &(*s).exhaustionReported; }
;;SETTER
static inline fn SparseSet_setExhaustionReported(SparseSet *s, bool v) { (*s).exhaustionReported = v; }
;;GETTER
static inline i32 SparseSet_maxEntitiesRaw(const SparseSet *s) { return (*s).maxEntities; }
;;SETTER
static inline fn SparseSet_setMaxEntities(SparseSet *s, i32 v) { (*s).maxEntities = v; }
;;GETTER
static inline i32 * SparseSet_sparse(const SparseSet *s) { return (*s).sparse; }
;;SETTER
static inline fn SparseSet_setSparse(SparseSet *s, i32 * v) { (*s).sparse = v; }
;;GETTER
static inline i32 SparseSet_stride(const SparseSet *s) { return (*s).stride; }
;;SETTER
static inline fn SparseSet_setStride(SparseSet *s, i32 v) { (*s).stride = v; }

// sparseset.c — SparseSet port (Legacy: struct/SparseSet.java).

static i32 *allocateInts(bytes count) {
    return (i32*) Memory_alloc(TYPE_INT_ARRAY, count * sizeof(i32));
}

SparseSet *SparseSet_3(bytes capacity, bytes maxEntities, bytes stride) {
    SparseSet *set = (SparseSet*) Memory_alloc(Type_make(PROJ_VEXSPOKE, FORM_SINGLETON, ID_SPARSE_SET), sizeof(SparseSet));
    if (!set) {
        (discard) Struct_reportConstructionExhaustion("sparseset", sizeof(SparseSet));
        return nullptr;
    }

    SparseSet_setCapacity(set, (i32)capacity);
    SparseSet_setMaxEntities(set, (i32)maxEntities);
    SparseSet_setCount(set, 0);
    SparseSet_setStride(set, (i32)stride);
    SparseSet_setExhaustionCount(set, 0);
    SparseSet_setExhaustionReported(set, false);

    SparseSet_setDense(set, allocateInts(capacity));
    if (!SparseSet_dense(set)) {
        (discard) Struct_reportConstructionExhaustion("sparseset", capacity * sizeof(i32));
        Memory_free(set);
        return nullptr;
    }

    SparseSet_setSparse(set, allocateInts(maxEntities));
    if (!SparseSet_sparse(set)) {
        (discard) Struct_reportConstructionExhaustion("sparseset", maxEntities * sizeof(i32));
        Memory_free(SparseSet_dense(set));
        Memory_free(set);
        return nullptr;
    }
    for (bytes i = 0; i < maxEntities; i++)
        SparseSet_sparse(set)[i] = -1;

    if (stride > 0) {
        bytes Bytes = capacity * stride;
        u64 bufType = Type_make(PROJ_VEXSPOKE, FORM_ARRAY, ID_SPARSE_SET);
        SparseSet_setData(set, (u8*) Memory_alloc(bufType, Bytes));
        if (!SparseSet_data(set)) {
            (discard) Struct_reportConstructionExhaustion("sparseset", Bytes);
            Memory_free(SparseSet_dense(set));
            Memory_free(SparseSet_sparse(set));
            Memory_free(set);
            return nullptr;
        }
    } else {
        SparseSet_setData(set, nullptr);
    }
    return set;
}

fn SparseSet_free(SparseSet *set) {
    if (!set) return;
    Memory_free(SparseSet_dense(set));
    Memory_free(SparseSet_sparse(set));
    Memory_free(SparseSet_data(set));
    Memory_free(set);
}

bytes SparseSet_count(SparseSet *set) {
    if (!set) return 0;
    return (bytes)SparseSet_countRaw(set);
}

;;TEST
u64 SparseSet_exhaustionCount(const SparseSet *set) {
    return set ? Struct_exhaustionCount(SparseSet_exhaustionCountPtr(set)) : 0;
}

bytes SparseSet_capacity(SparseSet *set) {
    if (!set) return 0;
    return (bytes)SparseSet_capacityRaw(set);
}

bytes SparseSet_maxEntities(SparseSet *set) {
    if (!set) return 0;
    return (bytes)SparseSet_maxEntitiesRaw(set);
}

bool SparseSet_contains(SparseSet *set, i32 entityId) {
    if (!set) return false;
    if (entityId < 0 || entityId >= SparseSet_maxEntitiesRaw(set))
        return false;
    return SparseSet_sparse(set)[entityId] != -1;
}

u8 *SparseSet_add(SparseSet *set, i32 entityId) {
    if (!set) return nullptr;
    if (entityId < 0 || entityId >= SparseSet_maxEntitiesRaw(set))
        return nullptr;

    i32 denseIndex = SparseSet_sparse(set)[entityId];
    if (denseIndex != -1) {
        if (SparseSet_stride(set) > 0)
            return SparseSet_data(set) + (bytes)denseIndex * (bytes)SparseSet_stride(set);
        return SparseSet_data(set);
    }

    if (SparseSet_count(set) >= SparseSet_capacity(set)) {
        // The dense capacity is full: a refusal, reported (once per epoch).
        (discard) Struct_reportExhaustion(SparseSet_exhaustionCountMut(set), SparseSet_exhaustionReportedMut(set),
                                       "sparseset", (bytes) SparseSet_capacity(set), (bytes) SparseSet_capacity(set));
        return nullptr;
    }

    i32 count = SparseSet_count(set);
    SparseSet_dense(set)[count] = entityId;
    SparseSet_sparse(set)[entityId] = count;
    SparseSet_setCount(set, count + 1);

    if (SparseSet_stride(set) > 0)
        return SparseSet_data(set) + (bytes)count * (bytes)SparseSet_stride(set);
    return SparseSet_data(set);
}

fn SparseSet_remove(SparseSet *set, i32 entityId) {
    if (!set) return;
    if (entityId < 0 || entityId >= SparseSet_maxEntitiesRaw(set))
        return;

    i32 denseIndex = SparseSet_sparse(set)[entityId];
    if (denseIndex == -1)
        return;

    i32 count = SparseSet_count(set) - 1;
    i32 lastEntity = SparseSet_dense(set)[count];

    if (denseIndex != count) {
        SparseSet_dense(set)[denseIndex] = lastEntity;
        SparseSet_sparse(set)[lastEntity] = denseIndex;
        if (SparseSet_stride(set) > 0) {
            u8 *dest = SparseSet_data(set) + (bytes)denseIndex * (bytes)SparseSet_stride(set);
            u8 *src = SparseSet_data(set) + (bytes)count * (bytes)SparseSet_stride(set);
            memcpy(dest, src, (bytes)SparseSet_stride(set));
        }
    }

    SparseSet_sparse(set)[entityId] = -1;
    SparseSet_setCount(set, count);
}

u8 *SparseSet_get(SparseSet *set, i32 entityId) {
    if (!set) return nullptr;
    if (entityId < 0 || entityId >= SparseSet_maxEntitiesRaw(set))
        return nullptr;

    i32 denseIndex = SparseSet_sparse(set)[entityId];
    if (denseIndex == -1)
        return nullptr;
    if (SparseSet_stride(set) == 0)
        return (u8*) set; // non-nullptr presence sentinel (legacy returns ptr)

    return SparseSet_data(set) + (bytes)denseIndex * (bytes)SparseSet_stride(set);
}

const i32 *SparseSet_denseEntities(SparseSet *set) {
    if (!set) return nullptr;
    return SparseSet_dense(set);
}

const u8 *SparseSet_denseData(SparseSet *set) {
    if (!set) return nullptr;
    return SparseSet_data(set);
}
