#ifndef REFLECTION_STRUCT_H
#define REFLECTION_STRUCT_H

#include "vex.h"
#include "reflection/field.h"
#include "struct/chunked_list.h"

// reflection/struct.h — the Struct metadata record (a list of Fields).
//
// A Struct is a named, ordered list of Fields — "struct { field }". It is the
// entity (table): each Field carries the value's physical layout, and the
// Struct's own `size` is the byte stride of one entity row. Rows live in a
// never-moved ChunkedList (data-oriented, growable, stable addresses), so a
// Field's address is stable for the Struct's whole life. The kind is the header
// typeId (TYPE_REFLECT_STRUCT). A Class adds a constructor and Methods. Block
// layout: [MemoryHeader 16][name 24][count 4][size 4][fields 8] = 56 Bytes.

#define REFLECT_STRUCT_NAME_BYTES VARIABLE_SLOT_NAME_BYTES
#define REFLECT_STRUCT_NAME_MAX VARIABLE_SLOT_NAME_MAX
#define REFLECT_STRUCT_INDEX_NONE UINT32_MAX

typedef struct Struct {
    char name[REFLECT_STRUCT_NAME_BYTES]; // folded name (atom grammar)
    u32 count;                       // live fields (== row count)
    u32 size;                        // entity row byte stride (0 = unknown)
    ChunkedList *fields;                  // never-moved Field rows
} Struct;

_Static_assert(sizeof(Struct) == 40u, "Struct must stay 40 Bytes");

bool Struct_init(Struct *self, const char *name);
Struct *Struct_0();
Struct *Struct_1(const char *name);
#define Struct(...) CONSTRUCTOR_DISPATCH(Struct, __VA_ARGS__)
fn Struct_free(Struct *self);

u64 Struct_kind(const Struct *self);
bool Struct_check(const Struct *self, u64 typeId);
// Append a copy of a field. Returns its index, or REFLECT_STRUCT_INDEX_NONE.
u32 Struct_add(Struct *self, const Field *field);
// The field row at `index` (mutable, stable), or null.
Field *Struct_get(const Struct *self, u32 index);
// Visit every field row in order (cold). Must not mutate the Struct.
typedef fn (*StructVisitFn)(Field *field, u32 index, any userdata);
fn Struct_forEach(Struct *self, StructVisitFn fn, any userdata);

bool Struct_setName(Struct *self, const char *name);
// The entity row byte stride (0 = unknown). Struct_add does not lay out
// offsets; a caller setting physical Fields also sets the row size.
fn Struct_setSize(Struct *self, u32 size);
u32 Struct_getSize(const Struct *self);
const char *Struct_getName(const Struct *self);
u32 Struct_count(const Struct *self);
bool Struct_isEmpty(const Struct *self);

fn Struct_toString(const Struct *self, char *dest, bytes cap, bool *outTruncated);
fn Struct_toStringStruct(const Struct *self, char *dest, bytes cap, bool *outTruncated);

#endif
