#ifndef GENERIC_CLASSGENERIC_REPO_H
#define GENERIC_CLASSGENERIC_REPO_H

#include "vex.h"

#include "generic/generic_cat.h"
#include "oop/vexspoke_type.h"
#include "primitive/buffer.h"

// generic/classgeneric_repo.h — the repo's generic registry and method sugar.
//
// VEX_LIST_FAMILY is the X-list every stamped List type appears in: the single
// place a reader (or a tool) learns what `List(T)` accepts. It is a list, not
// a translation table — the stamps themselves are the definitions.
//
// The method-first macros are the public spelling:
//
//     List_get(u8)(l, 0)   -> List_u8_get(l, 0)
//     List_add(u8)(l, 5)   -> List_u8_add(l, 5)
//     List_slot(u8)(l, 0)  -> List_u8_slot(l, 0)
//     List_free(u8)(l)     -> List_u8_free(l)
//
// The type token is the ONE thing the caller spells; the class, the element
// class id and the return type all follow from it.
//
// INTENTIONAL(vex): these four names shadow the type-erased engine accessors
// of struct/list.h inside a translation unit that includes this header. The
// generic surface is the public face; the engine names stay reachable in a
// translation unit that does not include generic/ (generic/list_stamps.c is
// one). The split is deliberate: one name means one layer, and a caller never
// has to remember which of two `List_get`s it is looking at.

// Every stamped List type. The second column is the engine element class id
// the stamp binds; it is here so the registry states the full contract, not
// only the spelling.
#define VEX_LIST_FAMILY(X) \
    X(u8,     ID_BYTE)     \
    X(i32,    ID_INT)      \
    X(u32,    ID_INT)      \
    X(u64,    ID_LONG)     \
    X(Buffer, ID_BUFFER)

#define List_get(T)  VEX_CAT(VEX_CAT(List_, T), _get)
#define List_add(T)  VEX_CAT(VEX_CAT(List_, T), _add)
#define List_slot(T) VEX_CAT(VEX_CAT(List_, T), _slot)
#define List_free(T) VEX_CAT(VEX_CAT(List_, T), _free)

#endif // GENERIC_CLASSGENERIC_REPO_H
