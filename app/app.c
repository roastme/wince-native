/* A "CE application" built only against the public SDK headers: every Win32 call below is a PSL trap into the kernel (WINCEMACRO). */
#include <windows.h>

extern void NKDbgPrintfW(const wchar_t *, ...);

static HANDLE g_done;
static volatile DWORD g_code;

static DWORD WINAPI Counter(LPVOID p)
{
    int i;

    for (i = 0; i < 3; i++) {
        Sleep(100);
        NKDbgPrintfW(L"app: counter %d tick=%d\r\n", i, GetTickCount());
    }
    g_code = 7;
    SetEvent(g_done);
    return 7;
}

DWORD AppMain(LPVOID unused)
{
    HANDLE h;
    DWORD code = 0, r;

    NKDbgPrintfW(L"app: started, thread id %x\r\n", GetCurrentThreadId());
    g_done = CreateEventW(NULL, FALSE, FALSE, NULL);
    h = CreateThread(NULL, 0, Counter, NULL, 0, NULL);
    NKDbgPrintfW(L"app: CreateThread -> %x\r\n", h);
    r = WaitForSingleObject(g_done, 2000);
    NKDbgPrintfW(L"app: wait -> %d\r\n", r);
    Sleep(50);
    NKDbgPrintfW(L"app: counter thread result %d\r\n", g_code);
    return 0;
}
