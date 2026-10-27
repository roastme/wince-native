/* The CE 5 SDK locale.h only builds for Mac/Win32 hosts. These are the parts the Lua 5.1 sources use. */
#ifndef _LUA_CE_LOCALE_H_
#define _LUA_CE_LOCALE_H_

struct lconv {
    char *decimal_point;
    char *thousands_sep;
    char *grouping;
};

#define LC_ALL 0
#define LC_COLLATE 1
#define LC_CTYPE 2
#define LC_MONETARY 3
#define LC_NUMERIC 4
#define LC_TIME 5

char *setlocale(int category, const char *locale);
struct lconv *localeconv(void);

#endif
#include "wceex.h"
