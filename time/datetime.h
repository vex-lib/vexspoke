#ifndef TIME_DATETIME_H
#define TIME_DATETIME_H

#include "vex.h"

// time/datetime.h — UTC date/time breakdown (Legacy: time/DateTime.java).
//
// Breaks epoch milliseconds into UTC calendar fields using Howard Hinnant's
// standard integer algorithm — no division by f32ing point, no libc
// localtime, allocation-free. dayOfWeek is ISO-8601: 1=Monday .. 7=Sunday.

typedef struct DateTime {
    int64_t epochMillis;
    i32 year;
    i32 month;      // 1-12
    i32 day;        // 1-31
    i32 hour;       // 0-23
    i32 minute;     // 0-59
    i32 second;     // 0-59
    i32 millisecond;// 0-999
    i32 dayOfWeek;  // 1=Monday .. 7=Sunday
} DateTime;

static inline DateTime DateTime_0() { return (DateTime) {0}; }
#define DateTime(...) CONSTRUCTOR_DISPATCH(DateTime, __VA_ARGS__)

// Set from the current system wall clock.
fn DateTime_set(DateTime *dt);

// Recompute every calendar field from epochMillis (Hinnant's algorithm).
fn setEpochMillis(DateTime *dt, int64_t epochMillis);

int64_t DateTime_epochMillis(const DateTime *dt);
i32 DateTime_year(const DateTime *dt);
i32 DateTime_month(const DateTime *dt);
i32 DateTime_day(const DateTime *dt);
i32 DateTime_hour(const DateTime *dt);
i32 DateTime_minute(const DateTime *dt);
i32 DateTime_second(const DateTime *dt);
i32 DateTime_millisecond(const DateTime *dt);
i32 DateTime_dayOfWeek(const DateTime *dt);

#endif
