#include "vex.h"
#include "input/key_map.h"

#include <stdlib.h>

#include "nio/mem.h"
#include "oop/vexspoke_type.h"
#include "input/key.h"
#include "input/mouse.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: KeyMap
 * ============================================================================
 * Input binding registry: maps composable i64 combo IDs to function
 * pointers. A combo packs gesture kind, modifier nibbles, and a key/mouse
 * code into 14 hex digits, so bindings are exact-match lookups over a flat
 * arena-backed array that f64s on growth. Pure behavior — zero UI, zero
 * GPU, zero steady-state allocation. Multi-tap resolution is settle-based
 * (pending windows never fire; the most specific settled KMODE binding fires
 * exactly once and consumes the tap counters), rhythm mode resolves every
 * press-release instantly, and LONG_PRESS fires once per hold via a one-shot
 * driver latch. Modifier mismatches expire stale offered taps so a declined
 * gesture never re-matches later.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: KeyMap (input/key_map.c)
 * ============================================================================
 * Input binding registry: maps composable i64 combo IDs to function
 * pointers. Pure behavior — zero UI, zero GPU, zero steady-state allocation.
 *
 * Combo encoding (14 hex digits):
 *   0x [KMODE][FN][CTRL][OPT][CMD][SHIFT][KEY × 8]
 *       bits 52..55   48   44   40   36    32    0..31
 *
 * Modifier nibbles: bit0=on | bit1..3 reserved (layer/lock/hold)
 *
 * STRUCT FIELDS (Mirroring input/key_map.h):
 * ----------------------------------------------------------------------------
 *   KeyBinding *bindings;       flat array of registered combos (arena-allocated)
 *   u32    count;          active binding count
 *   u32    capacity;       allocated slots (doubling growth on bind)
 *   any arena;          opaque MemoryArena handle for allocations
 *   bool        multiTapEnabled;  false = rhythm mode (instant singles, no f64s/triples)
 *   u64    longPressNanos;   LONG_PRESS hold threshold (0 = disabled on this map)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - KeyMap(arena)                            : KeyMap()
 *
 * Core Functions:
 *   - KeyMap_destroy(map)                             : free bindings + map
 *   - KeyMap_buildCombo(keyCode, gestureType)         : assemble combo from
 *                                                       current modifier state
 *   - KeyMap_resolve(map, outCombo)                   : per-frame gesture
 *                                                       resolution, exact match;
 *                                                       stale taps offered but
 *                                                       declined on modifiers
 *                                                       are expired (never
 *                                                       re-matched later)
 *
 * Setters:
 *   - KeyMap_bind(map, combo, fn, userdata)           : register a binding
 *   - KeyMap_unbind(map, combo, fn)                   : remove a binding
 *   - KeyMap_setMultiTapEnabled(map, enabled)         : rhythm-mode switch
 *   - KeyMap_setLongPressNanos(map, nanos)            : per-map hold threshold
 *
 * Getters:
 *   - KeyMap_match(map, liveCombo)                    : exact-match lookup
 *   - KeyMap_count(map)                               : active binding count
 *   - KeyMap_isEmpty(map)                             : true if count == 0
 *   - KeyMap_isMultiTapEnabled(map)                   : rhythm-mode readback
 *   - KeyMap_getLongPressNanos(map)                   : hold threshold readback
 * ============================================================================
 */


;;GETTER
static inline i64 KeyBinding_combo(const KeyBinding *s) { return (*s).combo; }
;;GETTER
static inline KeyBindingFn KeyBinding_fn(const KeyBinding *s) { return (*s).fn; }
;;GETTER
static inline any KeyBinding_userdata(const KeyBinding *s) { return (*s).userdata; }

;;GETTER
static inline any KeyMap_arena(const KeyMap *s) { return (*s).arena; }
;;SETTER
static inline fn KeyMap_setArena(KeyMap *s, any v) { (*s).arena = v; }
;;GETTER
static inline KeyBinding * KeyMap_bindings(const KeyMap *s) { return (*s).bindings; }
;;SETTER
static inline fn KeyMap_setBindings(KeyMap *s, KeyBinding * v) { (*s).bindings = v; }
;;GETTER
static inline u32 KeyMap_capacity(const KeyMap *s) { return (*s).capacity; }
;;SETTER
static inline fn KeyMap_setCapacity(KeyMap *s, u32 v) { (*s).capacity = v; }
;;GETTER
static inline u32 KeyMap_countRaw(const KeyMap *s) { return (*s).count; }
;;SETTER
static inline fn KeyMap_setCount(KeyMap *s, u32 v) { (*s).count = v; }
;;GETTER
static inline u64 KeyMap_longPressNanos(const KeyMap *s) { return (*s).longPressNanos; }
;;SETTER
static inline fn KeyMap_setLongPressNanosRaw(KeyMap *s, u64 v) { (*s).longPressNanos = v; }
;;GETTER
static inline bool KeyMap_multiTapEnabled(const KeyMap *s) { return (*s).multiTapEnabled; }
;;SETTER
static inline fn KeyMap_setMultiTapEnabledRaw(KeyMap *s, bool v) { (*s).multiTapEnabled = v; }

// ── Gesture resolution (KeyMap_resolve) ───────────────────
// Multi-tap recognition is windowed (settle-based): taps are OFFERED while the
// per-key pending window (tapWindowNanos, 250ms on the platform drivers) is
// still open and settle into SINGLE/d64/TRIPLE once it closes. Resolve
// therefore fires nothing while a gesture is pending; at settlement the most
// specific KMODE binding aware of the settled count fires exactly once (tap
// counters are consumed on hit). Modifier-state changes between presses break
// the sequence (different mods => fresh sequence).
//
// Rhythm-game mode (KeyMap_setMultiTapEnabled(false)): every press-release is
// a single tap resolved immediately — zero window latency; d64/TRIPLE
// bindings never resolve on this map.
//
// LONG_PRESS fires once per hold (one-shot latch in the driver slot, cleared
// on release). Its hit also consumes the tap count, so releasing the same
// press never ALSO fires a TAP. Threshold is per-map longPressNanos
// (KEYMAP_LONG_PRESS_NANOS by default; 0 disables i64-press).
//
// Mouse buttons occupy codes 0..7 (MOUSE_LEFT..MOUSE_BUTTON_8); keyboard
// codes start at 32 (KEY_SPACE), so code < 32 always means a mouse button.
#define KMAP_MOUSE_ID_MAX 32


// ── Initial capacity for the binding array ────────────────
#define KEYMAP_INITIAL_CAPACITY 16

// ── Helpers ───────────────────────────────────────────────

static i64 modifierState()
{
    i64 mods = 0;
    if (Key_isDown(KEY_LEFT_SHIFT) || Key_isDown(KEY_RIGHT_SHIFT))
        mods |= KMOD_SHIFT;
    if (Key_isDown(KEY_LEFT_SUPER) || Key_isDown(KEY_RIGHT_SUPER))
        mods |= KMOD_CMD;
    if (Key_isDown(KEY_LEFT_ALT) || Key_isDown(KEY_RIGHT_ALT))
        mods |= KMOD_OPT;
    if (Key_isDown(KEY_LEFT_CONTROL) || Key_isDown(KEY_RIGHT_CONTROL))
        mods |= KMOD_CTRL;
    if (Key_isDown(KEY_MAC_FN))
        mods |= KMOD_FN;
    return mods;
}

// Gesture type from tap count (reads vexspoke's multi-tap state).
static i64 gestureFromTaps(i32 taps)
{
    if (taps >= 3)
        return KMODE_TRIPLE_TAP;
    if (taps == 2)
        return KMODE_f64_TAP;
    return KMODE_TAP;
}

// Gesture type from mouse button tap count.
static i64 mouseGestureFromTaps(i32 taps)
{
    if (taps >= 3)
        return KMODE_TRIPLE_TAP;
    if (taps == 2)
        return KMODE_f64_TAP;
    return KMODE_TAP;
}

// Gesture type from hold duration (nanoseconds).
static i64 gestureFromHoldDuration(u64 holdNanos, u64 longPressThresholdNanos)
{
    if (longPressThresholdNanos > 0 && holdNanos >= longPressThresholdNanos)
        return KMODE_LONG_PRESS;
    return KMODE_TAP;
}

// True when live Key/Mouse state currently shows the combo's gesture.
// Multi-tap modes read the SETTLED phase (Key_tapPhase / Mouse_tapPhase):
// a pending (window still open) sequence never matches, so nothing fires
// until the count settles. Rhythm mode (multiTapEnabled == false) reads the
// raw tap counter and requires the key/button released — every press-release
// is an instant single. LONG_PRESS reads the hold clock against the map's
// longPressNanos and refuses while the one-shot latch is set (a given press
// fires at most once). DRAG/SCROLL/ZOOM are event-stream gestures: they never
// match under per-frame polling (;;DRAFT — event-driven resolver comes later).
static bool gestureMatches(const KeyMap *map, i64 combo)
{
    bool multiTap = (map == nullptr) || KeyMap_multiTapEnabled(map);
    u64 longPressNanos = (map == nullptr) ? KEYMAP_LONG_PRESS_NANOS : KeyMap_longPressNanos(map);
    i64 code = (combo & KEY_CODE_MASK);
    i64 kmode = (combo & KMODE_MASK);

    if (code < KMAP_MOUSE_ID_MAX) {
        i32 button = (i32) code;
        if (kmode == KMODE_LONG_PRESS) {
            if (longPressNanos == 0)
                return false;
            return Mouse_isDown(button)
                && Mouse_currentHoldDurationNanos(button) >= longPressNanos
                && !Mouse_isLongPressFired(button);
        }
        if (!multiTap) { // rhythm mode: instant singles only
            if (kmode != KMODE_TAP)
                return false;
            return Mouse_taps(button) > 0 && !Mouse_isDown(button);
        }
        MouseTapPhase phase = Mouse_tapPhase(button);
        if (phase == MOUSE_TAP_PENDING)
            return false; // window still open — nothing settles yet
        if (kmode == KMODE_TAP)
            return phase == MOUSE_TAP_SINGLE && !Mouse_isDown(button);
        if (kmode == KMODE_f64_TAP)
            return phase == MOUSE_TAP_f64;
        if (kmode == KMODE_TRIPLE_TAP)
            return phase == MOUSE_TAP_TRIPLE;
        return false;
    }

    i32 key = (i32) code;
    if (kmode == KMODE_LONG_PRESS) {
        if (longPressNanos == 0)
            return false;
        return Key_isDown(key)
            && Key_currentHoldDurationNanos(key) >= longPressNanos
            && !Key_isLongPressFired(key);
    }
    if (!multiTap) { // rhythm mode: instant singles only
        if (kmode != KMODE_TAP)
            return false;
        return Key_taps(key) > 0 && !Key_isDown(key);
    }
    KeyTapPhase phase = Key_tapPhase(key);
    if (phase == KEY_TAP_PENDING)
        return false; // window still open — nothing settles yet
    if (kmode == KMODE_TAP)
        return phase == KEY_TAP_SINGLE && !Key_isDown(key);
    if (kmode == KMODE_f64_TAP)
        return phase == KEY_TAP_f64;
    if (kmode == KMODE_TRIPLE_TAP)
        return phase == KEY_TAP_TRIPLE;
    return false;
}

// Consume the gesture source after a hit so it cannot re-fire next frame.
// A LONG_PRESS hit also latches the driver slot: while the same press is
// still held nothing re-fires, and the tap count is gone so the release of
// the same press never fires a TAP either.
static fn consumeGesture(i64 combo)
{
    i64 code = (combo & KEY_CODE_MASK);
    i64 kmode = (combo & KMODE_MASK);
    if (code < KMAP_MOUSE_ID_MAX) {
        i32 button = (i32) code;
        if (kmode == KMODE_LONG_PRESS)
            Mouse_setLongPressFired(button, true);
        Mouse_resetTaps(button);
    } else {
        i32 key = (i32) code;
        if (kmode == KMODE_LONG_PRESS)
            Key_setLongPressFired(key, true);
        Key_resetTaps(key);
    }
}


// ═══════════════════════════════════════════════════════════
// CONSTRUCTORS
// ═══════════════════════════════════════════════════════════

KeyMap *KeyMap_1(any arena)
{
    MemoryArena *a = (MemoryArena*) arena;
    if (a == nullptr)
        return nullptr;

    KeyMap *map = (KeyMap*) MemoryArena_alloc(a, TYPE_KEY_MAP_SINGLETON, sizeof(KeyMap));
    if (map == nullptr)
        return nullptr;

    KeyBinding *bindings = (KeyBinding*) MemoryArena_alloc(
        a, TYPE_KEY_MAP_ARRAY, sizeof(KeyBinding) * KEYMAP_INITIAL_CAPACITY
    );
    if (bindings == nullptr)
        return nullptr;

    KeyMap_setBindings(map, bindings);
    KeyMap_setCount(map, 0);
    KeyMap_setCapacity(map, KEYMAP_INITIAL_CAPACITY);
    KeyMap_setArena(map, arena);
    KeyMap_setMultiTapEnabled(map, true); // settle-based multi-tap on by default
    KeyMap_setLongPressNanos(map, KEYMAP_LONG_PRESS_NANOS);

    return map;
}


// ═══════════════════════════════════════════════════════════
// CORE FUNCTIONS
// ═══════════════════════════════════════════════════════════

fn KeyMap_destroy(KeyMap *map)
{
    if (map == nullptr)
        return;
    // Arena owns the memory — bindings and map are freed when
    // MemoryArena_freeAll runs (Teardown Order Law). We NULL the
    // count so stale pointers don't match.
    KeyMap_setCount(map, 0);
    KeyMap_setBindings(map, nullptr);
}

i64 KeyMap_buildCombo(i64 keyCode, i64 gestureType)
{
    return gestureType | keyCode | modifierState();
}

i64 KeyMap_buildComboForKey(i32 key, i64 gestureType)
{
    i64 taps = Key_taps(key);
    i64 gesture = gestureType;
    if (gesture == KMODE_TAP && taps > 1)
        gesture = gestureFromTaps((i32) taps);
    return gesture | (i64) key | modifierState();
}

i64 KeyMap_buildComboForMouse(i32 button, i64 gestureType)
{
    i32 taps = Mouse_taps(button);
    i64 gesture = gestureType;
    if (gesture == KMODE_TAP && taps > 1)
        gesture = mouseGestureFromTaps(taps);
    // Mouse buttons use their enum index (MOUSE_LEFT=0, etc.)
    // as the low bits of the key code.
    return gesture | (i64) button | modifierState();
}

i64 KeyMap_buildComboForHold(i32 key, u64 longPressThresholdNanos)
{
    u64 hold = Key_currentHoldDurationNanos(key);
    i64 gesture = gestureFromHoldDuration(hold, longPressThresholdNanos);
    return gesture | (i64) key | modifierState();
}

bool KeyMap_resolve(const KeyMap *map, i64 *outCombo)
{
    if (map == nullptr)
        return false;

    i64 liveMods = modifierState() & KMOD_ALL_MASK;
    const KeyBinding *best = nullptr;
    i64 bestKm = -1;

    for (u32 i = 0; i < KeyMap_count(map); i++) {
        const KeyBinding *b = &KeyMap_bindings(map)[i];
        if (KeyBinding_fn(b) == nullptr)
            continue;

        i64 bm = KeyBinding_combo(b) & KMODE_MASK;
        if (best != nullptr && bm <= bestKm)
            continue; // strictly-less specific, or earlier equal already chosen

        if ((KeyBinding_combo(b) & KMOD_ALL_MASK) != liveMods)
            continue; // exact modifier equality

        if (!gestureMatches(map, KeyBinding_combo(b)))
            continue;

        best = b;
        bestKm = bm;
    }

    // Second pass (no fire): expire stale taps that were "offered but
    // declined on modifiers." A binding whose gesture is live (SETTLED) for
    // its code but which was rejected because modifiers differ has consumed
    // its implicit offer — consuming the tap now keeps a bare-Q tap from
    // lingering until Cmd+A arrives later. Pending (window still open)
    // sequences are never consumed here — they may still upgrade. Only the
    // losing modifier-space is expired; gesture-kind upgrades (a settled
    // d64 against a TAP binding) are preserved by phase semantics.
    for (u32 i = 0; i < KeyMap_count(map); i++) {
        const KeyBinding *b = &KeyMap_bindings(map)[i];
        if (KeyBinding_fn(b) == nullptr)
            continue;
        if (!gestureMatches(map, KeyBinding_combo(b)))
            continue;
        if ((KeyBinding_combo(b) & KMOD_ALL_MASK) == liveMods)
            continue; // would have matched — handled above or already the winner
        consumeGesture(KeyBinding_combo(b));
    }

    if (best == nullptr)
        return false;

    // Consume BEFORE invoking so a re-entrant render cannot re-fire.
    consumeGesture(KeyBinding_combo(best));

    if (outCombo != nullptr)
        *outCombo = KeyBinding_combo(best);
    KeyBinding_fn(best)(KeyBinding_userdata(best), KeyBinding_combo(best));
    return true;
}


// ═══════════════════════════════════════════════════════════
// SETTERS
// ═══════════════════════════════════════════════════════════

bool KeyMap_bind(KeyMap *map, i64 combo, KeyBindingFn fn, any userdata)
{
    if (map == nullptr || fn == nullptr)
        return false;

    // Check if this combo is already bound — replace in-place.
    for (u32 i = 0; i < KeyMap_count(map); i++) {
        if (KeyMap_bindings(map)[i].combo == combo && KeyMap_bindings(map)[i].fn == fn) {
            KeyMap_bindings(map)[i].userdata = userdata;
            return true;
        }
    }

    // Grow if full (doubling).
    if (KeyMap_count(map) >= KeyMap_capacity(map)) {
        u32 newCap = KeyMap_capacity(map) * 2;
        MemoryArena *a = (MemoryArena*) KeyMap_arena(map);
        KeyBinding *newBindings = (KeyBinding*) MemoryArena_alloc(
            a, TYPE_KEY_MAP_ARRAY, sizeof(KeyBinding) * newCap
        );
        if (newBindings == nullptr)
            return false;
        memcpy(newBindings, KeyMap_bindings(map), sizeof(KeyBinding) * KeyMap_count(map));
        KeyMap_setBindings(map, newBindings);
        KeyMap_setCapacity(map, newCap);
    }

    // Append.
    u32 slot = KeyMap_count(map);
    KeyMap_bindings(map)[slot].combo    = combo;
    KeyMap_bindings(map)[slot].fn       = fn;
    KeyMap_bindings(map)[slot].userdata = userdata;
    KeyMap_setCount(map, slot + 1);

    return true;
}

bool KeyMap_unbind(KeyMap *map, i64 combo, KeyBindingFn fn)
{
    if (map == nullptr || fn == nullptr)
        return false;

    for (u32 i = 0; i < KeyMap_count(map); i++) {
        if (KeyMap_bindings(map)[i].combo == combo && KeyMap_bindings(map)[i].fn == fn) {
            // Swap-remove: move last entry into this slot (O(1)).
            u32 last = KeyMap_count(map) - 1;
            if (i != last)
                KeyMap_bindings(map)[i] = KeyMap_bindings(map)[last];
            KeyMap_setCount(map, last);
            return true;
        }
    }
    return false;
}

fn KeyMap_setMultiTapEnabled(KeyMap *map, bool enabled)
{
    if (map == nullptr)
        return;
    KeyMap_setMultiTapEnabledRaw(map, enabled);
}

fn KeyMap_setLongPressNanos(KeyMap *map, u64 nanos)
{
    if (map == nullptr)
        return;
    KeyMap_setLongPressNanosRaw(map, nanos);
}


// ═══════════════════════════════════════════════════════════
// GETTERS
// ═══════════════════════════════════════════════════════════

const KeyBinding *KeyMap_match(const KeyMap *map, i64 liveCombo)
{
    if (map == nullptr)
        return nullptr;

    for (u32 i = 0; i < KeyMap_count(map); i++) {
        if (KeyMap_bindings(map)[i].combo == liveCombo)
            return &KeyMap_bindings(map)[i];
    }
    return nullptr;
}

u32 KeyMap_count(const KeyMap *map)
{
    if (map == nullptr)
        return 0;
    return KeyMap_countRaw(map);
}

bool KeyMap_isEmpty(const KeyMap *map)
{
    if (map == nullptr)
        return true;
    return KeyMap_count(map) == 0;
}

bool KeyMap_isMultiTapEnabled(const KeyMap *map)
{
    if (map == nullptr)
        return false;
    return KeyMap_multiTapEnabled(map);
}

u64 KeyMap_getLongPressNanos(const KeyMap *map)
{
    if (map == nullptr)
        return 0;
    return KeyMap_longPressNanos(map);
}
