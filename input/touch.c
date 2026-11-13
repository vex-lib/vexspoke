// input/touch.c — trackpad touch state + event stream (Legacy: input/Touch.java port).
//
// Ten fixed touch slots, each 32 Bytes of state (press/release/hold/taps/action)
// plus 32 Bytes of geometry (x/y/pressure). The backend resolves OS touch
// identities into slots before pushing; dispatch reconstructs timestamps and
// fans _out to listeners on the game thread. Listener registries grow
// exponentially (the Dynamic Scalability & Anti-Hardcoding Law): window scope
// is a row keyed by the opaque OS window id, arena-backed, doubling on demand.

#include "vex.h"

#include "input/touch.h"

#include "input/focus.h"
#include "struct/ring.h"
#include "time/nanotime.h"
#include "nio/mem.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Touch
 * ============================================================================
 * Trackpad touch state + event stream (Legacy: input/Touch.java): ten fixed
 * 32-byte touch slots (press/release/hold/taps/action) plus 32 Bytes of
 * geometry (x/y/pressure) per contact, with a listener registry keyed by
 * opaque OS window id. Exists because the backend must resolve OS touch
 * identities into stable slots before dispatch, and dispatch must fan out to
 * listeners on the game thread without allocation. memory: slot arrays are
 * fixed-size; listener registries are arena-backed and d64 exponentially
 * (the Dynamic Scalability & Anti-Hardcoding Law). Lifetime:
 * Touch_init/Touch_shutdown; window rows attach/detach per window lifecycle.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Touch (input/touch.c)
 * ============================================================================
 * trackpad touch state + event stream (Legacy: input/Touch.java).
 *
 * STRUCT FIELDS (local to this file):
 * ----------------------------------------------------------------------------
 *   InputEvent {
 *     u64 packed;           // legacy 64-bit touch event packing
 *     u32 windowId;         // target window (0 = broadcast)
 *     u32 pad;              // alignment padding
 *   }
 *   TouchSlot {
 *     u64 pressTime;        // touchdown timestamp
 *     u64 lastHoldDuration; // active-contact duration
 *     u64 lastReleaseTime;  // lift timestamp
 *     i32 taps;              // tap-count state
 *     i32 action;            // current touch action
 *   }
 *   TouchPos {
 *     d64 x;                  // contact X position
 *     d64 y;                  // contact Y position
 *     d64 pressure;           // contact pressure
 *     u64 pad;              // alignment padding
 *   }
 *
 * PRIVATE HELPERS (kept file-local pure-data only, each with full fields):
 * ----------------------------------------------------------------------------
 *   WinRow {
 *     u32 windowId;         // opaque OS window tag (0 never attached)
 *     const TouchHandler **items; // arena-allocated listener segment, doubling
 *     int count;                 // live listener count
 *     int cap;                   // allocated segment capacity
 *   }
 *   (growSegment / growRows / rowFor: static behavior, zero struct fields)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Touch_init(void)
 *
 * Core Functions:
 *   - Touch_shutdown(void)
 *   - Touch_addListener(listener)
 *   - Touch_removeListener(listener)
 *   - Touch_attachWindow(windowId, listener)
 *   - Touch_detachWindow(windowId, listener)
 *   - Touch_detachWindowAll(windowId)
 *   - Touch_pushTouchEvent(windowId, touchId, action, x, y, pressure, holdThresholdNanos)
 *   - Touch_dispatchEvents(void)
 *   - Touch_pressTime(touchId)
 *   - Touch_lastHoldDuration(touchId)
 *   - Touch_taps(touchId)
 *   - Touch_lastAction(touchId)
 *   - Touch_x(touchId)
 *   - Touch_y(touchId)
 *   - Touch_pressure(touchId)
 *
 * Getters:
 *   - Touch_isDown(touchId)
 * ============================================================================
 */


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
    u64 lastHoldDuration;
    u64 lastReleaseTime;
    i32 taps;
    i32 action;
} TouchSlot;

_Static_assert(sizeof(TouchSlot) == 32, "touch slot must stay 32 Bytes");

typedef struct {
    d64 x;
    d64 y;
    d64 pressure;
    u64 pad;
} TouchPos;

_Static_assert(sizeof(TouchPos) == 32, "touch pos must stay 32 Bytes");

static TouchSlot s_slots[TOUCH_MAX];
;;INTENTION("fixed TOUCH_MAX: the trackpad touch domain is a hardware bound (contact identities map into slots), not a workload ceiling")
static TouchPos s_pos[TOUCH_MAX];
static RingBuffer s_queue;
static bool s_ready = false;

// --- PRIVATE HELPERS: growable listener registries ---
// The Dynamic Scalability & Anti-Hardcoding Law: registries start at zero
// allocations and d64 exponentially, arena-backed. windowId is an opaque
// OS tag (0 = FOCUS_BROADCAST, reserved, never attached); broadcast events
// fan _out to every attached row.
typedef struct WinRow {
    u32 windowId;
    const TouchHandler **items;  // arena-allocated segment, doubling
    int count;
    int cap;
} WinRow;

static const TouchHandler **s_listeners = nullptr;  // global listener segment
static int s_listenerCount = 0;
static int s_listenerCap = 0;

static WinRow *s_rows = nullptr;  // growable window-row table
static int s_rowCount = 0;
static int s_rowCap = 0;

// Grow a handler segment to at least `needed` slots. On OOM the segment is
// left untouched and the registration is silently dropped (legacy parity).
// Row structs move when the row TABLE grows, so their ITEM segments are
// separate arena blocks: item pointers survive, row pointers do not — always
// re-derive rows through rowFor() after any attach/detach.
static bool growSegment(const TouchHandler ***items, int *cap, int needed) {
    if (needed <= *cap) return true;
    int newCap = (*cap == 0) ? 16 : *cap * 2;
    while (newCap < needed) newCap *= 2;
    const TouchHandler **nb = (const TouchHandler**) Memory_alloc(
        TYPE_INT_POINTER, (bytes) newCap * sizeof(TouchHandler*));
    if (nb == nullptr) return false;
    if (*items != nullptr && *cap > 0)
        memcpy(nb, *items, (bytes) *cap * sizeof(TouchHandler*));
    *items = nb;
    *cap = newCap;
    return true;
}

// Grow the window-row table to at least `needed` rows (doubling, cold 8).
static bool growRows(int needed) {
    if (needed <= s_rowCap) return true;
    int newCap = (s_rowCap == 0) ? 8 : s_rowCap * 2;
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
    for (int i = 0; i < s_rowCount; i++)
        if (s_rows[i].windowId == windowId)
            return &s_rows[i];
    return nullptr;
}

void Touch_init(void) {
    if (s_ready) return;
    NanoTime_init();
    s_queue = RingBuffer(sizeof(InputEvent), QUEUE_CAPACITY);
    if (s_queue.slots == nullptr) return;
    memset(s_slots, 0, sizeof(s_slots));
    memset(s_pos, 0, sizeof(s_pos));
    s_ready = true;
}

void Touch_shutdown(void) {
    if (!s_ready) return;
    RingBuffer_shutdown(&s_queue);
    s_ready = false;
}

void Touch_addListener(const TouchHandler *listener) {
    if (listener == nullptr) return;
    if (!growSegment(&s_listeners, &s_listenerCap, s_listenerCount + 1)) return;
    s_listeners[s_listenerCount++] = listener;
}

bool Touch_removeListener(const TouchHandler *listener) {
    for (int i = 0; i < s_listenerCount; i++) {
        if (s_listeners[i] == listener) {
            s_listeners[i] = s_listeners[--s_listenerCount];
            return true;
        }
    }
    return false;
}

void Touch_attachWindow(u32 windowId, const TouchHandler *listener) {
    if (listener == nullptr || windowId == 0) return;
    WinRow *row = rowFor(windowId);
    if (row == nullptr) {
        if (!growRows(s_rowCount + 1)) return;
        row = &s_rows[s_rowCount++];
        (*row).windowId = windowId;
        (*row).items = nullptr;
        (*row).count = 0;
        (*row).cap = 0;
    }
    if (!growSegment(&(*row).items, &(*row).cap, (*row).count + 1)) return;
    (*row).items[(*row).count++] = listener;
}

bool Touch_detachWindow(u32 windowId, const TouchHandler *listener) {
    if (listener == nullptr || windowId == 0) return false;
    WinRow *row = rowFor(windowId);
    if (row == nullptr) return false;
    for (int i = 0; i < (*row).count; i++) {
        if ((*row).items[i] == listener) {
            (*row).count--;
            (*row).items[i] = (*row).items[(*row).count];
            return true;
        }
    }
    return false;
}

void Touch_detachWindowAll(u32 windowId) {
    if (windowId == 0) return;
    WinRow *row = rowFor(windowId);
    if (row != nullptr)
        (*row).count = 0;
}

void Touch_pushTouchEvent(u32 windowId, int touchId, int action, d64 x, d64 y,
                          d64 pressure, u64 holdThresholdNanos) {
    if (touchId < 0 || touchId >= TOUCH_MAX) return;
    if (!s_ready) Touch_init();
    if (!s_ready) return;

    TouchSlot *slot = &s_slots[touchId];
    TouchPos *pos = &s_pos[touchId];
    u64 now = NanoTime_now();

    (*pos).x = x;
    (*pos).y = y;
    (*pos).pressure = pressure;

    if (action == TOUCH_DOWN) {
        u64 lastRelease = (*slot).lastReleaseTime;
        if (lastRelease != 0 && (now - lastRelease) < holdThresholdNanos)
            (*slot).taps++;
        else
            (*slot).taps = 1;
        (*slot).pressTime = now;
    } else if (action == TOUCH_UP) {
        if ((*slot).pressTime != 0)
            (*slot).lastHoldDuration = now - (*slot).pressTime;
        (*slot).lastReleaseTime = now;
        (*slot).pressTime = 0;
    } else if (action == TOUCH_CANCEL) {
        (*slot).pressTime = 0;
    }
    (*slot).action = action;

    u64 micros = (NanoTime_elapsedNanos() / 1000ULL) & 0x3FFFFFFFFFFFULL;
    InputEvent ev = { .packed = (micros << 18)
                              | (((u64)touchId & 0xFFF) << 6)
                              | ((u64)action & 0x3F),
                      .windowId = windowId, .pad = 0 };
    RingBuffer_push(&s_queue, &ev);
}

// Deliver one touch event to one window's scoped list.
static void deliverTouch(u32 windowId, int action, int touchId,
                         d64 x, d64 y, d64 pressure, u64 exactNanos) {
    if (windowId == 0) return;
    WinRow *row = rowFor(windowId);
    if (row == nullptr) return;
    for (int i = 0; i < (*row).count; i++) {
        const TouchHandler *l = (*row).items[i];
        any self = (*l).self;
        if (action == TOUCH_DOWN && (*l).onTouchDown)
            (*l).onTouchDown(self, touchId, x, y, pressure, exactNanos);
        else if (action == TOUCH_UP && (*l).onTouchUp)
            (*l).onTouchUp(self, touchId, x, y, pressure, exactNanos);
        else if (action == TOUCH_MOVE && (*l).onTouchMove)
            (*l).onTouchMove(self, touchId, x, y, pressure, exactNanos);
        else if (action == TOUCH_CANCEL && (*l).onTouchCancel)
            (*l).onTouchCancel(self, touchId, exactNanos);
    }
}

void Touch_dispatchEvents(void) {
    if (!s_ready) return;

    InputEvent ev;
    while (RingBuffer_pop(&s_queue, &ev)) {
        u64 packed = ev.packed;
        u64 micros = (packed >> 18) & 0x3FFFFFFFFFFFULL;
        u64 exactNanos = NanoTime_startNanos() + (micros * 1000ULL);
        int touchId = (int)((packed >> 6) & 0xFFF);
        int action = (int)(packed & 0x3F);

        const TouchPos *pos = &s_pos[touchId];
        d64 x = (*pos).x;
        d64 y = (*pos).y;
        d64 pressure = (*pos).pressure;

        for (int i = 0; i < s_listenerCount; i++) {
            any self = (*s_listeners[i]).self;
            if (action == TOUCH_DOWN && (*s_listeners[i]).onTouchDown)
                (*s_listeners[i]).onTouchDown(self, touchId, x, y, pressure, exactNanos);
            else if (action == TOUCH_UP && (*s_listeners[i]).onTouchUp)
                (*s_listeners[i]).onTouchUp(self, touchId, x, y, pressure, exactNanos);
            else if (action == TOUCH_MOVE && (*s_listeners[i]).onTouchMove)
                (*s_listeners[i]).onTouchMove(self, touchId, x, y, pressure, exactNanos);
            else if (action == TOUCH_CANCEL && (*s_listeners[i]).onTouchCancel)
                (*s_listeners[i]).onTouchCancel(self, touchId, exactNanos);
        }
        if (ev.windowId == FOCUS_BROADCAST) {
            for (int r = 0; r < s_rowCount; r++)
                deliverTouch(s_rows[r].windowId, action, touchId, x, y, pressure, exactNanos);
        } else {
            deliverTouch(ev.windowId, action, touchId, x, y, pressure, exactNanos);
        }
    }
}

bool Touch_isDown(int touchId) {
    if (touchId < 0 || touchId >= TOUCH_MAX) return false;
    return s_slots[touchId].pressTime != 0;
}

u64 Touch_pressTime(int touchId) {
    if (touchId < 0 || touchId >= TOUCH_MAX) return 0;
    return s_slots[touchId].pressTime;
}

u64 Touch_lastHoldDuration(int touchId) {
    if (touchId < 0 || touchId >= TOUCH_MAX) return 0;
    return s_slots[touchId].lastHoldDuration;
}

int Touch_taps(int touchId) {
    if (touchId < 0 || touchId >= TOUCH_MAX) return 0;
    return s_slots[touchId].taps;
}

int Touch_lastAction(int touchId) {
    if (touchId < 0 || touchId >= TOUCH_MAX) return TOUCH_CANCEL;
    return s_slots[touchId].action;
}

d64 Touch_x(int touchId) {
    if (touchId < 0 || touchId >= TOUCH_MAX) return 0.0;
    return s_pos[touchId].x;
}

d64 Touch_y(int touchId) {
    if (touchId < 0 || touchId >= TOUCH_MAX) return 0.0;
    return s_pos[touchId].y;
}

d64 Touch_pressure(int touchId) {
    if (touchId < 0 || touchId >= TOUCH_MAX) return 0.0;
    return s_pos[touchId].pressure;
}
