// loop.c — EngineLoop port. The heart of the engine: a fixed-timestep
// loop that calls your tick() function. accumulator keeps a constant step
// (frame_ms) regardless of wall-clock jitter, so simulation speed is stable.
//
// The "while" the lessons keep mentioning lives here:
//     while (running) { poll events; tick at fixed steps; }

#include "vex.h"
#include "engine/loop.h"

#include <time.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Loop
 * ============================================================================
 * EngineLoop port: a fixed-timestep loop that calls the tick function once
 * per frame_ms of accumulated wall-clock time. An accumulator keeps the step
 * constant regardless of jitter, and catch-up is clamped to four steps per
 * frame so a stall can never spiral into a death loop — the backlog is
 * dropped instead. The running flag is atomic (release store on stop,
 * acquire load per iteration) so Loop_stop from another thread ends the loop
 * promptly. tick must never be nullptr and frame_ms must be positive.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Loop (engine/loop.c)
 * ============================================================================
 * the engine loop API (Legacy: engine/EngineLoop.java).
 *
 * STRUCT FIELDS (Mirroring engine/loop.h):
 * ----------------------------------------------------------------------------
 *   Loop {
 *     TickFn tick; // called once per fixed step (never nullptr)
 *     any userdata; // opaque context handed to tick
 *     i64 frame_ms; // fixed timestep in milliseconds (>0)
 *     _Atomic bool running; // Loop_stop flips this to end the loop
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Loop_run(loop)
 *   - Loop_stop(loop)
 * ============================================================================
 */



;;GETTER
static inline i64 Loop_frame_ms(const Loop *s) { return (*s).frame_ms; }
;;GETTER
static inline i64 *Loop_frame_msMut(Loop *s) { return &(*s).frame_ms; }
;;SETTER
static inline fn Loop_setFrame_ms(Loop *s, i64 v) { (*s).frame_ms = v; }
;;GETTER
static inline _Atomic bool Loop_running(const Loop *s) { return (*s).running; }
;;GETTER
static inline _Atomic bool *Loop_runningMut(Loop *s) { return &(*s).running; }
;;SETTER
static inline fn Loop_setRunning(Loop *s, _Atomic bool v) { (*s).running = v; }
;;GETTER
static inline TickFn Loop_tick(const Loop *s) { return (*s).tick; }
;;GETTER
static inline TickFn *Loop_tickMut(Loop *s) { return &(*s).tick; }
;;SETTER
static inline fn Loop_setTick(Loop *s, TickFn v) { (*s).tick = v; }
;;GETTER
static inline any Loop_userdata(const Loop *s) { return (*s).userdata; }
;;GETTER
static inline any *Loop_userdataMut(Loop *s) { return &(*s).userdata; }
;;SETTER
static inline fn Loop_setUserdata(Loop *s, any v) { (*s).userdata = v; }

static i64 nowMs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (i64) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// Runs fixed-step ticks until stopped, limiting catch-up to four steps per pass.
fn Loop_run(Loop *loop) {
    if (!loop || !Loop_tick(loop))
        return;
    if (Loop_frame_ms(loop) <= 0)
        return;

    atomic_store_explicit(Loop_runningMut(loop), true, memory_order_release);
    i64 accumulator = 0;
    i64 prev = nowMs();

    while (atomic_load_explicit(Loop_runningMut(loop), memory_order_acquire)) {
        i64 now = nowMs();
        accumulator += now - prev;
        prev = now;

        // Clamp catch-up: at most 4 steps per frame, then drop backlog.
        // Prevents spiral-of-death after a stall and bounds frame time.
        i32 steps = 0;
        while (accumulator >= Loop_frame_ms(loop) && steps < 4) {
            Loop_tick(loop)(Loop_userdata(loop));
            accumulator -= Loop_frame_ms(loop);
            steps++;
            if (!atomic_load_explicit(Loop_runningMut(loop), memory_order_acquire))
                break;
        }
        if (steps == 4)
            accumulator = 0;
    }
}

// Clears the atomic running flag so an active Loop_run can return.
fn Loop_stop(Loop *loop) {
    if (loop)
        atomic_store_explicit(Loop_runningMut(loop), false, memory_order_release);
}
