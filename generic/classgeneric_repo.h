#ifndef GENERIC_CLASSGENERIC_REPO_H
#define GENERIC_CLASSGENERIC_REPO_H

#include "vex.h"

#include "generic/generic_cat.h"
#include "generic/list_stamps.h"

// generic/classgeneric_repo.h — the repo's generic method sugar.
//
// The registry itself lives in generic/list_family.h (VEX_LIST_VALUES,
// VEX_LIST_OBJECTS, VEX_LIST_FAMILY); this header owns only the public
// method-first spelling over the stamped functions:
//
//     List_get(u8)(l, 0)   -> List_u8_get(l, 0)
//     List_add(u8)(l, 5)   -> List_u8_add(l, 5)
//     List_slot(u8)(l, 0)  -> List_u8_slot(l, 0)
//     List_free(u8)(l)     -> List_u8_free(l)
//
// The type token is the ONE thing the caller spells; the class, the element
// class id and the return type all follow from it. The same macros compose for
// a nested list: List_get(List(u32))(outer, 0) names List_List_u32_get.
//
// INTENTIONAL(vex): these four names shadow the type-erased engine accessors
// of struct/list.h inside a translation unit that includes this header. The
// generic surface is the public face; the engine names stay reachable in a
// translation unit that does not include generic/ (generic/list_stamps.c is
// one). The split is deliberate: one name means one layer, and a caller never
// has to remember which of two `List_get`s it is looking at.

#define List_get(T)  VEX_CAT(VEX_CAT(List_, T), _get)
#define List_add(T)  VEX_CAT(VEX_CAT(List_, T), _add)
#define List_slot(T) VEX_CAT(VEX_CAT(List_, T), _slot)
#define List_free(T) VEX_CAT(VEX_CAT(List_, T), _free)

#endif // GENERIC_CLASSGENERIC_REPO_H
