/* Hand-written stand-in for the parts of coredll.dll an app needs: each export is the PSL trap into the kernel's Win32 API set. */
#include <windows.h>
#include <psyscall.h>

void WINAPI Sleep(DWORD ms)
{
    IMPLICIT_DECL(void, SH_WIN32, W32_Sleep, (DWORD))(ms);
}

HANDLE WINAPI CreateEventW(LPSECURITY_ATTRIBUTES sa, BOOL manual, BOOL initial, LPCWSTR name)
{
    return IMPLICIT_DECL(HANDLE, SH_WIN32, W32_CreateEvent, (LPSECURITY_ATTRIBUTES, BOOL, BOOL, LPCWSTR))(sa, manual, initial, name);
}

HANDLE WINAPI CreateThread(LPSECURITY_ATTRIBUTES sa, DWORD stack, LPTHREAD_START_ROUTINE start, LPVOID param, DWORD flags, LPDWORD id)
{
    return IMPLICIT_DECL(HANDLE, SH_WIN32, W32_CreateThread, (LPSECURITY_ATTRIBUTES, DWORD, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD))
        (sa, stack, start, param, flags, id);
}

DWORD WINAPI WaitForMultipleObjects(DWORD n, CONST HANDLE *h, BOOL all, DWORD ms)
{
    return IMPLICIT_DECL(DWORD, SH_WIN32, W32_WaitForMultiple, (DWORD, CONST HANDLE *, BOOL, DWORD))(n, h, all, ms);
}

DWORD WINAPI WaitForSingleObject(HANDLE h, DWORD ms)
{
    return WaitForMultipleObjects(1, &h, FALSE, ms);
}

DWORD WINAPI GetTickCount(void)
{
    return IMPLICIT_DECL(DWORD, SH_WIN32, W32_GetTickCount, (void))();
}

BOOL WINAPI EventModify(HANDLE h, DWORD func)
{
    return IMPLICIT_DECL(BOOL, HT_EVENT, ID_EVENT_MODIFY, (HANDLE, DWORD))(h, func);
}

BOOL WINAPI InterruptInitialize(DWORD id, HANDLE ev, LPVOID data, DWORD cb)
{
    return IMPLICIT_DECL(BOOL, SH_WIN32, W32_InterruptInitialize, (DWORD, HANDLE, LPVOID, DWORD))(id, ev, data, cb);
}

void WINAPI InterruptDone(DWORD id)
{
    IMPLICIT_DECL(void, SH_WIN32, W32_InterruptDone, (DWORD))(id);
}
