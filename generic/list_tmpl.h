#ifndef GENERIC_LIST_TMPL_H
#define GENERIC_LIST_TMPL_H

#include "vex.h"

#include "generic/generic_cat.h"
#include "struct/list.h"

// generic/list_tmpl.h — the List stamp.
//
// A typed list is a thin, compile-time-typed facade over the type-erased List
// engine: the struct holds exactly one `List base`, so a typed handle is also
// an engine handle and construction is one List_initStride in place. The stamp
// binds the element class id and the slot width once; every method is a
// one-line forwarder, so the element type is checked by the compiler and the
// storage stays the engine's.
//
// Spelling (method-first, one token per type):
//
//     List(u32) *l = List(u32)(23);   // -> List_u32 *l = List_u32_2(23);
//     List_add(u32)(l, 7);            // -> List_u32_add(l, 7);
//     u32 v = List_get(u32)(l, 0);    // -> List_u32_get(l, 0);
//     u32 *p = List_slot(u32)(l, 0);  // -> List_u32_slot(l, 0);
//     List_free(u32)(l);              // -> List_u32_free(l);
//
// A macro cannot define another macro, so each instantiated type also needs
// one chooser line in generic/list_stamps.h:
//
//     #define List_u32(...) List_u32_2(__VA_ARGS__)
//
// That line is what makes `List_u32(23)` a constructor call while `List_u32 *`
// stays the type: a function-like macro only expands when followed by `(`.
//
// VALUES AND OBJECTS ARE DIFFERENT STAMPS.
//
//   TYPEDEF_LIST_VALUE(u8, ID_BYTE)        element value is u8; one slot is
//                                          sizeof(u8) bytes.
//   TYPEDEF_LIST_OBJECT(Buffer, ID_BUFFER) element value is Buffer*; one slot
//                                          is sizeof(Buffer*) bytes.
//
// A value stamp copies the whole element through List_addSlot/memcpy, so a
// multi-word shape (ld128, mat4f, SegmentEntry) is stored whole instead of
// being truncated to one engine word. An object stamp stores the pointer word.
// They are never mixed, because `(u64) someFloat` is not the value.
//
// NESTING. VEX_LIST_DECLARE_NESTED(T, LIST_CLASS_ID) is the depth-2 stamp:
// List(List(T)) is an object list of List_<T> handles, so it reuses the object
// body with the class id of the inner List. The caller passes the id because
// this header stays free of the repo id registry; generic/list_stamps.h passes
// ID_LIST from VEX_LIST_FAMILY.
//
// IMPLEMENTATION SIDE. `IMPLEMENT_LIST_*` expands calls to the engine names
// (List_add/List_get/List_slot/List_free). Expand it in a translation unit that
// does NOT include generic/classgeneric_repo.h: that header defines the
// method-first macros with those same names, and a function-like macro would
// capture the call. generic/list_stamps.c is that unit.

// The typed class and its per-type function names.
#define VEX_LIST_CLASS(T)   VEX_CAT(List_, T)
#define VEX_LIST_FN(T, SUF) VEX_CAT(VEX_CAT(List_, T), SUF)

// --- declarations ----------------------------------------------------------

#define TYPEDEF_LIST_VALUE(T, CLASS_ID)                                      \
    ;;CLASSNAME("List_" VEX_STR(T))                                          \
    typedef struct VEX_LIST_CLASS(T) {                                       \
        List base;                                                           \
    } VEX_LIST_CLASS(T);                                                     \
    VEX_LIST_CLASS(T) *VEX_LIST_FN(T, _2)(bytes capacity);                   \
    void VEX_LIST_FN(T, _add)(VEX_LIST_CLASS(T) *self, T value);             \
    T VEX_LIST_FN(T, _get)(VEX_LIST_CLASS(T) *self, bytes index);            \
    T *VEX_LIST_FN(T, _slot)(VEX_LIST_CLASS(T) *self, bytes index);          \
    void VEX_LIST_FN(T, _free)(VEX_LIST_CLASS(T) *self);

#define TYPEDEF_LIST_OBJECT(T, CLASS_ID)                                     \
    ;;CLASSNAME("List_" VEX_STR(T))                                          \
    typedef struct VEX_LIST_CLASS(T) {                                       \
        List base;                                                           \
    } VEX_LIST_CLASS(T);                                                     \
    VEX_LIST_CLASS(T) *VEX_LIST_FN(T, _2)(bytes capacity);                   \
    void VEX_LIST_FN(T, _add)(VEX_LIST_CLASS(T) *self, T *value);            \
    T *VEX_LIST_FN(T, _get)(VEX_LIST_CLASS(T) *self, bytes index);           \
    T **VEX_LIST_FN(T, _slot)(VEX_LIST_CLASS(T) *self, bytes index);         \
    void VEX_LIST_FN(T, _free)(VEX_LIST_CLASS(T) *self);

// List(List(T)): an object list whose element type is the inner typed handle.
// LIST_CLASS_ID is the inner List's class id (ID_LIST for this repo).
#define VEX_LIST_DECLARE_NESTED(T, LIST_CLASS_ID)                            \
    TYPEDEF_LIST_OBJECT(VEX_LIST_CLASS(T), LIST_CLASS_ID)

// --- definitions -----------------------------------------------------------

// The shared construction body: one engine block, initialized in place with an
// explicit stride. The block header is the engine's TYPE_LIST; the typed
// identity is compile-time, and the element class id lives in the embedded
// Collection. STRIDE is sizeof(T) for a value and sizeof(T*) for an object.
#define VEX_LIST_DEFINE_CTOR(T, CLASS_ID, STRIDE)                            \
    VEX_LIST_CLASS(T) *VEX_LIST_FN(T, _2)(bytes capacity) {                  \
        VEX_LIST_CLASS(T) *self = (VEX_LIST_CLASS(T)*)                       \
            Memory_alloc(TYPE_LIST, sizeof(VEX_LIST_CLASS(T)));              \
        if (!self) {                                                         \
            (void) Struct_reportConstructionExhaustion("List_" VEX_STR(T),   \
                                                       sizeof(VEX_LIST_CLASS(T))); \
            return nullptr;                                                  \
        }                                                                    \
        if (!List_initStride((List*) self, (CLASS_ID), (STRIDE), capacity)) { \
            List_free((List*) self);                                         \
            return nullptr;                                                  \
        }                                                                    \
        return self;                                                         \
    }

#define IMPLEMENT_LIST_VALUE(T, CLASS_ID)                                    \
    VEX_LIST_DEFINE_CTOR(T, CLASS_ID, sizeof(T))                             \
    void VEX_LIST_FN(T, _add)(VEX_LIST_CLASS(T) *self, T value) {            \
        u8 *slot = List_addSlot((List*) self);                               \
        if (slot) memcpy(slot, &value, sizeof value);                        \
    }                                                                        \
    T VEX_LIST_FN(T, _get)(VEX_LIST_CLASS(T) *self, bytes index) {           \
        T value;                                                             \
        memset(&value, 0, sizeof value);                                     \
        u8 *slot = List_slot((List*) self, index);                           \
        if (slot) memcpy(&value, slot, sizeof value);                        \
        return value;                                                        \
    }                                                                        \
    T *VEX_LIST_FN(T, _slot)(VEX_LIST_CLASS(T) *self, bytes index) {         \
        return (T*) List_slot((List*) self, index);                          \
    }                                                                        \
    void VEX_LIST_FN(T, _free)(VEX_LIST_CLASS(T) *self) {                    \
        List_free((List*) self);                                             \
    }

#define IMPLEMENT_LIST_OBJECT(T, CLASS_ID)                                   \
    VEX_LIST_DEFINE_CTOR(T, CLASS_ID, sizeof(T*))                            \
    void VEX_LIST_FN(T, _add)(VEX_LIST_CLASS(T) *self, T *value) {           \
        List_add((List*) self, (u64) (pointer) value);                       \
    }                                                                        \
    T *VEX_LIST_FN(T, _get)(VEX_LIST_CLASS(T) *self, bytes index) {          \
        return (T*) (pointer) List_get((List*) self, index);                 \
    }                                                                        \
    T **VEX_LIST_FN(T, _slot)(VEX_LIST_CLASS(T) *self, bytes index) {        \
        return (T**) List_slot((List*) self, index);                         \
    }                                                                        \
    void VEX_LIST_FN(T, _free)(VEX_LIST_CLASS(T) *self) {                    \
        List_free((List*) self);                                             \
    }

// List(List(T)): the object body over the inner handle. LIST_CLASS_ID is the
// inner List's class id, so the outer Collection records ID_LIST as its element
// class while the slot width is sizeof(List_<T>*).
#define VEX_LIST_IMPLEMENT_NESTED(T, LIST_CLASS_ID)                          \
    IMPLEMENT_LIST_OBJECT(VEX_LIST_CLASS(T), LIST_CLASS_ID)

#endif // GENERIC_LIST_TMPL_H
