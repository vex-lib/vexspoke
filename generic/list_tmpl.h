#ifndef GENERIC_LIST_TMPL_H
#define GENERIC_LIST_TMPL_H

#include "vex.h"

#include "generic/generic_cat.h"
#include "struct/list.h"

// generic/list_tmpl.h — the List stamp.
//
// A typed list is a thin, compile-time-typed facade over the type-erased List
// engine: the struct holds exactly one `List base`, so a typed handle is also
// an engine handle and construction is one List_init in place. The stamp binds
// the element class id once; every method is a one-line forwarder, so the
// element type is checked by the compiler and the storage stays the engine's.
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
// one chooser line in the family header (generic/list_stamps.h):
//
//     #define List_u32(...) List_u32_2(__VA_ARGS__)
//
// That line is what makes `List_u32(23)` a constructor call while `List_u32 *`
// stays the type: a function-like macro only expands when followed by `(`.
//
// SCALARS AND OBJECTS ARE DIFFERENT STAMPS.
//
//   TYPEDEF_LIST_SCALAR(u8, ID_BYTE)    element value is u8; get returns u8.
//   TYPEDEF_LIST_OBJECT(Buffer, ID_BUFFER) element value is Buffer*; get
//                                        returns Buffer*, slot returns Buffer**.
//
// A scalar stamp stores the value word; an object stamp stores the pointer
// word. They are never mixed, because `(u64) someFloat` is not the value.
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

#define TYPEDEF_LIST_SCALAR(T, CLASS_ID)                                     \
    typedef struct VEX_LIST_CLASS(T) {                                       \
        List base;                                                           \
    } VEX_LIST_CLASS(T);                                                     \
    VEX_LIST_CLASS(T) *VEX_LIST_FN(T, _2)(bytes capacity);                   \
    void VEX_LIST_FN(T, _add)(VEX_LIST_CLASS(T) *self, T value);             \
    T VEX_LIST_FN(T, _get)(VEX_LIST_CLASS(T) *self, bytes index);            \
    T *VEX_LIST_FN(T, _slot)(VEX_LIST_CLASS(T) *self, bytes index);          \
    void VEX_LIST_FN(T, _free)(VEX_LIST_CLASS(T) *self)

#define TYPEDEF_LIST_OBJECT(T, CLASS_ID)                                     \
    typedef struct VEX_LIST_CLASS(T) {                                       \
        List base;                                                           \
    } VEX_LIST_CLASS(T);                                                     \
    VEX_LIST_CLASS(T) *VEX_LIST_FN(T, _2)(bytes capacity);                   \
    void VEX_LIST_FN(T, _add)(VEX_LIST_CLASS(T) *self, T *value);            \
    T *VEX_LIST_FN(T, _get)(VEX_LIST_CLASS(T) *self, bytes index);           \
    T **VEX_LIST_FN(T, _slot)(VEX_LIST_CLASS(T) *self, bytes index);         \
    void VEX_LIST_FN(T, _free)(VEX_LIST_CLASS(T) *self)

// --- definitions -----------------------------------------------------------

// The shared construction body: one engine block, initialized in place. The
// block header is the engine's TYPE_LIST; the typed identity is compile-time,
// and the element class id lives in the embedded Collection.
#define VEX_LIST_DEFINE_CTOR(T, CLASS_ID)                                    \
    VEX_LIST_CLASS(T) *VEX_LIST_FN(T, _2)(bytes capacity) {                  \
        VEX_LIST_CLASS(T) *self = (VEX_LIST_CLASS(T)*)                       \
            Memory_alloc(TYPE_LIST, sizeof(VEX_LIST_CLASS(T)));              \
        if (!self) {                                                         \
            (void) Struct_reportConstructionExhaustion("List_" #T,           \
                                                       sizeof(VEX_LIST_CLASS(T))); \
            return nullptr;                                                  \
        }                                                                    \
        if (!List_init((List*) self, (CLASS_ID), capacity)) {                \
            List_free((List*) self);                                         \
            return nullptr;                                                  \
        }                                                                    \
        return self;                                                         \
    }

#define IMPLEMENT_LIST_SCALAR(T, CLASS_ID)                                   \
    VEX_LIST_DEFINE_CTOR(T, CLASS_ID)                                        \
    void VEX_LIST_FN(T, _add)(VEX_LIST_CLASS(T) *self, T value) {            \
        List_add((List*) self, (u64) value);                                 \
    }                                                                        \
    T VEX_LIST_FN(T, _get)(VEX_LIST_CLASS(T) *self, bytes index) {           \
        return (T) List_get((List*) self, index);                            \
    }                                                                        \
    T *VEX_LIST_FN(T, _slot)(VEX_LIST_CLASS(T) *self, bytes index) {         \
        return (T*) List_slot((List*) self, index);                          \
    }                                                                        \
    void VEX_LIST_FN(T, _free)(VEX_LIST_CLASS(T) *self) {                    \
        List_free((List*) self);                                             \
    }

#define IMPLEMENT_LIST_OBJECT(T, CLASS_ID)                                   \
    VEX_LIST_DEFINE_CTOR(T, CLASS_ID)                                        \
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

#endif // GENERIC_LIST_TMPL_H
