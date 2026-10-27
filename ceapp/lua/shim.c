/*
 * shim.c - the WCECOMPAT helpers luace.h names (wceex_*), built on the CE 5 SDK instead.
 * Only what the Lua 5.1 sources and luace.c call. Anything that needs a shell or a process returns failure.
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <time.h>

int errno;

/* the SYSTEMTIME/FILETIME helpers below use 1601-based FILETIME; Unix time starts 1970 */
#define UNIX_EPOCH_FT 116444736000000000ULL

time_t wceex_time(time_t *t)
{
    SYSTEMTIME st;
    FILETIME ft;
    unsigned __int64 v;
    time_t now;

    GetSystemTime(&st);
    SystemTimeToFileTime(&st, &ft);
    v = ((unsigned __int64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    now = (time_t)((v - UNIX_EPOCH_FT) / 10000000ULL);
    if (t) *t = now;
    return now;
}

clock_t wceex_clock(void)
{
    return (clock_t)GetTickCount();
}

struct tm *wceex_gmtime(const time_t *t)
{
    static struct tm r;
    SYSTEMTIME st;
    FILETIME ft;
    unsigned __int64 v = ((unsigned __int64)*t * 10000000ULL) + UNIX_EPOCH_FT;

    ft.dwLowDateTime = (DWORD)v;
    ft.dwHighDateTime = (DWORD)(v >> 32);
    FileTimeToSystemTime(&ft, &st);
    r.tm_sec = st.wSecond; r.tm_min = st.wMinute; r.tm_hour = st.wHour;
    r.tm_mday = st.wDay; r.tm_mon = st.wMonth - 1; r.tm_year = st.wYear - 1900;
    r.tm_wday = st.wDayOfWeek; r.tm_yday = 0; r.tm_isdst = 0;
    return &r;
}

struct tm *wceex_localtime(const time_t *t)
{
    return wceex_gmtime(t);
}

time_t wceex_mktime(struct tm *tm)
{
    SYSTEMTIME st;
    FILETIME ft;
    unsigned __int64 v;

    st.wYear = (WORD)(tm->tm_year + 1900); st.wMonth = (WORD)(tm->tm_mon + 1); st.wDay = (WORD)tm->tm_mday;
    st.wHour = (WORD)tm->tm_hour; st.wMinute = (WORD)tm->tm_min; st.wSecond = (WORD)tm->tm_sec;
    st.wMilliseconds = 0;
    if (!SystemTimeToFileTime(&st, &ft)) return (time_t)-1;
    v = ((unsigned __int64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return (time_t)((v - UNIX_EPOCH_FT) / 10000000ULL);
}

size_t wceex_strftime(char *s, size_t max, const char *fmt, const struct tm *tm)
{
    /* only %Y-%m-%d %H:%M:%S is supported; the rest is copied */
    int n = _snprintf(s, max, "%04d-%02d-%02d %02d:%02d:%02d", tm->tm_year + 1900, tm->tm_mon + 1,
                      tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
    (void)fmt;
    return n < 0 || (size_t)n >= max ? 0 : (size_t)n;
}

char *wceex_getenv(const char *name)
{
    (void)name;
    return NULL;
}

char *wceex_strerror(int e)
{
    static char buf[32];
    sprintf(buf, "error %d", e);
    return buf;
}

char *wceex_setlocale(int cat, const char *loc)
{
    (void)cat; (void)loc;
    return "C";
}

struct lconv *wceex_localeconv(void)
{
    static struct lconv lc;
    lc.decimal_point = ".";
    lc.thousands_sep = "";
    lc.grouping = "";
    return &lc;
}

int wceex_system(const char *cmd)
{
    (void)cmd;
    return -1;
}

int wceex_rename(const char *from, const char *to)
{
    WCHAR wf[MAX_PATH], wt[MAX_PATH];
    int i;

    for (i = 0; i < MAX_PATH - 1 && from[i]; i++) wf[i] = (unsigned char)from[i];
    wf[i] = 0;
    for (i = 0; i < MAX_PATH - 1 && to[i]; i++) wt[i] = (unsigned char)to[i];
    wt[i] = 0;
    return MoveFileW(wf, wt) ? 0 : -1;
}

int wceex_unlink(const char *name)
{
    WCHAR w[MAX_PATH];
    int i;

    for (i = 0; i < MAX_PATH - 1 && name[i]; i++) w[i] = (unsigned char)name[i];
    w[i] = 0;
    return DeleteFileW(w) ? 0 : -1;
}

char *wceex_tmpnam(char *s)
{
    if (s) strcpy(s, "\\Temp\\lua_tmp");
    return s ? s : "\\Temp\\lua_tmp";
}

FILE *wceex_tmpfile(void)
{
    return NULL;
}

FILE *wceex_freopen(const char *name, const char *mode, FILE *fp)
{
    (void)name; (void)mode;
    return fp;
}

HANDLE wceex_LoadLibraryA(const char *name)
{
    (void)name;
    return NULL;
}

DWORD wceex_FormatMessageA(DWORD flags, LPCVOID src, DWORD id, DWORD lang, LPSTR out, DWORD size, void *args)
{
    (void)flags; (void)src; (void)id; (void)lang; (void)args;
    if (size) out[0] = 0;
    return 0;
}

DWORD wceex_GetModuleFileNameA(void *mod, LPSTR out, DWORD size)
{
    WCHAR w[MAX_PATH];
    DWORD n, i;

    n = GetModuleFileNameW(mod, w, MAX_PATH);
    for (i = 0; i < n && i < size - 1; i++) out[i] = (char)w[i];
    if (size) out[i] = 0;
    return i;
}

char *wceex_wcstombs(const WCHAR *w)
{
    size_t n = wcslen(w), i;
    char *s = malloc(n + 1);

    if (!s) return NULL;
    for (i = 0; i < n; i++) s[i] = (char)w[i];
    s[n] = 0;
    return s;
}
