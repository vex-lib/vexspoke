#include "vex.h"
#include "nio/mem.h"

#include "struct/minheap.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: MinHeap
 * ============================================================================
 * Binary min-heap ported from struct/MinHeap.java: 1-based item/priority
 * arrays with sift-up on push and sift-down on pop, so the strictly lowest
 * priority always rises to the root. Both arrays are arena-allocated at
 * capacity + 1 slots; MinHeap_free releases them before the heap struct
 * (detach before free). Lives at R2 as a leaf container behavior.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Minheap (struct/minheap.c)
 * ============================================================================
 * the MinHeap class, ported from struct/MinHeap.java.
 *
 * STRUCT FIELDS (Mirroring struct/minheap.h):
 * ----------------------------------------------------------------------------
 *   MinHeap {
 *     i32 size; // number of live entries
 *     i32 capacity; // max entries (1-based slots are capacity + 1)
 *     i32 *items; // item at heap index i (i in [1, size])
 *     f32 *priorities; // priority at heap index i
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - MinHeap_1(capacity)
 *   - MinHeap_0()
 *
 * Core Functions:
 *   - MinHeap_free(heap)
 *   - MinHeap_size(heap)
 *   - MinHeap_capacity(heap)
 *   - MinHeap_push(heap, item, priority)
 *   - MinHeap_popItem(heap)
 *   - MinHeap_peekItem(heap)
 *
 * Getters:
 *   - MinHeap_isEmpty(heap)
 * ============================================================================
 */



;;GETTER
static inline i32 MinHeap_capacityRaw(const MinHeap *s) { return (*s).capacity; }
;;SETTER
static inline fn MinHeap_setCapacity(MinHeap *s, i32 v) { (*s).capacity = v; }
;;GETTER
static inline const u64 *MinHeap_exhaustionCountPtr(const MinHeap *s) { return &(*s).exhaustionCount; }
;;GETTER
static inline u64 *MinHeap_exhaustionCountMut(MinHeap *s) { return &(*s).exhaustionCount; }
;;SETTER
static inline fn MinHeap_setExhaustionCount(MinHeap *s, u64 v) { (*s).exhaustionCount = v; }
;;GETTER
static inline bool *MinHeap_exhaustionReportedMut(MinHeap *s) { return &(*s).exhaustionReported; }
;;SETTER
static inline fn MinHeap_setExhaustionReported(MinHeap *s, bool v) { (*s).exhaustionReported = v; }
;;GETTER
static inline i32 * MinHeap_items(const MinHeap *s) { return (*s).items; }
;;SETTER
static inline fn MinHeap_setItems(MinHeap *s, i32 * v) { (*s).items = v; }
;;GETTER
static inline f32 * MinHeap_priorities(const MinHeap *s) { return (*s).priorities; }
;;SETTER
static inline fn MinHeap_setPriorities(MinHeap *s, f32 * v) { (*s).priorities = v; }
;;GETTER
static inline i32 MinHeap_sizeRaw(const MinHeap *s) { return (*s).size; }
;;SETTER
static inline fn MinHeap_setSize(MinHeap *s, i32 v) { (*s).size = v; }

// minheap.c — MinHeap port (Legacy: struct/MinHeap.java). 1-based array, sift
// up on push, sift down on pop, strictly lower priority rises to the root.

static fn sift_up(MinHeap *heap, bytes index) {
    i32 item = MinHeap_items(heap)[index];
    f32 prio = MinHeap_priorities(heap)[index];
    while (index > 1) {
        bytes parent = index / 2;
        f32 parent_prio = MinHeap_priorities(heap)[parent];
        if (prio >= parent_prio)
            break;
        MinHeap_items(heap)[index] = MinHeap_items(heap)[parent];
        MinHeap_priorities(heap)[index] = parent_prio;
        index = parent;
    }
    MinHeap_items(heap)[index] = item;
    MinHeap_priorities(heap)[index] = prio;
}

static fn sift_down(MinHeap *heap, bytes index) {
    bytes size = (bytes)MinHeap_sizeRaw(heap);
    i32 item = MinHeap_items(heap)[index];
    f32 prio = MinHeap_priorities(heap)[index];
    bytes half = size / 2;
    while (index <= half) {
        bytes left = index * 2;
        bytes right = left + 1;

        bytes best = left;
        f32 best_prio = MinHeap_priorities(heap)[left];
        if (right <= size) {
            f32 right_prio = MinHeap_priorities(heap)[right];
            if (right_prio < best_prio) {
                best = right;
                best_prio = right_prio;
            }
        }
        if (prio <= best_prio)
            break;
        MinHeap_items(heap)[index] = MinHeap_items(heap)[best];
        MinHeap_priorities(heap)[index] = best_prio;
        index = best;
    }
    MinHeap_items(heap)[index] = item;
    MinHeap_priorities(heap)[index] = prio;
}

MinHeap *MinHeap_1(bytes capacity) {
    bytes slots = capacity + 1;
    MinHeap *heap = (MinHeap*) Memory_alloc(TYPE_MIN_HEAP, sizeof(MinHeap));
    if (!heap) {
        (discard) Struct_reportConstructionExhaustion("minheap", sizeof(MinHeap));
        return nullptr;
    }

    MinHeap_setSize(heap, 0);
    MinHeap_setCapacity(heap, (i32)capacity);
    MinHeap_setExhaustionCount(heap, 0);
    MinHeap_setExhaustionReported(heap, false);
    MinHeap_setItems(heap, (i32*) Memory_alloc(TYPE_INT_ARRAY, slots * sizeof(i32)));
    MinHeap_setPriorities(heap, (f32*) Memory_alloc(TYPE_FLOAT_ARRAY, slots * sizeof(f32)));
    if (!MinHeap_items(heap) || !MinHeap_priorities(heap)) {
        Memory_free(MinHeap_items(heap));
        Memory_free(MinHeap_priorities(heap));
        Memory_free(heap);
        return nullptr;
    }
    return heap;
}

fn MinHeap_free(MinHeap *heap) {
    if (!heap) return;
    Memory_free(MinHeap_items(heap));
    Memory_free(MinHeap_priorities(heap));
    Memory_free(heap);
}

bytes MinHeap_size(MinHeap *heap) {
    if (!heap) return 0;
    return (bytes)MinHeap_sizeRaw(heap);
}

bytes MinHeap_capacity(MinHeap *heap) {
    if (!heap) return 0;
    return (bytes)MinHeap_capacityRaw(heap);
}

bool MinHeap_isEmpty(MinHeap *heap) {
    return MinHeap_size(heap) == 0;
}

;;TEST
u64 MinHeap_exhaustionCount(const MinHeap *heap) {
    return heap ? Struct_exhaustionCount(MinHeap_exhaustionCountPtr(heap)) : 0;
}

i32 MinHeap_push(MinHeap *heap, i32 item, f32 priority) {
    if (!heap) return 0;
    if (MinHeap_size(heap) >= MinHeap_capacity(heap)) {
        // A full fixed-capacity heap is a refusal, not a silent no-op.
        (discard) Struct_reportExhaustion(MinHeap_exhaustionCountMut(heap), MinHeap_exhaustionReportedMut(heap),
                                       "minheap", sizeof(i32) * 2u, (bytes) MinHeap_capacity(heap));
        return 0;
    }
    MinHeap_setSize(heap, MinHeap_size(heap) + 1u);
    bytes index = (bytes)MinHeap_size(heap);
    MinHeap_items(heap)[index] = item;
    MinHeap_priorities(heap)[index] = priority;
    sift_up(heap, index);
    return 1;
}

i32 MinHeap_popItem(MinHeap *heap) {
    if (!heap) return 0;
    if (MinHeap_size(heap) == 0)
        return 0;
    i32 result = MinHeap_items(heap)[1];
    bytes size = (bytes)MinHeap_size(heap);
    MinHeap_items(heap)[1] = MinHeap_items(heap)[size];
    MinHeap_priorities(heap)[1] = MinHeap_priorities(heap)[size];
    MinHeap_setSize(heap, MinHeap_size(heap) - 1u);
    if (MinHeap_size(heap) > 0)
        sift_down(heap, 1);
    return result;
}

i32 MinHeap_peekItem(MinHeap *heap) {
    if (!heap) return 0;
    if (MinHeap_size(heap) == 0)
        return 0;
    return MinHeap_items(heap)[1];
}
MinHeap *MinHeap_0() {
    return MinHeap_1(16);
}
