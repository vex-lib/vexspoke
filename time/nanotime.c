// time/nanotime.c — the monotonic clock (Legacy: time/NanoTime.java).
//
// CLOCK_MONOTONIC_RAW is the raw hardware counter, unslewed by NTP — the
// closest thing to System.nanoTime(). The epoch is captured once; every
// consumer derives "time since engine start" from the same anchor so
// timestamps from different threads stay comparable.

#include "vex.h"
#include "time/nanotime.h"

#include <time.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Nanotime
 * ============================================================================
 * The monotonic clock (Legacy: time/NanoTime.java). CLOCK_MONOTONIC_RAW is
 * the raw hardware counter, unslewed by NTP — the closest thing to
 * System.nanoTime(). The epoch is captured once by NanoTime_init; every
 * consumer derives "time since engine start" from the same anchor so
 * timestamps from different threads stay comparable. NanoTimer is a
 * caller-owned tickable timer accumulating scaled delta and total time.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Nanotime (time/nanotime.c)
 * ============================================================================
 * the monotonic clock (Legacy: time/NanoTime.java).
 *
 * STRUCT FIELDS (Mirroring time/nanotime.h):
 * ----------------------------------------------------------------------------
 *   NanoTimer {
 *     u64 startNanos; // nanosecond timestamp
 *     u64 lastNanos; // nanosecond timestamp
 *     u64 currentNanos; // nanosecond timestamp
 *     d64 deltaTime; // seconds, Clock-scaled
 *     d64 totalTime; // seconds, accumulated scaled
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - NanoTime_init()
 *
 * Core Functions:
 *   - NanoTime_now()
 *   - NanoTime_startNanos()
 *   - NanoTime_elapsedNanos()
 *   - NanoTimer_reset(timer)
 *   - NanoTimer_tick(timer)
 *   - NanoTimer_tickWithClock(timer, clock)
 *   - NanoTimer_deltaTime(timer)
 *   - NanoTimer_totalTime(timer)
 *   - NanoTimer_deltaNanos(timer)
 *   - NanoTimer_elapsedNanosOf(timer)
 * ============================================================================
 */



;;GETTER
static inline u64 NanoTimer_currentNanos(const NanoTimer *s) { return (*s).currentNanos; }
;;GETTER
static inline u64 *NanoTimer_currentNanosMut(NanoTimer *s) { return &(*s).currentNanos; }
;;SETTER
static inline fn NanoTimer_setCurrentNanos(NanoTimer *s, u64 v) { (*s).currentNanos = v; }
;;GETTER
static inline d64 NanoTimer_deltaTime(const NanoTimer *s) { return (*s).deltaTime; }
;;GETTER
static inline d64 *NanoTimer_deltaTimeMut(NanoTimer *s) { return &(*s).deltaTime; }
;;SETTER
static inline fn NanoTimer_setDeltaTime(NanoTimer *s, d64 v) { (*s).deltaTime = v; }
;;GETTER
static inline u64 NanoTimer_lastNanos(const NanoTimer *s) { return (*s).lastNanos; }
;;GETTER
static inline u64 *NanoTimer_lastNanosMut(NanoTimer *s) { return &(*s).lastNanos; }
;;SETTER
static inline fn NanoTimer_setLastNanos(NanoTimer *s, u64 v) { (*s).lastNanos = v; }
;;GETTER
static inline u64 NanoTimer_startNanos(const NanoTimer *s) { return (*s).startNanos; }
;;GETTER
static inline u64 *NanoTimer_startNanosMut(NanoTimer *s) { return &(*s).startNanos; }
;;SETTER
static inline fn NanoTimer_setStartNanos(NanoTimer *s, u64 v) { (*s).startNanos = v; }
;;GETTER
static inline d64 NanoTimer_totalTime(const NanoTimer *s) { return (*s).totalTime; }
;;GETTER
static inline d64 *NanoTimer_totalTimeMut(NanoTimer *s) { return &(*s).totalTime; }
;;SETTER
static inline fn NanoTimer_setTotalTime(NanoTimer *s, d64 v) { (*s).totalTime = v; }

// little conflicted.. would need this to be localthread variable to ensure thread safety
static u64 s_startNanos = 0;
static i32 s_started = 0;

static u64 nanoTime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (u64)ts.tv_sec * 1000000000ULL + (u64)ts.tv_nsec;
}

fn NanoTime_init() {
    if (s_started) return;
    s_startNanos = nanoTime();
    s_started = 1;
}

u64 NanoTime_startNanos() {
    return s_startNanos;
}

// Raw reading only — pure function of the clock, no shared state involved
// (the init-race concern lives in the epoch accessors below, not here).
u64 NanoTime_now() {
    return nanoTime();
}

u64 NanoTime_elapsedNanos() {
    if (!s_started)
        return 0;
    return nanoTime() - s_startNanos;
}

// --- Tickable timer (Legacy: NanoTime.allocate/tick) ---

fn NanoTimer_reset(NanoTimer *timer) {
    u64 now = nanoTime();
    NanoTimer_setStartNanos(timer, now);
    NanoTimer_setLastNanos(timer, now);
    NanoTimer_setCurrentNanos(timer, now);
    NanoTimer_setDeltaTime(timer, 0.0);
    NanoTimer_setTotalTime(timer, 0.0);
}

fn NanoTimer_tickWithClock(NanoTimer *timer, const Clock *clock) {
    u64 now = nanoTime();
    u64 current = NanoTimer_currentNanos(timer);

    NanoTimer_setLastNanos(timer, current);
    NanoTimer_setCurrentNanos(timer, now);

    d64 realDeltaSec = (d64)(now - current) / 1000000000.0;

    d64 scale = 1.0;
    if (clock) {
        if (Clock_isPaused(clock))
            scale = 0.0;
        else
            scale = Clock_timeScale(clock);
    }

    d64 scaledDeltaSec = realDeltaSec * scale;
    NanoTimer_setDeltaTime(timer, scaledDeltaSec);
    NanoTimer_setTotalTime(timer, NanoTimer_totalTime(timer) + (scaledDeltaSec));
}

fn NanoTimer_tick(NanoTimer *timer) {
    NanoTimer_tickWithClock(timer, nullptr);
}

d64 NanoTimer_deltaTime(const NanoTimer *timer) {
    return NanoTimer_deltaTime(timer);
}

d64 NanoTimer_totalTime(const NanoTimer *timer) {
    return NanoTimer_totalTime(timer);
}

u64 NanoTimer_deltaNanos(const NanoTimer *timer) {
    return NanoTimer_currentNanos(timer) - NanoTimer_lastNanos(timer);
}

u64 NanoTimer_elapsedNanosOf(const NanoTimer *timer) {
    return NanoTimer_currentNanos(timer) - NanoTimer_startNanos(timer);
}
