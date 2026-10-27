/*
 * filebrowser.c - File browser window for the DSi shell (ThroatyMumbo's filebrowser.c, touch-adapted).
 * A full-width window on the bottom screen with a title strip (the folder path and an X to close) and an icon grid.
 * Each window owns its IconView in GWL_USERDATA.
 */
#include "dsshell.h"
#include "iconview.h"

#define CLOSE_W 16

static HBRUSH g_title_brush, g_face_brush;

static LRESULT CALLBACK FileBrowser_WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

BOOL FileBrowser_Register(HINSTANCE hInst)
{
    WNDCLASSW wc;

    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc   = FileBrowser_WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(255, 255, 255));
    wc.lpszClassName = L"DsFileBrowser";
    g_face_brush  = CreateSolidBrush(RGB(255, 255, 255));
    g_title_brush = CreateSolidBrush(RGB(0, 0, 128));
    return RegisterClassW(&wc) != 0;
}

HWND FileBrowser_Open(LPCWSTR path)
{
    HWND h = CreateWindowExW(0, L"DsFileBrowser", L"", WS_POPUP, 0, SHELL_Y, SHELL_W, DESK_H,
                             NULL, NULL, g_hInst, (LPVOID)path);
    if (h) {
        ShowWindow(h, SW_SHOW);
        UpdateWindow(h);
        SetForegroundWindow(h);
    }
    return h;
}

static LRESULT CALLBACK FileBrowser_WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    IconView *iv = (IconView *)GetWindowLongW(hwnd, GWL_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lp;
        IconView *new_iv = IconView_Create(hwnd, ICONVIEW_LAYOUT_GRID, TITLE_H);
        if (!new_iv) return -1;
        SetWindowLongW(hwnd, GWL_USERDATA, (LONG)new_iv);
        IconView_SetPath(new_iv, (LPCWSTR)cs->lpCreateParams);
        TaskBar_AddWindow(hwnd);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        RECT rc, bar, text, close, body;
        WCHAR title[MAX_PATH];
        HDC hdc = BeginPaint(hwnd, &ps);

        GetClientRect(hwnd, &rc);
        SetRect(&bar, rc.left, rc.top, rc.right, rc.top + TITLE_H);
        FillRect(hdc, &bar, g_title_brush);
        SetRect(&text, bar.left + 4, bar.top, bar.right - CLOSE_W - 2, bar.bottom);
        SetRect(&close, bar.right - CLOSE_W, bar.top + 1, bar.right - 1, bar.bottom - 1);
        GetWindowTextW(hwnd, title, MAX_PATH);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        DrawTextW(hdc, title, -1, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawEdge(hdc, &close, EDGE_RAISED, BF_RECT);
        DrawTextW(hdc, L"X", -1, &close, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SetRect(&body, rc.left, rc.top + TITLE_H, rc.right, rc.bottom);
        FillRect(hdc, &body, g_face_brush);
        IconView_OnPaint(iv, hdc, &rc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = LOWORD(lp), y = HIWORD(lp);
        if (StartMenu_IsVisible()) StartMenu_Close();
        if (y < TITLE_H) {
            if (x >= SHELL_W - CLOSE_W) DestroyWindow(hwnd);
            return 0;
        }
        IconView_OnLButtonDown(iv, x, y);
        return 0;
    }

    case WM_DESTROY:
        TaskBar_RemoveWindow(hwnd);
        SetWindowLongW(hwnd, GWL_USERDATA, 0);
        IconView_Destroy(iv);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
