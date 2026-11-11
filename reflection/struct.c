// reflection/struct.c — the Struct metadata record ("struct { field }").

#include "reflection/struct.h"

#include <stdio.h>
#include <string.h>

#include "util/annotation.h"
#include "nio/mem.h"
#include "oop/type.h"

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

static bool ensureFields(Struct *self) {
    if ((*self).fields)
        return true;
    ChunkedList *fields = ChunkedList(ID_REFLECT_FIELD, (u32) sizeof(Field), CHUNKED_LIST_TWO_LAYER_DEFAULT);
    if (!fields)
        return false;
    (*self).fields = fields;
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
    memcpy((*self).name, folded, strlen(folded) + 1);
    return ensureFields(self);
}

Struct *Struct_0(void) {
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

void Struct_free(Struct *self) {
    if (self == nullptr)
        return;
    if ((*self).fields)
        ChunkedList_free((*self).fields);
    Memory_free(self);
}

// CORE FUNCTIONS

u64 Struct_kind(const Struct *self) {
    if (self == nullptr)
        return 0u;
    return Memory_type((void*) self);
}

bool Struct_check(const Struct *self, u64 typeId) {
    if (self == nullptr)
        return false;
    return Memory_type((void*) self) == typeId;
}

u32 Struct_add(Struct *self, const Field *field) {
    if (self == nullptr || field == nullptr || (*self).fields == nullptr)
        return REFLECT_STRUCT_INDEX_NONE;
    Field *row = (Field*) ChunkedList_addSlot((*self).fields);
    if (row == nullptr)
        return REFLECT_STRUCT_INDEX_NONE;
    memcpy(row, field, sizeof(Field));
    u32 index = ChunkedList_size((*self).fields) - 1u;
    (*self).count = index + 1u;
    return index;
}

Field *Struct_get(const Struct *self, u32 index) {
    if (self == nullptr || (*self).fields == nullptr)
        return nullptr;
    if (index >= (*self).count)
        return nullptr;
    return (Field*) ChunkedList_slot((*self).fields, index);
}

void Struct_forEach(Struct *self, StructVisitFn fn, void *userdata) {
    if (self == nullptr || fn == nullptr || (*self).fields == nullptr)
        return;
    u32 n = (*self).count;
    for (u32 i = 0u; i < n; i++) {
        Field *row = (Field*) ChunkedList_slot((*self).fields, i);
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
    memset((*self).name, 0, sizeof((*self).name));
    memcpy((*self).name, folded, strlen(folded) + 1);
    return true;
}

void Struct_setSize(Struct *self, u32 size) {
    if (self != nullptr)
        (*self).size = size;
}

// GETTERS

const char *Struct_getName(const Struct *self) {
    return self ? (*self).name : nullptr;
}

u32 Struct_count(const Struct *self) {
    return self ? (*self).count : 0u;
}

u32 Struct_getSize(const Struct *self) {
    return self ? (*self).size : 0u;
}

bool Struct_isEmpty(const Struct *self) {
    if (self == nullptr)
        return true;
    return (*self).count == 0u;
}

// STRING PROJECTIONS (the toString Law)

void Struct_toString(const Struct *self, char *dest, bytes cap, bool *outTruncated) {
    if (outTruncated)
        *outTruncated = false;
    if (dest == nullptr || cap == 0u)
        return;
    if (self == nullptr) {
        snprintf(dest, cap, "nullptr");
        return;
    }
    int written = snprintf(dest, cap, "Struct(%s, fields=%u)",
                           (*self).name[0] == '\0' ? "?" : (*self).name, (*self).count);
    if (written < 0 || (bytes) written >= cap) {
        if (outTruncated)
            *outTruncated = true;
    }
}

void Struct_toStringStruct(const Struct *self, char *dest, bytes cap, bool *outTruncated) {
    if (outTruncated)
        *outTruncated = false;
    if (dest == nullptr || cap == 0u)
        return;
    if (self == nullptr) {
        snprintf(dest, cap, "nullptr");
        return;
    }
    int written = snprintf(dest, cap, "Struct { name=\"%s\", count=%u, size=%u, fields=0x%llx }",
                           (*self).name, (*self).count, (*self).size,
                           (unsigned long long) (uintptr_t) (*self).fields);
    if (written < 0 || (bytes) written >= cap) {
        if (outTruncated)
            *outTruncated = true;
    }
}
