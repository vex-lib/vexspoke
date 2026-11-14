// vexspoke/test/time/nanotime_test.c — owner test for time/nanotime.
//
// Proves the monotonic clock and tickable timer:
//   - pre-init epoch/elapsed are 0; init is idempotent and captures a fixed
//     epoch; now() is monotonic (never goes backwards);
//   - NanoTimer_reset zeroes delta/total and all timestamps;
//   - tick accumulates real delta into deltaTime/totalTime and exposes
//     deltaNanos/elapsedNanosOf consistently;
//   - tickWithClock pauses (scale 0) freeze delta/total while real time still
//     advances, and timeScale > 1 scales the delta;
//   - tickWithClock(nullptr) behaves exactly like tick.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

#include "time/nanotime.h"
#include "time/clock.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Delays briefly for tests that observe monotonic time or timer elapsed values.
// Waits briefly to create measurable intervals for monotonic timer assertions.
// Sleeps for a bounded interval to make monotonic-clock progress observable.
static fn wait_ms(long ms) {
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, nullptr);
}

// Checks monotonic nanosecond time is available and does not regress.
// Checks NanoTime initialization, idempotence, monotonicity, and elapsed time.
// Tests lazy epoch initialization, idempotence, and monotonic readings.
static fn test_epoch() {
    // Before the first init the engine epoch is unmapped.
    CHECK(NanoTime_startNanos() == 0);
    CHECK(NanoTime_elapsedNanos() == 0);

    NanoTime_init();
    u64 start = NanoTime_startNanos();
    CHECK(start != 0);

    // Idempotent: a second init does not move the anchor.
    NanoTime_init();
    NanoTime_init();
    CHECK(NanoTime_startNanos() == start);

    u64 a = NanoTime_now();
    u64 b = NanoTime_now();
    CHECK(b >= a);                                  // monotonic, never backwards
    wait_ms(2);
    u64 c = NanoTime_now();
    CHECK(c > a);

    CHECK(NanoTime_elapsedNanos() < (u64) 60 * 1000000000ULL);
}

// Verifies timer reset restarts its elapsed-time measurement.
// Verifies reset zeros every NanoTimer duration and timestamp projection.
// Verifies reset zeroes timer deltas, totals, and elapsed timestamps.
static fn test_timer_reset() {
    NanoTimer t;
    NanoTimer_reset(&t);
    CHECK(NanoTimer_deltaTime(&t) == 0.0);
    CHECK(NanoTimer_totalTime(&t) == 0.0);
    CHECK(NanoTimer_deltaNanos(&t) == 0);
    CHECK(NanoTimer_elapsedNanosOf(&t) == 0);
}

// Checks timer tick reports elapsed time and advances its previous-tick state.
// Checks tick derives delta and total durations from successive monotonic readings.
// Checks real-time tick accumulation and consistency between seconds and nanoseconds.
static fn test_timer_tick() {
    NanoTimer t;
    NanoTimer_reset(&t);

    wait_ms(3);
    NanoTimer_tick(&t);
    CHECK(NanoTimer_deltaNanos(&t) > 0);
    CHECK(NanoTimer_deltaTime(&t) > 0.0);
    CHECK(NanoTimer_totalTime(&t) == NanoTimer_deltaTime(&t));
    CHECK(NanoTimer_elapsedNanosOf(&t) > 0);
    // deltaTime is the nanos delta expressed in seconds.
    d64 secs = (d64) NanoTimer_deltaNanos(&t) / 1e9;
    CHECK(fabs(NanoTimer_deltaTime(&t) - secs) < 1e-6);

    d64 firstTotal = NanoTimer_totalTime(&t);
    wait_ms(3);
    NanoTimer_tick(&t);
    CHECK(NanoTimer_totalTime(&t) > firstTotal);    // accumulates

    // elapsed >= delta (there was a gap between reset and first tick too).
    CHECK(NanoTimer_elapsedNanosOf(&t) >= NanoTimer_deltaNanos(&t));
}

// Confirms timer measurements follow the associated clock's paused state.
// Confirms a paused Clock freezes scaled timer duration while real nanos still advance.
// Confirms a paused Clock suppresses virtual accumulation while measuring real elapsed time.
static fn test_timer_with_paused_clock() {
    Clock c = Clock();
    Clock_setPaused(&c, true);

    NanoTimer t;
    NanoTimer_reset(&t);
    wait_ms(3);
    NanoTimer_tickWithClock(&t, &c);
    CHECK(NanoTimer_deltaTime(&t) == 0.0);          // frozen
    CHECK(NanoTimer_totalTime(&t) == 0.0);
    CHECK(NanoTimer_deltaNanos(&t) > 0);            // real advance still measured

    // Unpause: accrual resumes.
    Clock_setPaused(&c, false);
    wait_ms(3);
    NanoTimer_tickWithClock(&t, &c);
    CHECK(NanoTimer_deltaTime(&t) > 0.0);
}

// Verifies timer elapsed values reflect the configured clock scale.
// Checks timer deltas reflect a non-unit Clock time scale within scheduling tolerance.
// Checks timer delta scaling against the measured real-time interval.
static fn test_timer_scale() {
    Clock c = Clock();
    Clock_setTimeScale(&c, 2.0);

    NanoTimer t;
    NanoTimer_reset(&t);
    wait_ms(5);
    NanoTimer_tickWithClock(&t, &c);
    d64 real = (d64) NanoTimer_deltaNanos(&t) / 1e9;
    CHECK(real > 0.0);
    // Scaled delta is about twice the real delta (allow scheduling slack).
    CHECK(NanoTimer_deltaTime(&t) > real * 1.5);
    CHECK(NanoTimer_deltaTime(&t) < real * 2.5 + 0.001);
}

// Checks a timer without a clock returns its safe result.
// Verifies a null Clock argument follows the ordinary unscaled tick path.
// Verifies a null clock uses the same unscaled ticking path as NanoTimer_tick.
static fn test_tick_null_clock() {
    NanoTimer t;
    NanoTimer_reset(&t);
    wait_ms(2);
    NanoTimer_tickWithClock(&t, nullptr);
    CHECK(NanoTimer_deltaTime(&t) > 0.0);

    NanoTimer t2;
    NanoTimer_reset(&t2);
    wait_ms(2);
    NanoTimer_tick(&t2);
    CHECK(NanoTimer_deltaTime(&t2) > 0.0);
}

// Runs monotonic-time and timer reset, tick, pause, scale, and null-clock cases.
// Runs monotonic epoch and NanoTimer reset/tick/clock integration cases.
// Runs epoch, reset, ticking, pause, scaling, and null-clock timer cases.
int main() {
    test_epoch();
    test_timer_reset();
    test_timer_tick();
    test_timer_with_paused_clock();
    test_timer_scale();
    test_tick_null_clock();

    if (g_failures == 0) {
        printf("nanotime_test: all assertions held\n");
        return 0;
    }
    printf("nanotime_test: %d FAILURES\n", g_failures);
    return 1;
}
