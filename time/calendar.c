// time/calendar.c — stateless date arithmetic (Legacy: time/Calendar.java port).
//
// addMonths/addYears shift the month/year fields and clip the day into the
// target month (Jan 31 + 1 month => Feb 28), then reconstitute the epoch via
// the inverse of Hinnant's civil-from-days algorithm — same as legacy.

// should support calendar at 1500s i think? may regurgitate...

#include "vex.h"

#include "time/calendar.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Calendar
 * ============================================================================
 * Stateless date arithmetic over DateTime values: leap-year and daysInMonth
 * tables, Zeller's congruence for ISO-8601 day-of-week, and addDays/
 * addMonths/addYears that shift fields then reconstitute the epoch via the
 * inverse of Hinnant's civil-from-days algorithm. Month/year shifts clip the
 * day into the target month (Jan 31 + 1 month => Feb 28) so the result is
 * always a valid date. No state, no allocation — pure functions on the
 * caller's DateTime.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Calendar (time/calendar.c)
 * ============================================================================
 * stateless date arithmetic (Legacy: time/Calendar.java).
 *
 * STRUCT FIELDS: none — procedural (operates on DateTime values (pure date arithmetic))
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Calendar_daysInMonth(year, month)
 *   - Calendar_dayOfWeek(year, month, day)
 *   - Calendar_addDays(dt, days)
 *   - Calendar_addMonths(dt, months)
 *   - Calendar_addYears(dt, years)
 *
 * Getters:
 *   - Calendar_isLeapYear(year)
 * ============================================================================
 */


// Reports whether year has the Gregorian leap-year extra day.
bool Calendar_isLeapYear(i32 year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// Returns the month's Gregorian day count, or zero for a month outside 1..12.
i32 Calendar_daysInMonth(i32 year, i32 month) {
    if (month < 1 || month > 12) return 0;
    if (month == 2) return Calendar_isLeapYear(year) ? 29 : 28;
    if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
    return 31;
}

// Returns the ISO weekday number (Monday 1 through Sunday 7) for the date.
i32 Calendar_dayOfWeek(i32 year, i32 month, i32 day) {
    if (month < 3) {
        month += 12;
        year--;
    }
    i32 k = year % 100;
    i32 j = year / 100;
    // Zeller's congruence: h = 0 Saturday, 1 Sunday, .. 6 Friday.
    i32 h = (day + 13 * (month + 1) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;

    // Convert to ISO-8601 (1=Monday .. 7=Sunday).
    switch (h) {
        case 0: return 6; // Saturday
        case 1: return 7; // Sunday
        case 2: return 1; // Monday
        case 3: return 2; // Tuesday
        case 4: return 3; // Wednesday
        case 5: return 4; // Thursday
        default: return 5; // Friday
    }
}

// Advances the date-time by the requested number of 24-hour days.
void Calendar_addDays(DateTime *dt, i32 days) {
    setEpochMillis(dt, DateTime_epochMillis(dt) + (int64_t)days * 86400000LL);
}

// Inverse of Hinnant's algorithm: calendar fields => UTC epoch millis.
static int64_t toEpochMillis(i32 year, i32 month, i32 day,
                             i32 hour, i32 minute, i32 second,
                             i32 millisecond) {
    i32 y = year - (month <= 2 ? 1 : 0);
    i32 m = month + (month <= 2 ? 9 : -3);
    i32 era = (y >= 0 ? y : y - 399) / 400;
    i32 yoe = y - era * 400;
    i32 doy = (153 * m + 2) / 5 + day - 1;
    i32 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int64_t totalDays = (int64_t)era * 146097 + doe - 719468;

    int64_t totalSecs = totalDays * 86400LL + hour * 3600LL + minute * 60LL + second;
    return totalSecs * 1000 + millisecond;
}

// Shifts the month while retaining time-of-day and clipping an invalid day to month end.
void Calendar_addMonths(DateTime *dt, i32 months) {
    i32 year = DateTime_year(dt);
    i32 month = DateTime_month(dt);
    i32 day = DateTime_day(dt);
    i32 hour = DateTime_hour(dt);
    i32 minute = DateTime_minute(dt);
    i32 second = DateTime_second(dt);
    i32 millisecond = DateTime_millisecond(dt);

    i32 totalMonths = (year * 12) + (month - 1) + months;
    i32 newYear = totalMonths / 12;
    i32 newMonth = (totalMonths % 12) + 1;

    // Clip day into the target month (Jan 31 + 1 month => Feb 28).
    i32 maxDays = Calendar_daysInMonth(newYear, newMonth);
    i32 newDay = day < maxDays ? day : maxDays;

    setEpochMillis(dt, toEpochMillis(newYear, newMonth, newDay,
                                              hour, minute, second, millisecond));
}

// Shifts the year while retaining month and time, clipping the day when needed.
void Calendar_addYears(DateTime *dt, i32 years) {
    i32 year = DateTime_year(dt);
    i32 month = DateTime_month(dt);
    i32 day = DateTime_day(dt);
    i32 hour = DateTime_hour(dt);
    i32 minute = DateTime_minute(dt);
    i32 second = DateTime_second(dt);
    i32 millisecond = DateTime_millisecond(dt);

    i32 newYear = year + years;
    i32 maxDays = Calendar_daysInMonth(newYear, month);
    i32 newDay = day < maxDays ? day : maxDays;

    setEpochMillis(dt, toEpochMillis(newYear, month, newDay,
                                              hour, minute, second, millisecond));
}
