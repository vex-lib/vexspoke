// reflection/field.c — the Field metadata record ("field { variable }").

#include "vex.h"
#include "reflection/field.h"

#include "nio/mem.h"
#include "struct/stride.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Field
 * ============================================================================
 * A Field EMBEDS a Variable and adds a setter plus the value's physical layout
 * (typeId, offset, size, flags): the name, reader and target all live in the
 * embedded Variable, so "field { variable }" is literal, while offset/size/typeId
 * describe where the value's Bytes sit for persistence and export. The kind is
 * the header typeId (TYPE_REFLECT_FIELD) when arena-allocated; a Field stored as
 * a row inside a Struct carries no header (identity is its index in the Struct).
 *
 * Lifetime: arena-allocated (TYPE_REFLECT_FIELD), embedded, or a Struct row.
 * Cold path only (the Cold-Only Reflection Law); getters are null-safe.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Field (reflection/field.c)
 * ============================================================================
 * the Field metadata record (an embedded Variable + setter).
 *
 * STRUCT FIELDS (Mirroring reflection/field.h):
 * ----------------------------------------------------------------------------
 *   Field {
 *     Variable variable; // the embedded name + reader + target
 *     FieldSetFn set;    // the setter
 *     u64 typeId;   // physical: value's project|form|class id
 *     u32 offset;   // physical: byte offset within the entity row
 *     u32 size;     // physical: value byte width
 *     u32 flags;    // physical: REFLECT_FIELD_* bits
 *     u32 pad;      // explicit padding
 *   }
 *
 * PRIVATE HELPERS (kept file-local, pure logic, no behavior of their own): none.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Constructors: (.h) Field_0() .. _4(name, read, set, target), _init, _free
 * Public Core Functions: (.h) Field_kind, _check, _getVariable, _read, _write
 * Public Setters: (.h) Field_setName / _setRead / _setSet / _setTarget
 *                      Field_setTypeId / _setOffset / _setSize / _setFlags
 * Public Getters: (.h) Field_getName / _getRead / _getSet / _getTarget
 *                      Field_getTypeId / _getOffset / _getSize / _getFlags
 *                      Field_isNullable / _isKey / _isIndexed
 * Public String Projections: (.h) Field_toString / _toStringStruct
 * ============================================================================
 */


;;GETTER
static inline u32 Field_flags(const Field *s) { return (*s).flags; }
;;GETTER
static inline u32 *Field_flagsMut(Field *s) { return &(*s).flags; }
;;SETTER
static inline fn Field_setFlags(Field *s, u32 v) { (*s).flags = v; }
;;GETTER
static inline u32 Field_offset(const Field *s) { return (*s).offset; }
;;GETTER
static inline u32 *Field_offsetMut(Field *s) { return &(*s).offset; }
;;SETTER
static inline fn Field_setOffset(Field *s, u32 v) { (*s).offset = v; }
;;GETTER
static inline u32 Field_pad(const Field *s) { return (*s).pad; }
;;GETTER
static inline u32 *Field_padMut(Field *s) { return &(*s).pad; }
;;SETTER
static inline fn Field_setPad(Field *s, u32 v) { (*s).pad = v; }
;;GETTER
static inline FieldSetFn Field_set(const Field *s) { return (*s).set; }
;;GETTER
static inline FieldSetFn *Field_mutSet(Field *s) { return &(*s).set; }
;;SETTER
static inline fn Field_setSet(Field *s, FieldSetFn v) { (*s).set = v; }
;;GETTER
static inline u32 Field_size(const Field *s) { return (*s).size; }
;;GETTER
static inline u32 *Field_sizeMut(Field *s) { return &(*s).size; }
;;SETTER
static inline fn Field_setSize(Field *s, u32 v) { (*s).size = v; }
;;GETTER
static inline u64 Field_typeId(const Field *s) { return (*s).typeId; }
;;GETTER
static inline u64 *Field_typeIdMut(Field *s) { return &(*s).typeId; }
;;SETTER
static inline fn Field_setTypeId(Field *s, u64 v) { (*s).typeId = v; }
;;GETTER
static inline Variable Field_variable(const Field *s) { return (*s).variable; }
;;GETTER
static inline Variable *Field_variableMut(Field *s) { return &(*s).variable; }
;;SETTER
static inline fn Field_setVariable(Field *s, Variable v) { (*s).variable = v; }

;;GETTER
static inline char *Variable_nameMut(Variable *s) { return (*s).name; }
;;GETTER
static inline const char *Variable_name(const Variable *s) { return (*s).name; }

// CONSTRUCTORS

bool Field_init(Field *self, const char *name, VariableReadFn read, FieldSetFn set, any target) {
    if (self == nullptr)
        return false;
    Variable *variable = Field_variableMut(self);
    if (!Variable_init(variable, name, read, target))
        return false;
    Field_setSet(self, set);
    Field_setTypeId(self, 0u);
    Field_setOffset(self, 0u);
    Field_setSize(self, 0u);
    Field_setFlags(self, REFLECT_FIELD_NONE);
    Field_setPad(self, 0u);
    return true;
}

Field *Field_0() {
    Field *self = (Field*) Memory_alloc(TYPE_REFLECT_FIELD, sizeof(Field));
    if (self == nullptr)
        return nullptr;
    memset(self, 0, sizeof(*self));
    return self;
}

Field *Field_1(const char *name) {
    return Field_4(name, nullptr, nullptr, nullptr);
}

Field *Field_2(const char *name, VariableReadFn read) {
    return Field_4(name, read, nullptr, nullptr);
}

Field *Field_3(const char *name, VariableReadFn read, FieldSetFn set) {
    return Field_4(name, read, set, nullptr);
}

Field *Field_4(const char *name, VariableReadFn read, FieldSetFn set, any target) {
    Field *self = Field_0();
    if (self == nullptr)
        return nullptr;
    if (!Field_init(self, name, read, set, target)) {
        Memory_free(self);
        return nullptr;
    }
    return self;
}

fn Field_free(Field *self) {
    if (self != nullptr)
        Memory_free(self);
}

// CORE FUNCTIONS

u64 Field_kind(const Field *self) {
    if (self == nullptr)
        return 0u;
    return Memory_type((any) self);
}

bool Field_check(const Field *self, u64 typeId) {
    if (self == nullptr)
        return false;
    return Memory_type((any) self) == typeId;
}

Variable *Field_getVariable(Field *self) {
    if (self == nullptr)
        return nullptr;
    return Field_variableMut(self);
}

any Field_read(Field *self, any receiver) {
    if (self == nullptr)
        return nullptr;
    Variable *variable = Field_variableMut(self);
    return Variable_read(variable, receiver);
}

fn Field_write(Field *self, any receiver, any value) {
    if (self == nullptr || Field_set(self) == nullptr)
        return;
    Field_set(self)(receiver, value);
}

// SETTERS

bool Field_setName(Field *self, const char *name) {
    if (self == nullptr)
        return false;
    Variable *variable = Field_variableMut(self);
    return Variable_setName(variable, name);
}

fn Field_setRead(Field *self, VariableReadFn read) {
    if (self == nullptr)
        return;
    Variable *variable = Field_variableMut(self);
    Variable_setRead(variable, read);
}

fn Field_setSet(Field *self, FieldSetFn set) {
    if (self == nullptr)
        return;
    Field_setSet(self, set);
}

fn Field_setTarget(Field *self, any target) {
    if (self == nullptr)
        return;
    Variable *variable = Field_variableMut(self);
    Variable_setTarget(variable, target);
}

// GETTERS

const char *Field_getName(const Field *self) {
    if (self == nullptr)
        return nullptr;
    const Variable *variable = Field_variableMut(self);
    return Variable_getName(variable);
}

VariableReadFn Field_getRead(const Field *self) {
    if (self == nullptr)
        return nullptr;
    const Variable *variable = Field_variableMut(self);
    return Variable_getRead(variable);
}

FieldSetFn Field_getSet(const Field *self) {
    if (self == nullptr)
        return nullptr;
    return Field_set(self);
}

any Field_getTarget(const Field *self) {
    if (self == nullptr)
        return nullptr;
    const Variable *variable = Field_variableMut(self);
    return Variable_getTarget(variable);
}

// PHYSICAL LAYOUT (the entity-field half)

fn Field_setTypeId(Field *self, u64 typeId) {
    if (self == nullptr)
        return;
    Field_setTypeId(self, typeId);
    if (Field_size(self) == 0u && typeId != 0u)
        Field_setSize(self, (u32) Stride_get((u32) Type_class(typeId)));
}

u64 Field_getTypeId(const Field *self) {
    return self ? Field_typeId(self) : 0u;
}

fn Field_setOffset(Field *self, u32 offset) {
    if (self != nullptr)
        Field_setOffset(self, offset);
}

u32 Field_getOffset(const Field *self) {
    return self ? Field_offset(self) : 0u;
}

fn Field_setSize(Field *self, u32 size) {
    if (self != nullptr)
        Field_setSize(self, size);
}

u32 Field_getSize(const Field *self) {
    return self ? Field_size(self) : 0u;
}

fn Field_setFlags(Field *self, u32 flags) {
    if (self != nullptr)
        Field_setFlags(self, flags);
}

u32 Field_getFlags(const Field *self) {
    return self ? Field_flags(self) : REFLECT_FIELD_NONE;
}

bool Field_isNullable(const Field *self) {
    return self != nullptr && (Field_flags(self) & REFLECT_FIELD_NULLABLE) != 0u;
}

bool Field_isKey(const Field *self) {
    return self != nullptr && (Field_flags(self) & REFLECT_FIELD_KEY) != 0u;
}

bool Field_isIndexed(const Field *self) {
    return self != nullptr && (Field_flags(self) & REFLECT_FIELD_INDEXED) != 0u;
}

// STRING PROJECTIONS (the toString Law)

fn Field_toString(const Field *self, char *dest, bytes cap, bool *outTruncated) {
    if (outTruncated)
        *outTruncated = false;
    if (dest == nullptr || cap == 0u)
        return;
    if (self == nullptr) {
        snprintf(dest, cap, "nullptr");
        return;
    }
    const Variable *variable = Field_variableMut(self);
    i32 written = snprintf(dest, cap, "Field(%s)", Variable_nameMut(variable)[0] == '\0' ? "?" : Variable_nameMut(variable));
    if (written < 0 || (bytes) written >= cap) {
        if (outTruncated)
            *outTruncated = true;
    }
}

fn Field_toStringStruct(const Field *self, char *dest, bytes cap, bool *outTruncated) {
    if (outTruncated)
        *outTruncated = false;
    if (dest == nullptr || cap == 0u)
        return;
    if (self == nullptr) {
        snprintf(dest, cap, "nullptr");
        return;
    }
    const Variable *variable = Field_variableMut(self);
    char inner[96];
    bool innerTruncated = false;
    Variable_toString(variable, inner, sizeof(inner), &innerTruncated);
    i32 written = snprintf(dest, cap,
                           "Field { variable=%s, set=0x%llx, typeId=0x%llx, offset=%u, size=%u, flags=0x%x }",
                           inner, (u64) (pointer) Field_set(self),
                           (u64) Field_typeId(self), Field_offset(self),
                           Field_size(self), Field_flags(self));
    if (written < 0 || (bytes) written >= cap || innerTruncated) {
        if (outTruncated)
            *outTruncated = true;
    }
}
