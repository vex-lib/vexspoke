#ifndef INPUT_TURNTABLE_H
#define INPUT_TURNTABLE_H

#include "vex.h"

// input/turntable.h — DJ Turntable & Jog Wheel Hardware Model.
//
// Represents DJ jog wheel/platter interaction, vinyl scratch mode,
// platter angular velocity, pitch fader, cue buttons, and crossfader dynamics.

typedef enum TurntableRpm {
    TURNTABLE_RPM_33 = 0,
    TURNTABLE_RPM_45 = 1
} TurntableRpm;

typedef enum CrossfaderCurve {
    CROSSFADER_LINEAR = 0,
    CROSSFADER_SHARP_CUT,  // For DJ scratching (instant cut near edges)
    CROSSFADER_SMOOTH_DIP  // Equal-power crossfade (-3dB center)
} CrossfaderCurve;

typedef struct TurntableCrossfade {
    f32 left;
    f32 right;
} TurntableCrossfade;

typedef struct TurntableEvent {
    u8  deckIndex;         // 0 = Deck A, 1 = Deck B, etc.
    bool     platterTouched;    // True if DJ's hand is contacting the touch platter
    bool     rimTouched;        // True if outer edge nudged
    f32    angularDeltaRad;   // Radians rotated this tick (+ forward, - backward)
    f32    pitchFader;        // Pitch / tempo slider [-1.0, +1.0] (0.0 = 0% pitch)
    bool     cuePressed;        // Main cue button
    bool     playPausePressed;  // Play/pause motor button
    u8  hotCueIndex;       // 1..8 hot cue button triggered (0 = none)
    u64 timestampUs;
} TurntableEvent;

static inline TurntableEvent TurntableEvent_3(u8 deckIndex, bool platterTouched,
                                               f32 angularDeltaRadians) {
    return (TurntableEvent) { .deckIndex = deckIndex, .platterTouched = platterTouched,
        .angularDeltaRad = angularDeltaRadians };
}
#define TurntableEvent(...) CONSTRUCTOR_DISPATCH(TurntableEvent, __VA_ARGS__)
static inline fn TurntableEvent_setPlatterTouched(TurntableEvent *event, bool touched) {
    if (event) (*event).platterTouched = touched;
}
static inline fn TurntableEvent_setAngularDelta(TurntableEvent *event, f32 radians) {
    if (event) (*event).angularDeltaRad = radians;
}

typedef struct TurntableState {
    u8      deckIndex;
    bool         platterTouched;     // In scratch mode when touched
    bool         isPlaying;          // Motor active
    TurntableRpm targetRpm;
    f32        currentAngularVelocityRadPerSec;
    f32        pitchFader;         // Current tempo slider
    f32        playheadSeconds;
    f32        cuePositionSeconds;
    f32        hotCues[8];         // Saved hot cue positions in seconds (-1 = unset)
} TurntableState;

TurntableState TurntableState_1(u8 deckIndex);
#define TurntableState(...) CONSTRUCTOR_DISPATCH(TurntableState, __VA_ARGS__)
fn TurntableState_processEvent(TurntableState *state, TurntableEvent event);
fn TurntableState_tick(TurntableState *state, f32 deltaSeconds);

// Crossfader volume calculation; returns both channel volumes by value.
TurntableCrossfade Turntable_calculateCrossfade(f32 position, CrossfaderCurve curve);

#endif
