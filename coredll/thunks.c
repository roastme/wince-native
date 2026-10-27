// Thunks for CE 5 coredll exports whose kernel side is a numbered Win32 slot (psyscall.h W32_*).
// Each xxx_ entry is IMPLICIT_DECL through its slot, which WINCEMACRO turns into a kernel trap address.
#include <windows.h>
#include <psyscall.h>

// LoadLibraryW is marked "build break" in mkfuncs.h. Slot 8 (W32_LoadLibraryW) is null in kwin32.c, so route it to LoadLibraryExW (slot 148).
HINSTANCE WINAPI xxx_LoadLibraryW(LPCWSTR lpLibFileName) {
    DWORD cnt = 0;
    return IMPLICIT_DECL(HMODULE, SH_WIN32, W32_LoadLibraryExW, (LPCWSTR, DWORD, LPDWORD))(lpLibFileName, 0, &cnt);
}

// WaitForSingleObject has no slot of its own. Use WaitForMultiple (slot 58) with one handle.
DWORD WINAPI xxx_WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) {
    return IMPLICIT_DECL(DWORD, SH_WIN32, W32_WaitForMultiple, (DWORD, CONST HANDLE *, BOOL, DWORD))(1, &hHandle, FALSE, dwMilliseconds);
}

// Slot-based exports with no kernel-side wrapper of their own: call the Win32 slot directly.
DWORD WINAPI xxx_GetProcessVersion(DWORD ProcessId) {
    return IMPLICIT_DECL(DWORD, SH_WIN32, W32_GetProcessVersion, (DWORD))(ProcessId);
}

BOOL WINAPI xxx_KernelLibIoControl(HANDLE hLib, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned) {
    return IMPLICIT_DECL(BOOL, SH_WIN32, W32_KernelLibIoControl, (HANDLE, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD))(hLib, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
}

DWORD xxx_TlsCall(DWORD p1, DWORD p2) {
    return IMPLICIT_DECL(DWORD, SH_WIN32, W32_TlsCall, (DWORD, DWORD))(p1, p2);
}

// Names that mkfuncs.h maps to a different kernel slot name, so the generator skips them.
BOOL WINAPI xxx_FreeLibrary(HMODULE hLibModule) {
    DWORD cnt = 0;
    return IMPLICIT_DECL(BOOL, SH_WIN32, W32_FreeLibrary, (HANDLE, LPDWORD))(hLibModule, &cnt);
}

HANDLE WINAPI xxx_GetProcessIDFromIndex(DWORD dwIdx) {
    return IMPLICIT_DECL(HANDLE, SH_WIN32, W32_ProcGetIDFromIndex, (DWORD))(dwIdx);
}

DWORD WINAPI xxx_GetProcessIndexFromID(HANDLE hProcess) {
    return IMPLICIT_DECL(DWORD, SH_WIN32, W32_ProcGetIndex, (HANDLE))(hProcess);
}

BOOL WINAPI xxx_SetStdioPathW(DWORD id, LPCWSTR pwszPath) {
    return IMPLICIT_DECL(BOOL, SH_WIN32, W32_SetStdioPathW, (DWORD, LPCWSTR))(id, pwszPath);
}

BOOL WINAPI xxx_GetStdioPathW(DWORD id, PWSTR pwszBuf, LPDWORD lpdwLen) {
    return IMPLICIT_DECL(BOOL, SH_WIN32, W32_GetStdioPathW, (DWORD, PWSTR, LPDWORD))(id, pwszBuf, lpdwLen);
}
