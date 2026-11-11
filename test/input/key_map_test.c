// key_map_test/test.c — headless verification of the KeyMap input binding
// registry. Covers lifecycle, bind/match/unbind, growth, empty/null safety,
// combo building from live modifier state, and per-frame resolution.

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include "input/key_map.h"
#include "input/key.h"
#include "input/mouse.h"
#include "nio/mem.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// ── Test callbacks ────────────────────────────────────────
static int g_fires = 0;
static int64_t g_firedCombo = 0;

// Records the most recent combo delivered by a KeyMap binding.
static void onFire(void *userdata, int64_t combo) {
    (void) userdata;
    g_fires++;
    g_firedCombo = combo;
}

// Adds a distinct callback marker to verify replacement and dispatch routing.
static void onFire2(void *userdata, int64_t combo) {
    (void) userdata;
    (void) combo;
    g_fires += 10;
}

// ── Tests ─────────────────────────────────────────────────

// Checks arena-backed map creation, empty state, and destruction.
static void test_create_destroy(void) {
    MemoryArena *arena = MemoryArena_create(64u << 20);
    CHECK(arena != nullptr);

    KeyMap *map = KeyMap(arena);
    CHECK(map != nullptr);
    CHECK(KeyMap_isEmpty(map));
    CHECK(KeyMap_count(map) == 0);

    KeyMap_destroy(map);
    CHECK(KeyMap_count(map) == 0);
    CHECK(KeyMap_isEmpty(map));

    MemoryArena_destroy(arena);
}

// Checks safe results for null map arguments across the public operations.
static void test_null_safety(void) {
    // All operations on NULL should be safe
    CHECK(KeyMap(nullptr) == nullptr);
    CHECK(KeyMap_count(nullptr) == 0);
    CHECK(KeyMap_isEmpty(nullptr) == true);
    CHECK(KeyMap_match(nullptr, 0) == nullptr);
    CHECK(KeyMap_bind(nullptr, 0, onFire, nullptr) == false);
    CHECK(KeyMap_unbind(nullptr, 0, onFire) == false);
    CHECK(KeyMap_isMultiTapEnabled(nullptr) == false);
    CHECK(KeyMap_getLongPressNanos(nullptr) == 0);
}

// Checks exact combo binding, lookup hits, misses, and count updates.
static void test_bind_and_match(void) {
    MemoryArena *arena = MemoryArena_create(64u << 20);
    KeyMap *map = KeyMap(arena);

    // Bind Cmd+A
    int64_t cmdA = KMOD_CMD | KEY_A;
    CHECK(KeyMap_bind(map, cmdA, onFire, nullptr));
    CHECK(KeyMap_count(map) == 1);
    CHECK(!KeyMap_isEmpty(map));

    // Exact match
    const KeyBinding *hit = KeyMap_match(map, cmdA);
    CHECK(hit != nullptr);
    CHECK((*hit).combo == cmdA);
    CHECK((*hit).fn == onFire);

    // Miss: different combo
    CHECK(KeyMap_match(map, KEY_A) == nullptr);
    CHECK(KeyMap_match(map, KMOD_CMD | KEY_B) == nullptr);

    // Bind a second combo
    int64_t f64Left = KMODE_f64_TAP | MOUSE_LEFT;
    CHECK(KeyMap_bind(map, f64Left, onFire2, nullptr));
    CHECK(KeyMap_count(map) == 2);

    // Both match
    hit = KeyMap_match(map, cmdA);
    CHECK(hit != nullptr);
    CHECK((*hit).fn == onFire);

    hit = KeyMap_match(map, f64Left);
    CHECK(hit != nullptr);
    CHECK((*hit).fn == onFire2);

    KeyMap_destroy(map);
    MemoryArena_destroy(arena);
}

// Checks rebinding a combo updates its userdata without duplicating the entry.
static void test_bind_replaces(void) {
    MemoryArena *arena = MemoryArena_create(64u << 20);
    KeyMap *map = KeyMap(arena);

    int64_t combo = KMOD_CMD | KEY_S;
    CHECK(KeyMap_bind(map, combo, onFire, nullptr));
    CHECK(KeyMap_count(map) == 1);

    // Rebind same combo + same function: updates userdata
    CHECK(KeyMap_bind(map, combo, onFire, (void*) 0x1234));
    CHECK(KeyMap_count(map) == 1);  // no duplicate

    const KeyBinding *hit = KeyMap_match(map, combo);
    CHECK(hit != nullptr);
    CHECK((*hit).userdata == (void*) 0x1234);

    KeyMap_destroy(map);
    MemoryArena_destroy(arena);
}

// Checks removal of selected bindings and preservation of unrelated entries.
static void test_unbind(void) {
    MemoryArena *arena = MemoryArena_create(64u << 20);
    KeyMap *map = KeyMap(arena);

    int64_t cmdA = KMOD_CMD | KEY_A;
    int64_t cmdB = KMOD_CMD | KEY_B;
    CHECK(KeyMap_bind(map, cmdA, onFire, nullptr));
    CHECK(KeyMap_bind(map, cmdB, onFire2, nullptr));
    CHECK(KeyMap_count(map) == 2);

    // Unbind cmdA
    CHECK(KeyMap_unbind(map, cmdA, onFire));
    CHECK(KeyMap_count(map) == 1);
    CHECK(KeyMap_match(map, cmdA) == nullptr);
    CHECK(KeyMap_match(map, cmdB) != nullptr);

    // Unbind cmdB
    CHECK(KeyMap_unbind(map, cmdB, onFire2));
    CHECK(KeyMap_count(map) == 0);
    CHECK(KeyMap_isEmpty(map));

    // Unbind non-existent
    CHECK(!KeyMap_unbind(map, cmdA, onFire));

    KeyMap_destroy(map);
    MemoryArena_destroy(arena);
}

// Checks the binding table grows beyond its initial capacity and remains searchable.
static void test_growth(void) {
    MemoryArena *arena = MemoryArena_create(64u << 20);
    KeyMap *map = KeyMap(arena);

    // Bind more than KEYMAP_INITIAL_CAPACITY (16) entries
    for (int i = 0; i < 24; i++) {
        int64_t combo = (int64_t)(KEY_A + i);
        CHECK(KeyMap_bind(map, combo, onFire, nullptr));
    }
    CHECK(KeyMap_count(map) == 24);

    // All should be findable
    for (int i = 0; i < 24; i++) {
        int64_t combo = (int64_t)(KEY_A + i);
        const KeyBinding *hit = KeyMap_match(map, combo);
        CHECK(hit != nullptr);
        CHECK((*hit).combo == combo);
    }

    KeyMap_destroy(map);
    MemoryArena_destroy(arena);
}

// Checks encoded modifier, gesture, key, and mask bit positions.
static void test_combo_constants(void) {
    // Verify the combo bit layout is correct via compile-time constants
    // Cmd+A: modifier nibble at bit 36, key code A=0x41
    int64_t cmdA = KMOD_CMD | KEY_A;
    CHECK(cmdA == ((1LL << 36) | 0x41));

    // d64-click left mouse: gesture nibble at bit 52, MOUSE_LEFT=0
    int64_t dblLeft = KMODE_f64_TAP | MOUSE_LEFT;
    CHECK(dblLeft == (1LL << 52));

    // Cmd+Shift+F4: bits 36, 32, and key F4=293 (0x125)
    int64_t cmdShiftF4 = KMOD_CMD | KMOD_SHIFT | KEY_F4;
    CHECK(cmdShiftF4 == ((1LL << 36) | (1LL << 32) | 293));

    // Mask constants cover the right bit ranges
    CHECK(KEY_CODE_MASK == 0x00000000FFFFFFFFLL);
    CHECK(KMODE_MASK    == 0x00F0000000000000LL);
}

// Checks combo construction from current key modifier state.
static void test_build_combo(void) {
    // buildCombo reads modifier state from Key_isDown.
    // No modifiers held → combo should be just gestureType | keyCode.
    Key_init();

    int64_t combo = KeyMap_buildCombo(KEY_A, KMODE_TAP);
    CHECK(combo == KEY_A);  // no modifiers → pure key code

    // With Cmd held
    Key_pushEvent(0, KEY_LEFT_SUPER, KEY_ACTION_DOWN, 250000000ULL);
    Key_dispatchEvents();
    CHECK(Key_isDown(KEY_LEFT_SUPER));

    combo = KeyMap_buildCombo(KEY_A, KMODE_TAP);
    CHECK((combo & KMOD_CMD) != 0);
    CHECK((combo & KEY_CODE_MASK) == KEY_A);

    // With Cmd+Shift held
    Key_pushEvent(0, KEY_LEFT_SHIFT, KEY_ACTION_DOWN, 250000000ULL);
    Key_dispatchEvents();

    combo = KeyMap_buildCombo(KEY_A, KMODE_TAP);
    CHECK((combo & KMOD_CMD) != 0);
    CHECK((combo & KMOD_SHIFT) != 0);

    // Release modifiers
    Key_pushEvent(0, KEY_LEFT_SUPER, KEY_ACTION_UP, 0);
    Key_pushEvent(0, KEY_LEFT_SHIFT, KEY_ACTION_UP, 0);
    Key_dispatchEvents();

    combo = KeyMap_buildCombo(KEY_A, KMODE_TAP);
    CHECK(combo == KEY_A);  // back to clean

    Key_shutdown();
}

// Checks hold-duration threshold mapping into the gesture bits.
static void test_build_combo_with_hold(void) {
    Key_init();

    // Press A and check hold duration builds long-press gesture
    Key_pushEvent(0, KEY_A, KEY_ACTION_DOWN, 250000000ULL);
    Key_dispatchEvents();

    // With 0 threshold, any hold is long-press
    int64_t combo = KeyMap_buildComboForHold(KEY_A, 0);
    // holdNanos > 0 and threshold == 0 means gestureFromHoldDuration returns TAP (threshold > 0 guard)
    // Actually: threshold 0 → condition `longPressThresholdNanos > 0` is false → returns TAP
    CHECK((combo & KMODE_MASK) == KMODE_TAP);

    // With very small threshold, current hold should exceed it
    combo = KeyMap_buildComboForHold(KEY_A, 1ULL);  // 1 nanosecond threshold
    CHECK((combo & KMODE_MASK) == KMODE_LONG_PRESS);

    // Key code still present
    CHECK((combo & KEY_CODE_MASK) == KEY_A);

    Key_pushEvent(0, KEY_A, KEY_ACTION_UP, 0);
    Key_dispatchEvents();
    Key_shutdown();
}

// ── Resolution (KeyMap_resolve) ────────────────────────────

static int g_fireCount = 0;
static int64_t g_fireCombo = -1;

// Records callback count and resolved combo for gesture-resolution cases.
static void onResolveFire(void *userdata, int64_t combo) {
    g_fireCount++;
    g_fireCombo = combo;
    (void) userdata;
}

// Short tap window + settle sleep: settlement is an OWNERSHIP of the pending
// window, so tests drive it with a 30ms window and wait 40ms — same settle
// semantics as the 250ms platform drivers, without 250ms of test latency.
#define TAP_WIN_NS 30000000ULL   // 30 ms tap window
#define SETTLE_SLEEP_US 40000    // 40 ms > window, so settlement always lands

// Sends one synthetic key press/release using the requested tap window.
static void tapKeyWin(int key, u64 winNanos) {
    Key_pushEvent(0, key, KEY_ACTION_DOWN, winNanos);
    Key_dispatchEvents();
    Key_pushEvent(0, key, KEY_ACTION_UP, winNanos);
    Key_dispatchEvents();
}

// Waits beyond the configured short tap window so pending taps settle.
static void settle(void) {
    usleep(SETTLE_SLEEP_US);
}

// Presses or releases the synthetic Super modifier and dispatches it.
static void holdSuper(bool down) {
    Key_pushEvent(0, KEY_LEFT_SUPER, down ? KEY_ACTION_DOWN : KEY_ACTION_UP, 250000000ULL);
    Key_dispatchEvents();
}

// Checks one pending tap settles, fires its binding once, and is consumed.
static void test_resolve_tap(void) {
    // Cmd+A tap: while the tap window is open nothing settles (pending);
    // once it closes, resolve fires exactly the Cmd+A binding once, and the
    // consumed tap cannot re-fire on the next frame.
    Key_init();
    MemoryArena *arena = MemoryArena_create(64u << 20);
    KeyMap *map = KeyMap(arena);

    int64_t cmdA = KMOD_CMD | KEY_A;
    CHECK(KeyMap_bind(map, cmdA, onResolveFire, nullptr));

    g_fireCount = 0;
    holdSuper(true);
    tapKeyWin(KEY_A, TAP_WIN_NS);

    // Window still open: tap is PENDING — nothing may fire yet.
    int64_t combo = -1;
    CHECK(Key_tapPhase(KEY_A) == KEY_TAP_PENDING);
    CHECK(!KeyMap_resolve(map, &combo));
    CHECK(g_fireCount == 0);

    // Settlement: the single tap resolves and fires.
    settle();
    CHECK(KeyMap_resolve(map, &combo));
    CHECK(g_fireCount == 1);
    CHECK(g_fireCombo == cmdA);
    CHECK(combo == cmdA);

    // Re-resolve: tap consumed → nothing fires.
    CHECK(!KeyMap_resolve(map, &combo));
    CHECK(g_fireCount == 1);

    holdSuper(false);
    KeyMap_destroy(map);
    MemoryArena_destroy(arena);
    Key_shutdown();
}

// Checks a settled d64 tap selects the more specific d64-tap binding.
static void test_resolve_specificity(void) {
    // A TAP and A f64_TAP both bound: two taps within the window settle as
    // d64 and resolve the f64_TAP binding (higher KMODE wins), never the
    // TAP — settlement is what makes the d64 reachable.
    Key_init();
    MemoryArena *arena = MemoryArena_create(64u << 20);
    KeyMap *map = KeyMap(arena);

    CHECK(KeyMap_bind(map, KEY_A | KMODE_TAP, onResolveFire, nullptr));
    CHECK(KeyMap_bind(map, KEY_A | KMODE_f64_TAP, onResolveFire, nullptr));

    g_fireCount = 0;
    tapKeyWin(KEY_A, TAP_WIN_NS);
    tapKeyWin(KEY_A, TAP_WIN_NS);

    // Window still open: two taps offered, nothing settled.
    int64_t combo = -1;
    CHECK(Key_tapPhase(KEY_A) == KEY_TAP_PENDING);
    CHECK(!KeyMap_resolve(map, &combo));
    CHECK(g_fireCount == 0);

    settle();
    CHECK(KeyMap_resolve(map, &combo));
    CHECK(g_fireCount == 1);
    CHECK((combo & KMODE_MASK) == KMODE_f64_TAP);

    // Consumed: nothing left to fire.
    CHECK(!KeyMap_resolve(map, &combo));
    CHECK(g_fireCount == 1);

    KeyMap_destroy(map);
    MemoryArena_destroy(arena);
    Key_shutdown();
}

// Checks modifier matching is exact for plain and Cmd-modified bindings.
static void test_resolve_modifier_exact(void) {
    // Plain A and Cmd+A both bound. Exact modifier equality: with Super held
    // only Cmd+A fires; without modifiers only plain A fires.
    Key_init();
    MemoryArena *arena = MemoryArena_create(64u << 20);
    KeyMap *map = KeyMap(arena);

    int64_t plainA = KEY_A | KMODE_TAP;
    int64_t cmdA = KMOD_CMD | KEY_A | KMODE_TAP;
    CHECK(KeyMap_bind(map, plainA, onResolveFire, nullptr));
    CHECK(KeyMap_bind(map, cmdA, onResolveFire, nullptr));

    // With Super held: Cmd+A only.
    g_fireCount = 0;
    holdSuper(true);
    tapKeyWin(KEY_A, TAP_WIN_NS);
    int64_t combo = -1;
    settle();
    CHECK(KeyMap_resolve(map, &combo));
    CHECK(g_fireCount == 1);
    CHECK(g_fireCombo == cmdA);

    // Without modifiers: plain A only.
    holdSuper(false);
    g_fireCount = 0;
    tapKeyWin(KEY_A, TAP_WIN_NS);
    settle();
    CHECK(KeyMap_resolve(map, &combo));
    CHECK(g_fireCount == 1);
    CHECK(g_fireCombo == plainA);

    KeyMap_destroy(map);
    MemoryArena_destroy(arena);
    Key_shutdown();
}

// Checks a settled right-button d64 click resolves and consumes its binding.
static void test_resolve_mouse(void) {
    // Right-button d64-click settles and resolves the MOUSE_RIGHT
    // f64_TAP binding; Mouse_resetTaps consumes it via the winner path.
    Mouse_init();
    MemoryArena *arena = MemoryArena_create(64u << 20);
    KeyMap *map = KeyMap(arena);

    int64_t dblRight = MOUSE_RIGHT | KMODE_f64_TAP;
    CHECK(KeyMap_bind(map, dblRight, onResolveFire, nullptr));

    Mouse_pushButtonEvent(0, MOUSE_RIGHT, KEY_ACTION_DOWN, TAP_WIN_NS);
    Mouse_dispatchEvents();
    Mouse_pushButtonEvent(0, MOUSE_RIGHT, KEY_ACTION_UP, TAP_WIN_NS);
    Mouse_dispatchEvents();
    Mouse_pushButtonEvent(0, MOUSE_RIGHT, KEY_ACTION_DOWN, TAP_WIN_NS);
    Mouse_dispatchEvents();
    Mouse_pushButtonEvent(0, MOUSE_RIGHT, KEY_ACTION_UP, TAP_WIN_NS);
    Mouse_dispatchEvents();

    g_fireCount = 0;
    int64_t combo = -1;
    CHECK(Mouse_tapPhase(MOUSE_RIGHT) == MOUSE_TAP_PENDING);
    CHECK(!KeyMap_resolve(map, &combo)); // clicks still offered
    CHECK(g_fireCount == 0);

    settle();
    CHECK(KeyMap_resolve(map, &combo));
    CHECK(g_fireCount == 1);
    CHECK(g_fireCombo == dblRight);

    // Consumed: no re-fire.
    CHECK(!KeyMap_resolve(map, &combo));
    CHECK(g_fireCount == 1);

    KeyMap_destroy(map);
    MemoryArena_destroy(arena);
    Mouse_shutdown();
}

// Checks a long press fires after its threshold and remains latched until release.
static void test_resolve_long_press(void) {
    // Hold A past KEYMAP_LONG_PRESS_NANOS: the LONG_PRESS binding fires.
    // Uses a real 450 ms hold — single deliberate sleep in the suite.
    Key_init();
    MemoryArena *arena = MemoryArena_create(64u << 20);
    KeyMap *map = KeyMap(arena);

    int64_t holdA = KEY_A | KMODE_LONG_PRESS;
    CHECK(KeyMap_bind(map, holdA, onResolveFire, nullptr));

    Key_pushEvent(0, KEY_A, KEY_ACTION_DOWN, 250000000ULL);
    Key_dispatchEvents();

    // Too early: the hold clock is below the threshold, no fire.
    g_fireCount = 0;
    int64_t combo = -1;
    CHECK(!KeyMap_resolve(map, &combo));
    CHECK(g_fireCount == 0);

    // Wait past the 400ms threshold (50ms margin).
    usleep(450 * 1000);
    CHECK(KeyMap_resolve(map, &combo));
    CHECK(g_fireCount == 1);
    CHECK((combo & KMODE_MASK) == KMODE_LONG_PRESS);

    // One-shot latch: the SAME hold cannot re-fire while still held.
    CHECK(!KeyMap_resolve(map, &combo));
    CHECK(g_fireCount == 1);
    CHECK(Key_isLongPressFired(KEY_A));

    Key_pushEvent(0, KEY_A, KEY_ACTION_UP, 0);
    Key_dispatchEvents();
    CHECK(!Key_isLongPressFired(KEY_A)); // release clears the latch
    KeyMap_destroy(map);
    MemoryArena_destroy(arena);
    Key_shutdown();
}

// Checks a long-press hit consumes the press so release cannot also fire tap.
static void test_long_press_single_fire(void) {
    // ONE press → at most ONE fire: a LONG_PRESS hit consumes the tap count
    // as well, so releasing the same press never ALSO fires a TAP.
    Key_init();
    MemoryArena *arena = MemoryArena_create(64u << 20);
    KeyMap *map = KeyMap(arena);

    CHECK(KeyMap_bind(map, KEY_A | KMODE_LONG_PRESS, onResolveFire, nullptr));
    CHECK(KeyMap_bind(map, KEY_A | KMODE_TAP, onResolveFire, nullptr));

    Key_pushEvent(0, KEY_A, KEY_ACTION_DOWN, TAP_WIN_NS);
    Key_dispatchEvents();

    g_fireCount = 0;
    int64_t combo = -1;
    usleep(450 * 1000); // past KEYMAP_LONG_PRESS_NANOS (400ms) — 50ms margin
    CHECK(KeyMap_resolve(map, &combo));
    CHECK(g_fireCount == 1);
    CHECK((combo & KMODE_MASK) == KMODE_LONG_PRESS);

    // Latch: same hold cannot re-fire.
    CHECK(!KeyMap_resolve(map, &combo));
    CHECK(g_fireCount == 1);

    Key_pushEvent(0, KEY_A, KEY_ACTION_UP, 0);
    Key_dispatchEvents();
    // Release of the same press: the taps were consumed by the long-press hit
    // — no TAP fires on release.
    CHECK(!KeyMap_resolve(map, &combo));
    CHECK(g_fireCount == 1);

    KeyMap_destroy(map);
    MemoryArena_destroy(arena);
    Key_shutdown();
}

// Checks disabled multi-tap mode resolves taps immediately and excludes f64s.
static void test_multi_tap_disabled(void) {
    // Rhythm-game mode (KeyMap_setMultiTapEnabled(false)): every press-release
    // is a single tap resolved IMMEDIATELY — zero window latency; d64 and
    // TRIPLE bindings never resolve.
    Key_init();
    MemoryArena *arena = MemoryArena_create(64u << 20);
    KeyMap *map = KeyMap(arena);

    CHECK(KeyMap_isMultiTapEnabled(map)); // settle-based multi-tap default on
    KeyMap_setMultiTapEnabled(map, false);
    CHECK(!KeyMap_isMultiTapEnabled(map));

    CHECK(KeyMap_bind(map, KEY_A | KMODE_TAP, onResolveFire, nullptr));
    CHECK(KeyMap_bind(map, KEY_A | KMODE_f64_TAP, onResolveFire, nullptr));

    // Single tap: resolved on the very next resolve call, no window wait.
    g_fireCount = 0;
    tapKeyWin(KEY_A, TAP_WIN_NS);
    int64_t combo = -1;
    CHECK(KeyMap_resolve(map, &combo));
    CHECK(g_fireCount == 1);
    CHECK((combo & KMODE_MASK) == KMODE_TAP);

    // Two fast taps still fire TAP — twice. d64 never resolves.
    tapKeyWin(KEY_A, TAP_WIN_NS);
    tapKeyWin(KEY_A, TAP_WIN_NS);
    CHECK(KeyMap_resolve(map, &combo));
    CHECK(g_fireCount == 2);
    CHECK((combo & KMODE_MASK) == KMODE_TAP);
    CHECK(!KeyMap_resolve(map, &combo));
    CHECK(g_fireCount == 2);

    KeyMap_destroy(map);
    MemoryArena_destroy(arena);
    Key_shutdown();
}

// Checks per-key gesture phases through pending, settlement, and consumption.
static void test_tap_phase(void) {
    // Public per-key gesture timeline: NONE → PENDING (window open) →
    // settled SINGLE/d64/TRIPLE once the window closes. Consumed → NONE.
    Key_init();

    CHECK(Key_tapPhase(KEY_A) == KEY_TAP_NONE);
    CHECK(!Key_isLongPressFired(KEY_A));

    Key_pushEvent(0, KEY_A, KEY_ACTION_DOWN, TAP_WIN_NS);
    Key_dispatchEvents();
    CHECK(Key_tapPhase(KEY_A) == KEY_TAP_PENDING);
    Key_pushEvent(0, KEY_A, KEY_ACTION_UP, TAP_WIN_NS);
    Key_dispatchEvents();
    CHECK(Key_tapPhase(KEY_A) == KEY_TAP_PENDING); // released, window still open
    settle();
    CHECK(Key_tapPhase(KEY_A) == KEY_TAP_SINGLE);

    // Consumed → NONE again.
    Key_resetTaps(KEY_A);
    CHECK(Key_tapPhase(KEY_A) == KEY_TAP_NONE);

    // Two taps within the window settle as d64.
    Key_pushEvent(0, KEY_A, KEY_ACTION_DOWN, TAP_WIN_NS);
    Key_dispatchEvents();
    Key_pushEvent(0, KEY_A, KEY_ACTION_UP, TAP_WIN_NS);
    Key_dispatchEvents();
    Key_pushEvent(0, KEY_A, KEY_ACTION_DOWN, TAP_WIN_NS);
    Key_dispatchEvents();
    Key_pushEvent(0, KEY_A, KEY_ACTION_UP, TAP_WIN_NS);
    Key_dispatchEvents();
    CHECK(Key_tapPhase(KEY_A) == KEY_TAP_PENDING);
    settle();
    CHECK(Key_tapPhase(KEY_A) == KEY_TAP_f64);
    Key_resetTaps(KEY_A);

    // Three taps settle as TRIPLE.
    for (int i = 0; i < 3; i++) {
        Key_pushEvent(0, KEY_A, KEY_ACTION_DOWN, TAP_WIN_NS);
        Key_dispatchEvents();
        Key_pushEvent(0, KEY_A, KEY_ACTION_UP, TAP_WIN_NS);
        Key_dispatchEvents();
    }
    CHECK(Key_tapPhase(KEY_A) == KEY_TAP_PENDING);
    settle();
    CHECK(Key_tapPhase(KEY_A) == KEY_TAP_TRIPLE);
    Key_resetTaps(KEY_A);
    CHECK(Key_tapPhase(KEY_A) == KEY_TAP_NONE);

    Key_shutdown();
}

// Checks a modifier change starts a fresh tap sequence rather than accumulating.
static void test_modifier_breaks_sequence(void) {
    // A modifier-state change between presses breaks the tap sequence:
    // plain Q then Cmd+Q within the window must NOT accumulate into a
    // d64 — the Cmd+Q key press starts a fresh single sequence.
    Key_init();
    MemoryArena *arena = MemoryArena_create(64u << 20);
    KeyMap *map = KeyMap(arena);

    int64_t cmdQ = KMOD_CMD | KMODE_TAP | KEY_Q;
    CHECK(KeyMap_bind(map, cmdQ, onResolveFire, nullptr));

    g_fireCount = 0;
    tapKeyWin(KEY_Q, TAP_WIN_NS);  // plain Q (no Cmd)
    holdSuper(true);               // modifier state changes between presses
    tapKeyWin(KEY_Q, TAP_WIN_NS);  // Cmd+Q — fresh sequence, never a d64

    int64_t combo = -1;
    CHECK(!KeyMap_resolve(map, &combo)); // pending
    settle();
    CHECK(KeyMap_resolve(map, &combo));
    CHECK(g_fireCount == 1);
    CHECK(g_fireCombo == cmdQ);

    KeyMap_destroy(map);
    MemoryArena_destroy(arena);
    Key_shutdown();
}

// Runs KeyMap lifecycle, binding, combo construction, and gesture-resolution cases.
int main(void) {
    test_create_destroy();
    test_null_safety();
    test_bind_and_match();
    test_bind_replaces();
    test_unbind();
    test_growth();
    test_combo_constants();
    test_build_combo();
    test_build_combo_with_hold();
    test_resolve_tap();
    test_resolve_specificity();
    test_resolve_modifier_exact();
    test_resolve_mouse();
    test_resolve_long_press();
    test_long_press_single_fire();
    test_multi_tap_disabled();
    test_tap_phase();
    test_modifier_breaks_sequence();

    if (g_failures == 0)
        printf("key_map_test: all checks passed\n");
    else
        printf("key_map_test: %d FAILURES\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
