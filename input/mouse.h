#ifndef INPUT_MOUSE_H
#define INPUT_MOUSE_H

#include "vex.h"
#include "event/mousehandler.h"

// Window id carried by every queued event (0 = FOCUS_BROADCAST). windowId is
// an opaque OS tag, never a slot index: scoped registries grow on demand (the
// Dynamic Scalability & Anti-Hardcoding Law), so any id attaches.

// input/mouse.h — mouse buttons, position, and event stream
// (Legacy: input/Mouse.java).
//
// Same shape as key.h: a small off-heap state table (16 buttons), a packed
// 64-bit ring between Thread 0 producers and the game-thread dispatcher, and
// listener callbacks. Motion events carry coordinates instead of timestamps;
// button events carry timestamp + modifiers instead of coordinates.
//
// Click counting is windowed exactly like key taps: a press opens a click
// sequence whose pending window closes `tapWindowNanos` after the LAST press;
// while open the sequence is pending (Mouse_tapPhase), then it settles into
// SINGLE / d64 / TRIPLE.
//
// Button-event wire format (identical to legacy):
//   [63:18] micros since engine start | [17:14] modifiers
//   [13: 2] button | [1:0] action (0 up | 1 down | 2 repeat)
// Motion events use the legacy marker Bytes in bits [15:8]:
//   move: 255/5, delta: 255/9, zoom: 255/8, scroll: 254/6 — coords/f32 in
//   bits [47:16]; drag: real button in [15:8] with action 7.

enum {
    MOUSE_LEFT     = 0,
    MOUSE_RIGHT    = 1,
    MOUSE_MIDDLE   = 2,
    MOUSE_BUTTON_4 = 3,
    MOUSE_BUTTON_5 = 4,
    MOUSE_BUTTON_6 = 5,
    MOUSE_BUTTON_7 = 6,
    MOUSE_BUTTON_8 = 7,
};

fn Mouse_init();
fn Mouse_shutdown();
fn Mouse_addListener(const MouseHandler *listener);
bool Mouse_removeListener(const MouseHandler *listener);

// Window-scoped registration (see input/key.h for the routing rules).
fn Mouse_attachWindow(u32 windowId, const MouseHandler *listener);
bool Mouse_detachWindow(u32 windowId, const MouseHandler *listener);
fn Mouse_detachWindowAll(u32 windowId);

// Producers: Thread 0 only. windowId tags the receiving window.
// tapWindowNanos gates click settlement (see the header comment above).
fn Mouse_pushButtonEvent(u32 windowId, int button, int action, u64 tapWindowNanos);
fn Mouse_pushMoveEvent(u32 windowId, d64 x, d64 y);
fn Mouse_pushMoveDeltaEvent(u32 windowId, d64 dx, d64 dy);
fn Mouse_pushDragEvent(u32 windowId, int button, d64 x, d64 y);
fn Mouse_pushScrollEvent(u32 windowId, d64 dx, d64 dy);
fn Mouse_pushZoomEvent(u32 windowId, d64 magnification);

// Consumer: drain the ring into the listeners. Game thread, once per frame.
fn Mouse_dispatchEvents();

// --- State queries ---
bool     Mouse_isDown(int button);
u64 Mouse_pressTime(int button);
u64 Mouse_lastReleaseTime(int button);
u64 Mouse_lastHoldDurationNanos(int button);
u64 Mouse_currentHoldDurationNanos(int button);
int      Mouse_taps(int button);
fn     Mouse_resetTaps(int button);

// --- Multi-tap gesture timeline (windowed settlement, mirrors KeyTapPhase) ---
typedef enum MouseTapPhase {
    MOUSE_TAP_NONE    = 0, // no clicks in flight (or consumed)
    MOUSE_TAP_PENDING = 1, // clicks counted, window still open — not yet settled
    MOUSE_TAP_SINGLE  = 2, // settled: window closed, taps == 1
    MOUSE_TAP_f64  = 3, // settled: window closed, taps == 2
    MOUSE_TAP_TRIPLE  = 4, // settled: window closed, taps >= 3
} MouseTapPhase;

// Settled-or-windowed click readout for a button.
MouseTapPhase Mouse_tapPhase(int button);

// One-shot LONG_PRESS latch (mirrors Key_isLongPressFired).
bool Mouse_isLongPressFired(int button);
fn Mouse_setLongPressFired(int button, bool fired);

// Last dispatched cursor position in content coordinates (top-left origin).
// Written on dispatch of move/drag events — legacy left these dead; here they
// are fed so cameras/UI can poll without a listener.
d64 Mouse_x();
d64 Mouse_y();

// --- Packed mouseEvent helpers ---
int  Mouse_button(int mouseEvent);
bool Mouse_hasShift(int mouseEvent);
bool Mouse_hasControl(int mouseEvent);
bool Mouse_hasOption(int mouseEvent);
bool Mouse_hasCommand(int mouseEvent);

const char *Mouse_name(int button);

#endif
