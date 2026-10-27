/* Wide-string helpers (coredll exports the shell expects; libc's wchar functions are not usable with -fshort-wchar). */
#include "priv.h"
#include <stdarg.h>

size_t wcslen(const wchar_t *s) { size_t n = 0; while (s[n]) n++; return n; }
wchar_t *wcscpy(wchar_t *d, const wchar_t *s) { wchar_t *r = d; while ((*d++ = *s++)) ; return r; }
wchar_t *wcscat(wchar_t *d, const wchar_t *s) { wcscpy(d + wcslen(d), s); return d; }
wchar_t *wcsncpy(wchar_t *d, const wchar_t *s, size_t n)
{
    size_t i;
    for (i = 0; i < n && s[i]; i++) d[i] = s[i];
    for (; i < n; i++) d[i] = 0;
    return d;
}
LPWSTR WINAPI lstrcpynW_(LPWSTR d, LPCWSTR s, int n)
{
    int i;
    if (n <= 0) return d;
    for (i = 0; i < n - 1 && s[i]; i++) d[i] = s[i];
    d[i] = 0;
    return d;
}
int wcscmp(const wchar_t *a, const wchar_t *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (int)*a - (int)*b;
}
static int lc(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
int wcsicmp(const wchar_t *a, const wchar_t *b)
{
    while (*a && lc(*a) == lc(*b)) { a++; b++; }
    return lc(*a) - lc(*b);
}
int WINAPI lstrcmpiW(LPCWSTR a, LPCWSTR b) { return wcsicmp(a, b); }
int WINAPI lstrcmpW(LPCWSTR a, LPCWSTR b) { return wcscmp(a, b); }

int WINAPIV wsprintfW(LPWSTR out, LPCWSTR fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = NKwvsprintfW(out, fmt, *(void **)&ap, 1024);
    va_end(ap);
    return n;
}
