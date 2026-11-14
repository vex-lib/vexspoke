#ifndef EVENT_TOUCH_H
#define EVENT_TOUCH_H

#include "vex.h"

// event/touchhandler.h — high-level listener contract for trackpad touch events
// (Legacy: event/TouchEvent.java). Interface vtable + `self` (see event/keyhandler.h).

typedef struct TouchHandler {
    any self; // the implementing object, handed back to every callback
    fn (*onTouchDown)(any self, int touchId, d64 x, d64 y, d64 pressure, u64 exactNanos);
    fn (*onTouchUp)(any self, int touchId, d64 x, d64 y, d64 pressure, u64 exactNanos);
    fn (*onTouchMove)(any self, int touchId, d64 x, d64 y, d64 pressure, u64 exactNanos);
    fn (*onTouchCancel)(any self, int touchId, u64 exactNanos); // default no-op in legacy
} TouchHandler;

#endif
