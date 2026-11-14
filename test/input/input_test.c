// input_test/test.c — headless verification of the input pipeline.
//
// No window: producers are driven directly (as Thread 0 would from the event
// pump) and dispatchers drain on the same thread. Covers packing/unpacking,
// tap counting, hold durations, modifier mapping, char events, scroll/zoom
// markers, and the button classes the legacy dispatcher dropped silently
// (right-down, middle-up).

#include <stdio.h>

#include "input/focus.h"
#include "input/key.h"
#include "input/mouse.h"
#include "input/touch.h"
#include "time/nanotime.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

static int g_keyDowns = 0, g_keyUps = 0, g_keyRepeats = 0, g_chars = 0;
static int g_lastKeyEvent = -1;
static u64 g_lastNanos = 0;

// Records a key-down callback's event and timestamp for dispatch assertions.
static fn k_down(any self, int e, u64 n) {
    (fn)self; g_keyDowns++; g_lastKeyEvent = e; g_lastNanos = n;
}
// Counts key-up delivery and saves its packed event.
static fn k_up(any self, int e, u64 n) { (fn)self; (fn)n; g_keyUps++; g_lastKeyEvent = e; }
// Counts repeated-key delivery without changing the recorded event.
static fn k_repeat(any self, int e, u64 n) { (fn)self; (fn)e; (fn)n; g_keyRepeats++; }
// Accumulates typed character values received by the listener.
static fn k_char(any self, u32 c) { (fn)self; g_chars += (int)c; }

static int g_mouseDowns = 0, g_mouseUps = 0;
static int g_lastMouseEvent = -1;
static d64 g_scrollX = 0, g_scrollY = 0, g_zoom = 0, g_moveX = -1, g_moveY = -1;

// Counts mouse-down events and remembers the last button.
static fn m_down(any self, int e, u64 n) { (fn)self; (fn)n; g_mouseDowns++; g_lastMouseEvent = e; }
// Counts mouse-up events and remembers the last button.
static fn m_up(any self, int e, u64 n) { (fn)self; (fn)n; g_mouseUps++; g_lastMouseEvent = e; }
// Saves the latest pointer coordinates delivered by the mouse dispatcher.
static fn m_move(any self, d64 x, d64 y) { (fn)self; g_moveX = x; g_moveY = y; }
// Saves the latest horizontal and vertical scroll deltas.
static fn m_scroll(any self, d64 dx, d64 dy) { (fn)self; g_scrollX = dx; g_scrollY = dy; }
// Saves the latest zoom magnification delivered by the mouse dispatcher.
static fn m_zoom(any self, d64 mag) { (fn)self; g_zoom = mag; }

static int g_touchDowns = 0, g_touchUps = 0;
static d64 g_touchPressure = 0;

// Records touch-down delivery and pressure for the selected touch.
static fn t_down(any self, int id, d64 x, d64 y, d64 p, u64 n) {
    (fn)self; (fn)id; (fn)x; (fn)y; (fn)n; g_touchDowns++; g_touchPressure = p;
}
// Counts touch-up delivery after the touch has been released.
static fn t_up(any self, int id, d64 x, d64 y, d64 p, u64 n) {
    (fn)self; (fn)id; (fn)x; (fn)y; (fn)p; (fn)n; g_touchUps++;
}

// A second key listener used to prove removal stops delivery.
static int g_otherDowns = 0;
// Counts events received by the secondary listener used for routing checks.
static fn k_other_down(any self, int e, u64 n) {
    (fn)self; (fn)e; (fn)n; g_otherDowns++;
}

// Drives keyboard, mouse, touch, removal, and window-routing paths headlessly.
int main() {
    Key_init();
    Mouse_init();
    Touch_init();

    static const KeyHandler kl = { .self = nullptr, .onKeyDown = k_down, .onKeyUp = k_up, .onKeyRepeat = k_repeat, .onCharTyped = k_char };
    static const MouseHandler ml = { .self = nullptr, .onMouseDown = m_down, .onMouseUp = m_up, .onMouseMove = m_move, .onMouseScroll = m_scroll, .onMouseZoom = m_zoom };
    static const TouchHandler tl = { .self = nullptr, .onTouchDown = t_down, .onTouchUp = t_up };
    Key_addListener(&kl);
    Mouse_addListener(&ml);
    Touch_addListener(&tl);

    // --- Keyboard: down/up cycle, tap counting, hold duration ---
    Key_pushEvent(FOCUS_BROADCAST, KEY_A, KEY_ACTION_DOWN, 250000000ULL);
    Key_dispatchEvents();
    CHECK(g_keyDowns == 1);
    CHECK(Key_isDown(KEY_A));
    CHECK(g_lastKeyEvent == KEY_A);
    CHECK(Key_taps(KEY_A) == 1);

    // OS repeat while held => repeat event, state untouched.
    Key_pushEvent(FOCUS_BROADCAST, KEY_A, KEY_ACTION_DOWN, 250000000ULL);
    Key_dispatchEvents();
    CHECK(g_keyRepeats == 1);
    CHECK(Key_taps(KEY_A) == 1);

    Key_pushEvent(FOCUS_BROADCAST, KEY_A, KEY_ACTION_UP, 0);
    Key_dispatchEvents();
    CHECK(g_keyUps == 1 && !Key_isDown(KEY_A));
    CHECK(Key_taps(KEY_A) == 1);

    // Quick re-press inside the threshold => second tap.
    Key_pushEvent(FOCUS_BROADCAST, KEY_A, KEY_ACTION_DOWN, 250000000ULL);
    Key_pushEvent(FOCUS_BROADCAST, KEY_A, KEY_ACTION_UP, 0);
    Key_dispatchEvents();
    CHECK(Key_taps(KEY_A) == 2);
    CHECK(Key_holdDurationNanos(KEY_A) >= 0); // last hold readable after release

    // Modifiers: hold shift, press B => MOD_SHIFT rides the packed event.
    Key_pushEvent(FOCUS_BROADCAST, KEY_LEFT_SHIFT, KEY_ACTION_DOWN, 250000000ULL);
    Key_dispatchEvents();
    Key_pushEvent(FOCUS_BROADCAST, KEY_B, KEY_ACTION_DOWN, 250000000ULL);
    Key_dispatchEvents();
    CHECK(g_lastKeyEvent == (KEY_B | KEY_MOD_SHIFT));
    CHECK(Key_hasShift(g_lastKeyEvent));
    Key_pushEvent(FOCUS_BROADCAST, KEY_LEFT_SHIFT, KEY_ACTION_UP, 0);
    Key_pushEvent(FOCUS_BROADCAST, KEY_B, KEY_ACTION_UP, 0);
    Key_dispatchEvents();

    // Char event.
    Key_pushCharEvent(FOCUS_BROADCAST, 'Z');
    Key_dispatchEvents();
    CHECK(g_chars == 'Z');

    // Timestamps reconstruct onto the engine epoch.
    CHECK(g_lastNanos >= NanoTime_startNanos());

    // --- Mouse: every button class must survive the dispatcher ---
    Mouse_pushButtonEvent(FOCUS_BROADCAST, MOUSE_LEFT, KEY_ACTION_DOWN, 250000000ULL);
    Mouse_pushButtonEvent(FOCUS_BROADCAST, MOUSE_RIGHT, KEY_ACTION_DOWN, 250000000ULL); // legacy dropped this
    Mouse_pushButtonEvent(FOCUS_BROADCAST, MOUSE_MIDDLE, KEY_ACTION_DOWN, 250000000ULL);
    Mouse_pushButtonEvent(FOCUS_BROADCAST, MOUSE_LEFT, KEY_ACTION_UP, 0);
    Mouse_pushButtonEvent(FOCUS_BROADCAST, MOUSE_RIGHT, KEY_ACTION_UP, 0);
    Mouse_pushButtonEvent(FOCUS_BROADCAST, MOUSE_MIDDLE, KEY_ACTION_UP, 0); // legacy dropped this
    Mouse_dispatchEvents();
    CHECK(g_mouseDowns == 3);
    CHECK(g_mouseUps == 3);
    CHECK(Mouse_isDown(MOUSE_LEFT) == false);

    // Motion markers vs button payloads.
    Mouse_pushMoveEvent(FOCUS_BROADCAST, 120, 240);
    Mouse_pushScrollEvent(FOCUS_BROADCAST, 0.5, -1.25);
    Mouse_pushZoomEvent(FOCUS_BROADCAST, 0.25f);
    Mouse_pushDragEvent(FOCUS_BROADCAST, MOUSE_RIGHT, 7, 9);
    Mouse_dispatchEvents();
    CHECK(g_moveX == 120 && g_moveY == 240);
    CHECK(g_scrollX == 0.5 && g_scrollY == -1.25);
    CHECK(g_zoom > 0.24 && g_zoom < 0.26);

    // Position tracks the last dispatched motion event (the drag above).
    CHECK(Mouse_x() == 7 && Mouse_y() == 9);

    // --- Touch ---
    Touch_pushTouchEvent(FOCUS_BROADCAST, 3, TOUCH_DOWN, 10, 20, 0.8, 250000000ULL);
    Touch_dispatchEvents();
    CHECK(g_touchDowns == 1 && g_touchPressure == 0.8);
    CHECK(Touch_isDown(3) && Touch_x(3) == 10 && Touch_y(3) == 20);
    Touch_pushTouchEvent(FOCUS_BROADCAST, 3, TOUCH_UP, 10, 20, 0.8, 250000000ULL);
    Touch_dispatchEvents();
    CHECK(g_touchUps == 1 && !Touch_isDown(3));

    // --- Removal stops delivery; unknown removes report false ---
    static const KeyHandler other = { .self = nullptr, .onKeyDown = k_other_down };
    Key_addListener(&other);
    Key_pushEvent(FOCUS_BROADCAST, KEY_C, KEY_ACTION_DOWN, 250000000ULL);
    Key_dispatchEvents();
    CHECK(g_otherDowns == 1);
    CHECK(Key_removeListener(&other));
    CHECK(!Key_removeListener(&other)); // already gone
    Key_pushEvent(FOCUS_BROADCAST, KEY_C, KEY_ACTION_DOWN, 250000000ULL);
    Key_dispatchEvents();
    CHECK(g_otherDowns == 1); // unchanged
    CHECK(g_keyDowns == 5);   // first listener still attached

    // --- Window-scoped routing: listeners only hear their own window ---
    static const KeyHandler scoped = { .self = nullptr, .onKeyDown = k_other_down };
    Key_attachWindow(3, &scoped);
    Focus_set(3);
    CHECK(Focus_isFocused(3) && !Focus_isFocused(4));

    // NOTE: the key state table is one keyboard machine-wide, so each probe
    // uses a fresh key — a second "down" of a held key is an OS repeat.
    Key_pushEvent(5, KEY_D, KEY_ACTION_DOWN, 250000000ULL); // other window: dropped
    Key_dispatchEvents();
    CHECK(g_otherDowns == 1);

    Key_pushEvent(3, KEY_F, KEY_ACTION_DOWN, 250000000ULL); // our window: delivered
    Key_dispatchEvents();
    CHECK(g_otherDowns == 2);

    Key_pushEvent(FOCUS_BROADCAST, KEY_G, KEY_ACTION_DOWN, 250000000ULL); // broadcast
    Key_dispatchEvents();
    CHECK(g_otherDowns == 3);

    CHECK(Key_detachWindow(3, &scoped));
    Key_pushEvent(3, KEY_H, KEY_ACTION_DOWN, 250000000ULL);
    Key_dispatchEvents();
    CHECK(g_otherDowns == 3);

    Key_shutdown();
    Mouse_shutdown();
    Touch_shutdown();

    if (g_failures == 0)
        printf("input_test: all checks passed\n");
    else
        printf("input_test: %d FAILURES\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
