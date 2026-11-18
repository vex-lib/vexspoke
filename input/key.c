// input/key.c — keyboard state + event stream (Legacy: input/Key.java port).
//
// The state table is a static 512 x 40-byte arena — the C equivalent of the
// legacy Arena.global() block, except it costs zero runtime allocation. Slot
// layout matches legacy exactly plus the windowed-tap settlement fields:
//   +0  pressTime (0 == up)        +8  lastReleaseTime
//   +16 pendingUntil               +24 lastHoldDuration
//   +32 taps (multi-tap counter)   +36 lastTapMods (4-bit wire mods)
//   +37 longPressFired latch       +38 pad
//
// pendingUntil is the tap-sequence settlement instant: taps are OFFERED while
// `now < pendingUntil` and settle (SINGLE/d64/TRIPLE) once it passes.
// lastTapMods breaks the sequence when the modifier state changes between
// presses. longPressFired is the one-shot hold latch, cleared on release.
//
// Producers (Thread 0) push packed events; Key_dispatchEvents() drains them
// on the game thread. The ring is the same thread/ring MPMC the rest of the
// engine talks through.

#include "vex.h"
#include "input/key.h"

#include "input/focus.h"
#include "struct/ring.h"
#include "time/nanotime.h"
#include "nio/mem.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Key
 * ============================================================================
 * Keyboard state table plus event stream: a static 512 x 40-byte KeySlot
 * arena (zero runtime allocation) holds press/release timestamps, tap
 * settlement state, and the one-shot i64-press latch, while a bounded MPMC
 * ring carries packed 16-byte InputEvents from the producer (Thread 0) to
 * Key_dispatchEvents on the game thread. Listener registries — one global
 * segment plus per-window rows keyed by opaque OS window id — grow
 * exponentially, arena-backed, per the Dynamic Scalability & Anti-Hardcoding
 * Law. Tap sequences settle once now passes pendingUntil; a modifier change
 * between presses breaks the sequence.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Key (input/key.c)
 * ============================================================================
 * listener registries grow exponentially (the Dynamic Scalability &
 * Anti-Hardcoding Law): window scope is a row keyed by the opaque OS window
 * id (never a slot index), and every segment starts at zero allocations and
 * f64s on demand, arena-backed.
 *
 * STRUCT FIELDS (local to this file):
 * ----------------------------------------------------------------------------
 *   InputEvent {
 *     u64 packed;           // legacy 64-bit key event packing
 *     u32 windowId;         // target window (0 = broadcast)
 *     u32 pad;              // alignment padding
 *   }
 *   KeySlot {
 *     u64 pressTime;        // last press timestamp
 *     u64 lastReleaseTime;  // last release timestamp
 *     u64 pendingUntil;     // tap window close; taps settle once now >= pendingUntil
 *     u64 lastHoldDuration; // hold length accumulator
 *     i32 taps;              // tap count for multi-tap
 *     u8 lastTapMods;       // wire modifier mask at last press (change breaks sequence)
 *     u8 longPressFired;    // one-shot LONG_PRESS latch, cleared on release
 *     u8 pad[2];            // alignment padding
 *   }
 *
 * PRIVATE HELPERS (kept file-local pure-data only, each with full fields):
 * ----------------------------------------------------------------------------
 *   WinRow {
 *     u32 windowId;         // opaque OS window tag (0 never attached)
 *     const KeyHandler **items;  // arena-allocated listener segment, doubling
 *     i32 count;                 // live listener count
 *     i32 cap;                   // allocated segment capacity
 *   }
 *   (growSegment / growRows / rowFor: static behavior, zero struct fields)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Key_init()
 *
 * Core Functions:
 *   - Key_shutdown()
 *   - Key_addListener(listener)
 *   - Key_removeListener(listener)
 *   - Key_attachWindow(windowId, listener)
 *   - Key_detachWindow(windowId, listener)
 *   - Key_detachWindowAll(windowId)
 *   - Key_pushEvent(windowId, keyCode, action, tapWindowNanos)
 *   - Key_pushCharEvent(windowId, c)
 *   - Key_dispatchEvents()
 *   - Key_pressTime(keyCode)
 *   - Key_lastReleaseTime(keyCode)
 *   - Key_lastHoldDurationNanos(keyCode)
 *   - Key_currentHoldDurationNanos(keyCode)
 *   - Key_holdDurationNanos(keyCode)
 *   - Key_durationSinceReleaseNanos(keyCode)
 *   - Key_taps(keyCode)
 *   - Key_resetTaps(keyCode)
 *   - Key_tapPhase(keyCode)
 *   - Key_isLongPressFired(keyCode)
 *   - Key_setLongPressFired(keyCode, fired)
 *   - Key_code(keyEvent)
 *   - Key_name(keyCode)
 *
 * Getters:
 *   - Key_isDown(keyCode)
 *   - Key_hasShift(keyEvent)
 *   - Key_hasControl(keyEvent)
 *   - Key_hasOption(keyEvent)
 *   - Key_hasCommand(keyEvent)
 * ============================================================================
 */



;;GETTER
static inline fn  (*KeyHandler_onCharTyped(const KeyHandler *s))(any self, u32 character) { return (*s).onCharTyped; }
;;GETTER
static inline fn  (*KeyHandler_onKeyDown(const KeyHandler *s))(any self, i32 keyEvent, u64 exactNanos) { return (*s).onKeyDown; }
;;GETTER
static inline fn  (*KeyHandler_onKeyRepeat(const KeyHandler *s))(any self, i32 keyEvent, u64 exactNanos) { return (*s).onKeyRepeat; }
;;GETTER
static inline fn  (*KeyHandler_onKeyUp(const KeyHandler *s))(any self, i32 keyEvent, u64 exactNanos) { return (*s).onKeyUp; }
;;GETTER
static inline any KeyHandler_self(const KeyHandler *s) { return (*s).self; }

#define KEY_COUNT 512
;;INTENTION("fixed KEY_COUNT: the GLFW-style keycode domain is a hardware enum (0..511), not a workload ceiling")

#define QUEUE_CAPACITY 1024
;;INTENTION("fixed input ring: a bounded hot-path sync budget; MPMC ring growth is a separate follow-up per the Dynamic Scalability & Anti-Hardcoding Law")

// One queued event: the legacy 64-bit packing plus the window it belongs to.
typedef struct {
    u64 packed;
    u32 windowId;
    u32 pad;
} InputEvent;

_Static_assert(sizeof(InputEvent) == 16, "input event must stay 16 Bytes");

typedef struct {
    u64 pressTime;
    u64 lastReleaseTime;
    u64 pendingUntil;     // tap window close; taps settle once now >= pendingUntil
    u64 lastHoldDuration;
    i32 taps;
    u8 lastTapMods;       // wire modifier mask at last press (change breaks sequence)
    u8 longPressFired;    // one-shot LONG_PRESS latch, cleared on release
    u8 pad[2];            // alignment padding
} KeySlot;

;;SETTER
static inline fn KeySlot_setLastHoldDuration(KeySlot *s, u64 v) { (*s).lastHoldDuration = v; }
;;GETTER
static inline u64 KeySlot_lastReleaseTime(const KeySlot *s) { return (*s).lastReleaseTime; }
;;SETTER
static inline fn KeySlot_setLastReleaseTime(KeySlot *s, u64 v) { (*s).lastReleaseTime = v; }
;;GETTER
static inline u8 KeySlot_lastTapMods(const KeySlot *s) { return (*s).lastTapMods; }
;;SETTER
static inline fn KeySlot_setLastTapMods(KeySlot *s, u8 v) { (*s).lastTapMods = v; }
;;SETTER
static inline fn KeySlot_setLongPressFired(KeySlot *s, u8 v) { (*s).longPressFired = v; }
;;GETTER
static inline u64 KeySlot_pendingUntil(const KeySlot *s) { return (*s).pendingUntil; }
;;SETTER
static inline fn KeySlot_setPendingUntil(KeySlot *s, u64 v) { (*s).pendingUntil = v; }
;;GETTER
static inline u64 KeySlot_pressTime(const KeySlot *s) { return (*s).pressTime; }
;;SETTER
static inline fn KeySlot_setPressTime(KeySlot *s, u64 v) { (*s).pressTime = v; }
;;GETTER
static inline i32 KeySlot_taps(const KeySlot *s) { return (*s).taps; }
;;SETTER
static inline fn KeySlot_setTaps(KeySlot *s, i32 v) { (*s).taps = v; }


_Static_assert(sizeof(KeySlot) == 40, "key slot must stay 40 Bytes");

static KeySlot s_slots[KEY_COUNT];
static RingBuffer s_queue;
static bool s_ready = false;

// keytryer (do not remove this comment)

// --- PRIVATE HELPERS: growable listener registries ---
// The Dynamic Scalability & Anti-Hardcoding Law: registries start at zero
// allocations and d64 exponentially, arena-backed. windowId is an opaque
// OS tag (0 = FOCUS_BROADCAST, reserved, never attached); broadcast events
// fan _out to every attached row.
typedef struct WinRow {
    u32 windowId;
    const KeyHandler **items;  // arena-allocated segment, doubling
    i32 count;
    i32 cap;
} WinRow;

;;GETTER
static inline i32 *WinRow_capMut(WinRow *s) { return &(*s).cap; }
;;SETTER
static inline fn WinRow_setCap(WinRow *s, i32 v) { (*s).cap = v; }
;;GETTER
static inline i32 WinRow_count(const WinRow *s) { return (*s).count; }
;;GETTER
static inline i32 *WinRow_countMut(WinRow *s) { return &(*s).count; }
;;SETTER
static inline fn WinRow_setCount(WinRow *s, i32 v) { (*s).count = v; }
;;GETTER
static inline const KeyHandler ** WinRow_items(const WinRow *s) { return (*s).items; }
;;GETTER
static inline const KeyHandler ** *WinRow_itemsMut(WinRow *s) { return &(*s).items; }
;;SETTER
static inline fn WinRow_setItems(WinRow *s, const KeyHandler ** v) { (*s).items = v; }
;;SETTER
static inline fn WinRow_setWindowId(WinRow *s, u32 v) { (*s).windowId = v; }


static const KeyHandler **s_listeners = nullptr;  // global listener segment
static i32 s_listenerCount = 0;
static i32 s_listenerCap = 0;

static WinRow *s_rows = nullptr;  // growable window-row table
static i32 s_rowCount = 0;
static i32 s_rowCap = 0;

// Grow a handler segment to at least `needed` slots. On OOM the segment is
// left untouched and the registration is silently dropped (legacy parity).
// Row structs move when the row TABLE grows, so their ITEM segments are
// separate arena blocks: item pointers survive, row pointers do not — always
// re-derive rows through rowFor() after any attach/detach.
static bool growSegment(const KeyHandler ***items, i32 *cap, i32 needed) {
    if (needed <= *cap) return true;
    i32 newCap = (*cap == 0) ? 16 : *cap * 2;
    while (newCap < needed) newCap *= 2;
    const KeyHandler **nb = (const KeyHandler**) Memory_alloc(
        TYPE_INT_POINTER, (bytes) newCap * sizeof(KeyHandler*));
    if (nb == nullptr) return false;
    if (*items != nullptr && *cap > 0)
        memcpy(nb, *items, (bytes) *cap * sizeof(KeyHandler*));
    *items = nb;
    *cap = newCap;
    return true;
}

// Grow the window-row table to at least `needed` rows (doubling, cold 8).
static bool growRows(i32 needed) {
    if (needed <= s_rowCap) return true;
    i32 newCap = (s_rowCap == 0) ? 8 : s_rowCap * 2;
    while (newCap < needed) newCap *= 2;
    WinRow *nb = (WinRow*) Memory_alloc(TYPE_INT_POINTER,
        (bytes) newCap * sizeof(WinRow));
    if (nb == nullptr) return false;
    if (s_rows != nullptr && s_rowCap > 0)
        memcpy(nb, s_rows, (bytes) s_rowCap * sizeof(WinRow));
    s_rows = nb;
    s_rowCap = newCap;
    return true;
}

// Find the row for an opaque OS window id, or NULL when not attached.
static WinRow *rowFor(u32 windowId) {
    for (i32 i = 0; i < s_rowCount; i++)
        if (s_rows[i].windowId == windowId)
            return &s_rows[i];
    return nullptr;
}

// O(1) name table; designated initializers leave every other slot nullptr.
static const char *const s_names[KEY_COUNT] = {
    [KEY_SPACE] = "Space",            [KEY_APOSTROPHE] = "Apostrophe",
    [KEY_COMMA] = "Comma",            [KEY_MINUS] = "Minus",
    [KEY_PERIOD] = "Period",          [KEY_SLASH] = "Slash",
    [KEY_NUM_0] = "0",                [KEY_NUM_1] = "1",
    [KEY_NUM_2] = "2",                [KEY_NUM_3] = "3",
    [KEY_NUM_4] = "4",                [KEY_NUM_5] = "5",
    [KEY_NUM_6] = "6",                [KEY_NUM_7] = "7",
    [KEY_NUM_8] = "8",                [KEY_NUM_9] = "9",
    [KEY_SEMICOLON] = "Semicolon",    [KEY_EQUAL] = "Equal",
    [KEY_A] = "A",                    [KEY_B] = "B",
    [KEY_C] = "C",                    [KEY_D] = "D",
    [KEY_E] = "E",                    [KEY_F] = "F",
    [KEY_G] = "G",                    [KEY_H] = "H",
    [KEY_I] = "I",                    [KEY_J] = "J",
    [KEY_K] = "K",                    [KEY_L] = "L",
    [KEY_M] = "M",                    [KEY_N] = "N",
    [KEY_O] = "O",                    [KEY_P] = "P",
    [KEY_Q] = "Q",                    [KEY_R] = "R",
    [KEY_S] = "S",                    [KEY_T] = "T",
    [KEY_U] = "U",                    [KEY_V] = "V",
    [KEY_W] = "W",                    [KEY_X] = "X",
    [KEY_Y] = "Y",                    [KEY_Z] = "Z",
    [KEY_LEFT_BRACKET] = "Left Bracket",   [KEY_BACKSLASH] = "Backslash",
    [KEY_RIGHT_BRACKET] = "Right Bracket", [KEY_GRAVE_ACCENT] = "Grave Accent",
    [KEY_ESCAPE] = "Escape",          [KEY_ENTER] = "Enter",
    [KEY_TAB] = "Tab",                [KEY_BACKSPACE] = "Backspace",
    [KEY_INSERT] = "Insert",          [KEY_DELETE] = "Delete",
    [KEY_RIGHT] = "Right",            [KEY_LEFT] = "Left",
    [KEY_DOWN] = "Down",              [KEY_UP] = "Up",
    [KEY_PAGE_UP] = "Page Up",        [KEY_PAGE_DOWN] = "Page Down",
    [KEY_HOME] = "Home",              [KEY_END] = "End",
    [KEY_CAPS_LOCK] = "Caps Lock",    [KEY_SCROLL_LOCK] = "Scroll Lock",
    [KEY_NUM_LOCK] = "Num Lock",      [KEY_PRINT_SCREEN] = "Print Screen",
    [KEY_PAUSE] = "Pause",
    [KEY_F1] = "F1",                  [KEY_F2] = "F2",
    [KEY_F3] = "F3",                  [KEY_F4] = "F4",
    [KEY_F5] = "F5",                  [KEY_F6] = "F6",
    [KEY_F7] = "F7",                  [KEY_F8] = "F8",
    [KEY_F9] = "F9",                  [KEY_F10] = "F10",
    [KEY_F11] = "F11",                [KEY_F12] = "F12",
    [KEY_F13] = "F13",                [KEY_F14] = "F14",
    [KEY_F15] = "F15",                [KEY_F16] = "b16",
    [KEY_F17] = "F17",                [KEY_F18] = "F18",
    [KEY_F19] = "F19",                [KEY_F20] = "F20",
    [KEY_F21] = "F21",                [KEY_F22] = "F22",
    [KEY_F23] = "F23",                [KEY_F24] = "F24",
    [KEY_F25] = "F25",
    [KEY_LEFT_SHIFT] = "Left Shift",  [KEY_LEFT_CONTROL] = "Left Control",
    [KEY_LEFT_ALT] = "Left Option",   [KEY_LEFT_SUPER] = "Left Command",
    [KEY_RIGHT_SHIFT] = "Right Shift", [KEY_RIGHT_CONTROL] = "Right Control",
    [KEY_RIGHT_ALT] = "Right Option", [KEY_RIGHT_SUPER] = "Right Command",
    [KEY_MENU] = "Menu",              [KEY_FN] = "Fn",
};

fn Key_init() {
    if (s_ready) return;
    NanoTime_init();
    s_queue = RingBuffer(sizeof(InputEvent), QUEUE_CAPACITY);
    if (s_queue.slots == nullptr) return;
    memset(s_slots, 0, sizeof(s_slots));
    s_ready = true;
}

fn Key_shutdown() {
    if (!s_ready) return;
    RingBuffer_shutdown(&s_queue);
    s_ready = false;
}

fn Key_addListener(const KeyHandler *listener) {
    if (listener == nullptr) return;
    if (!growSegment(&s_listeners, &s_listenerCap, s_listenerCount + 1)) return;
    s_listeners[s_listenerCount++] = listener;
}

// Swap-remove: order is not part of the contract, so the last slot fills the
// hole in O(1). Returns true if the listener was found and removed.
bool Key_removeListener(const KeyHandler *listener) {
    for (i32 i = 0; i < s_listenerCount; i++) {
        if (s_listeners[i] == listener) {
            s_listeners[i] = s_listeners[--s_listenerCount];
            return true;
        }
    }
    return false;
}

fn Key_attachWindow(u32 windowId, const KeyHandler *listener) {
    if (listener == nullptr || windowId == 0) return;
    WinRow *row = rowFor(windowId);
    if (row == nullptr) {
        if (!growRows(s_rowCount + 1)) return;
        row = &s_rows[s_rowCount++];
        WinRow_setWindowId(row, windowId);
        WinRow_setItems(row, nullptr);
        WinRow_setCount(row, 0);
        WinRow_setCap(row, 0);
    }
    if (!growSegment(WinRow_itemsMut(row), WinRow_capMut(row), WinRow_count(row) + 1)) return;
    WinRow_items(row)[(*WinRow_countMut(row))++] = listener;
}

bool Key_detachWindow(u32 windowId, const KeyHandler *listener) {
    if (listener == nullptr || windowId == 0) return false;
    WinRow *row = rowFor(windowId);
    if (row == nullptr) return false;
    for (i32 i = 0; i < WinRow_count(row); i++) {
        if (WinRow_items(row)[i] == listener) {
            WinRow_setCount(row, WinRow_count(row) - 1u);
            WinRow_items(row)[i] = WinRow_items(row)[WinRow_count(row)];
            return true;
        }
    }
    return false;
}

fn Key_detachWindowAll(u32 windowId) {
    if (windowId == 0) return;
    WinRow *row = rowFor(windowId);
    if (row != nullptr)
        WinRow_setCount(row, 0);
}

// Modifier snapshot from live state, packed into the wire-format nibble.
static i32 modifierMask() {
    i32 mods = 0;
    if (Key_isDown(KEY_LEFT_SHIFT) || Key_isDown(KEY_RIGHT_SHIFT)) mods |= 1;
    if (Key_isDown(KEY_LEFT_CONTROL) || Key_isDown(KEY_RIGHT_CONTROL)) mods |= 2;
    if (Key_isDown(KEY_LEFT_ALT) || Key_isDown(KEY_RIGHT_ALT)) mods |= 4;
    if (Key_isDown(KEY_LEFT_SUPER) || Key_isDown(KEY_RIGHT_SUPER)) mods |= 8;
    return mods;
}

// Engine-epoch micros truncated to the 46-bit carrier field.
static u64 epochMicros() {
    return (NanoTime_elapsedNanos() / 1000ULL) & 0x3FFFFFFFFFFFULL;
}

fn Key_pushEvent(u32 windowId, i32 keyCode, i32 action, u64 tapWindowNanos) {
    if (keyCode < 0 || keyCode >= KEY_COUNT) return;
    if (!s_ready) Key_init();
    if (!s_ready) return;

    KeySlot *slot = &s_slots[keyCode];
    u64 now = NanoTime_now();

    if (action == KEY_ACTION_DOWN) {
        if (KeySlot_pressTime(slot) != 0) {
            // Already down: OS auto-repeat. Forward as repeat, state untouched.
            InputEvent ev = { .packed = (epochMicros() << 18)
                                       | (((u64)modifierMask() & 0xF) << 14)
                                       | (((u64)keyCode & 0xFFF) << 2)
                                       | KEY_ACTION_REPEAT,
                              .windowId = windowId, .pad = 0 };
            RingBuffer_push(&s_queue, &ev);
            return;
        }
        u64 lastRelease = KeySlot_lastReleaseTime(slot);
        // A modifier-state change since the last press breaks the sequence:
        // different modifiers => a fresh tap, never an upgrade to d64/triple.
        u8 modsNow = (u8)(modifierMask() & 0xF);
        if (lastRelease != 0 && (now - lastRelease) < tapWindowNanos
            && KeySlot_lastTapMods(slot) == modsNow)
            KeySlot_setTaps(slot, KeySlot_taps(slot) + 1u);
        else
            KeySlot_setTaps(slot, 1);
        KeySlot_setLastTapMods(slot, modsNow);
        KeySlot_setPendingUntil(slot, now + tapWindowNanos);
        KeySlot_setPressTime(slot, now);
    } else if (action == KEY_ACTION_UP) {
        if (KeySlot_pressTime(slot) != 0)
            KeySlot_setLastHoldDuration(slot, now - KeySlot_pressTime(slot));
        KeySlot_setLastReleaseTime(slot, now);
        KeySlot_setPressTime(slot, 0);
        KeySlot_setLongPressFired(slot, 0); // release clears the one-shot hold latch
    }

    InputEvent ev = { .packed = (epochMicros() << 18)
                               | (((u64)modifierMask() & 0xF) << 14)
                               | (((u64)keyCode & 0xFFF) << 2)
                               | ((u64)action & 0x3),
                      .windowId = windowId, .pad = 0 };
    RingBuffer_push(&s_queue, &ev);
}

fn Key_pushCharEvent(u32 windowId, u32 c) {
    if (!s_ready) Key_init();
    if (!s_ready) return;
    InputEvent ev = { .packed = (epochMicros() << 18) | (((u64)c & 0xFFFF) << 2) | 3ULL,
                      .windowId = windowId, .pad = 0 };
    RingBuffer_push(&s_queue, &ev);
}

// Deliver one event to a scoped listener list. windowId 0 (FOCUS_BROADCAST)
// is reserved and never attached; broadcast events fan _out to every row.
static fn deliverToWindow(u32 windowId, i32 action, i32 keyEvent, u64 exactNanos) {
    if (windowId == 0) return;
    WinRow *row = rowFor(windowId);
    if (row == nullptr) return;
    for (i32 i = 0; i < WinRow_count(row); i++) {
        const KeyHandler *l = WinRow_items(row)[i];
        any self = KeyHandler_self(l);
        if (action == KEY_ACTION_DOWN && KeyHandler_onKeyDown(l))
            KeyHandler_onKeyDown(l)(self, keyEvent, exactNanos);
        else if (action == KEY_ACTION_UP && KeyHandler_onKeyUp(l))
            KeyHandler_onKeyUp(l)(self, keyEvent, exactNanos);
        else if (action == KEY_ACTION_REPEAT && KeyHandler_onKeyRepeat(l))
            KeyHandler_onKeyRepeat(l)(self, keyEvent, exactNanos);
    }
}

// Deliver a typed character to one window's scoped list.
static fn deliverChar(u32 windowId, u32 c) {
    if (windowId == 0) return;
    WinRow *row = rowFor(windowId);
    if (row == nullptr) return;
    for (i32 i = 0; i < WinRow_count(row); i++) {
        const KeyHandler *l = WinRow_items(row)[i];
        if (KeyHandler_onCharTyped(l))
            KeyHandler_onCharTyped(l)(KeyHandler_self(l), c);
    }
}

fn Key_dispatchEvents() {
    if (!s_ready) return;

    InputEvent ev;
    while (RingBuffer_pop(&s_queue, &ev)) {
        u64 packed = ev.packed;
        i32 action = (i32)(packed & 0x3);
        u64 exactNanos = NanoTime_startNanos()
                            + (((packed >> 18) & 0x3FFFFFFFFFFFULL) * 1000ULL);

        if (action == 3) { // char event
            u32 c = (u32)((packed >> 2) & 0xFFFF);
            for (i32 i = 0; i < s_listenerCount; i++)
                if (KeyHandler_onCharTyped(s_listeners[i]))
                    KeyHandler_onCharTyped(s_listeners[i])(KeyHandler_self(s_listeners[i]), c);
            if (ev.windowId == FOCUS_BROADCAST) {
                for (i32 r = 0; r < s_rowCount; r++)
                    deliverChar(s_rows[r].windowId, c);
            } else {
                deliverChar(ev.windowId, c);
            }
            continue;
        }

        i32 keyCode = (i32)((packed >> 2) & 0xFFF);
        i32 modifiers = (i32)((packed >> 14) & 0xF);

        i32 mappedMods = 0;
        if ((modifiers & 1) != 0) mappedMods |= KEY_MOD_SHIFT;
        if ((modifiers & 2) != 0) mappedMods |= KEY_MOD_CONTROL;
        if ((modifiers & 4) != 0) mappedMods |= KEY_MOD_OPTION;
        if ((modifiers & 8) != 0) mappedMods |= KEY_MOD_COMMAND;
        i32 keyEvent = keyCode | mappedMods;

        // Global taps hear everything; window listeners only their own
        // window (or broadcast).
        for (i32 i = 0; i < s_listenerCount; i++) {
            any self = KeyHandler_self(s_listeners[i]);
            if (action == KEY_ACTION_DOWN && KeyHandler_onKeyDown(s_listeners[i]))
                KeyHandler_onKeyDown(s_listeners[i])(self, keyEvent, exactNanos);
            else if (action == KEY_ACTION_UP && KeyHandler_onKeyUp(s_listeners[i]))
                KeyHandler_onKeyUp(s_listeners[i])(self, keyEvent, exactNanos);
            else if (action == KEY_ACTION_REPEAT && KeyHandler_onKeyRepeat(s_listeners[i]))
                KeyHandler_onKeyRepeat(s_listeners[i])(self, keyEvent, exactNanos);
        }
        if (ev.windowId == FOCUS_BROADCAST) {
            for (i32 r = 0; r < s_rowCount; r++)
                deliverToWindow(s_rows[r].windowId, action, keyEvent, exactNanos);
        } else {
            deliverToWindow(ev.windowId, action, keyEvent, exactNanos);
        }
    }
}

bool Key_isDown(i32 keyCode) {
    if (keyCode < 0 || keyCode >= KEY_COUNT) return false;
    return s_slots[keyCode].pressTime != 0;
}

u64 Key_pressTime(i32 keyCode) {
    if (keyCode < 0 || keyCode >= KEY_COUNT) return 0;
    return s_slots[keyCode].pressTime;
}

u64 Key_lastReleaseTime(i32 keyCode) {
    if (keyCode < 0 || keyCode >= KEY_COUNT) return 0;
    return s_slots[keyCode].lastReleaseTime;
}

u64 Key_lastHoldDurationNanos(i32 keyCode) {
    if (keyCode < 0 || keyCode >= KEY_COUNT) return 0;
    return s_slots[keyCode].lastHoldDuration;
}

u64 Key_currentHoldDurationNanos(i32 keyCode) {
    u64 p = Key_pressTime(keyCode);
    return p == 0 ? 0 : NanoTime_now() - p;
}

u64 Key_holdDurationNanos(i32 keyCode) {
    u64 p = Key_pressTime(keyCode);
    return p != 0 ? NanoTime_now() - p : Key_lastHoldDurationNanos(keyCode);
}

u64 Key_durationSinceReleaseNanos(i32 keyCode) {
    u64 r = Key_lastReleaseTime(keyCode);
    return r == 0 ? 0 : NanoTime_now() - r;
}

i32 Key_taps(i32 keyCode) {
    if (keyCode < 0 || keyCode >= KEY_COUNT) return 0;
    return s_slots[keyCode].taps;
}

fn Key_resetTaps(i32 keyCode) {
    if (keyCode < 0 || keyCode >= KEY_COUNT) return;
    s_slots[keyCode].taps = 0;
}

KeyTapPhase Key_tapPhase(i32 keyCode) {
    if (keyCode < 0 || keyCode >= KEY_COUNT) return KEY_TAP_NONE;
    const KeySlot *slot = &s_slots[keyCode];
    if (KeySlot_taps(slot) == 0)
        return KEY_TAP_NONE;
    if (NanoTime_now() < KeySlot_pendingUntil(slot))
        return KEY_TAP_PENDING;
    if (KeySlot_taps(slot) >= 3)
        return KEY_TAP_TRIPLE;
    if (KeySlot_taps(slot) == 2)
        return KEY_TAP_f64;
    return KEY_TAP_SINGLE;
}

bool Key_isLongPressFired(i32 keyCode) {
    if (keyCode < 0 || keyCode >= KEY_COUNT) return false;
    return s_slots[keyCode].longPressFired != 0;
}

fn Key_setLongPressFired(i32 keyCode, bool fired) {
    if (keyCode < 0 || keyCode >= KEY_COUNT) return;
    s_slots[keyCode].longPressFired = (u8)(fired ? 1 : 0);
}

i32 Key_code(i32 keyEvent) {
    return keyEvent & KEY_MASK_CODE;
}

bool Key_hasShift(i32 keyEvent) {
    return (keyEvent & KEY_MOD_SHIFT) != 0;
}

bool Key_hasControl(i32 keyEvent) {
    return (keyEvent & KEY_MOD_CONTROL) != 0;
}

bool Key_hasOption(i32 keyEvent) {
    return (keyEvent & KEY_MOD_OPTION) != 0;
}

bool Key_hasCommand(i32 keyEvent) {
    return (keyEvent & KEY_MOD_COMMAND) != 0;
}

const char *Key_name(i32 keyCode) {
    if (keyCode < 0 || keyCode >= KEY_COUNT) return "";
    const char *name = s_names[keyCode];
    return name ? name : "";
}
