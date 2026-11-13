#ifndef GENERIC_GENERIC_H
#define GENERIC_GENERIC_H

#include "vex.h"

#include "generic/generic_cat.h"
#include "generic/list_family.h"
#include "generic/list_tmpl.h"
#include "generic/list_stamps.h"
#include "generic/classgeneric_repo.h"
#include "generic/reactive_vexspoke.h"

// generic/generic.h — THE generic surface of vexspoke, one include.
//
//   #include "generic/generic.h"
//
// Two families live here, both stamped rather than hand-written:
//
//   List(T)   the typed list facade over the type-erased List engine:
//                 List(u32) *l = List(u32)(23);
//                 List_add(u32)(l, 7);
//                 u32 v = List_get(u32)(l, 0);
//                 List_free(u32)(l);
//
//             The element registry is generic/list_family.h: every registered
//             class plus the floor value types, and List(List(T)) at depth 2
//             for each entry.
//
//   Reactive(T)   re-exported from reactive/ (see reactive_vexspoke.h):
//                 Reactive(Vec4) *p = Reactive(Vec4)(initial);
//
// Layering, in the order a consumer should read it:
//   generic_cat.h          the two-level paste and stringize
//   list_family.h          the List element registry (X-lists)
//   list_tmpl.h            the value/object/nested stamp macros
//   list_stamps.h          the instantiated classes and their constructors
//   classgeneric_repo.h    the method-first macros
//   reactive_vexspoke.h    the reactive re-export
//
// The value stamps reach algo/segment_index.h for the two segment slot records
// because a by-value parameter needs a complete type; everything else comes
// from the floor and struct/list.h.

#endif // GENERIC_GENERIC_H
