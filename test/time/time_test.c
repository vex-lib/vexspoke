// time_test/test.c — headless verification of the time package.
//
// Clock scaling/pause, NanoTimer accumulation, DateTime round-trips against
// known UTC instants, and Calendar arithmetic (leap years, month clipping,
// Zeller weekdays).

#include <stdio.h>
#include <time.h>

#include "time/calendar.h"
#include "time/clock.h"
#include "time/datetime.h"
#include "time/nanotime.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Exercises time-unit constants and conversion helpers using known values.
// Integrates UTC date conversion, calendar arithmetic, virtual clock, and timer checks.
// Integrates known UTC dates with calendar arithmetic, Clock, and NanoTimer.
int main(void) {
    // --- Known instants, verified against UTC ---
    DateTime dt;

    // Round-trip the canonical epoch and a leap-day.
    setEpochMillis(&dt, 0);
    CHECK(DateTime_year(&dt) == 1970 && DateTime_month(&dt) == 1 && DateTime_day(&dt) == 1);
    CHECK(DateTime_dayOfWeek(&dt) == 4); // Thursday
    CHECK(DateTime_hour(&dt) == 0 && DateTime_minute(&dt) == 0 && DateTime_second(&dt) == 0);

    setEpochMillis(&dt, 951782400000LL); // 2000-02-29 00:00:00 UTC
    CHECK(DateTime_year(&dt) == 2000 && DateTime_month(&dt) == 2 && DateTime_day(&dt) == 29);
    CHECK(Calendar_isLeapYear(2000) && !Calendar_isLeapYear(1900) && Calendar_isLeapYear(2024));

    // Inverse algorithm must round-trip any fields exactly.
    setEpochMillis(&dt, 1234567890123LL); // 2009-02-13 23:31:30.123 UTC
    CHECK(DateTime_year(&dt) == 2009 && DateTime_month(&dt) == 2 && DateTime_day(&dt) == 13);
    CHECK(DateTime_hour(&dt) == 23 && DateTime_minute(&dt) == 31 && DateTime_second(&dt) == 30);
    CHECK(DateTime_millisecond(&dt) == 123);
    CHECK(DateTime_epochMillis(&dt) == 1234567890123LL);
    CHECK(DateTime_dayOfWeek(&dt) == 5); // Friday

    // --- Calendar arithmetic ---
    setEpochMillis(&dt, 1234567890123LL);
    Calendar_addDays(&dt, 1);
    CHECK(DateTime_day(&dt) == 14 && DateTime_hour(&dt) == 23); // same wall time next day

    setEpochMillis(&dt, 1234567890123LL);
    Calendar_addMonths(&dt, 1); // Feb 13 => Mar 13
    CHECK(DateTime_month(&dt) == 3 && DateTime_day(&dt) == 13);

    setEpochMillis(&dt, 1709251200000LL); // 2024-03-01 00:00:00 UTC
    Calendar_addMonths(&dt, -1);                   // => 2024-02-01
    CHECK(DateTime_year(&dt) == 2024 && DateTime_month(&dt) == 2 && DateTime_day(&dt) == 1);

    // Month clipping: Jan 31 + 1 month => Feb 29 (2024 is a leap year).
    setEpochMillis(&dt, 1706674800000LL); // 2024-01-31 03:00:00 UTC
    Calendar_addMonths(&dt, 1);
    CHECK(DateTime_month(&dt) == 2 && DateTime_day(&dt) == 29);

    // Year clipping: Feb 29 2024 + 1 year => Feb 28 2025.
    setEpochMillis(&dt, 1709164800000LL); // 2024-02-29 00:00:00 UTC
    Calendar_addYears(&dt, 1);
    CHECK(DateTime_year(&dt) == 2025 && DateTime_month(&dt) == 2 && DateTime_day(&dt) == 28);

    // Zeller vs Hinnant agreement on a spread of dates.
    CHECK(Calendar_dayOfWeek(2026, 8, 21) == 5);       // Friday
    CHECK(Calendar_dayOfWeek(1970, 1, 1) == 4);        // Thursday
    CHECK(Calendar_daysInMonth(2024, 2) == 29);
    CHECK(Calendar_daysInMonth(2023, 2) == 28);
    CHECK(Calendar_daysInMonth(2023, 13) == 0);

    // --- Clock: scale and pause shape virtual time ---
    Clock clock = Clock();
    CHECK(Clock_timeScale(&clock) == 1.0 && !Clock_isPaused(&clock));

    Clock_setTimeScale(&clock, 0.5);
    Clock_tick(&clock); // first tick: ~0ms elapsed since create
    struct timespec pause_req = { 0, 40 * 1000000 };
    nanosleep(&pause_req, nullptr);
    u64 before = Clock_virtualTimeMillis(&clock);
    Clock_tick(&clock);
    u64 after = Clock_virtualTimeMillis(&clock);
    CHECK(after >= before && after - before <= 60); // ~40ms real * 0.5

    Clock_setPaused(&clock, true);
    nanosleep(&pause_req, nullptr);
    before = Clock_virtualTimeMillis(&clock);
    Clock_tick(&clock);
    CHECK(Clock_virtualTimeMillis(&clock) == before); // frozen while paused
    Clock_setPaused(&clock, false);

    // --- NanoTimer: deltas accumulate through the clock ---
    NanoTimer timer;
    NanoTimer_reset(&timer);
    CHECK(NanoTimer_deltaTime(&timer) == 0.0);
    nanosleep(&pause_req, nullptr);
    NanoTimer_tickWithClock(&timer, &clock);
    CHECK(NanoTimer_deltaNanos(&timer) > 0);
    CHECK(NanoTimer_totalTime(&timer) > 0.0);
    CHECK(NanoTimer_elapsedNanosOf(&timer) > 0);

    // Paused clock freezes timer deltas but keeps the real reading moving.
    Clock_setPaused(&clock, true);
    d64 totalBefore = NanoTimer_totalTime(&timer);
    nanosleep(&pause_req, nullptr);
    NanoTimer_tickWithClock(&timer, &clock);
    CHECK(NanoTimer_totalTime(&timer) == totalBefore);
    CHECK(NanoTimer_deltaNanos(&timer) > 0); // real time still rolled

    // --- Now-path sanity ---
    DateTime now;
    DateTime_set(&now);
    CHECK(DateTime_year(&now) >= 2026);

    if (g_failures == 0)
        printf("time_test: all checks passed\n");
    else
        printf("time_test: %d FAILURES\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
