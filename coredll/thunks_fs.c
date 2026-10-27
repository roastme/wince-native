//
// Thunks for the file-system API set (SH_FILESYS_APIS), AFS table.
//
// Indices come from FSDSERV/fsdtables.cpp (apfnAFSAPIs). Position in that
// array is the method number IMPLICIT_CALL uses. mwinbase.h lists different
// numbers for the same names (CreateFileW is 9 there, 6 in fsdtables.cpp);
// those look stale, so they are not used here.
//
// Handle-based calls (ReadFile, WriteFile, ...) dispatch on HT_FILE, the
// open-file handle type, through apfnFileAPIs in the same file. Their numbers
// match mwinbase.h, so only the AFS numbers disagree.
//
#include <windows.h>
#include "psyscall.h"

#define FS_AFS_GetFileAttributesW  4
#define FS_AFS_SetFileAttributesW  5
#define FS_AFS_CreateFileW         6
#define FS_AFS_DeleteFileW         7
#define FS_AFS_FindFirstFileW      9

#define FS_FILE_ReadFile           2
#define FS_FILE_WriteFile          3
#define FS_FILE_GetFileSize        4
#define FS_FILE_SetFilePointer     5
#define FS_FILE_GetFileTime        8
#define FS_FILE_SetFileTime        9
#define FS_FILE_SetEndOfFile      10

DWORD WINAPI xxx_GetFileAttributesW(LPCWSTR lpFileName) {
    return IMPLICIT_DECL(DWORD, SH_FILESYS_APIS, FS_AFS_GetFileAttributesW, (LPCWSTR))(lpFileName);
}

BOOL WINAPI xxx_SetFileAttributesW(LPCWSTR lpFileName, DWORD dwFileAttributes) {
    return IMPLICIT_DECL(BOOL, SH_FILESYS_APIS, FS_AFS_SetFileAttributesW, (LPCWSTR, DWORD))(lpFileName, dwFileAttributes);
}

HANDLE WINAPI xxx_CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                              LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                              DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    return IMPLICIT_DECL(HANDLE, SH_FILESYS_APIS, FS_AFS_CreateFileW,
                         (LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE))
        (lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition,
         dwFlagsAndAttributes, hTemplateFile);
}

BOOL WINAPI xxx_DeleteFileW(LPCWSTR lpFileName) {
    return IMPLICIT_DECL(BOOL, SH_FILESYS_APIS, FS_AFS_DeleteFileW, (LPCWSTR))(lpFileName);
}

HANDLE WINAPI xxx_FindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData) {
    return IMPLICIT_DECL(HANDLE, SH_FILESYS_APIS, FS_AFS_FindFirstFileW,
                         (LPCWSTR, LPWIN32_FIND_DATAW))(lpFileName, lpFindFileData);
}

BOOL WINAPI xxx_ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                         LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped) {
    return IMPLICIT_DECL(BOOL, HT_FILE, FS_FILE_ReadFile,
                         (HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED))
        (hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
}

BOOL WINAPI xxx_WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
                          LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped) {
    return IMPLICIT_DECL(BOOL, HT_FILE, FS_FILE_WriteFile,
                         (HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED))
        (hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped);
}

DWORD WINAPI xxx_GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh) {
    return IMPLICIT_DECL(DWORD, HT_FILE, FS_FILE_GetFileSize, (HANDLE, LPDWORD))(hFile, lpFileSizeHigh);
}

DWORD WINAPI xxx_SetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh,
                                DWORD dwMoveMethod) {
    return IMPLICIT_DECL(DWORD, HT_FILE, FS_FILE_SetFilePointer,
                         (HANDLE, LONG, PLONG, DWORD))
        (hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
}

BOOL WINAPI xxx_GetFileTime(HANDLE hFile, LPFILETIME lpCreationTime, LPFILETIME lpLastAccessTime,
                            LPFILETIME lpLastWriteTime) {
    return IMPLICIT_DECL(BOOL, HT_FILE, FS_FILE_GetFileTime,
                         (HANDLE, LPFILETIME, LPFILETIME, LPFILETIME))
        (hFile, lpCreationTime, lpLastAccessTime, lpLastWriteTime);
}

BOOL WINAPI xxx_SetFileTime(HANDLE hFile, CONST FILETIME *lpCreationTime,
                            CONST FILETIME *lpLastAccessTime, CONST FILETIME *lpLastWriteTime) {
    return IMPLICIT_DECL(BOOL, HT_FILE, FS_FILE_SetFileTime,
                         (HANDLE, CONST FILETIME *, CONST FILETIME *, CONST FILETIME *))
        (hFile, lpCreationTime, lpLastAccessTime, lpLastWriteTime);
}

BOOL WINAPI xxx_SetEndOfFile(HANDLE hFile) {
    return IMPLICIT_DECL(BOOL, HT_FILE, FS_FILE_SetEndOfFile, (HANDLE))(hFile);
}

//
// CloseHandle is METHOD_DECL(BOOL, ID_ALLHANDLE_FREE, ...) in mkfuncs.h: method
// 0 on the handle itself, so the handle's own type dispatches it. Every handle
// type has it, so no handle-type index is needed here.
//
BOOL WINAPI xxx_CloseHandle(HANDLE hObject) {
    return METHOD_DECL(BOOL, ID_ALLHANDLE_FREE, (HANDLE))(hObject);
}

// Slot 7 of apfnFileAPIs (FSDSERV/fsdtables.cpp), matching kernel.h's FlushFileBuffers macro.
#define FS_FILE_FlushFileBuffers   7

BOOL WINAPI xxx_FlushFileBuffers(HANDLE hFile) {
    return IMPLICIT_DECL(BOOL, HT_FILE, FS_FILE_FlushFileBuffers, (HANDLE))(hFile);
}
