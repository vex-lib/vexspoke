#ifndef REACTIVE_REACTIVE_OBJECT_H
#define REACTIVE_REACTIVE_OBJECT_H

#include "lang/constructor.h"
#include "lang/rect/rectangle.h"
#include "lang/vec2/vec2.h"
#include "lang/vec3/vec3.h"
#include "lang/vec4/vec4.h"
#include "primitive/int_f64.h"
#include "primitive/long_f64.h"
#include "primitive/long_f32.h"
#include "reactive/generic.h"

// reactive/reactive_object.h — the standard OBJECT reactives in one file.
//
// ;;INTENTION("these are the same one-line TYPEDEF_REACTIVE stamp applied to the
// standard object types (Coord2/3/4, Rectangle, and the 16-byte pairs). Mechanical
// stamps, not hand-authored classes — the Single Class Per File Law is waived BY
// INTENT here, one file pair instead of seven. Per the Conflict Triage Law.")
//
// Each word holds a NAME* (a published object); a rebind is a change. Reactive(Coord4),
// Reactive(Rectangle), Reactive(Intf64), ... resolve here.

TYPEDEF_REACTIVE(Coord2);
TYPEDEF_REACTIVE(Coord3);
TYPEDEF_REACTIVE(Coord4);
TYPEDEF_REACTIVE(Rectangle);
TYPEDEF_REACTIVE(Intf64);
TYPEDEF_REACTIVE(LongFloat);
TYPEDEF_REACTIVE(Longf64);

// TYPEDEF_REACTIVE stamps the constructor arms; expose normal class chooser
// macros for the standard object family at the consumer boundary.
#define ReactiveVec2(...) CONSTRUCTOR_DISPATCH(ReactiveVec2, __VA_ARGS__)
#define ReactiveVec3(...) CONSTRUCTOR_DISPATCH(ReactiveVec3, __VA_ARGS__)
#define ReactiveVec4(...) CONSTRUCTOR_DISPATCH(ReactiveVec4, __VA_ARGS__)
#define ReactiveRectangle(...) CONSTRUCTOR_DISPATCH(ReactiveRectangle, __VA_ARGS__)
#define ReactiveIntf64(...) CONSTRUCTOR_DISPATCH(ReactiveIntf64, __VA_ARGS__)
#define ReactiveLongFloat(...) CONSTRUCTOR_DISPATCH(ReactiveLongFloat, __VA_ARGS__)
#define ReactiveLongf64(...) CONSTRUCTOR_DISPATCH(ReactiveLongf64, __VA_ARGS__)

#endif
