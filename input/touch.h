#ifndef INPUT_TOUCH_H
#define INPUT_TOUCH_H

#include "vex.h"

#include "event/touchhandler.h"

// Window id carried by every queued event (0 = FOCUS_BROADCAST). windowId is
// an opaque OS tag, never a slot index: scoped registries grow on demand (the
// Dynamic Scalability & Anti-Hardcoding Law), so any id attaches.

// input/touch.h — trackpad touch state + event stream (Legacy: input/Touch.java).
//
// Ten fixed touch slots (state + position/pressure), a packed 64-bit ring,
// and listener callbacks. The backend maps OS touch identities into slots
// (hash % TOUCH_MAX); all per-touch queries index those slots. TOUCH_MAX is a
// hardware touch-domain bound, not a workload ceiling.
//
// Wire format (identical to legacy):
//   [63:18] micros since engine start | [17: 6] touch slot | [5:0] action

enum {
    TOUCH_DOWN   = 0,
    TOUCH_UP     = 1,
    TOUCH_MOVE   = 2,
    TOUCH_CANCEL = 3,
};

#define TOUCH_MAX 10

void Touch_init(void);
void Touch_shutdown(void);
void Touch_addListener(const TouchHandler *listener);
bool Touch_removeListener(const TouchHandler *listener);

// Window-scoped registration (see input/key.h for the routing rules).
void Touch_attachWindow(u32 windowId, const TouchHandler *listener);
bool Touch_detachWindow(u32 windowId, const TouchHandler *listener);
void Touch_detachWindowAll(u32 windowId);

// Producer: Thread 0 only. touchId is the slot (0..TOUCH_MAX-1); windowId
// tags the receiving window.
void Touch_pushTouchEvent(u32 windowId, int touchId, int action, d64 x, d64 y,
                          d64 pressure, u64 holdThresholdNanos);

// Consumer: drain the ring into the listeners. Game thread, once per frame.
void Touch_dispatchEvents(void);

// --- State queries ---
bool    Touch_isDown(int touchId);
u64 Touch_pressTime(int touchId);
u64 Touch_lastHoldDuration(int touchId);
int     Touch_taps(int touchId);
int     Touch_lastAction(int touchId);
d64  Touch_x(int touchId);
d64  Touch_y(int touchId);
d64  Touch_pressure(int touchId);

#endif
