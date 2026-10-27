/*
 * shell_main.c - WinMain for the DSi shell (ThroatyMumbo's shell_main.c, touch-adapted).
 * Registers the window classes, creates the desktop and the taskbar, then runs one message loop.
 * The startup chime, the Tahoma font and the wait for the wave/filesystem services are gone: the mini GWES has its
 * own font and starts with the kernel, and the FAT volume is mounted before the shell runs.
 */
#include "dsshell.h"

HINSTANCE g_hInst;
HWND      g_hwndDesktop;
HWND      g_hwndTaskBar;

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR cmd, int show)
{
    MSG msg;

    g_hInst = hInst;
    Shell_RegisterIcons();
    if (!Desktop_Register(hInst) || !TaskBar_Register(hInst) || !FileBrowser_Register(hInst) || !StartMenu_Register(hInst)) {
        OutputDebugStringW(L"dsshell: RegisterClass failed\r\n");
        return 1;
    }
    g_hwndDesktop = Desktop_Create();
    g_hwndTaskBar = TaskBar_Create();
    if (!g_hwndDesktop || !g_hwndTaskBar) {
        OutputDebugStringW(L"dsshell: window creation failed\r\n");
        return 1;
    }
    OutputDebugStringW(L"dsshell: entering message loop\r\n");
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
