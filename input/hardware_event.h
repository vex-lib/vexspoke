#ifndef INPUT_HARDWARE_EVENT_H
#define INPUT_HARDWARE_EVENT_H

#include "vex.h"
#include "input/piano_key.h"
#include "input/turntable.h"
#include "input/gesture.h"
#include "input/gamepad.h"

// input/hardware_event.h — Unified Hardware Event Multiplexer.
//
// Unifies disparate hardware inputs (standard keyboards, musical piano keys,
// DJ turntables, trackpad multi-touch gestures, and game controllers) into
// a single, fixed-size discriminated event structure.

typedef enum HardwareEventType {
    HW_EVENT_NONE = 0,
    HW_EVENT_PIANO,
    HW_EVENT_TURNTABLE,
    HW_EVENT_GESTURE,
    HW_EVENT_GAMEPAD
} HardwareEventType;

typedef struct HardwareEvent {
    HardwareEventType type;
    u32          deviceId;
    u64          timestampUs;
    union {
        PianoKeyEvent  piano;
        TurntableEvent turntable;
        GestureEvent   gesture;
        GamepadState   gamepad;
    } data;
} HardwareEvent;

// Event constructors / factory helpers
HardwareEvent HardwareEvent_makePiano(u32 devId, PianoKeyEvent piano);
HardwareEvent HardwareEvent_makeTurntable(u32 devId, TurntableEvent tt);
HardwareEvent HardwareEvent_makeGesture(u32 devId, GestureEvent gesture);
HardwareEvent HardwareEvent_makeGamepad(u32 devId, GamepadState gamepad);

#endif
