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
 *   - MinHeap_0(void)
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


// minheap.c — MinHeap port (Legacy: struct/MinHeap.java). 1-based array, sift
// up on push, sift down on pop, strictly lower priority rises to the root.

static void sift_up(MinHeap *heap, bytes index) {
    i32 item = (*heap).items[index];
    f32 prio = (*heap).priorities[index];
    while (index > 1) {
        bytes parent = index / 2;
        f32 parent_prio = (*heap).priorities[parent];
        if (prio >= parent_prio)
            break;
        (*heap).items[index] = (*heap).items[parent];
        (*heap).priorities[index] = parent_prio;
        index = parent;
    }
    (*heap).items[index] = item;
    (*heap).priorities[index] = prio;
}

static void sift_down(MinHeap *heap, bytes index) {
    bytes size = (bytes)(*heap).size;
    i32 item = (*heap).items[index];
    f32 prio = (*heap).priorities[index];
    bytes half = size / 2;
    while (index <= half) {
        bytes left = index * 2;
        bytes right = left + 1;

        bytes best = left;
        f32 best_prio = (*heap).priorities[left];
        if (right <= size) {
            f32 right_prio = (*heap).priorities[right];
            if (right_prio < best_prio) {
                best = right;
                best_prio = right_prio;
            }
        }
        if (prio <= best_prio)
            break;
        (*heap).items[index] = (*heap).items[best];
        (*heap).priorities[index] = best_prio;
        index = best;
    }
    (*heap).items[index] = item;
    (*heap).priorities[index] = prio;
}

MinHeap *MinHeap_1(bytes capacity) {
    bytes slots = capacity + 1;
    MinHeap *heap = (MinHeap*) Memory_alloc(TYPE_MIN_HEAP, sizeof(MinHeap));
    if (!heap) {
        (void) Struct_reportConstructionExhaustion("minheap", sizeof(MinHeap));
        return nullptr;
    }

    (*heap).size = 0;
    (*heap).capacity = (i32)capacity;
    (*heap).exhaustionCount = 0;
    (*heap).exhaustionReported = false;
    (*heap).items = (i32*) Memory_alloc(TYPE_INT_ARRAY, slots * sizeof(i32));
    (*heap).priorities = (f32*) Memory_alloc(TYPE_FLOAT_ARRAY, slots * sizeof(f32));
    if (!(*heap).items || !(*heap).priorities) {
        Memory_free((*heap).items);
        Memory_free((*heap).priorities);
        Memory_free(heap);
        return nullptr;
    }
    return heap;
}

void MinHeap_free(MinHeap *heap) {
    if (!heap) return;
    Memory_free((*heap).items);
    Memory_free((*heap).priorities);
    Memory_free(heap);
}

bytes MinHeap_size(MinHeap *heap) {
    if (!heap) return 0;
    return (bytes)(*heap).size;
}

bytes MinHeap_capacity(MinHeap *heap) {
    if (!heap) return 0;
    return (bytes)(*heap).capacity;
}

bool MinHeap_isEmpty(MinHeap *heap) {
    return MinHeap_size(heap) == 0;
}

;;TEST
u64 MinHeap_exhaustionCount(const MinHeap *heap) {
    return heap ? Struct_exhaustionCount(&(*heap).exhaustionCount) : 0;
}

int MinHeap_push(MinHeap *heap, i32 item, f32 priority) {
    if (!heap) return 0;
    if ((*heap).size >= (*heap).capacity) {
        // A full fixed-capacity heap is a refusal, not a silent no-op.
        (void) Struct_reportExhaustion(&(*heap).exhaustionCount, &(*heap).exhaustionReported,
                                       "minheap", sizeof(i32) * 2u, (bytes) (*heap).capacity);
        return 0;
    }
    (*heap).size++;
    bytes index = (bytes)(*heap).size;
    (*heap).items[index] = item;
    (*heap).priorities[index] = priority;
    sift_up(heap, index);
    return 1;
}

i32 MinHeap_popItem(MinHeap *heap) {
    if (!heap) return 0;
    if ((*heap).size == 0)
        return 0;
    i32 result = (*heap).items[1];
    bytes size = (bytes)(*heap).size;
    (*heap).items[1] = (*heap).items[size];
    (*heap).priorities[1] = (*heap).priorities[size];
    (*heap).size--;
    if ((*heap).size > 0)
        sift_down(heap, 1);
    return result;
}

i32 MinHeap_peekItem(MinHeap *heap) {
    if (!heap) return 0;
    if ((*heap).size == 0)
        return 0;
    return (*heap).items[1];
}
MinHeap *MinHeap_0(void) {
    return MinHeap_1(16);
}
