/*
 * desktop.c - Desktop window for the DSi shell (ThroatyMumbo's desktop.c, touch-adapted).
 * Full width, DESK_H rows, teal background, a column of icons on the left.  Taps are handled by iconview.c.
 */
#include "dsshell.h"
#include "iconview.h"

static HBRUSH g_desk_brush;

static LRESULT CALLBACK Desktop_WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

static void Desktop_OpenStorage(HWND owner) { (void)owner; FileBrowser_Open(STORAGE_ROOT); }
static void Desktop_OpenPrograms(HWND owner) { (void)owner; FileBrowser_Open(PROGRAMS_DIR); }

BOOL Desktop_Register(HINSTANCE hInst)
{
    WNDCLASSW wc;

    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc   = Desktop_WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(DESKTOP_BG);
    wc.lpszClassName = L"DsDesktop";
    g_desk_brush = wc.hbrBackground;
    return RegisterClassW(&wc) != 0;
}

HWND Desktop_Create(void)
{
    HWND h = CreateWindowExW(0, L"DsDesktop", L"", WS_POPUP, 0, SHELL_Y, SHELL_W, DESK_H, NULL, NULL, g_hInst, NULL);
    if (h) {
        ShowWindow(h, SW_SHOW);
        UpdateWindow(h);
    }
    return h;
}

static LRESULT CALLBACK Desktop_WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    IconView *iv = (IconView *)GetWindowLongW(hwnd, GWL_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        IconView *new_iv = IconView_Create(hwnd, ICONVIEW_LAYOUT_COLUMN, 0);
        if (!new_iv) return -1;
        IconView_AddVirtualItem(new_iv, L"My Storage", Shell_Icon(IDI_DRIVE), Desktop_OpenStorage);
        IconView_AddVirtualItem(new_iv, L"Programs", Shell_Icon(IDI_FOLDER), Desktop_OpenPrograms);
        SetWindowLongW(hwnd, GWL_USERDATA, (LONG)new_iv);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        RECT rc;
        HDC hdc = BeginPaint(hwnd, &ps);
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, g_desk_brush);
        IconView_OnPaint(iv, hdc, &rc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN:
        if (StartMenu_IsVisible()) {
            StartMenu_Close();
            return 0;
        }
        IconView_OnLButtonDown(iv, LOWORD(lp), HIWORD(lp));
        return 0;

    case WM_DESTROY:
        SetWindowLongW(hwnd, GWL_USERDATA, 0);
        IconView_Destroy(iv);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
