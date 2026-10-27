/* Prototypes for the wceex_* names luace.h maps the C library to. Definitions are in ceapp/lua/shim.c. */
#ifndef _LUA_CE_WCEEX_H_
#define _LUA_CE_WCEEX_H_

#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <locale.h>

time_t wceex_time(time_t *t);
clock_t wceex_clock(void);
struct tm *wceex_gmtime(const time_t *t);
struct tm *wceex_localtime(const time_t *t);
time_t wceex_mktime(struct tm *tm);
size_t wceex_strftime(char *s, size_t max, const char *fmt, const struct tm *tm);
char *wceex_getenv(const char *name);
char *wceex_strerror(int e);
char *wceex_setlocale(int cat, const char *loc);
struct lconv *wceex_localeconv(void);
int wceex_system(const char *cmd);
int wceex_rename(const char *from, const char *to);
int wceex_unlink(const char *name);
char *wceex_tmpnam(char *s);
FILE *wceex_tmpfile(void);
FILE *wceex_freopen(const char *name, const char *mode, FILE *fp);
HANDLE wceex_LoadLibraryA(const char *name);
DWORD wceex_FormatMessageA(DWORD flags, LPCVOID src, DWORD id, DWORD lang, LPSTR out, DWORD size, void *args);
DWORD wceex_GetModuleFileNameA(void *mod, LPSTR out, DWORD size);
char *wceex_wcstombs(const WCHAR *w);

#endif
