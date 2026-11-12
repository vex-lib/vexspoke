#ifndef TIME_CALENDAR_H
#define TIME_CALENDAR_H

#include "vex.h"

#include "time/datetime.h"

// time/calendar.h — stateless date arithmetic (Legacy: time/Calendar.java).
//
// Pure utilities over DateTime: leap years, month lengths, Zeller's
// congruence for weekday queries, and add-days/months/years mutations.

bool Calendar_isLeapYear(i32 year);

// 0 for an _out-of-range month, else 28/29/30/31.
i32 Calendar_daysInMonth(i32 year, i32 month);

// ISO-8601 weekday (1=Monday .. 7=Sunday) via Zeller's congruence.
i32 Calendar_dayOfWeek(i32 year, i32 month, i32 day);

void Calendar_addDays(DateTime *dt, i32 days);
void Calendar_addMonths(DateTime *dt, i32 months);
void Calendar_addYears(DateTime *dt, i32 years);

#endif
