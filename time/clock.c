// time/clock.c — the virtual clock (Legacy: time/Clock.java port).
//
// tick() is the whole idea: elapsedReal * timeScale accrues into the virtual
// timeline, and pause freezes accrual while still advancing the real reading
// — so unpausing never dumps a backlog of phantom milliseconds into the
// simulation.

#include "vex.h"
#include "time/clock.h"

#include <time.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Clock
 * ============================================================================
 * The virtual clock (Legacy: time/Clock.java): tick() accrues
 * elapsedReal * timeScale into the virtual timeline, and pause freezes
 * accrual while the real reading keeps advancing — so unpausing never dumps
 * a backlog of phantom milliseconds into the simulation. Accrual reads a
 * monotonic clock; the baseRealMillis anchor uses the realtime clock for
 * informational wall time. Clock is a value struct returned by Clock()
 * and mutated in place by pointer; reset re-anchors both readings and zeroes
 * the virtual timeline.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Clock (time/clock.c)
 * ============================================================================
 * the virtual clock (Legacy: time/Clock.java).
 *
 * STRUCT FIELDS (Mirroring time/clock.h):
 * ----------------------------------------------------------------------------
 *   Clock {
 *     d64 timeScale; // default 1.0
 *     u64 baseRealMillis; // wall anchor at create/reset (informational)
 *     u64 lastTickRealMillis; // previous tick's real reading
 *     u64 virtualTimeMillis; // accumulated scaled time
 *     bool paused; // frozen when true
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Clock()
 *
 * Core Functions:
 *   - Clock_tick(clock)
 *   - Clock_timeScale(clock)
 *   - Clock_virtualTimeMillis(clock)
 *   - Clock_reset(clock)
 *
 * Setters:
 *   - Clock_setTimeScale(clock, scale)
 *   - Clock_setPaused(clock, paused)
 *
 * Getters:
 *   - Clock_isPaused(clock)
 * ============================================================================
 */



;;GETTER
static inline u64 Clock_baseRealMillis(const Clock *s) { return (*s).baseRealMillis; }
;;GETTER
static inline u64 *Clock_baseRealMillisMut(Clock *s) { return &(*s).baseRealMillis; }
;;SETTER
static inline fn Clock_setBaseRealMillis(Clock *s, u64 v) { (*s).baseRealMillis = v; }
;;GETTER
static inline u64 Clock_lastTickRealMillis(const Clock *s) { return (*s).lastTickRealMillis; }
;;GETTER
static inline u64 *Clock_lastTickRealMillisMut(Clock *s) { return &(*s).lastTickRealMillis; }
;;SETTER
static inline fn Clock_setLastTickRealMillis(Clock *s, u64 v) { (*s).lastTickRealMillis = v; }
;;GETTER
static inline bool Clock_paused(const Clock *s) { return (*s).paused; }
;;GETTER
static inline bool *Clock_pausedMut(Clock *s) { return &(*s).paused; }
;;SETTER
static inline fn Clock_setPaused(Clock *s, bool v) { (*s).paused = v; }
;;GETTER
static inline d64 Clock_timeScale(const Clock *s) { return (*s).timeScale; }
;;GETTER
static inline d64 *Clock_timeScaleMut(Clock *s) { return &(*s).timeScale; }
;;SETTER
static inline fn Clock_setTimeScale(Clock *s, d64 v) { (*s).timeScale = v; }
;;GETTER
static inline u64 Clock_virtualTimeMillis(const Clock *s) { return (*s).virtualTimeMillis; }
;;GETTER
static inline u64 *Clock_virtualTimeMillisMut(Clock *s) { return &(*s).virtualTimeMillis; }
;;SETTER
static inline fn Clock_setVirtualTimeMillis(Clock *s, u64 v) { (*s).virtualTimeMillis = v; }

// Wall-clock millis (legacy used System.currentTimeMillis(); kept for the
// informational base anchor).
static u64 currentTimeMillis() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (u64)ts.tv_sec * 1000ULL + (u64)ts.tv_nsec / 1000000ULL;
}

// Monotonic millis for the accrual side.
static u64 monoMillis() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (u64)ts.tv_sec * 1000ULL + (u64)ts.tv_nsec / 1000000ULL;
}

// Returns a value-initialized clock anchored to current real and monotonic time.
Clock Clock_0() {
    Clock c;
    c.timeScale = 1.0;
    c.baseRealMillis = currentTimeMillis();
    c.lastTickRealMillis = monoMillis();
    c.virtualTimeMillis = 0;
    c.paused = false;
    return c;
}

// Accrues monotonic elapsed milliseconds scaled by timeScale unless paused.
fn Clock_tick(Clock *clock) {
    u64 now = monoMillis();
    u64 lastReal = Clock_lastTickRealMillis(clock);
    u64 elapsedReal = now - lastReal;
    Clock_setLastTickRealMillis(clock, now);

    if (!Clock_paused(clock))
        Clock_setVirtualTimeMillis(clock, Clock_virtualTimeMillis(clock) + ((u64)((d64)elapsedReal * Clock_timeScale(clock))));
}

// Sets the multiplier applied to elapsed real time on subsequent ticks.
fn Clock_setTimeScale(Clock *clock, d64 scale) {
    Clock_setTimeScale(clock, scale);
}



// Returns the configured elapsed-time multiplier.
d64 Clock_timeScale(const Clock *clock) {
    return Clock_timeScale(clock);
}

// Enables or disables virtual-time accrual while ticks continue updating the real anchor.
fn Clock_setPaused(Clock *clock, bool paused) {
    Clock_setPaused(clock, paused);
}

// Reports whether virtual-time accrual is paused.
bool Clock_isPaused(const Clock *clock) {
    return Clock_paused(clock);
}

// Returns accumulated scaled virtual time in milliseconds.
u64 Clock_virtualTimeMillis(const Clock *clock) {
    return Clock_virtualTimeMillis(clock);
}

// Reanchors real readings and clears accumulated virtual time.
fn Clock_reset(Clock *clock) {
    Clock_setBaseRealMillis(clock, currentTimeMillis());
    Clock_setLastTickRealMillis(clock, monoMillis());
    Clock_setVirtualTimeMillis(clock, 0);
}
