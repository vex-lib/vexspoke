#ifndef EVENT_KEY_H
#define EVENT_KEY_H

#include "vex.h"

// event/keyhandler.h — high-level listener contract for hardware key events
// (Legacy: event/KeyEvent.java).
//
// A KeyHandler is C's version of a Java interface: a vtable of callbacks plus
// a leading `self` pointer. Any struct "implements" it by exposing one of
// these with .self pointing at itself; every callback receives that pointer
// back as its first argument (the `this` Java implied). Callbacks mirror the
// Java methods; Java's `default void` no-ops become nullptr pointers — every
// dispatcher checks before invoking, so a listener only needs the callbacks
// it cares about.
//
// Executed safely on the game thread via the lock-free input queue,
// guaranteeing zero Thread-0 blocking from event handling.
//
// keyEvent packs the code + modifier flags (see input/key.h); exactNanos is
// the reconstructed engine-epoch timestamp of the OS delivery.

typedef struct KeyHandler {
    any self; // the implementing object, handed back to every callback
    void (*onKeyDown)(any self, int keyEvent, u64 exactNanos);
    void (*onKeyUp)(any self, int keyEvent, u64 exactNanos);
    void (*onKeyRepeat)(any self, int keyEvent, u64 exactNanos);
    void (*onCharTyped)(any self, u32 character); // default no-op in legacy
} KeyHandler;

#endif
