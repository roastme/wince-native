/*
 * startmenu.c - Start menu for the DSi shell (ThroatyMumbo's startmenu.c, reduced).
 * Row 0 opens My Storage; the rows below it are the .exe files in PROGRAMS_DIR.  A tap launches the row and closes
 * the menu; a tap anywhere else (desktop, taskbar, file browser) closes it too.
 */
#include "dsshell.h"

#define MENU_W    128
#define ROW_H     22
#define MAX_ROWS  12
/* The menu must stay on the bottom screen: row 0 plus programs, at most (TASKBAR_Y - SHELL_Y - 4) / ROW_H rows. */
#define MENU_ROWS ((TASKBAR_Y - SHELL_Y - 4) / ROW_H)

static HWND  g_menu;
static HBRUSH g_menu_brush;
static WCHAR g_prog[MAX_ROWS][MAX_PATH];
static int   g_nprog;

static LRESULT CALLBACK StartMenu_WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

BOOL StartMenu_Register(HINSTANCE hInst)
{
    WNDCLASSW wc;

    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc   = StartMenu_WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(192, 192, 192));
    wc.lpszClassName = L"DsStartMenu";
    g_menu_brush = wc.hbrBackground;
    return RegisterClassW(&wc) != 0;
}

static void load_programs(void)
{
    WCHAR pat[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE h;

    g_nprog = 0;
    if (!Shell_JoinPath(pat, MAX_PATH, PROGRAMS_DIR, L"*")) return;
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (g_nprog < MENU_ROWS - 1 && !(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            Shell_HasExtCI(fd.cFileName, 'e', 'x', 'e'))
            Shell_Wcopyn(g_prog[g_nprog++], fd.cFileName, MAX_PATH);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

void StartMenu_Open(void)
{
    int rows, h;

    if (g_menu) return;
    load_programs();
    rows = 1 + g_nprog;
    h = rows * ROW_H + 4;
    g_menu = CreateWindowExW(WS_EX_TOPMOST, L"DsStartMenu", L"", WS_POPUP, 0, TASKBAR_Y - h, MENU_W, h,
                             NULL, NULL, g_hInst, NULL);
    if (g_menu) {
        ShowWindow(g_menu, SW_SHOW);
        UpdateWindow(g_menu);
    }
    InvalidateRect(g_hwndTaskBar, NULL, TRUE);
}

void StartMenu_Close(void)
{
    HWND h = g_menu;
    g_menu = NULL;
    if (h) DestroyWindow(h);
    InvalidateRect(g_hwndTaskBar, NULL, TRUE);
}

BOOL StartMenu_IsVisible(void)
{
    return g_menu != NULL;
}

static LRESULT CALLBACK StartMenu_WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        RECT rc, row;
        int i;
        HDC hdc = BeginPaint(hwnd, &ps);

        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, g_menu_brush);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0, 0, 0));
        for (i = 0; i <= g_nprog; i++) {
            SetRect(&row, 2, 2 + i * ROW_H, rc.right - 2, 2 + (i + 1) * ROW_H);
            if (i == 0) DrawTextW(hdc, L"My Storage", -1, &row, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            else {
                /* Show the name without its ".exe". */
                int n = Shell_Wlen(g_prog[i - 1]) - 4;
                row.left += 4;
                DrawTextW(hdc, g_prog[i - 1], n, &row, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int row = (HIWORD(lp) - 2) / ROW_H;
        WCHAR path[MAX_PATH];

        if (row == 0) {
            StartMenu_Close();
            FileBrowser_Open(STORAGE_ROOT);
        } else if (row > 0 && row <= g_nprog) {
            Shell_JoinPath(path, MAX_PATH, PROGRAMS_DIR, g_prog[row - 1]);
            StartMenu_Close();
            Shell_LaunchExe(path);
        }
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
