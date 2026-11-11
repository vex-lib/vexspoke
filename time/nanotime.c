// time/nanotime.c — the monotonic clock (Legacy: time/NanoTime.java).
//
// CLOCK_MONOTONIC_RAW is the raw hardware counter, unslewed by NTP — the
// closest thing to System.nanoTime(). The epoch is captured once; every
// consumer derives "time since engine start" from the same anchor so
// timestamps from different threads stay comparable.

#include "time/nanotime.h"

#include <time.h>
#include "util/annotation.h"

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
 *   - NanoTime_init(void)
 *
 * Core Functions:
 *   - NanoTime_now(void)
 *   - NanoTime_startNanos(void)
 *   - NanoTime_elapsedNanos(void)
 *   - NanoTimer_reset(timer)
 *   - NanoTimer_tick(timer)
 *   - NanoTimer_tickWithClock(timer, clock)
 *   - NanoTimer_deltaTime(timer)
 *   - NanoTimer_totalTime(timer)
 *   - NanoTimer_deltaNanos(timer)
 *   - NanoTimer_elapsedNanosOf(timer)
 * ============================================================================
 */


// little conflicted.. would need this to be localthread variable to ensure thread safety
static u64 s_startNanos = 0;
static int s_started = 0;

static u64 nanoTime(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (u64)ts.tv_sec * 1000000000ULL + (u64)ts.tv_nsec;
}

void NanoTime_init(void) {
    if (s_started) return;
    s_startNanos = nanoTime();
    s_started = 1;
}

u64 NanoTime_startNanos(void) {
    return s_startNanos;
}

// Raw reading only — pure function of the clock, no shared state involved
// (the init-race concern lives in the epoch accessors below, not here).
u64 NanoTime_now(void) {
    return nanoTime();
}

u64 NanoTime_elapsedNanos(void) {
    if (!s_started)
        return 0;
    return nanoTime() - s_startNanos;
}

// --- Tickable timer (Legacy: NanoTime.allocate/tick) ---

void NanoTimer_reset(NanoTimer *timer) {
    u64 now = nanoTime();
    (*timer).startNanos = now;
    (*timer).lastNanos = now;
    (*timer).currentNanos = now;
    (*timer).deltaTime = 0.0;
    (*timer).totalTime = 0.0;
}

void NanoTimer_tickWithClock(NanoTimer *timer, const Clock *clock) {
    u64 now = nanoTime();
    u64 current = (*timer).currentNanos;

    (*timer).lastNanos = current;
    (*timer).currentNanos = now;

    d64 realDeltaSec = (d64)(now - current) / 1000000000.0;

    d64 scale = 1.0;
    if (clock) {
        if (Clock_isPaused(clock))
            scale = 0.0;
        else
            scale = Clock_timeScale(clock);
    }

    d64 scaledDeltaSec = realDeltaSec * scale;
    (*timer).deltaTime = scaledDeltaSec;
    (*timer).totalTime += scaledDeltaSec;
}

void NanoTimer_tick(NanoTimer *timer) {
    NanoTimer_tickWithClock(timer, nullptr);
}

d64 NanoTimer_deltaTime(const NanoTimer *timer) {
    return (*timer).deltaTime;
}

d64 NanoTimer_totalTime(const NanoTimer *timer) {
    return (*timer).totalTime;
}

u64 NanoTimer_deltaNanos(const NanoTimer *timer) {
    return (*timer).currentNanos - (*timer).lastNanos;
}

u64 NanoTimer_elapsedNanosOf(const NanoTimer *timer) {
    return (*timer).currentNanos - (*timer).startNanos;
}
