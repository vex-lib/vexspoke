#ifndef STRUCT_LIST_H
#define STRUCT_LIST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lang/constructor.h"

#include "struct/collection.h"

// struct/list.h — the List class, ported from struct/List.java.
//
// Dynamic stride-based list. Elements live in a contiguous buffer of
// stride(element_class) Bytes each; the buffer grows by List grow chunks when
// full. Because the embedded Collection is the first member, a List pointer is
// also a Collection pointer — Collection_* accessors work on it directly.

typedef struct List {
    Collection collection;
} List;

// Empty list for element_class with initial capacity (min 1024, legacy default).
List *List_2(u32 element_class, bytes capacity);

// List pre-filled to count slots (legacy allocate): active_count = count.
List *List_3(u32 element_class, bytes capacity, bytes count);

// Free the list and its data buffer.
void List_free(List *list);

// Append a scalar value or pointer, growing the buffer if needed.
void List_add(List *list, u64 value_or_pointer);

// Append a new (zeroed) struct slot and return a pointer to it.
u8 *List_addSlot(List *list);

// Get/set the value or pointer at index (bounds-checked).
u64 List_get(List *list, bytes index);
void List_set(List *list, bytes index, u64 value);

// Pointer to the struct element at index (bounds-checked).
u8 *List_slot(List *list, bytes index);

// Remove the element at index, shifting the tail left.
void List_remove(List *list, bytes index);

// Element-wise equality (stride + Bytes must match).
bool List_compare(List *a, List *b);

bool List_isEmpty(List *list);
bytes List_size(List *list);
// Refused-grow count since construction (the Exhaustion Loudness Law).
;;TEST
u64 List_exhaustionCount(const List *list);
bytes List_length(List *list);
bytes List_capacity(List *list);
u32 List_elementClassId(List *list);
bytes List_stride(List *list);
u8 *List_dataBuffer(List *list);


List *List_1(u32 element_class);

#define List(...) CONSTRUCTOR_DISPATCH(List, __VA_ARGS__)

#endif