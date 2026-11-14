#ifndef REFLECTION_FIELD_H
#define REFLECTION_FIELD_H

#include "vex.h"
#include "reflection/variable.h"

// reflection/field.h — the Field metadata record.
//
// A Field IS a Variable plus a setter, plus the value's PHYSICAL layout: one
// record both reads/writes a value AND describes where its Bytes live. "field {
// variable }" made literal — the name, reader and target all live in the
// embedded Variable; the field adds the writer (set) and the layout (typeId,
// offset, size, flags). The same Field therefore serves live reflection
// (read/write), persistence and export (offset/size/typeId drive row Bytes), and
// schema walking, so vexspoke, the storage engine and their consumers speak one
// vocabulary (the Entity Model Law). A plain Struct is the entity; a Class adds
// a constructor and Methods. The kind is the header typeId (TYPE_REFLECT_FIELD).
// Block layout:
// [MemoryHeader 16][Variable 40][set 8][typeId 8][offset 4][size 4][flags 4][pad 4] = 88 Bytes.

typedef fn (*FieldSetFn)(any , any );

// A Field's name is the embedded Variable's name (same 24-byte grammar).
#define REFLECT_FIELD_NAME_BYTES REFLECT_VARIABLE_NAME_BYTES
#define REFLECT_FIELD_NAME_MAX REFLECT_VARIABLE_NAME_MAX

// Physical-layout flags (the REFLECT_FIELD_* bits).
#define REFLECT_FIELD_NONE     0x00u
#define REFLECT_FIELD_NULLABLE 0x01u // the value may be null/absent
#define REFLECT_FIELD_KEY      0x02u // identifies the row within its entity
#define REFLECT_FIELD_INDEXED  0x04u // an index is maintained for this field

typedef struct Field {
    Variable variable; // the embedded name + reader + target
    FieldSetFn set;    // the setter
    u64 typeId;   // physical: value's project|form|class id (0 = unset)
    u32 offset;   // physical: byte offset within the owning entity row
    u32 size;     // physical: value byte width (0 = derive from typeId)
    u32 flags;    // physical: REFLECT_FIELD_* bits
    u32 pad;      // explicit padding
} Field;

_Static_assert(sizeof(Field) == 72u, "Field must stay 72 Bytes");

bool Field_init(Field *self, const char *name, VariableReadFn read, FieldSetFn set, any target);
Field *Field_0();
Field *Field_1(const char *name);
Field *Field_2(const char *name, VariableReadFn read);
Field *Field_3(const char *name, VariableReadFn read, FieldSetFn set);
Field *Field_4(const char *name, VariableReadFn read, FieldSetFn set, any target);
#define Field(...) CONSTRUCTOR_DISPATCH(Field, __VA_ARGS__)
fn Field_free(Field *self);

u64 Field_kind(const Field *self);
bool Field_check(const Field *self, u64 typeId);
// The embedded Variable (borrowed; lifetime == the field's).
Variable *Field_getVariable(Field *self);
// Read / write through the embedded reader and the setter (null-safe).
any Field_read(Field *self, any receiver);
fn Field_write(Field *self, any receiver, any value);

bool Field_setName(Field *self, const char *name);
const char *Field_getName(const Field *self);
fn Field_setRead(Field *self, VariableReadFn read);
VariableReadFn Field_getRead(const Field *self);
fn Field_setSet(Field *self, FieldSetFn set);
FieldSetFn Field_getSet(const Field *self);
fn Field_setTarget(Field *self, any target);
any Field_getTarget(const Field *self);

// --- Physical layout (the entity-field half) ---
// setTypeId stamps the value's type id and, when size is still unset (0),
// derives the byte width from the class via Stride_get. setSize overrides.
fn Field_setTypeId(Field *self, u64 typeId);
u64 Field_getTypeId(const Field *self);
fn Field_setOffset(Field *self, u32 offset);
u32 Field_getOffset(const Field *self);
fn Field_setSize(Field *self, u32 size);
u32 Field_getSize(const Field *self);
fn Field_setFlags(Field *self, u32 flags);
u32 Field_getFlags(const Field *self);
bool Field_isNullable(const Field *self);
bool Field_isKey(const Field *self);
bool Field_isIndexed(const Field *self);

fn Field_toString(const Field *self, char *dest, bytes cap, bool *outTruncated);
fn Field_toStringStruct(const Field *self, char *dest, bytes cap, bool *outTruncated);

#endif
