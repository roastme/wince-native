/*
 * frotz_ce.c - Windows CE front-end for the portable Frotz core (core/*.c, unchanged).
 *
 * The core is line based: with one screen row and 32 columns, every line break reaches os_scroll_area, the same
 * model GENERIC.C uses for stdio. Story text fills the top screen; the bottom screen holds the input line and a touch
 * keyboard. Keys are taken from WM_LBUTTONDOWN (keyboard) and WM_CHAR (hardware Enter/Backspace).
 */
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "core/frotz.h"

extern int main(int argc, char *argv[]);

#define COLS        32      /* 256 px / 8 px glyphs */
#define ROWS_SHOWN  16      /* top screen: 192 px / 12 px lines */
#define LINE_H      12
#define LINES       64      /* ring of story lines kept */
#define INPUT_MAX   80
#define KEY_W       25
#define KEY_H       22
#define KB_TOP      216     /* first keyboard row, bottom screen */

static HWND g_hwnd;
static char g_text[LINES][COLS + 1];
static long g_abs;             /* absolute number of the line being written */
static int  g_col;             /* characters on the line being written */
static int  swallow_nl;        /* the input echo already ended the line */


static char g_in[INPUT_MAX];   /* input line being typed */
static int  g_inlen, g_inmax;
static int  g_mode;            /* 0 idle, 1 reading a line, 2 reading a key */
static int  g_done;

#define KQ_MAX 16
static int  kq[KQ_MAX];       /* keys tapped while no read was waiting */
static int  kq_head, kq_n;

/* Touch keyboard: one string per row, a run of equal characters is one key. '_' is space, '\r' Enter, '<' Backspace. */
static const char *kb_rows[] = {
    "qwertyuiop",
    "asdfghjkl",
    "zxcvbnm<",
    "1234567890",
    ".,!?'-__\r\r",
};
#define KB_NROWS ((int)(sizeof kb_rows / sizeof kb_rows[0]))

static int key_code(char c)
{
    if (c == '_') return ' ';
    if (c == '<') return 8;
    return c;
}

/* Find the key covering column col of row r: its first and last column, and the character it sends. */
static int key_at(int r, int col, int *first, int *last)
{
    const char *s = kb_rows[r];
    int n = (int)strlen(s), i = 0;

    while (i < n) {
        int j = i;
        while (j + 1 < n && s[j + 1] == s[i]) j++;
        if (col >= i && col <= j) {
            *first = i;
            *last = j;
            return key_code(s[i]);
        }
        i = j + 1;
    }
    return 0;
}

static void redraw(void)
{
    if (!g_hwnd) return;
    InvalidateRect(g_hwnd, NULL, TRUE);
    UpdateWindow(g_hwnd);
}

static void text_clear(void)
{
    g_abs = 0;
    g_col = 0;
    g_text[0][0] = 0;
}

static void text_newline(void)
{
    g_abs++;
    g_col = 0;
    g_text[g_abs % LINES][0] = 0;
    redraw();
}

static void text_put(int c)
{
    if (g_col >= COLS && c == ' ') {
        /* a space at the edge is the break itself */
        text_newline();
        return;
    }
    if (g_col >= COLS) {
        /* full line: move the words after its last space down, so a word is never split */
        char *cur = g_text[g_abs % LINES];
        char tail[COLS + 1];
        int sp = COLS - 1, from, n = 0;

        while (sp > 0 && cur[sp] != ' ') sp--;
        from = sp > 0 ? sp + 1 : COLS;
        if (sp > 0) cur[sp] = 0;
        while (from < COLS && cur[from]) tail[n++] = cur[from++];
        tail[n] = 0;
        text_newline();
        memcpy(g_text[g_abs % LINES], tail, n + 1);
        g_col = n;
    }
    g_text[g_abs % LINES][g_col++] = (char)c;
    g_text[g_abs % LINES][g_col] = 0;
}

/* Line editing for the input line; Enter is handled by the caller. */
static void line_key(int c)
{
    if (c == 8) {
        if (g_inlen > 0) g_in[--g_inlen] = 0;
    } else if (c >= 32 && c < 127 && g_inlen < g_inmax - 1) {
        g_in[g_inlen++] = (char)c;
        g_in[g_inlen] = 0;
    }
}

static void key_tap(int c)
{
    if (g_mode == 1) {
        if (c == 13) g_done = 1;
        else line_key(c);
    } else if (kq_n < KQ_MAX) {
        kq[(kq_head + kq_n++) % KQ_MAX] = c;
        g_done = 1;
    } else
        return;
    if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
}

static void paint(HDC hdc)
{
    RECT rc;
    WCHAR w[COLS + 2];
    int r, i, k, y, n;
    HBRUSH key_br = CreateSolidBrush(RGB(64, 64, 64));

    GetClientRect(g_hwnd, &rc);
    FillRect(hdc, &rc, GetStockObject(BLACK_BRUSH));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    for (r = 0; r < ROWS_SHOWN; r++) {
        long abs = g_abs - (ROWS_SHOWN - 1) + r;
        if (abs < 0) continue;
        for (n = 0; n < COLS && g_text[abs % LINES][n]; n++) w[n] = (unsigned char)g_text[abs % LINES][n];
        ExtTextOutW(hdc, 0, r * LINE_H, 0, NULL, w, n, NULL);
    }

    /* Input line: "> " then what is typed so far, with a cursor when a line is being read. */
    w[0] = '>';
    w[1] = ' ';
    for (i = 0; i < g_inlen && i < COLS - 3; i++) w[2 + i] = (unsigned char)g_in[i];
    n = 2 + i;
    if (g_mode == 1) w[n++] = '_';
    ExtTextOutW(hdc, 0, 196, 0, NULL, w, n, NULL);

    for (r = 0; r < KB_NROWS; r++) {
        int col = 0;
        y = KB_TOP + r * KEY_H;
        while (col < 10) {
            int first, last, code = key_at(r, col, &first, &last);
            RECT kr;
            WCHAR label[8];
            int ln = 0;
            char c = kb_rows[r][first];

            if (!code) { col++; continue; }
            SetRect(&kr, first * KEY_W + 1, y + 1, (last + 1) * KEY_W - 1, y + KEY_H - 1);
            FillRect(hdc, &kr, key_br);
            if (c == '_') { label[ln++] = 'S'; label[ln++] = 'P'; }
            else if (c == '\r') { label[ln++] = 'E'; label[ln++] = 'N'; label[ln++] = 'T'; }
            else label[ln++] = (unsigned char)c;
            DrawTextW(hdc, label, ln, &kr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            col = last + 1;
        }
    }
    DeleteObject(key_br);
}

static LRESULT CALLBACK FrotzProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        paint(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int x = LOWORD(lp), y = HIWORD(lp);
        int r, first, last, code;
        if (y < KB_TOP) return 0;
        r = (y - KB_TOP) / KEY_H;
        if (r < 0 || r >= KB_NROWS) return 0;
        code = key_at(r, x / KEY_W, &first, &last);
        if (code) key_tap(code);
        return 0;
    }
    case WM_CHAR:
        key_tap((int)wp);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* Run the message loop until a key handler sets g_done. A quit message ends the app thread. */
static void pump(void)
{
    MSG msg;

    g_done = 0;
    while (!g_done) {
        if (!GetMessageW(&msg, NULL, 0, 0)) ExitThread(0);
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

/* ---- os_* hooks used by the core ------------------------------------------------------------------------------- */

void os_beep (int n) {}
int  os_char_width (int c) { return 1; }
void os_cursor_off (void) {}
void os_cursor_on (void) {}

void os_display_char (int c)
{
    if (c == NEW_FONT || c == NEW_STYLE) return;
    if (c == '\n') { text_newline(); return; }
    if (c == 8) {                           /* backspace: step back so the next character overwrites */
        if (g_col > 0) g_col--;
        return;
    }
    if (c == '\t') {                        /* tab: spaces to the next 8-column stop */
        do text_put(' ');
        while (g_col % 8 && g_col < COLS);
        return;
    }
    text_put(c >= 32 && c < 127 ? c : '?');
}

void os_display_string (const char *s)
{
    while (*s) {
        os_display_char ((unsigned char) *s);
        s++;
    }
}

void os_draw_picture (int n, int x, int y) {}

void os_erase_area (int top, int left, int bottom, int right)
{
    if (top == 1 && left == 1 && cwin == 0) text_clear();
}

void os_fatal (const char *s)
{
    os_display_string ("Fatal: ");
    os_display_string (s);
    text_newline();
    redraw();
    for (;;) Sleep(1000);
}

void os_finish_with_sample (void) {}
int  os_font_data (int n, int *height, int *width) { return 0; }

void os_init_screen (void)
{
    /* The core wraps at h_screen_width; keep it wide so it never wraps, and text_put does the word wrap at COLS. */
    h_screen_width = 255;
    h_screen_cols = COLS;
    h_screen_height = h_screen_rows = 1;
    h_config |= CONFIG_NOSTATUSLINE;
    text_clear();
}

void os_more_prompt (void) {}
int  os_peek_colour (void) { return 0; }
int  os_picture_data (int n, int *height, int *width) { return 0; }
void os_prepare_sample (int n) {}

void os_process_arguments (int argc, char *argv[])
{
    story_name = (argc > 1) ? argv[1] : "\\Apps\\curses-r14.z5";
}

int os_random_seed (void) { return (int)GetTickCount(); }

/* Save and restore are not wired up yet: the core treats a refused file name as a failed save. */
/* CE port: no file dialog yet. Save and restore use the default name in the Apps folder; script and record are off. */
int os_read_file_name (char *name, const char *default_name, int type)
{
    const char *base;

    if (type == FILE_SCRIPT || type == FILE_RECORD || type == FILE_PLAYBACK) return 0;
    base = strrchr(default_name, '\\');
    base = base ? base + 1 : default_name;
    strcpy(name, "\\Apps\\");
    strcat(name, base);
    return 1;
}

int os_read_key (int timeout)
{
    int key;

    redraw();
    if (!kq_n) {
        g_mode = 2;
        pump();
        g_mode = 0;
    }
    key = kq[kq_head];
    kq_head = (kq_head + 1) % KQ_MAX;
    kq_n--;
    return key;
}

int os_read_line (int max, char *buf, int timeout, int width, int continued)
{
    int i, done = 0;

    g_inlen = 0;
    g_in[0] = 0;
    g_inmax = (max < INPUT_MAX) ? max : INPUT_MAX;

    /* Keys tapped before this read started are queued; they belong to this line. */
    while (kq_n && !done) {
        int c = kq[kq_head];
        kq_head = (kq_head + 1) % KQ_MAX;
        kq_n--;
        if (c == 13) done = 1;
        else line_key(c);
    }
    if (!done) {
        g_mode = 1;
        redraw();
        pump();
        g_mode = 0;
    }

    for (i = 0; i <= g_inlen; i++) buf[i] = g_in[i];
    for (i = 0; i < g_inlen; i++) text_put((unsigned char)g_in[i]);
    text_newline();
    swallow_nl = 1;
    return 13;
}

void os_reset_screen (void) {}

void os_scroll_area (int top, int left, int bottom, int right, int pixels)
{
    if (!swallow_nl)
        text_newline();
    swallow_nl = 0;
}

void os_set_colour (int fg, int bg) {}
/* A z-code newline only moves the core cursor back to the left margin (x 1), and reports it here. */
void os_set_cursor (int y, int x)
{
    if (x == 1 && g_col > 0) text_newline();
}
void os_set_font (int font) {}
void os_set_text_style (int style) {}
void os_start_sample (int n, int r, int vol) {}
void os_stop_sample (void) {}

int os_string_width (const char *s)
{
    int width = 0;
    while (*s) {
        if (*s != NEW_FONT && *s != NEW_STYLE) width++;
        s++;
    }
    return width;
}

void os_wait_sample (void) {}

/* ---- entry point --------------------------------------------------------------------------------------------- */

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR cmd, int show)
{
    WNDCLASSW wc;
    MSG msg;
    char story[MAX_PATH];
    char *argv[2];
    int i;

    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = FrotzProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"FrotzWindow";
    if (!RegisterClassW(&wc)) return 1;

    g_hwnd = CreateWindowExW(0, L"FrotzWindow", L"Frotz", WS_POPUP, 0, 0, 256, 384, NULL, NULL, hInst, NULL);
    if (!g_hwnd) return 1;
    ShowWindow(g_hwnd, SW_SHOW);

    /* The command line, if any, names the story file; otherwise the default story is used. */
    for (i = 0; cmd && cmd[i] && i < MAX_PATH - 1; i++) story[i] = (char)cmd[i];
    story[i] = 0;
    argv[0] = "frotz";
    argv[1] = story;
    main(story[0] ? 2 : 1, argv);

    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
