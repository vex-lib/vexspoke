// reflection/class.c — the Class metadata record ("class { struct; methods }").

#include "vex.h"
#include "reflection/class.h"

#include "nio/mem.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Class
 * ============================================================================
 * The top of the reflection hierarchy: a folded name, a constructor, a Struct
 * LAYOUT (its fields, borrowed), and a list of Methods held in a never-moved
 * ChunkedList. "class { struct of the class; methods }". The kind is the header
 * typeId (TYPE_REFLECT_CLASS). Class IS the generic kind — there is no separate
 * Generic record.
 *
 * Lifetime: arena-allocated (TYPE_REFLECT_CLASS); free releases the method list
 * but never the borrowed layout. Cold path only (the Cold-Only Reflection Law);
 * getters are null-safe.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Class (reflection/class.c)
 * ============================================================================
 * the Class metadata record (name + construct + Struct layout + Methods).
 *
 * STRUCT FIELDS (Mirroring reflection/class.h):
 * ----------------------------------------------------------------------------
 *   Class {
 *     char name[24];         // folded name (atom grammar)
 *     ClassConstructFn construct; // the constructor
 *     Struct *layout;        // the class's field layout (borrowed)
 *     ChunkedList *methods;  // never-moved Method rows
 *     u32 methodCount;  // live methods (== row count)
 *     u32 pad;          // explicit padding
 *   }
 *
 * PRIVATE HELPERS (kept file-local, pure logic, no behavior of their own):
 *   ensureMethods(self)  // mint the method list lazily (static)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Constructors: (.h) Class_0() .. _3(name, layout, construct), _init, _free
 * Public Core Functions: (.h) Class_kind, _check, _construct, _addMethod, _getMethod, _forEachMethod
 * Public Setters: (.h) Class_setName / _setLayout / _setConstruct
 * Public Getters: (.h) Class_getName / _getLayout / _getConstruct / _methodCount
 * Public String Projections: (.h) Class_toString / _toStringStruct
 * ============================================================================
 */


;;GETTER
static inline ClassConstructFn Class_constructRaw(const Class *s) { return (*s).construct; }
;;SETTER
static inline fn Class_setConstructRaw(Class *s, ClassConstructFn v) { (*s).construct = v; }
;;GETTER
static inline Struct * Class_layout(const Class *s) { return (*s).layout; }
;;SETTER
static inline fn Class_setLayoutRaw(Class *s, Struct * v) { (*s).layout = v; }
;;GETTER
static inline u32 Class_methodCountRaw(const Class *s) { return (*s).methodCount; }
;;SETTER
static inline fn Class_setMethodCount(Class *s, u32 v) { (*s).methodCount = v; }
;;GETTER
static inline ChunkedList * Class_methods(const Class *s) { return (*s).methods; }
;;SETTER
static inline fn Class_setMethods(Class *s, ChunkedList * v) { (*s).methods = v; }
;;GETTER
static inline char *Class_nameMut(Class *s) { return (*s).name; }
;;GETTER
static inline const char *Class_name(const Class *s) { return (*s).name; }

static bool ensureMethods(Class *self) {
    if (Class_methods(self))
        return true;
    ChunkedList *methods = ChunkedList(ID_REFLECT_METHOD, (u32) sizeof(Method), CHUNKED_LIST_TWO_LAYER_DEFAULT);
    if (!methods)
        return false;
    Class_setMethods(self, methods);
    return true;
}

// CONSTRUCTORS

bool Class_init(Class *self, const char *name, Struct *layout, ClassConstructFn construct) {
    if (self == nullptr)
        return false;
    char folded[REFLECT_CLASS_NAME_BYTES];
    if (!VariableSlot_foldName(name, folded))
        return false;
    memset(self, 0, sizeof(*self));
    memcpy(Class_nameMut(self), folded, strlen(folded) + 1);
    Class_setConstructRaw(self, construct);
    Class_setLayoutRaw(self, layout);
    return ensureMethods(self);
}

Class *Class_0() {
    Class *self = (Class*) Memory_alloc(TYPE_REFLECT_CLASS, sizeof(Class));
    if (self == nullptr)
        return nullptr;
    memset(self, 0, sizeof(*self));
    if (!ensureMethods(self)) {
        Memory_free(self);
        return nullptr;
    }
    return self;
}

Class *Class_1(const char *name) {
    return Class_3(name, nullptr, nullptr);
}

Class *Class_2(const char *name, Struct *layout) {
    return Class_3(name, layout, nullptr);
}

Class *Class_3(const char *name, Struct *layout, ClassConstructFn construct) {
    Class *self = (Class*) Memory_alloc(TYPE_REFLECT_CLASS, sizeof(Class));
    if (self == nullptr)
        return nullptr;
    if (!Class_init(self, name, layout, construct)) {
        Memory_free(self);
        return nullptr;
    }
    return self;
}

fn Class_free(Class *self) {
    if (self == nullptr)
        return;
    if (Class_methods(self))
        ChunkedList_free(Class_methods(self));
    Memory_free(self);
}

// CORE FUNCTIONS

u64 Class_kind(const Class *self) {
    if (self == nullptr)
        return 0u;
    return Memory_type((any) self);
}

bool Class_check(const Class *self, u64 typeId) {
    if (self == nullptr)
        return false;
    return Memory_type((any) self) == typeId;
}

any Class_construct(Class *self, any arg) {
    if (self == nullptr || Class_constructRaw(self) == nullptr)
        return nullptr;
    return Class_constructRaw(self)(arg);
}

u32 Class_addMethod(Class *self, const Method *method) {
    if (self == nullptr || method == nullptr || Class_methods(self) == nullptr)
        return REFLECT_CLASS_METHOD_NONE;
    Method *row = (Method*) ChunkedList_addSlot(Class_methods(self));
    if (row == nullptr)
        return REFLECT_CLASS_METHOD_NONE;
    memcpy(row, method, sizeof(Method));
    u32 index = ChunkedList_size(Class_methods(self)) - 1u;
    Class_setMethodCount(self, index + 1u);
    return index;
}

Method *Class_getMethod(const Class *self, u32 index) {
    if (self == nullptr || Class_methods(self) == nullptr)
        return nullptr;
    if (index >= Class_methodCount(self))
        return nullptr;
    return (Method*) ChunkedList_slot(Class_methods(self), index);
}

fn Class_forEachMethod(Class *self, ClassMethodVisitFn fn, any userdata) {
    if (self == nullptr || fn == nullptr || Class_methods(self) == nullptr)
        return;
    u32 n = Class_methodCount(self);
    for (u32 i = 0u; i < n; i++) {
        Method *row = (Method*) ChunkedList_slot(Class_methods(self), i);
        if (row)
            fn(row, i, userdata);
    }
}

// SETTERS

bool Class_setName(Class *self, const char *name) {
    if (self == nullptr)
        return false;
    char folded[REFLECT_CLASS_NAME_BYTES];
    if (!VariableSlot_foldName(name, folded))
        return false;
    memset(Class_nameMut(self), 0, sizeof ((Class){0}).name);
    memcpy(Class_nameMut(self), folded, strlen(folded) + 1);
    return true;
}

fn Class_setLayout(Class *self, Struct *layout) {
    if (self == nullptr)
        return;
    Class_setLayoutRaw(self, layout);
}

fn Class_setConstruct(Class *self, ClassConstructFn construct) {
    if (self == nullptr)
        return;
    Class_setConstructRaw(self, construct);
}

// GETTERS

Struct *Class_getLayout(const Class *self) {
    if (self == nullptr)
        return nullptr;
    return Class_layout(self);
}

ClassConstructFn Class_getConstruct(const Class *self) {
    if (self == nullptr)
        return nullptr;
    return Class_constructRaw(self);
}

const char *Class_getName(const Class *self) {
    return self ? Class_name(self) : nullptr;
}

u32 Class_methodCount(const Class *self) {
    return self ? Class_methodCountRaw(self) : 0u;
}

// STRING PROJECTIONS (the toString Law)

fn Class_toString(const Class *self, char *dest, bytes cap, bool *outTruncated) {
    if (outTruncated)
        *outTruncated = false;
    if (dest == nullptr || cap == 0u)
        return;
    if (self == nullptr) {
        snprintf(dest, cap, "nullptr");
        return;
    }
    i32 written = snprintf(dest, cap, "Class(%s, methods=%u)",
                           Class_name(self)[0] == '\0' ? "?" : Class_name(self), Class_methodCount(self));
    if (written < 0 || (bytes) written >= cap) {
        if (outTruncated)
            *outTruncated = true;
    }
}

fn Class_toStringStruct(const Class *self, char *dest, bytes cap, bool *outTruncated) {
    if (outTruncated)
        *outTruncated = false;
    if (dest == nullptr || cap == 0u)
        return;
    if (self == nullptr) {
        snprintf(dest, cap, "nullptr");
        return;
    }
    i32 written = snprintf(dest, cap, "Class { name=\"%s\", construct=0x%llx, layout=0x%llx, methods=0x%llx, methodCount=%u }",
                           Class_name(self),
                           (u64) (pointer) Class_constructRaw(self),
                           (u64) (pointer) Class_layout(self),
                           (u64) (pointer) Class_methods(self),
                           Class_methodCount(self));
    if (written < 0 || (bytes) written >= cap) {
        if (outTruncated)
            *outTruncated = true;
    }
}
