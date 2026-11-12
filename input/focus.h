#ifndef INPUT_FOCUS_H
#define INPUT_FOCUS_H

#include "vex.h"

// input/focus.h — THE spotlight (one focused window per machine).
//
// The OS already decides which window has keyboard focus (the key window);
// we just mirror that decision into one atomic word so any thread can ask
// "who is focused?" without touching AppKit (which is Thread-0-only).
// Window id 0 is reserved as BROADCAST: it means "no particular window".

#define FOCUS_BROADCAST 0u

// Mirror the OS's current focus. Thread 0 calls this after each pump pass.
void Focus_set(u32 windowId);

// The currently focused window id (FOCUS_BROADCAST if none).
u32 Focus_id(void);

bool Focus_isFocused(u32 windowId);

#endif
