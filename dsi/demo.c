/* First CE thread: services the kernel needs from coredll, then the WM6 coredll (heap, files, registry, strings), the mini GWES
 * (window manager + GDI on both screens) and the shell EXE from the SD card. */
#include <windows.h>
#include <kernel.h>
#include "flatld.h"
#include "fat.h"

/* The shell EXE loaded from \\wm6\\Windows on the SD card. Any CE app built with relocations works here: it gets WinMain(hInst, 0, L"", 1). */
#ifndef SHELL_EXE
#define SHELL_EXE "dsshell.exe"
#endif

extern HANDLE CreateKernelThread(LPTHREAD_START_ROUTINE, LPVOID, WORD, DWORD);
extern DWORD CurMSec;
extern void NKDbgPrintfW(const wchar_t *, ...);

extern void (*TBFf)(LPVOID, ulong);
extern LPVOID pExitThread, pExcpExitThread;
extern void SC_NKTerminateThread(DWORD);

/* Stand-ins for the coredll entry points the ROM loader would normally fetch (ThreadBaseFunc, ExitThread). */
static void DsiExitThread(DWORD code) { SC_NKTerminateThread(code); }
static void DsiThreadBase(LPVOID start, ulong param)
{
    DsiExitThread(((DWORD (*)(LPVOID))start)((LPVOID)param));
}

extern const char wm6_coredll[];

typedef unsigned long long u64;
extern BOOL (*KSystemTimeToFileTime)(LPSYSTEMTIME, LPFILETIME);
extern BOOL (*KLocalFileTimeToFileTime)(const FILETIME *, LPFILETIME);
extern BOOL (*KFileTimeToSystemTime)(const FILETIME *, LPSYSTEMTIME);
extern LONG (*KCompareFileTime)(LPFILETIME, LPFILETIME);
extern void (*pPSLNotify)(DWORD, DWORD, DWORD);

/* Days from 1970-01-01 for a civil date (proleptic Gregorian). */
static long days_from_civil(long y, long m, long d)
{
    long era, yoe, doy, doe;

    y -= m <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static BOOL DsiSystemTimeToFileTime(LPSYSTEMTIME st, LPFILETIME ft)
{
    u64 secs = (u64)((long long)days_from_civil(st->wYear, st->wMonth, st->wDay) * 86400LL)
             + st->wHour * 3600u + st->wMinute * 60u + st->wSecond + 11644473600ULL;
    u64 t = (secs * 1000 + st->wMilliseconds) * 10000;

    ft->dwLowDateTime = (DWORD)t;
    ft->dwHighDateTime = (DWORD)(t >> 32);
    return TRUE;
}

static BOOL DsiFileTimeToSystemTime(const FILETIME *ft, LPSYSTEMTIME st)
{
    u64 t = ((u64)ft->dwHighDateTime << 32) | ft->dwLowDateTime;
    u64 ms = t / 10000, secs = ms / 1000;
    long days = (long)(secs / 86400) - 134774;      /* days since 1970 */
    DWORD rem = (DWORD)(secs % 86400);
    long z = days + 719468, era = (z >= 0 ? z : z - 146096) / 146097;
    long doe = z - era * 146097, yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long y = yoe + era * 400, doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;

    st->wDay = doy - (153 * mp + 2) / 5 + 1;
    st->wMonth = mp < 10 ? mp + 3 : mp - 9;
    st->wYear = y + (st->wMonth <= 2);
    st->wDayOfWeek = (days + 4) % 7;
    st->wHour = rem / 3600;
    st->wMinute = rem % 3600 / 60;
    st->wSecond = rem % 60;
    st->wMilliseconds = (WORD)(ms % 1000);
    return TRUE;
}

static BOOL DsiLocalFileTimeToFileTime(const FILETIME *l, LPFILETIME ft) { *ft = *l; return TRUE; }

static LONG DsiCompareFileTime(LPFILETIME a, LPFILETIME b)
{
    if (a->dwHighDateTime != b->dwHighDateTime)
        return a->dwHighDateTime < b->dwHighDateTime ? -1 : 1;
    return a->dwLowDateTime < b->dwLowDateTime ? -1 : a->dwLowDateTime > b->dwLowDateTime;
}

static void DsiPSLNotify(DWORD a, DWORD b, DWORD c) {}

/* WinMain(hInst, hPrev, lpCmdLine, nShow) on its own kernel thread; the thread parameter is the loaded module. */
static DWORD AppMain(LPVOID param)
{
    FMOD *m = (FMOD *)param;
    static const WCHAR empty[] = L"";
    return ((int (*)(DWORD, DWORD, LPCWSTR, int))m->entry)(m->base, 0, empty, 1);
}

/* CreateProcessW for apps: the program is the file name part of lpApplicationName, looked up in \\wm6\\Windows then \\wm6.
 * FlatLoad refuses EXEs without relocations. Process and thread handles are not modelled: pi is zeroed. */
BOOL WINAPI Dsi_CreateProcessW(LPCWSTR app, LPWSTR cmd, LPVOID sa1, LPVOID sa2, BOOL inherit, DWORD flags,
                               LPVOID env, LPCWSTR dir, LPVOID si, LPPROCESS_INFORMATION pi)
{
    LPCWSTR name = app;
    FMOD *m;
    int i;

    if (!app) return FALSE;
    for (i = 0; app[i]; i++)
        if (app[i] == '\\' || app[i] == '/') name = app + i + 1;
    m = FlatLoadSDW(name);
    if (!m || !m->entry || m->nmiss) {
        NKDbgPrintfW(L"CreateProcessW: %s not started\r\n", name);
        return FALSE;
    }
    if (pi) memset(pi, 0, sizeof *pi);
    CreateKernelThread((LPTHREAD_START_ROUTINE)AppMain, m, (MAX_PRIORITY_LEVELS-4), 0);
    return TRUE;
}

static FMOD *g_shell;

void DsiMain(void)
{
    FMOD *c;

    TBFf = DsiThreadBase;
    KSystemTimeToFileTime = DsiSystemTimeToFileTime;
    KFileTimeToSystemTime = DsiFileTimeToSystemTime;
    KLocalFileTimeToFileTime = DsiLocalFileTimeToFileTime;
    KCompareFileTime = DsiCompareFileTime;
    pPSLNotify = DsiPSLNotify;
    pExitThread = pExcpExitThread = (LPVOID)DsiExitThread;
    NKDbgPrintfW(L"DsiMain: first CE thread running, CurMSec=%d\r\n", CurMSec);
    if (fat_mount()) NKDbgPrintfW(L"DsiMain: no FAT volume on the SD card\r\n");

    c = FlatLoad("coredll.dll", wm6_coredll);
    if (c && c->entry) {
        extern void Fs20Init(void);
        void (*pInitLocale)(void);

        ((BOOL (*)(HANDLE, DWORD, LPVOID))c->entry)((HANDLE)c->base, DLL_PROCESS_ATTACH, NULL);
        /* api set 20 (files, registry) in place of filesys.exe; InitLocale needs it for the locale tables wsprintfW uses */
        Fs20Init();
        pInitLocale = FlatProc(c, 0, "InitLocale");
        if (pInitLocale) pInitLocale();
    }

    {
        extern void Gwes_Init(void);
        extern void *Gwes_Export(const char *);
        Gwes_Init();
        FlatSetOverride(Gwes_Export);
    }
    g_shell = FlatLoadSD(SHELL_EXE);
    if (g_shell && g_shell->entry && !g_shell->nmiss) {
        NKDbgPrintfW(L"DsiMain: starting %S\r\n", SHELL_EXE);
        CreateKernelThread((LPTHREAD_START_ROUTINE)AppMain, g_shell, (MAX_PRIORITY_LEVELS-4), 0);
    } else
        NKDbgPrintfW(L"DsiMain: %S not started (loaded=%x, %d unresolved imports)\r\n", SHELL_EXE, g_shell ? g_shell->base : 0, g_shell ? g_shell->nmiss : -1);
    for (;;) Sleep(1000);
}
