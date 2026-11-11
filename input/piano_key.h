#ifndef INPUT_PIANO_KEY_H
#define INPUT_PIANO_KEY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lang/constructor.h"

// input/piano_key.h — Musical / MIDI Keyboard Pitch & Event Model.
//
// Represents musical keyboard input, MIDI note numbers (0..127), tuning frequencies,
// polyphonic key velocity, channel pressure, pitch bend, and sustain pedal.

#define PIANO_MAX_NOTES 128
#define PIANO_A4_NOTE   69
#define PIANO_A4_FREQ   440.0f

typedef enum PianoPitchClass {
    PITCH_C = 0,
    PITCH_CSHARP,
    PITCH_D,
    PITCH_DSHARP,
    PITCH_E,
    PITCH_F,
    PITCH_FSHARP,
    PITCH_G,
    PITCH_GSHARP,
    PITCH_A,
    PITCH_ASHARP,
    PITCH_B
} PianoPitchClass;

typedef struct PianoKeyEvent {
    u8  note;        // 0..127 (MIDI standard; 60 = Middle C / C4)
    u8  channel;     // 0..15 MIDI channel
    bool     isDown;      // true = Note On, false = Note Off
    f32    velocity;    // Normalized strike velocity [0.0, 1.0]
    f32    pressure;    // Polyphonic aftertouch [0.0, 1.0]
    f32    pitchBend;   // [-1.0, +1.0]
    u64 timestampUs; // Microsecond timestamp
} PianoKeyEvent;

static inline PianoKeyEvent PianoKeyEvent_3(u8 note, bool isDown, f32 velocity) {
    return (PianoKeyEvent) { .note = note, .isDown = isDown, .velocity = velocity };
}
#define PianoKeyEvent(...) CONSTRUCTOR_DISPATCH(PianoKeyEvent, __VA_ARGS__)
static inline void PianoKeyEvent_setNote(PianoKeyEvent *event, u8 note) {
    if (event) (*event).note = note;
}
static inline void PianoKeyEvent_setDown(PianoKeyEvent *event, bool isDown) {
    if (event) (*event).isDown = isDown;
}
static inline void PianoKeyEvent_setVelocity(PianoKeyEvent *event, f32 velocity) {
    if (event) (*event).velocity = velocity;
}

typedef struct PianoState {
    f32    velocities[PIANO_MAX_NOTES]; // 0.0 if not pressed
    f32    pressures[PIANO_MAX_NOTES];  // Aftertouch per key
    u32 activeNotesCount;
    f32    pitchBend;                   // Current pitch wheel bend [-1.0, 1.0]
    f32    modWheel;                    // CC 1 modulation [0.0, 1.0]
    bool     sustainPedal;                // CC 64 damper pedal
} PianoState;

// Note tuning and name calculations
f32           PianoKey_frequency(u8 note);
static inline f32 PianoKey_equal_temperament_hz(u8 note) { return PianoKey_frequency(note); }
PianoPitchClass PianoKey_pitchClass(u8 note);
i32         PianoKey_octave(u8 note);
void            PianoKey_noteName(u8 note, char *dest, bytes maxLen);

// State container management
PianoState PianoState_0(void);
#define PianoState(...) CONSTRUCTOR_DISPATCH(PianoState, __VA_ARGS__)
void PianoState_processEvent(PianoState *state, PianoKeyEvent event);
bool PianoState_isNoteOn(const PianoState *state, u8 note);
f32 PianoState_noteVelocity(const PianoState *state, u8 note);

#endif
