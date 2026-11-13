#ifndef GENERIC_REACTIVE_VEXSPOKE_H
#define GENERIC_REACTIVE_VEXSPOKE_H

#include "vex.h"

#include "reactive/generic.h"
#include "reactive/reactive_object.h"
#include "reactive/dispatch.h"

// generic/reactive_vexspoke.h — the reactive family, reached from generic/.
//
// The reactive stamps already live under reactive/; this header re-exports the
// public face so a consumer that includes generic/generic.h gets both generic
// families without knowing which directory owns which. It adds no declarations
// of its own: reactive/generic.h is the stamp engine, reactive_object.h is the
// standard object family (Vec2/3/4, Rectangle, Ld128), and dispatch.h is the
// `_Generic` method surface.

#endif // GENERIC_REACTIVE_VEXSPOKE_H
