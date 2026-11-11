#include "input/piano_key.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "util/annotation.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: PianoKey
 * ============================================================================
 * Musical / MIDI keyboard pitch and event model. Converts MIDI note numbers
 * (0..127) to standard equal-temperament A440 frequencies, provides note
 * naming (e.g. C4, A4), and maintains polyphonic velocity and aftertouch
 * state. PianoState is a plain value struct with fixed 128-entry velocity and
 * pressure arrays plus pitch bend, mod wheel, and sustain pedal state; callers
 * own it on the stack or in an input slot. Note math is pure and allocation
 * free, so it is safe on hot input paths.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: PianoKey (input/piano_key.c)
 * ============================================================================
 * Musical / MIDI Keyboard Pitch & Event Model.
 *
 * Converts MIDI note numbers (0..127) to standard equal temperament A440
 * frequencies, provides note naming (e.g. C4, A4), and maintains polyphonic
 * velocity and aftertouch state.
 *
 * STRUCT FIELDS (Mirroring input/piano_key.h):
 * ----------------------------------------------------------------------------
 *   PianoKeyEvent {
 *     u8  note;        // 0..127 (MIDI standard; 60 = Middle C / C4)
 *     u8  channel;     // 0..15 MIDI channel
 *     bool     isDown;      // true = Note On, false = Note Off
 *     f32    velocity;    // normalized strike velocity [0.0, 1.0]
 *     f32    pressure;    // polyphonic aftertouch [0.0, 1.0]
 *     f32    pitchBend;   // [-1.0, +1.0]
 *     u64 timestampUs; // microsecond timestamp
 *   }
 *   PianoState {
 *     f32    velocities[PIANO_MAX_NOTES]; // 0.0 if not pressed
 *     f32    pressures[PIANO_MAX_NOTES];  // aftertouch per key
 *     u32 activeNotesCount;
 *     f32    pitchBend;                   // current pitch wheel bend [-1.0, 1.0]
 *     f32    modWheel;                    // CC 1 modulation [0.0, 1.0]
 *     bool     sustainPedal;                // CC 64 damper pedal
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Core Functions: (.h)
 *   - PianoKey_frequency(note)
 *   - PianoKey_pitchClass(note)
 *   - PianoKey_octave(note)
 *   - PianoKey_noteName(note, dest, maxLen)
 *   - PianoState()
 *   - PianoState_processEvent(state, event)
 *   - PianoState_isNoteOn(state, note)
 *   - PianoState_noteVelocity(state, note)
 * ============================================================================
 */

static const char *const PITCH_NAMES[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

f32 PianoKey_frequency(u8 note) {
    if (note >= PIANO_MAX_NOTES) {
        note = 127;
    }
    // Equal temperament formula: f = 440 * 2^((note - 69) / 12)
    f32 semitoneOffset = (f32) ((i32) note - PIANO_A4_NOTE);
    return PIANO_A4_FREQ * powf(2.0f, semitoneOffset / 12.0f);
}

PianoPitchClass PianoKey_pitchClass(u8 note) {
    return (PianoPitchClass) (note % 12);
}

i32 PianoKey_octave(u8 note) {
    // MIDI 60 (Middle C) is C4. 60 / 12 = 5, minus 1 = 4.
    return ((i32) note / 12) - 1;
}

void PianoKey_noteName(u8 note, char *dest, bytes maxLen) {
    if (dest == nullptr || maxLen == 0) return;
    PianoPitchClass pc = PianoKey_pitchClass(note);
    i32 oct = PianoKey_octave(note);
    snprintf(dest, maxLen, "%s%d", PITCH_NAMES[pc], oct);
}

PianoState PianoState_0(void) {
    PianoState state;
    memset(&state, 0, sizeof state);
    return state;
}

void PianoState_processEvent(PianoState *state, PianoKeyEvent event) {
    if (state == nullptr || event.note >= PIANO_MAX_NOTES) {
        return;
    }

    u8 n = event.note;
    bool wasDown = ((*state).velocities[n] > 0.0f);

    if (event.isDown) {
        (*state).velocities[n] = (event.velocity > 0.0f) ? event.velocity : 0.8f;
        (*state).pressures[n] = event.pressure;
        if (!wasDown) {
            (*state).activeNotesCount++;
        }
    } else {
        (*state).velocities[n] = 0.0f;
        (*state).pressures[n] = 0.0f;
        if (wasDown && (*state).activeNotesCount > 0) {
            (*state).activeNotesCount--;
        }
    }

    (*state).pitchBend = event.pitchBend;
}

bool PianoState_isNoteOn(const PianoState *state, u8 note) {
    if (state == nullptr || note >= PIANO_MAX_NOTES) return false;
    return (*state).velocities[note] > 0.0f;
}

f32 PianoState_noteVelocity(const PianoState *state, u8 note) {
    if (state == nullptr || note >= PIANO_MAX_NOTES) return 0.0f;
    return (*state).velocities[note];
}
