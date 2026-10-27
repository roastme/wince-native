/*
 * taskbar.c - Bottom bar for the DSi shell (ThroatyMumbo's taskbar.c, touch-adapted).
 * Start button on the left, one button per open file browser in the middle, clock on the right.
 * Open windows are tracked by the shell (TaskBar_AddWindow / TaskBar_RemoveWindow) rather than read from GWES z-order.
 */
#include "dsshell.h"

#define START_X     2
#define START_Y     2
#define START_W     64
#define START_H     20
#define CLOCK_W     44
#define BTN_AREA_L  (START_X + START_W + 4)
#define BTN_AREA_R  (SHELL_W - CLOCK_W - 6)
#define BTN_MAX_W   72
#define BTN_MIN_W   40
#define BTN_GAP     2
#define MAX_TRACKED 6
#define TIMER_CLOCK 2

static HWND  g_btn[MAX_TRACKED];
static int   g_nbtn;
static HBRUSH g_bar_brush;

static LRESULT CALLBACK TaskBar_WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

BOOL TaskBar_Register(HINSTANCE hInst)
{
    WNDCLASSW wc;

    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc   = TaskBar_WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(192, 192, 192));
    wc.lpszClassName = L"DsTaskBar";
    g_bar_brush = wc.hbrBackground;
    return RegisterClassW(&wc) != 0;
}

HWND TaskBar_Create(void)
{
    HWND h = CreateWindowExW(WS_EX_TOPMOST, L"DsTaskBar", L"", WS_POPUP, 0, TASKBAR_Y, SHELL_W, TASKBAR_H,
                             NULL, NULL, g_hInst, NULL);
    if (h) {
        ShowWindow(h, SW_SHOW);
        UpdateWindow(h);
    }
    return h;
}

void TaskBar_AddWindow(HWND hwnd)
{
    if (g_nbtn < MAX_TRACKED) g_btn[g_nbtn++] = hwnd;
    InvalidateRect(g_hwndTaskBar, NULL, TRUE);
}

void TaskBar_RemoveWindow(HWND hwnd)
{
    int i;
    for (i = 0; i < g_nbtn; i++) {
        if (g_btn[i] == hwnd) {
            for (; i < g_nbtn - 1; i++) g_btn[i] = g_btn[i + 1];
            g_nbtn--;
            break;
        }
    }
    InvalidateRect(g_hwndTaskBar, NULL, TRUE);
}

static void start_rect(RECT *rc)
{
    SetRect(rc, START_X, START_Y, START_X + START_W, START_Y + START_H);
}

static void clock_rect(RECT *rc)
{
    SetRect(rc, SHELL_W - CLOCK_W - 2, 2, SHELL_W - 2, TASKBAR_H - 2);
}

/* Window buttons share the strip between Start and the clock; width shrinks with the count, clamped. */
static void btn_rect(int i, RECT *rc)
{
    int avail = BTN_AREA_R - BTN_AREA_L;
    int step = (avail + BTN_GAP) / g_nbtn;
    int w = step - BTN_GAP;
    int x;

    if (w > BTN_MAX_W) w = BTN_MAX_W;
    if (w < BTN_MIN_W) w = BTN_MIN_W;
    x = BTN_AREA_L + i * (w + BTN_GAP);
    SetRect(rc, x, START_Y, x + w, START_Y + START_H);
}

static void paint_bar(HDC hdc)
{
    RECT rc;
    int i;
    BOOL pressed = StartMenu_IsVisible();

    GetClientRect(g_hwndTaskBar, &rc);
    FillRect(hdc, &rc, g_bar_brush);

    start_rect(&rc);
    DrawEdge(hdc, &rc, pressed ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT);
    DrawIconEx(hdc, rc.left + 4 + pressed, rc.top + 2 + pressed, Shell_Icon(IDI_START), 16, 16, 0, NULL, DI_NORMAL);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));
    rc.left += 24 + pressed;
    DrawTextW(hdc, L"Start", -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    for (i = 0; i < g_nbtn; i++) {
        WCHAR title[64];
        RECT b;
        btn_rect(i, &b);
        DrawEdge(hdc, &b, g_btn[i] == GetForegroundWindow() ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT);
        if (GetWindowTextW(g_btn[i], title, 64) == 0) lstrcpyW(title, L"(untitled)");
        b.left += 4; b.right -= 4;
        DrawTextW(hdc, title, -1, &b, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    {
        SYSTEMTIME st;
        WCHAR text[16];
        UINT h12;

        GetLocalTime(&st);
        h12 = st.wHour % 12;
        if (h12 == 0) h12 = 12;
        wsprintfW(text, L"%u:%02u", h12, st.wMinute);
        clock_rect(&rc);
        DrawTextW(hdc, text, -1, &rc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }
}

static LRESULT CALLBACK TaskBar_WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        SetTimer(hwnd, TIMER_CLOCK, 30000, NULL);
        return 0;

    case WM_TIMER:
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;

    case WM_LBUTTONDOWN: {
        POINT pt;
        RECT rc;
        int i;

        pt.x = LOWORD(lp);
        pt.y = HIWORD(lp);
        start_rect(&rc);
        if (PtInRect(&rc, pt)) {
            if (StartMenu_IsVisible()) StartMenu_Close();
            else StartMenu_Open();
            return 0;
        }
        if (StartMenu_IsVisible()) StartMenu_Close();
        for (i = 0; i < g_nbtn; i++) {
            btn_rect(i, &rc);
            if (PtInRect(&rc, pt)) {
                SetForegroundWindow(g_btn[i]);
                break;
            }
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        paint_bar(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
