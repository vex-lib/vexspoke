#ifndef STRUCT_MINHEAP_H
#define STRUCT_MINHEAP_H

#include "vex.h"
#include "struct/exhaustion.h"

// struct/minheap.h — the MinHeap class, ported from struct/MinHeap.java.
//
// Priority queue over (int item, f32 priority) pairs. 1-based indexing for
// cheap heap math; index 0 is unused. Pops the lowest-priority item first.

typedef struct MinHeap {
    i32 size;       // number of live entries
    i32 capacity;   // max entries (1-based slots are capacity + 1)
    i32 *items;     // item at heap index i (i in [1, size])
    f32 *priorities;  // priority at heap index i
    // Exhaustion observability (the Exhaustion Loudness Law): a full heap (or a
    // refused construction) is counted, and the epoch's first refusal reports.
    u64 exhaustionCount;
    bool exhaustionReported;
} MinHeap;

// Heap with room for capacity entries.
MinHeap *MinHeap_1(bytes capacity);

fn MinHeap_free(MinHeap *heap);

bytes MinHeap_size(MinHeap *heap);
bytes MinHeap_capacity(MinHeap *heap);
bool MinHeap_isEmpty(MinHeap *heap);
// Refused-insertion count since construction (the Exhaustion Loudness Law).
;;TEST
u64 MinHeap_exhaustionCount(const MinHeap *heap);

// Push (item, priority). Returns 0 if the heap is full.
int MinHeap_push(MinHeap *heap, i32 item, f32 priority);

// Pop and return the lowest-priority item. Returns 0 if empty.
i32 MinHeap_popItem(MinHeap *heap);

// Peek the lowest-priority item without removing it. Returns 0 if empty.
i32 MinHeap_peekItem(MinHeap *heap);


MinHeap *MinHeap_0();

#define MinHeap(...) CONSTRUCTOR_DISPATCH(MinHeap, __VA_ARGS__)

#endif