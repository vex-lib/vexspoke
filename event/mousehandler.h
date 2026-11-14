#ifndef EVENT_MOUSE_H
#define EVENT_MOUSE_H

#include "vex.h"

// event/mousehandler.h — high-level listener contract for mouse events
// (Legacy: event/MouseEvent.java).
//
// Interface vtable + `self` (see event/keyhandler.h). mouseEvent packs the button +
// modifier flags (see input/mouse.h).

typedef struct MouseHandler {
    any self; // the implementing object, handed back to every callback
    fn (*onMouseDown)(any self, int mouseEvent, u64 exactNanos);
    fn (*onMouseUp)(any self, int mouseEvent, u64 exactNanos);
    fn (*onMouseRepeat)(any self, int mouseEvent, u64 exactNanos);
    fn (*onMouseMove)(any self, d64 x, d64 y);
    // Relative movement while the cursor is locked. (dx, dy) is how much the
    // mouse travelled this step; the absolute position stays at the anchor.
    fn (*onMouseMoveDelta)(any self, d64 dx, d64 dy); // default no-op in legacy
    fn (*onMouseDrag)(any self, int mouseEvent, d64 x, d64 y); // default no-op
    fn (*onMouseScroll)(any self, d64 dx, d64 dy);             // default no-op
    // Trackpad pinch-to-zoom (macOS NSEventTypeMagnify). Positive = zoom in
    // (fingers spread), negative = zoom _out (fingers pinched).
    fn (*onMouseZoom)(any self, d64 magnification);               // default no-op
} MouseHandler;

#endif
