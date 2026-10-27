//
// Stubs for names the coredll build needs to link but that have no
// implementation in this tree. Not real behaviour.
//
// - Registry (Reg*): the kernel implements them (NKReg*), but no user-mode
//   route is in the tree. Return ERROR_CALL_NOT_IMPLEMENTED.
// - GWES (MessageBoxW, RegisterClassW, DefDlgProcW, SendDlgItemMessageW,
//   GetStockObject, xxx_SystemParametersInfo_GWE): the GWES implementation is not in ce5-ref. Return failure.
// - OutputDebugStringW: no-op.
//
// StringCchLengthW is real library code (strsafe), not a stub.
//
#include <windows.h>

#define STUB_FAIL() SetLastError(ERROR_CALL_NOT_IMPLEMENTED)

LONG WINAPI xxx_RegOpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult) {
    STUB_FAIL(); return ERROR_CALL_NOT_IMPLEMENTED;
}

LONG WINAPI xxx_RegCreateKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD Reserved, LPWSTR lpClass, DWORD dwOptions,
                                REGSAM samDesired, LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult,
                                LPDWORD lpdwDisposition) {
    STUB_FAIL(); return ERROR_CALL_NOT_IMPLEMENTED;
}

LONG WINAPI xxx_RegQueryValueExW(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType,
                                 LPBYTE lpData, LPDWORD lpcbData) {
    STUB_FAIL(); return ERROR_CALL_NOT_IMPLEMENTED;
}

LONG WINAPI xxx_RegSetValueExW(HKEY hKey, LPCWSTR lpValueName, DWORD Reserved, DWORD dwType,
                               CONST BYTE *lpData, DWORD cbData) {
    STUB_FAIL(); return ERROR_CALL_NOT_IMPLEMENTED;
}

LONG WINAPI xxx_RegCloseKey(HKEY hKey) {
    STUB_FAIL(); return ERROR_CALL_NOT_IMPLEMENTED;
}

VOID WINAPI xxx_OutputDebugStringW(LPCWSTR lpOutputString) {
    (void)lpOutputString;
}

int WINAPI MessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) {
    (void)hWnd; (void)lpText; (void)lpCaption; (void)uType;
    STUB_FAIL(); return 0;
}

ATOM WINAPI RegisterClassW(CONST WNDCLASSW *lpWndClass) {
    (void)lpWndClass;
    STUB_FAIL(); return 0;
}

LRESULT WINAPI DefDlgProcW(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam) {
    (void)hDlg; (void)Msg; (void)wParam; (void)lParam;
    STUB_FAIL(); return 0;
}

LRESULT WINAPI SendDlgItemMessageW(HWND hDlg, int nIDDlgItem, UINT Msg, WPARAM wParam, LPARAM lParam) {
    (void)hDlg; (void)nIDDlgItem; (void)Msg; (void)wParam; (void)lParam;
    STUB_FAIL(); return 0;
}

// Called by SystemParametersInfoW (ce5-ref apis.c) for every action the kernel does not handle.
BOOL xxx_SystemParametersInfo_GWE(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni) {
    (void)uiAction; (void)uiParam; (void)pvParam; (void)fWinIni;
    STUB_FAIL(); return FALSE;
}

HGDIOBJ WINAPI GetStockObject(int fnObject) {
    (void)fnObject;
    STUB_FAIL(); return NULL;
}

HRESULT WINAPI StringCchLengthW(LPCWSTR psz, size_t cchMax, size_t *pcch) {
    size_t i;
    if (psz == NULL || cchMax > 0x7FFFFFFF) {
        return E_INVALIDARG;
    }
    for (i = 0; i < cchMax; i++) {
        if (psz[i] == 0) {
            if (pcch != NULL) {
                *pcch = i;
            }
            return S_OK;
        }
    }
    if (pcch != NULL) {
        *pcch = 0;
    }
    return STRSAFE_E_INSUFFICIENT_BUFFER;
}

// Single-byte code page only: no lead bytes exist, so every byte is a non-lead byte.
BOOL WINAPI xxx_IsDBCSLeadByte(BYTE TestChar) {
    (void)TestChar;
    return FALSE;
}

//
// Kernel-internal names that coredll references but that have no definition anywhere in the CE 5
// tree (ce5-ref). Functions fail with 0 / NULL and do nothing. Prototypes come from OAK/INC/pkfuncs.h
// and SDK/INC/excpt.h (ce5-ref/work/cd1x), where the names are declared.
//
// UnhandledExceptionFilter returns 0 = EXCEPTION_CONTINUE_SEARCH: the exception goes on to the default handler.
//
// The v_pfn* pointers are the optional LMEM debug hooks heap.c fills from an lmemdebug DLL; NULL is the
// state heap.c itself sets when no such DLL is loaded.
//
LPVOID CeVirtualSharedAlloc(LPVOID lpvAddr, DWORD cbSize, DWORD fdwAction) {
    (void)lpvAddr; (void)cbSize; (void)fdwAction;
    STUB_FAIL(); return NULL;
}
BOOL CeEventHasOccurred(LONG lEvent, LPWSTR pszName) {
    (void)lEvent; (void)pszName;
    STUB_FAIL(); return FALSE;
}
int GetUserInformation(void)                    { STUB_FAIL(); return 0; }
void I_BatteryNotifyOfTimeChange(void)          { STUB_FAIL(); }
int Imm_DllEntry(void)                          { STUB_FAIL(); return 0; }
int InitLocale(void)                            { STUB_FAIL(); return 0; }
void ShowErrThrd(void)                          { STUB_FAIL(); }
int WinRegInit(void)                            { STUB_FAIL(); return 0; }
ULONG GetCallStackSnapshot(ULONG dwMaxFrames, CallSnapshot lpFrames[], DWORD dwFlags, DWORD dwSkip) {
    (void)dwMaxFrames; (void)lpFrames; (void)dwFlags; (void)dwSkip;
    STUB_FAIL(); return 0;
}
HANDLE GetCallerProcess(void)                   { STUB_FAIL(); return NULL; }
HINSTANCE LoadDriver(LPCTSTR lpszFile) {
    (void)lpszFile;
    STUB_FAIL(); return NULL;
}
LPVOID MapPtrToProcess(LPVOID lpv, HANDLE hProc) {
    (void)lpv; (void)hProc;
    STUB_FAIL(); return NULL;
}
LPVOID MapPtrUnsecure(LPVOID lpv, HANDLE hProc) {
    (void)lpv; (void)hProc;
    STUB_FAIL(); return NULL;
}
DWORD PerformCallBack4(PCALLBACKINFO pcbi, ...) {
    (void)pcbi;
    STUB_FAIL(); return 0;
}
int __cdecl UnhandledExceptionFilter(struct _EXCEPTION_POINTERS *pExceptionInfo) {
    (void)pExceptionInfo;
    return 0;
}
LPBYTE THGrow(THSNAP *pSnap, DWORD dwSize) {
    (void)pSnap; (void)dwSize;
    STUB_FAIL(); return NULL;
}
void *v_pfnCloseHandle = NULL;
void *v_pfnCreateEventW = NULL;
void *v_pfnSetAllocAdditionalData = NULL;
