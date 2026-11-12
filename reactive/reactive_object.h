#ifndef REACTIVE_REACTIVE_OBJECT_H
#define REACTIVE_REACTIVE_OBJECT_H

#include "vex.h"

#include "lang/rect/rectangle.h"
#include "reactive/generic.h"

// reactive/reactive_object.h — the standard OBJECT reactives in one file.
//
// ;;INTENTION("these are the same one-line TYPEDEF_REACTIVE stamp applied to the
// standard object types (Vec2/3/4, Rectangle, and the 128-bit pair). Mechanical
// stamps, not hand-authored classes — the Single Class Per File Law is waived BY
// INTENT here, one file pair instead of seven. Per the Conflict Triage Law.")
//
// Each word holds a NAME* (a published object); a rebind is a change. Reactive(Vec4),
// Reactive(Rectangle), Reactive(Ld128), ... resolve here.

// The floor owns the numeric shapes; these aliases give the reactive family
// the class names its comments and constructor choosers already use.
typedef vec2f Vec2;
typedef vec3f Vec3;
typedef vec4f Vec4;
typedef ld128 Ld128;

TYPEDEF_REACTIVE(Vec2);
TYPEDEF_REACTIVE(Vec3);
TYPEDEF_REACTIVE(Vec4);
TYPEDEF_REACTIVE(Rectangle);
TYPEDEF_REACTIVE(Ld128);

// TYPEDEF_REACTIVE stamps the constructor arms; expose normal class chooser
// macros for the standard object family at the consumer boundary.
#define ReactiveVec2(...) CONSTRUCTOR_DISPATCH(ReactiveVec2, __VA_ARGS__)
#define ReactiveVec3(...) CONSTRUCTOR_DISPATCH(ReactiveVec3, __VA_ARGS__)
#define ReactiveVec4(...) CONSTRUCTOR_DISPATCH(ReactiveVec4, __VA_ARGS__)
#define ReactiveRectangle(...) CONSTRUCTOR_DISPATCH(ReactiveRectangle, __VA_ARGS__)
#define ReactiveLd128(...) CONSTRUCTOR_DISPATCH(ReactiveLd128, __VA_ARGS__)

#endif
