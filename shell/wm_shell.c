/* A Windows Mobile 6 style shell (Today screen, Start menu, soft keys, a few built-in programs) written against the Win32
 * subset provided by gwes/. The 256x384 desktop is the two DSi screens stacked; the stylus works on the lower half
 * (y >= 192), the buttons drive the same UI: D-pad = move, A = select, L/R = left/right soft key, START = Start menu. */
#include <windows.h>
#include "../gwes/gwes.h"

extern int WINAPIV wsprintfW(LPWSTR, LPCWSTR, ...);
extern void NKDbgPrintfW(const wchar_t *, ...);
extern DWORD WINAPI GetTickCount(void);

#define SW_  GW_SCREEN_W
#define SH_  GW_SCREEN_H
#define TITLE_H 24
#define SOFT_H  26

#define C_TITLE  RGB(0, 60, 130)
#define C_TITLE2 RGB(0, 90, 170)
#define C_BODY   RGB(255, 255, 255)
#define C_SOFT   RGB(40, 40, 48)
#define C_SEL    RGB(255, 214, 120)
#define C_TEXT   RGB(0, 0, 0)
#define C_GREY   RGB(120, 120, 120)
#define C_LINE   RGB(200, 200, 210)

/* ---- colour brushes ---- */
static HBRUSH brush(COLORREF c)
{
    static struct { COLORREF c; HBRUSH b; } cache[24];
    int i;
    for (i = 0; i < 24; i++) {
        if (cache[i].b && cache[i].c == c) return cache[i].b;
        if (!cache[i].b) { cache[i].c = c; cache[i].b = CreateSolidBrush(c); return cache[i].b; }
    }
    return CreateSolidBrush(c);
}

static void box(HDC dc, int l, int t, int r, int b, COLORREF c)
{
    RECT rc;
    SetRect(&rc, l, t, r, b);
    FillRect(dc, &rc, brush(c));
}

static void text(HDC dc, int x, int y, const WCHAR *s, COLORREF fg)
{
    RECT rc;
    SetRect(&rc, x, y, SW_, y + 14);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, fg);
    DrawTextW(dc, s, -1, &rc, DT_LEFT | DT_SINGLELINE | DT_NOCLIP);
}

static HFONT big_font(void)
{
    static HFONT f;
    if (!f) {
        LOGFONTW lf;
        memset(&lf, 0, sizeof lf);
        lf.lfHeight = 16; lf.lfWeight = FW_NORMAL;
        f = CreateFontIndirectW(&lf);
    }
    return f;
}

static void text_in(HDC dc, const RECT *r, const WCHAR *s, COLORREF fg, UINT fmt)
{
    RECT rc = *r;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, fg);
    DrawTextW(dc, s, -1, &rc, fmt | DT_SINGLELINE | DT_VCENTER);
}

/* ---- icons: 16x16 Windows-flag logo built at startup ---- */
static unsigned short g_logo[16 * 16];
#define ICON_LOGO 1
static void make_icons(void)
{
    int x, y;
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++) {
            unsigned short c = 0;
            if (x >= 1 && x <= 14 && y >= 1 && y <= 14 && x != 7 && x != 8 && y != 7 && y != 8) {
                int q = (x > 7) + 2 * (y > 7);
                static const COLORREF qc[4] = { RGB(240, 80, 40), RGB(110, 200, 40), RGB(40, 140, 240), RGB(250, 200, 30) };
                c = 0x8000 | ((qc[q] & 0xff) >> 3) | ((((qc[q] >> 8) & 0xff) >> 3) << 5) | ((((qc[q] >> 16) & 0xff) >> 3) << 10);
            }
            g_logo[y * 16 + x] = c;
        }
    Gwes_RegisterIcon(ICON_LOGO, 16, 16, g_logo);
}

/* ---- common chrome ---- */
static HICON g_logo_icon;

static void clock_str(WCHAR *out)
{
    SYSTEMTIME st;
    UINT h12;
    GetLocalTime(&st);
    h12 = st.wHour % 12; if (!h12) h12 = 12;
    wsprintfW(out, L"%d:%02d %s", h12, st.wMinute, st.wHour < 12 ? L"AM" : L"PM");
}

static void draw_titlebar(HDC dc, const WCHAR *title, int show_start)
{
    WCHAR clk[16];
    RECT rc;

    box(dc, 0, 0, SW_, TITLE_H, C_TITLE);
    box(dc, 0, TITLE_H - 2, SW_, TITLE_H, C_TITLE2);
    if (show_start) {
        DrawIconEx(dc, 4, 4, g_logo_icon, 16, 16, 0, NULL, DI_NORMAL);
        text(dc, 24, 5, L"Start", RGB(255, 255, 255));
        SetRect(&rc, 70, 0, SW_ - 70, TITLE_H);
        if (title) text_in(dc, &rc, title, RGB(255, 255, 255), DT_CENTER);
    } else if (title) {
        text(dc, 6, 5, title, RGB(255, 255, 255));
    }
    clock_str(clk);
    SetRect(&rc, SW_ - 90, 0, SW_ - 4, TITLE_H);
    text_in(dc, &rc, clk, RGB(255, 255, 255), DT_RIGHT);
}

static void draw_softkeys(HDC dc, const WCHAR *left, const WCHAR *right)
{
    RECT rc;
    box(dc, 0, SH_ - SOFT_H, SW_, SH_, C_SOFT);
    if (left) { SetRect(&rc, 8, SH_ - SOFT_H, SW_ / 2, SH_); text_in(dc, &rc, left, RGB(255, 255, 255), DT_LEFT); }
    if (right) { SetRect(&rc, SW_ / 2, SH_ - SOFT_H, SW_ - 8, SH_); text_in(dc, &rc, right, RGB(255, 255, 255), DT_RIGHT); }
}

static HWND g_today, g_start;
static const WCHAR c_today[] = L"WMToday", c_menu[] = L"WMStartMenu", c_app[] = L"WMApp";

typedef LRESULT (*APPPROC)(HWND, UINT, WPARAM, LPARAM);

static void open_app(int id);
static void toggle_start(void);

/* ---- Today screen ---- */
static int g_sel;
#define TODAY_ITEMS 5
#define ROW_Y(i) (204 + (i) * 30)

static const WCHAR *day_names[] = { L"Sunday", L"Monday", L"Tuesday", L"Wednesday", L"Thursday", L"Friday", L"Saturday" };
static const WCHAR *mon_names[] = { L"January", L"February", L"March", L"April", L"May", L"June", L"July", L"August",
                                    L"September", L"October", L"November", L"December" };

static void today_paint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    SYSTEMTIME st;
    WCHAR buf[64];
    int i;
    static const WCHAR *rows[TODAY_ITEMS] = { NULL, L"Tap to set owner info", L"No unread messages",
                                              L"No upcoming appointments", L"No tasks" };

    GetLocalTime(&st);
    box(dc, 0, TITLE_H, SW_, SH_ - SOFT_H, C_BODY);
    draw_titlebar(dc, NULL, 1);
    {
        RECT rc;
        HFONT old = (HFONT)SelectObject(dc, big_font());
        clock_str(buf);
        SetRect(&rc, 0, TITLE_H + 16, SW_, TITLE_H + 56);
        text_in(dc, &rc, buf, C_TITLE, DT_CENTER);
        SelectObject(dc, old);
        wsprintfW(buf, L"%s", day_names[st.wDayOfWeek]);
        SetRect(&rc, 0, TITLE_H + 64, SW_, TITLE_H + 84);
        text_in(dc, &rc, buf, C_TEXT, DT_CENTER);
        wsprintfW(buf, L"%s %d, %d", mon_names[(st.wMonth - 1) % 12], st.wDay, st.wYear);
        SetRect(&rc, 0, TITLE_H + 84, SW_, TITLE_H + 104);
        text_in(dc, &rc, buf, C_TEXT, DT_CENTER);
    }
    box(dc, 6, TITLE_H + 120, SW_ - 6, TITLE_H + 121, C_LINE);
    text(dc, 10, TITLE_H + 132, L"Windows CE 5.0 (native)", C_GREY);
    text(dc, 10, TITLE_H + 148, L"Nintendo DSi, ARM946E-S", C_GREY);
    for (i = 0; i < TODAY_ITEMS; i++) {
        RECT rc;
        if (i == g_sel) box(dc, 4, ROW_Y(i) - 4, SW_ - 4, ROW_Y(i) + 25, C_SEL);
        if (i == 0) {
            text(dc, 12, ROW_Y(i) + 2, L"Date & Time", C_TEXT);
            SetRect(&rc, 0, ROW_Y(i), SW_ - 12, ROW_Y(i) + 22);
            clock_str(buf);
            text_in(dc, &rc, buf, C_GREY, DT_RIGHT);
        } else {
            text(dc, 12, ROW_Y(i) + 2, rows[i], i == 1 ? C_TITLE : C_TEXT);
        }
        box(dc, 6, ROW_Y(i) + 25, SW_ - 6, ROW_Y(i) + 26, C_LINE);
    }
    draw_softkeys(dc, L"Start", L"Contacts");
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK today_proc(HWND hwnd, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_CREATE: SetTimer(hwnd, 1, 1000, NULL); return 0;
    case WM_TIMER: {
        RECT rc;
        SetRect(&rc, SW_ - 90, 0, SW_, TITLE_H);
        InvalidateRect(hwnd, &rc, FALSE);
        return 0;
    }
    case WM_PAINT: today_paint(hwnd); return 0;
    case WM_LBUTTONDOWN: {
        int x = LOWORD(lp), y = HIWORD(lp), i;
        if (y < TITLE_H) { if (x < 64) toggle_start(); return 0; }
        if (y >= SH_ - SOFT_H) { if (x < SW_ / 2) toggle_start(); else open_app(11); return 0; }
        for (i = 0; i < TODAY_ITEMS; i++)
            if (y >= ROW_Y(i) - 4 && y < ROW_Y(i) + 26) {
                g_sel = i;
                InvalidateRect(hwnd, NULL, FALSE);
                open_app(i == 0 ? 4 : i == 1 ? 3 : i == 3 ? 2 : 0);
                break;
            }
        return 0;
    }
    case WM_KEYDOWN:
        switch (wp) {
        case VK_UP: if (g_sel > 0) { g_sel--; InvalidateRect(hwnd, NULL, FALSE); } break;
        case VK_DOWN: if (g_sel < TODAY_ITEMS - 1) { g_sel++; InvalidateRect(hwnd, NULL, FALSE); } break;
        case VK_RETURN: open_app(g_sel == 0 ? 4 : g_sel == 1 ? 3 : g_sel == 3 ? 2 : 0); break;
        case VK_LWIN: toggle_start(); break;
        case VK_F1: toggle_start(); break;
        case VK_F2: open_app(11); break;
        }
        return 0;
    }
    return DefWindowProcW(hwnd, m, wp, lp);
}

/* ---- Start menu ---- */
static const struct { const WCHAR *name; int app; } start_items[] = {
    { L"Today", 0 }, { L"Calendar", 2 }, { L"Calculator", 1 }, { L"Clock", 4 }, { L"About", 3 }, { L"Settings", 12 },
};
#define NSTART (int)(sizeof start_items / sizeof start_items[0])
#define MENU_W 150
#define MENU_ROW 28
static int g_msel;

static void menu_paint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    int i;
    box(dc, 0, 0, MENU_W, NSTART * MENU_ROW + 2, RGB(250, 250, 250));
    for (i = 0; i < NSTART; i++) {
        if (i == g_msel) box(dc, 1, 1 + i * MENU_ROW, MENU_W - 1, 1 + (i + 1) * MENU_ROW, C_SEL);
        box(dc, 8, 6 + i * MENU_ROW, 24, 22 + i * MENU_ROW, i == 0 ? C_TITLE : RGB(80 + i * 25, 130, 200 - i * 20));
        text(dc, 32, 7 + i * MENU_ROW, start_items[i].name, C_TEXT);
    }
    box(dc, 0, 0, MENU_W, 1, C_GREY); box(dc, 0, 0, 1, NSTART * MENU_ROW + 2, C_GREY);
    box(dc, MENU_W - 1, 0, MENU_W, NSTART * MENU_ROW + 2, C_GREY); box(dc, 0, NSTART * MENU_ROW + 1, MENU_W, NSTART * MENU_ROW + 2, C_GREY);
    EndPaint(hwnd, &ps);
}

static void close_start(void)
{
    if (g_start) { HWND h = g_start; g_start = NULL; ReleaseCapture(); DestroyWindow(h); }
}

static void menu_pick(int i)
{
    int app = start_items[i].app;
    close_start();
    open_app(app);
}

static LRESULT CALLBACK menu_proc(HWND hwnd, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_PAINT: menu_paint(hwnd); return 0;
    case WM_LBUTTONDOWN: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
        if (x < 0 || x >= MENU_W || y < 0 || y >= NSTART * MENU_ROW + 2) close_start();
        else menu_pick((y - 1) / MENU_ROW);
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_UP && g_msel > 0) { g_msel--; InvalidateRect(hwnd, NULL, FALSE); }
        else if (wp == VK_DOWN && g_msel < NSTART - 1) { g_msel++; InvalidateRect(hwnd, NULL, FALSE); }
        else if (wp == VK_RETURN) menu_pick(g_msel);
        else if (wp == VK_ESCAPE || wp == VK_LWIN) close_start();
        return 0;
    }
    return DefWindowProcW(hwnd, m, wp, lp);
}

static void toggle_start(void)
{
    if (g_start) { close_start(); return; }
    g_msel = 0;
    g_start = CreateWindowExW(WS_EX_TOPMOST, c_menu, L"", WS_POPUP, 2, SH_ - SOFT_H - (NSTART * MENU_ROW + 2), MENU_W, NSTART * MENU_ROW + 2, NULL, NULL, NULL, NULL);
    ShowWindow(g_start, SW_SHOW);
    SetCapture(g_start);          /* taps outside the menu close it */
    SetFocus(g_start);
}

/* ---- built-in programs: one generic top-level window class, state kept in GWL_USERDATA (app id) ---- */
static int g_calc_acc, g_calc_cur, g_calc_op, g_calc_new = 1;

static const WCHAR *calc_keys[5][4] = {
    { L"C", L"+/-", L"%", L"/" }, { L"7", L"8", L"9", L"x" }, { L"4", L"5", L"6", L"-" }, { L"1", L"2", L"3", L"+" }, { L"0", L"0", L".", L"=" },
};
#define CALC_Y  196
#define CALC_CELL_W 60
#define CALC_CELL_H 32

static void calc_press(const WCHAR *k)
{
    WCHAR c = k[0];
    if (c >= L'0' && c <= L'9') {
        g_calc_cur = (g_calc_new ? 0 : g_calc_cur) * 10 + (c - L'0');
        g_calc_new = 0;
    } else if (c == L'C') { g_calc_acc = g_calc_cur = g_calc_op = 0; g_calc_new = 1; }
    else if (c == L'+' && k[1] == L'/') g_calc_cur = -g_calc_cur;
    else if (c == L'%') g_calc_cur /= 100;
    else if (c == L'.') { }
    else {
        if (g_calc_op && !g_calc_new) {
            switch (g_calc_op) {
            case '+': g_calc_acc += g_calc_cur; break;
            case '-': g_calc_acc -= g_calc_cur; break;
            case 'x': g_calc_acc *= g_calc_cur; break;
            case '/': g_calc_acc = g_calc_cur ? g_calc_acc / g_calc_cur : 0; break;
            }
        } else if (!g_calc_op) g_calc_acc = g_calc_cur;
        g_calc_cur = g_calc_acc;
        g_calc_op = (c == L'=') ? 0 : (int)c;
        g_calc_new = 1;
    }
}

static int dow(int y, int m, int d)           /* 0 = Sunday */
{
    static const int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    if (m < 3) y--;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

static void app_paint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    int id = GetWindowLongW(hwnd, GWL_USERDATA), i, j;
    WCHAR buf[64];
    SYSTEMTIME st;

    GetLocalTime(&st);
    box(dc, 0, TITLE_H, SW_, SH_ - SOFT_H, C_BODY);
    switch (id) {
    case 1:
        draw_titlebar(dc, L"Calculator", 0);
        box(dc, 6, TITLE_H + 30, SW_ - 6, TITLE_H + 90, RGB(235, 240, 235));
        {
            RECT rc;
            HFONT old = (HFONT)SelectObject(dc, big_font());
            SetRect(&rc, 6, TITLE_H + 30, SW_ - 12, TITLE_H + 90);
            wsprintfW(buf, L"%d", g_calc_cur);
            text_in(dc, &rc, buf, C_TEXT, DT_RIGHT);
            SelectObject(dc, old);
        }
        for (i = 0; i < 5; i++)
            for (j = 0; j < 4; j++) {
                RECT rc;
                if (i == 4 && j == 0) continue;
                SetRect(&rc, 8 + j * CALC_CELL_W, CALC_Y + i * CALC_CELL_H, 8 + (j + 1) * CALC_CELL_W - 4, CALC_Y + (i + 1) * CALC_CELL_H - 4);
                if (i == 4 && j == 1) rc.left -= CALC_CELL_W;
                box(dc, rc.left, rc.top, rc.right, rc.bottom, j == 3 ? C_TITLE2 : RGB(225, 228, 235));
                text_in(dc, &rc, calc_keys[i][j], j == 3 ? RGB(255, 255, 255) : C_TEXT, DT_CENTER);
            }
        break;
    case 2: {
        int first = dow(st.wYear, st.wMonth, 1), days, d;
        static const int mdays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        static const WCHAR *hd = L"SMTWTFS";
        days = mdays[st.wMonth - 1] + ((st.wMonth == 2 && st.wYear % 4 == 0) ? 1 : 0);
        draw_titlebar(dc, L"Calendar", 0);
        wsprintfW(buf, L"%s %d", mon_names[st.wMonth - 1], st.wYear);
        { RECT rc; SetRect(&rc, 0, TITLE_H + 6, SW_, TITLE_H + 28); text_in(dc, &rc, buf, C_TEXT, DT_CENTER); }
        for (i = 0; i < 7; i++) { WCHAR h[2]; h[0] = hd[i]; h[1] = 0; text(dc, 14 + i * 34, TITLE_H + 40, h, C_GREY); }
        for (d = 1; d <= days; d++) {
            int cell = first + d - 1, cx = 8 + (cell % 7) * 34, cy = TITLE_H + 58 + (cell / 7) * 34;
            if (d == st.wDay) box(dc, cx, cy, cx + 30, cy + 30, C_SEL);
            wsprintfW(buf, L"%d", d);
            { RECT rc; SetRect(&rc, cx, cy, cx + 30, cy + 30); text_in(dc, &rc, buf, (cell % 7 == 0 || cell % 7 == 6) ? C_TITLE : C_TEXT, DT_CENTER); }
        }
        break;
    }
    case 3:
        draw_titlebar(dc, L"About", 0);
        text(dc, 10, TITLE_H + 12, L"Windows CE 5.0 kernel", C_TEXT);
        text(dc, 10, TITLE_H + 28, L"running natively on the", C_TEXT);
        text(dc, 10, TITLE_H + 44, L"Nintendo DSi ARM946E-S", C_TEXT);
        text(dc, 10, TITLE_H + 66, L"No MMU: flat address", C_GREY);
        text(dc, 10, TITLE_H + 82, L"space, MPU only.", C_GREY);
        wsprintfW(buf, L"Uptime: %d s", GetTickCount() / 1000);
        text(dc, 10, TITLE_H + 110, buf, C_TEXT);
        wsprintfW(buf, L"Frames: %d", Gwes_Frames);
        text(dc, 10, TITLE_H + 126, buf, C_TEXT);
        break;
    case 4:
        draw_titlebar(dc, L"Clock", 0);
        clock_str(buf);
        { RECT rc; SetRect(&rc, 0, TITLE_H + 40, SW_, TITLE_H + 100); text_in(dc, &rc, buf, C_TITLE, DT_CENTER); }
        wsprintfW(buf, L"%s, %s %d", day_names[st.wDayOfWeek], mon_names[st.wMonth - 1], st.wDay);
        { RECT rc; SetRect(&rc, 0, TITLE_H + 110, SW_, TITLE_H + 140); text_in(dc, &rc, buf, C_TEXT, DT_CENTER); }
        break;
    case 10: case 11: case 12:
        draw_titlebar(dc, id == 10 ? L"Phone" : id == 11 ? L"Contacts" : L"Settings", 0);
        text(dc, 10, TITLE_H + 14, id == 10 ? L"Phone is off." : id == 11 ? L"No contacts." : L"Nothing to set yet.", C_GREY);
        break;
    }
    draw_softkeys(dc, id == 1 ? L"Clear" : NULL, L"OK");
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK app_proc(HWND hwnd, UINT m, WPARAM wp, LPARAM lp)
{
    int id = GetWindowLongW(hwnd, GWL_USERDATA);
    switch (m) {
    case WM_CREATE: SetTimer(hwnd, 1, 1000, NULL); return 0;
    case WM_TIMER: InvalidateRect(hwnd, NULL, FALSE); return 0;
    case WM_PAINT: app_paint(hwnd); return 0;
    case WM_LBUTTONDOWN: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
        if (y >= SH_ - SOFT_H) {
            if (x >= SW_ / 2) DestroyWindow(hwnd);
            else if (id == 1) { calc_press(L"C"); InvalidateRect(hwnd, NULL, FALSE); }
            return 0;
        }
        if (id == 1 && y >= CALC_Y) {
            int i = (y - CALC_Y) / CALC_CELL_H, j = (x - 8) / CALC_CELL_W;
            if (i >= 0 && i < 5 && j >= 0 && j < 4) {
                if (i == 4 && j == 0) j = 1;
                calc_press(calc_keys[i][j]);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE || wp == VK_F2) DestroyWindow(hwnd);
        else if (id == 1 && wp >= '0' && wp <= '9') { WCHAR k[2]; k[0] = (WCHAR)wp; k[1] = 0; calc_press(k); InvalidateRect(hwnd, NULL, FALSE); }
        return 0;
    }
    return DefWindowProcW(hwnd, m, wp, lp);
}

static void open_app(int id)
{
    HWND h;
    if (id == 0) { SetForegroundWindow(g_today); return; }
    if (id == 1) { g_calc_acc = g_calc_cur = g_calc_op = 0; g_calc_new = 1; }
    h = CreateWindowExW(0, c_app, L"", WS_POPUP, 0, 0, SW_, SH_, NULL, NULL, NULL, NULL);
    SetWindowLongW(h, GWL_USERDATA, id);
    ShowWindow(h, SW_SHOW);
    SetForegroundWindow(h);
    SetFocus(h);
}

DWORD ShellMain(LPVOID unused)
{
    WNDCLASSW wc;
    MSG msg;

    Gwes_Init();
    make_icons();
    g_logo_icon = LoadIconW(NULL, MAKEINTRESOURCEW(ICON_LOGO));

    memset(&wc, 0, sizeof wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = today_proc; wc.lpszClassName = c_today; wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);
    wc.lpfnWndProc = menu_proc; wc.lpszClassName = c_menu; wc.hbrBackground = NULL;
    RegisterClassW(&wc);
    wc.lpfnWndProc = app_proc; wc.lpszClassName = c_app; wc.hbrBackground = NULL;
    RegisterClassW(&wc);

    g_today = CreateWindowExW(0, c_today, L"Today", WS_POPUP, 0, 0, SW_, SH_, NULL, NULL, NULL, NULL);
    ShowWindow(g_today, SW_SHOW);
    SetForegroundWindow(g_today);
    SetFocus(g_today);
    NKDbgPrintfW(L"shell: Today screen up\r\n");
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
