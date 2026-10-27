/*
 * lua_ce.c - Windows CE front-end for the unmodified Lua 5.1.4 core (build/ce-apps/luace/src, not edited).
 *
 * Replaces lua.c's stdin REPL: the screen is the console, the bottom half is the input line and a touch keyboard.
 * print is registered here so output goes to the screen and never through stdout. Only the base, string, table and
 * math libraries are opened; io and os need the stdio streams, which this port does not use yet.
 * A line runs when Enter is tapped. If it does not compile as an expression or statement and the error is "<eof>",
 * the next line continues it, as lua.c does.
 */
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#define COLS        32      /* 256 px / 8 px glyphs */
#define ROWS_SHOWN  16      /* top screen: 192 px / 12 px lines */
#define LINE_H      12
#define LINES       64      /* ring of console lines kept */
#define INPUT_MAX   120
#define KEY_W       25
#define KEY_H       20
#define KB_TOP      216     /* first keyboard row, bottom screen */

static HWND g_hwnd;
static lua_State *L;

static char g_text[LINES][COLS + 1];
static long g_abs;            /* absolute number of the line being written */
static int  g_col;            /* characters on the line being written */

static char g_in[INPUT_MAX];  /* line being typed */
static int  g_inlen;
static char g_pending[1024];  /* statement text waiting for its continuation lines */
static int  g_pendlen;

/* Touch keyboard: one string per row, a run of equal characters is one key. '_' is space, '\r' Enter, '<' Backspace. */
static const char *kb_rows[] = {
    "qwertyuiop",
    "asdfghjkl",
    "zxcvbnm<",
    "1234567890",
    "()[]{}=+-*/",
    ".,:;\"'#%^&<>",
    "__\r\r",
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

static void con_newline(void)
{
    g_abs++;
    g_col = 0;
    g_text[g_abs % LINES][0] = 0;
}

/* Console output: characters wrap at COLS; newlines start a new line. */
static void con_put(int c)
{
    if (c == '\n' || g_col >= COLS) con_newline();
    if (c == '\n') return;
    g_text[g_abs % LINES][g_col++] = (char)(c >= 32 && c < 127 ? c : '?');
    g_text[g_abs % LINES][g_col] = 0;
}

static void con_print(const char *s)
{
    while (*s) con_put((unsigned char)*s++);
    con_newline();
}

static void paint(HDC hdc)
{
    RECT rc;
    WCHAR w[COLS + 2];
    int r, i, n, y;
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

    /* Input line: a prompt ("> " or ">> " while a statement continues), then what is typed so far, then a cursor. */
    w[0] = '>';
    w[1] = g_pendlen ? '>' : ' ';
    w[2] = ' ';
    for (i = 0; i < g_inlen && i < COLS - 4; i++) w[3 + i] = (unsigned char)g_in[i];
    n = 3 + i;
    w[n++] = '_';
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
            else if (c == '<') { label[ln++] = 'D'; label[ln++] = 'E'; label[ln++] = 'L'; }
            else label[ln++] = (unsigned char)c;
            DrawTextW(hdc, label, ln, &kr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            col = last + 1;
        }
    }
    DeleteObject(key_br);
}

/* ---- Lua side ----------------------------------------------------------------------------------------------- */

/* print: the arguments as tostring would show them, tab separated, then a console line. */
static int con_lua_print(lua_State *ls)
{
    int n = lua_gettop(ls), i;
    char buf[256];

    buf[0] = 0;
    for (i = 1; i <= n; i++) {
        const char *s;
        if (i > 1) strcat(buf, "\t");
        if (lua_isnumber(ls, i) || lua_isstring(ls, i)) {
            s = lua_tostring(ls, i);
        } else if (lua_isnil(ls, i)) {
            s = "nil";
        } else if (lua_isboolean(ls, i)) {
            s = lua_toboolean(ls, i) ? "true" : "false";
        } else {
            s = lua_typename(ls, lua_type(ls, i));
        }
        strncat(buf, s, sizeof buf - strlen(buf) - 1);
    }
    con_print(buf);
    return 0;
}

static void lua_report(const char *msg)
{
    con_print(msg ? msg : "(error with no message)");
}

/* Run the typed line. It is tried as an expression first (so "1+1" shows its value), then as a statement. */
static void run_line(const char *line)
{
    char stmt[1024];
    int status;
    int n;

    if (g_pendlen) {
        n = (int)strlen(line);
        if (g_pendlen + 1 + n >= (int)sizeof g_pending) { g_pendlen = 0; con_print("line too long"); return; }
        g_pending[g_pendlen++] = '\n';
        memcpy(g_pending + g_pendlen, line, n + 1);
        g_pendlen += n;
    } else {
        strcpy(g_pending, line);
        g_pendlen = (int)strlen(line);
    }

    /* "=expr" and bare expressions: try "return expr" first */
    if (g_pending[0] == '=') {
        strcpy(stmt, "return ");
        strcat(stmt, g_pending + 1);
        status = luaL_loadbuffer(L, stmt, strlen(stmt), "=stdin");
    } else {
        status = luaL_loadbuffer(L, g_pending, g_pendlen, "=stdin");
    }
    if (status == LUA_ERRSYNTAX) {
        const char *msg = lua_tostring(L, -1);
        size_t len = msg ? strlen(msg) : 0;
        if (len >= 5 && strcmp(msg + len - 5, "<eof>") == 0) {
            /* incomplete: wait for more lines */
            lua_pop(L, 1);
            return;
        }
    }
    g_pendlen = 0;
    if (status == 0) {
        status = lua_pcall(L, 0, LUA_MULTRET, 0);
        if (status == 0) {
            int nres = lua_gettop(L);
            if (nres > 0) {
                lua_getglobal(L, "print");
                lua_insert(L, 1);
                lua_pcall(L, nres, 0, 0);
            }
        }
    }
    if (status != 0) {
        lua_report(lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_settop(L, 0);
}

static void line_submit(void)
{
    char line[INPUT_MAX];

    memcpy(line, g_in, g_inlen + 1);
    run_line(line);
    g_inlen = 0;
    g_in[0] = 0;
    redraw();
}

static void key_tap(int c)
{
    if (c == 13) {
        line_submit();
        return;
    }
    if (c == 8) {
        if (g_inlen > 0) g_in[--g_inlen] = 0;
    } else if (c >= 32 && c < 127 && g_inlen < INPUT_MAX - 1) {
        g_in[g_inlen++] = (char)c;
        g_in[g_inlen] = 0;
    }
    redraw();
}

static LRESULT CALLBACK LuaProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
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

static void console_open(void)
{
    static const luaL_Reg libs[] = {
        { "", luaopen_base }, { LUA_STRLIBNAME, luaopen_string },
        { LUA_TABLIBNAME, luaopen_table }, { LUA_MATHLIBNAME, luaopen_math }, { NULL, NULL }
    };
    const luaL_Reg *lib;

    L = luaL_newstate();
    for (lib = libs; lib->func; lib++) {
        lua_pushcfunction(L, lib->func);
        lua_pushstring(L, lib->name);
        lua_call(L, 1, 0);
    }
    lua_register(L, "print", con_lua_print);
}

/* ---- entry point ------------------------------------------------------------------------------------------- */

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR cmd, int show)
{
    WNDCLASSW wc;
    MSG msg;

    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = LuaProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"LuaWindow";
    if (!RegisterClassW(&wc)) return 1;

    g_hwnd = CreateWindowExW(0, L"LuaWindow", L"Lua", WS_POPUP, 0, 0, 256, 384, NULL, NULL, hInst, NULL);
    if (!g_hwnd) return 1;
    ShowWindow(g_hwnd, SW_SHOW);

    console_open();
    con_print("Lua 5.1.4 on Windows CE");
    redraw();

    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    lua_close(L);
    return 0;
}
