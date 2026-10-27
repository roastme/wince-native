/* The CE 5 SDK has no time.h. Only what the Lua 5.1 libraries use. */
#ifndef _LUA_CE_TIME_H_
#define _LUA_CE_TIME_H_

typedef unsigned long time_t;
typedef unsigned long clock_t;
#define CLOCKS_PER_SEC 1000

#include <stddef.h>

struct tm {
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst;
};

time_t time(time_t *t);
clock_t clock(void);
struct tm *localtime(const time_t *t);
struct tm *gmtime(const time_t *t);
time_t mktime(struct tm *tm);
size_t strftime(char *s, size_t max, const char *fmt, const struct tm *tm);

#endif
#include "wceex.h"
