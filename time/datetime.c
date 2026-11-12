// time/datetime.c — UTC date/time breakdown (Legacy: time/DateTime.java port).
//
// setEpochMillis carries the legacy math verbatim: floor-division chains for
// the h/m/s/ms fields, then Hinnant's era-based civil-from-days conversion.
// All integer, all branches, zero allocation.

#include "vex.h"

#include "time/datetime.h"

#include <time.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: DateTime
 * ============================================================================
 * UTC date/time breakdown (Legacy: time/DateTime.java): setEpochMillis
 * carries the legacy math verbatim — floor-division chains for the
 * h/m/s/ms fields, then Hinnant's era-based civil-from-days conversion for
 * the calendar fields, all integer, all branches, zero allocation. The
 * DateTime struct is caller-owned; DateTime_set snapshots CLOCK_REALTIME.
 * Lives at R2 as a leaf time behavior.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Datetime (time/datetime.c)
 * ============================================================================
 * UTC date/time breakdown (Legacy: time/DateTime.java).
 *
 * STRUCT FIELDS (Mirroring time/datetime.h):
 * ----------------------------------------------------------------------------
 *   DateTime {
 *     int64_t epochMillis; // epoch millis anchor
 *     i32 year; // calendar component
 *     i32 month; // 1-12
 *     i32 day; // 1-31
 *     i32 hour; // 0-23
 *     i32 minute; // 0-59
 *     i32 second; // 0-59
 *     i32 millisecond; // 0-999
 *     i32 dayOfWeek; // 1=Monday .. 7=Sunday
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - DateTime_epochMillis(dt)
 *   - DateTime_year(dt)
 *   - DateTime_month(dt)
 *   - DateTime_day(dt)
 *   - DateTime_hour(dt)
 *   - DateTime_minute(dt)
 *   - DateTime_second(dt)
 *   - DateTime_millisecond(dt)
 *   - DateTime_dayOfWeek(dt)
 *
 * Setters:
 *   - DateTime_set(dt)
 *   - setEpochMillis(dt, epochMillis)
 * ============================================================================
 */


// Stores epoch milliseconds and derives the corresponding UTC calendar fields.
void setEpochMillis(DateTime *dt, int64_t epochMillis) {
    (*dt).epochMillis = epochMillis;

    int64_t totalSeconds = epochMillis / 1000;
    i32 millisecond = (i32)(epochMillis % 1000);
    if (millisecond < 0) {
        millisecond += 1000;
        totalSeconds--;
    }

    int64_t totalMinutes = totalSeconds / 60;
    i32 second = (i32)(totalSeconds % 60);
    if (second < 0) {
        second += 60;
        totalMinutes--;
    }

    int64_t totalHours = totalMinutes / 60;
    i32 minute = (i32)(totalMinutes % 60);
    if (minute < 0) {
        minute += 60;
        totalHours--;
    }

    int64_t totalDays = totalHours / 24;
    i32 hour = (i32)(totalHours % 24);
    if (hour < 0) {
        hour += 24;
        totalDays--;
    }

    // Day of week: 1970-01-01 was Thursday (=4); shift into ISO 1=Mon..7=Sun.
    i32 dayOfWeek = (i32)((totalDays + 3) % 7);
    if (dayOfWeek < 0)
        dayOfWeek += 7;
    dayOfWeek += 1;

    // Howard Hinnant's civil-from-days algorithm.
    int64_t z = totalDays + 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    i32 doe = (i32)(z - era * 146097);
    i32 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    i32 y = yoe + (i32)(era * 400);
    i32 doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    i32 mp = (5 * doy + 2) / 153;
    i32 day = doy - (153 * mp + 2) / 5 + 1;
    i32 month = mp + (mp < 10 ? 3 : -9);
    i32 year = y + (month <= 2 ? 1 : 0);

    (*dt).year = year;
    (*dt).month = month;
    (*dt).day = day;
    (*dt).hour = hour;
    (*dt).minute = minute;
    (*dt).second = second;
    (*dt).millisecond = millisecond;
    (*dt).dayOfWeek = dayOfWeek;
}

// Captures the current realtime clock and decomposes it into UTC fields.
void DateTime_set(DateTime *dt) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    setEpochMillis(dt, (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// Returns the stored Unix epoch timestamp in milliseconds.
int64_t DateTime_epochMillis(const DateTime *dt) {
    return (*dt).epochMillis;
}

// Returns the UTC calendar year.
i32 DateTime_year(const DateTime *dt) {
    return (*dt).year;
}

// Returns the UTC month number from 1 through 12.
i32 DateTime_month(const DateTime *dt) {
    return (*dt).month;
}

// Returns the UTC day of the month.
i32 DateTime_day(const DateTime *dt) {
    return (*dt).day;
}

// Returns the UTC hour from 0 through 23.
i32 DateTime_hour(const DateTime *dt) {
    return (*dt).hour;
}

// Returns the UTC minute from 0 through 59.
i32 DateTime_minute(const DateTime *dt) {
    return (*dt).minute;
}

// Returns the UTC second from 0 through 59.
i32 DateTime_second(const DateTime *dt) {
    return (*dt).second;
}

// Returns the millisecond component from 0 through 999.
i32 DateTime_millisecond(const DateTime *dt) {
    return (*dt).millisecond;
}

// Returns the ISO weekday number from 1 (Monday) through 7 (Sunday).
i32 DateTime_dayOfWeek(const DateTime *dt) {
    return (*dt).dayOfWeek;
}
