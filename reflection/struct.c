// reflection/struct.c — the Struct metadata record ("struct { field }").

#include "vex.h"
#include "reflection/struct.h"

#include "nio/mem.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Struct
 * ============================================================================
 * A named, ordered list of Fields held in a never-moved ChunkedList (page-sized
 * leaves) — data-oriented, growable, and address-stable. "struct { field }". The
 * kind is the header typeId (TYPE_REFLECT_STRUCT); the Field rows inside carry no
 * header (a field is identified by its index here).
 *
 * Lifetime: arena-allocated (TYPE_REFLECT_STRUCT); free releases the field list.
 * Cold path only (the Cold-Only Reflection Law); getters are null-safe.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Struct (reflection/struct.c)
 * ============================================================================
 * the Struct metadata record (a named list of Fields).
 *
 * STRUCT FIELDS (Mirroring reflection/struct.h):
 * ----------------------------------------------------------------------------
 *   Struct {
 *     char name[24];     // folded name (atom grammar)
 *     u32 count;    // live fields (== row count)
 *     u32 size;     // entity row byte stride (0 = unknown)
 *     ChunkedList *fields; // never-moved Field rows
 *   }
 *
 * PRIVATE HELPERS (kept file-local, pure logic, no behavior of their own):
 *   ensureFields(self)  // mint the field list lazily (static)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Constructors: (.h) Struct_0(), Struct_1(name), _init, _free
 * Public Core Functions: (.h) Struct_kind, _check, _add, _get, _forEach
 * Public Setters: (.h) Struct_setName, Struct_setSize
 * Public Getters: (.h) Struct_getName, _count, _getSize, _isEmpty
 * Public String Projections: (.h) Struct_toString / _toStringStruct
 * ============================================================================
 */


;;GETTER
static inline u32 Struct_count(const Struct *s) { return (*s).count; }
;;GETTER
static inline u32 *Struct_countMut(Struct *s) { return &(*s).count; }
;;SETTER
static inline fn Struct_setCount(Struct *s, u32 v) { (*s).count = v; }
;;GETTER
static inline ChunkedList * Struct_fields(const Struct *s) { return (*s).fields; }
;;GETTER
static inline ChunkedList * *Struct_fieldsMut(Struct *s) { return &(*s).fields; }
;;SETTER
static inline fn Struct_setFields(Struct *s, ChunkedList * v) { (*s).fields = v; }
;;GETTER
static inline char *Struct_nameMut(Struct *s) { return (*s).name; }
;;GETTER
static inline const char *Struct_name(const Struct *s) { return (*s).name; }
;;GETTER
static inline u32 Struct_size(const Struct *s) { return (*s).size; }
;;GETTER
static inline u32 *Struct_sizeMut(Struct *s) { return &(*s).size; }
;;SETTER
static inline fn Struct_setSize(Struct *s, u32 v) { (*s).size = v; }

static bool ensureFields(Struct *self) {
    if (Struct_fields(self))
        return true;
    ChunkedList *fields = ChunkedList(ID_REFLECT_FIELD, (u32) sizeof(Field), CHUNKED_LIST_TWO_LAYER_DEFAULT);
    if (!fields)
        return false;
    Struct_setFields(self, fields);
    return true;
}

// CONSTRUCTORS

bool Struct_init(Struct *self, const char *name) {
    if (self == nullptr)
        return false;
    char folded[REFLECT_STRUCT_NAME_BYTES];
    if (!VariableSlot_foldName(name, folded))
        return false;
    memset(self, 0, sizeof(*self));
    memcpy(Struct_nameMut(self), folded, strlen(folded) + 1);
    return ensureFields(self);
}

Struct *Struct_0() {
    Struct *self = (Struct*) Memory_alloc(TYPE_REFLECT_STRUCT, sizeof(Struct));
    if (self == nullptr)
        return nullptr;
    memset(self, 0, sizeof(*self));
    if (!ensureFields(self)) {
        Memory_free(self);
        return nullptr;
    }
    return self;
}

Struct *Struct_1(const char *name) {
    Struct *self = (Struct*) Memory_alloc(TYPE_REFLECT_STRUCT, sizeof(Struct));
    if (self == nullptr)
        return nullptr;
    if (!Struct_init(self, name)) {
        Memory_free(self);
        return nullptr;
    }
    return self;
}

fn Struct_free(Struct *self) {
    if (self == nullptr)
        return;
    if (Struct_fields(self))
        ChunkedList_free(Struct_fields(self));
    Memory_free(self);
}

// CORE FUNCTIONS

u64 Struct_kind(const Struct *self) {
    if (self == nullptr)
        return 0u;
    return Memory_type((any) self);
}

bool Struct_check(const Struct *self, u64 typeId) {
    if (self == nullptr)
        return false;
    return Memory_type((any) self) == typeId;
}

u32 Struct_add(Struct *self, const Field *field) {
    if (self == nullptr || field == nullptr || Struct_fields(self) == nullptr)
        return REFLECT_STRUCT_INDEX_NONE;
    Field *row = (Field*) ChunkedList_addSlot(Struct_fields(self));
    if (row == nullptr)
        return REFLECT_STRUCT_INDEX_NONE;
    memcpy(row, field, sizeof(Field));
    u32 index = ChunkedList_size(Struct_fields(self)) - 1u;
    Struct_setCount(self, index + 1u);
    return index;
}

Field *Struct_get(const Struct *self, u32 index) {
    if (self == nullptr || Struct_fields(self) == nullptr)
        return nullptr;
    if (index >= Struct_count(self))
        return nullptr;
    return (Field*) ChunkedList_slot(Struct_fields(self), index);
}

fn Struct_forEach(Struct *self, StructVisitFn fn, any userdata) {
    if (self == nullptr || fn == nullptr || Struct_fields(self) == nullptr)
        return;
    u32 n = Struct_count(self);
    for (u32 i = 0u; i < n; i++) {
        Field *row = (Field*) ChunkedList_slot(Struct_fields(self), i);
        if (row)
            fn(row, i, userdata);
    }
}

// SETTERS

bool Struct_setName(Struct *self, const char *name) {
    if (self == nullptr)
        return false;
    char folded[REFLECT_STRUCT_NAME_BYTES];
    if (!VariableSlot_foldName(name, folded))
        return false;
    memset(Struct_nameMut(self), 0, sizeof ((Struct){0}).name);
    memcpy(Struct_nameMut(self), folded, strlen(folded) + 1);
    return true;
}

fn Struct_setSize(Struct *self, u32 size) {
    if (self != nullptr)
        Struct_setSize(self, size);
}

// GETTERS

const char *Struct_getName(const Struct *self) {
    return self ? Struct_nameMut(self) : nullptr;
}

u32 Struct_count(const Struct *self) {
    return self ? Struct_count(self) : 0u;
}

u32 Struct_getSize(const Struct *self) {
    return self ? Struct_size(self) : 0u;
}

bool Struct_isEmpty(const Struct *self) {
    if (self == nullptr)
        return true;
    return Struct_count(self) == 0u;
}

// STRING PROJECTIONS (the toString Law)

fn Struct_toString(const Struct *self, char *dest, bytes cap, bool *outTruncated) {
    if (outTruncated)
        *outTruncated = false;
    if (dest == nullptr || cap == 0u)
        return;
    if (self == nullptr) {
        snprintf(dest, cap, "nullptr");
        return;
    }
    i32 written = snprintf(dest, cap, "Struct(%s, fields=%u)",
                           Struct_nameMut(self)[0] == '\0' ? "?" : Struct_nameMut(self), Struct_count(self));
    if (written < 0 || (bytes) written >= cap) {
        if (outTruncated)
            *outTruncated = true;
    }
}

fn Struct_toStringStruct(const Struct *self, char *dest, bytes cap, bool *outTruncated) {
    if (outTruncated)
        *outTruncated = false;
    if (dest == nullptr || cap == 0u)
        return;
    if (self == nullptr) {
        snprintf(dest, cap, "nullptr");
        return;
    }
    i32 written = snprintf(dest, cap, "Struct { name=\"%s\", count=%u, size=%u, fields=0x%llx }",
                           Struct_nameMut(self), Struct_count(self), Struct_size(self),
                           (u64) (pointer) Struct_fields(self));
    if (written < 0 || (bytes) written >= cap) {
        if (outTruncated)
            *outTruncated = true;
    }
}
