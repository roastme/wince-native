/* crt0.c - entry point of a GCC-built CE application. The loader calls it like WinMainCRTStartup(hInst, hPrev, lpCmdLine, nShow). */
#include <windows.h>

extern int WINAPI WinMain(HINSTANCE, HINSTANCE, LPWSTR, int);

int WinMainCRTStartup(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR cmd, int show)
{
    return WinMain(hInst, hPrev, cmd, show);
}
