/* Pieces the NK links against that normally come from other CE modules (memory-mapped files, ROM compression,
 * huge VA reservations). Stubbed until the flat-VM port decides what the DSi image really needs. */
#include <windows.h>
#include <kernel.h>

BOOL g_fCompressionSupported = FALSE;
int BinDecompressROM(LPBYTE s, int cs, LPBYTE d, int *cd) { return 0; }
DWORD CECompress(LPBYTE s, DWORD n, LPBYTE d, DWORD m, WORD a, DWORD b) { return (DWORD)-1; }
DWORD CEDecompress(LPBYTE s, DWORD n, LPBYTE d, DWORD m, DWORD skip, WORD step, DWORD page) { return (DWORD)-1; }
DWORD CEDecompressROM(LPBYTE s, DWORD n, LPBYTE d, DWORD m, DWORD skip, WORD step, DWORD page) { return (DWORD)-1; }

BOOL ChangeMapFlushing(LPCVOID p, DWORD f) { return FALSE; }
void CloseMappedFileHandles(void) {}
BOOL TryCloseMappedHandle(HANDLE h) { return FALSE; }
int MappedPageIn(BOOL bWrite, DWORD addr) { return 0; }
void PageOutFile(void) {}

const PFNVOID MapMthds[16];
const DWORD MapSigs[16];
BOOL SC_FlushViewOfFile(LPCVOID p, DWORD n) { return TRUE; }
BOOL SC_FlushViewOfFileMaybe(LPCVOID p, DWORD n) { return FALSE; }


LPVOID HugeVirtualReserve(DWORD size, DWORD flags) { return NULL; }
BOOL HugeVirtualRelease(LPVOID p) { return FALSE; }

DWORD SC_GetTickCount(void) { return CurMSec; }

void DebugBreak(void)
{
    for (;;)
        ;
}

/* software-only first-level page table: the kernel still keeps its bookkeeping there, the ARM946 never reads it */
ulong g_FirstPT[4096];
