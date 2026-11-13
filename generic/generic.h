#ifndef GENERIC_GENERIC_H
#define GENERIC_GENERIC_H

#include "vex.h"

#include "generic/generic_cat.h"
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
//   Reactive(T)   re-exported from reactive/ (see reactive_vexspoke.h):
//                 Reactive(Vec4) *p = Reactive(Vec4)(initial);
//
// This header is standalone: it reaches the floor (vex.h) and the engine
// (struct/list.h), never vexspoke.h. A custom library can include it without
// adopting the whole repository surface.
//
// Layering, in the order a consumer should read it:
//   generic_cat.h          the two-level paste
//   list_tmpl.h            the scalar/object stamp macros
//   list_stamps.h          the instantiated classes and their constructors
//   classgeneric_repo.h    the family registry and the method-first macros
//   reactive_vexspoke.h    the reactive re-export

#endif // GENERIC_GENERIC_H
