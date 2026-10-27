/*
 * shell_icons.c - Shell icon art and small string helpers (ThroatyMumbo's shell_icons.c, without the .rsrc).
 *
 * The reference loaded its icons from a resource file.  This port draws them in code once at startup and registers the
 * bitmaps with the mini GWES (Gwes_RegisterIcon), so LoadIconW can hand them out by id.
 * Pixels are 15-bit colour with bit 15 set on painted pixels; 0 is transparent.
 */
#include "dsshell.h"

extern void Gwes_RegisterIcon(int id, int w, int h, const unsigned short *px);

#define RGB15(r, g, b) ((unsigned short)(0x8000 | ((r) >> 3) | (((g) >> 3) << 5) | (((b) >> 3) << 10)))

static unsigned short g_px_folder[32 * 32], g_px_file[32 * 32], g_px_exe[32 * 32], g_px_drive[32 * 32], g_px_start[16 * 16];
static HICON g_icon[IDI_START + 1];
static BOOL  g_icons_ready;

/* Fill an inclusive rectangle. */
static void fillr(unsigned short *px, int w, int x0, int y0, int x1, int y1, unsigned short c)
{
    int x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            px[y * w + x] = c;
}

static void paint_folder(unsigned short *px)
{
    fillr(px, 32, 2, 6, 13, 10, RGB15(120, 90, 20));
    fillr(px, 32, 3, 7, 12, 9, RGB15(250, 220, 100));
    fillr(px, 32, 2, 9, 29, 27, RGB15(120, 90, 20));
    fillr(px, 32, 3, 10, 28, 26, RGB15(250, 220, 100));
}

static void paint_file(unsigned short *px)
{
    int y;
    fillr(px, 32, 7, 3, 24, 28, RGB15(90, 90, 90));
    fillr(px, 32, 8, 4, 23, 27, RGB15(250, 250, 250));
    for (y = 9; y <= 21; y += 3)
        fillr(px, 32, 11, y, 20, y, RGB15(150, 150, 150));
}

static void paint_exe(unsigned short *px)
{
    fillr(px, 32, 3, 6, 28, 26, RGB15(30, 30, 80));
    fillr(px, 32, 4, 7, 27, 11, RGB15(0, 0, 170));
    fillr(px, 32, 4, 12, 27, 25, RGB15(250, 250, 250));
}

static void paint_drive(unsigned short *px)
{
    fillr(px, 32, 3, 8, 28, 24, RGB15(60, 60, 60));
    fillr(px, 32, 4, 9, 27, 23, RGB15(170, 170, 170));
    fillr(px, 32, 6, 12, 20, 14, RGB15(50, 50, 50));
    fillr(px, 32, 23, 19, 25, 21, RGB15(0, 220, 0));
}

/* The Start button mark: four coloured squares, like the classic Windows flag. */
static void paint_start(unsigned short *px)
{
    fillr(px, 16, 1, 1, 7, 7, RGB15(230, 60, 40));
    fillr(px, 16, 8, 1, 14, 7, RGB15(60, 180, 60));
    fillr(px, 16, 1, 8, 7, 14, RGB15(40, 90, 220));
    fillr(px, 16, 8, 8, 14, 14, RGB15(240, 200, 40));
}

void Shell_RegisterIcons(void)
{
    if (g_icons_ready) return;
    g_icons_ready = TRUE;

    paint_folder(g_px_folder);
    paint_file(g_px_file);
    paint_exe(g_px_exe);
    paint_drive(g_px_drive);
    paint_start(g_px_start);
    Gwes_RegisterIcon(IDI_FOLDER, 32, 32, g_px_folder);
    Gwes_RegisterIcon(IDI_FILE, 32, 32, g_px_file);
    Gwes_RegisterIcon(IDI_EXE, 32, 32, g_px_exe);
    Gwes_RegisterIcon(IDI_DRIVE, 32, 32, g_px_drive);
    Gwes_RegisterIcon(IDI_START, 16, 16, g_px_start);

    g_icon[IDI_FOLDER] = LoadIconW(g_hInst, MAKEINTRESOURCEW(IDI_FOLDER));
    g_icon[IDI_FILE]   = LoadIconW(g_hInst, MAKEINTRESOURCEW(IDI_FILE));
    g_icon[IDI_EXE]    = LoadIconW(g_hInst, MAKEINTRESOURCEW(IDI_EXE));
    g_icon[IDI_DRIVE]  = LoadIconW(g_hInst, MAKEINTRESOURCEW(IDI_DRIVE));
    g_icon[IDI_START]  = LoadIconW(g_hInst, MAKEINTRESOURCEW(IDI_START));
}

HICON Shell_Icon(int id)
{
    return (id > 0 && id <= IDI_START) ? g_icon[id] : NULL;
}

int Shell_Wlen(LPCWSTR s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}

BOOL Shell_Weq(LPCWSTR a, LPCWSTR b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

void Shell_Wcopyn(WCHAR *dst, LPCWSTR src, int max)
{
    int i;
    if (max <= 0) return;
    for (i = 0; i < max - 1 && src[i]; i++)
        dst[i] = src[i];
    dst[i] = 0;
}

BOOL Shell_JoinPath(WCHAR *out, int max, LPCWSTR parent, LPCWSTR name)
{
    int n = Shell_Wlen(parent), m = Shell_Wlen(name), sep = (n > 0 && parent[n - 1] != '\\');
    if (n + sep + m + 1 > max) return FALSE;
    Shell_Wcopyn(out, parent, max);
    if (sep) out[n++] = '\\';
    Shell_Wcopyn(out + n, name, max - n);
    return TRUE;
}

static WCHAR lower(WCHAR c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

/* True when the name ends in ".abc", ignoring case. */
int Shell_HasExtCI(LPCWSTR name, WCHAR a, WCHAR b, WCHAR c)
{
    int n = Shell_Wlen(name);
    if (n < 4) return 0;
    return name[n - 4] == '.' && lower(name[n - 3]) == lower(a) &&
           lower(name[n - 2]) == lower(b) && lower(name[n - 1]) == lower(c);
}

/* CE 2.11 has no DT_END_ELLIPSIS: keep the longest prefix that fits, then append "...". */
int Shell_Ellipsize(HDC hdc, LPCWSTR src, int max_w, WCHAR *out)
{
    SIZE sz;
    int n = Shell_Wlen(src), k, j;

    Shell_Wcopyn(out, src, n + 1);
    GetTextExtentExPointW(hdc, out, n, 0, NULL, NULL, &sz);
    if (sz.cx <= max_w) return n;
    for (k = n - 1; k > 0; k--) {
        for (j = 0; j < k; j++) out[j] = src[j];
        out[k] = '.'; out[k + 1] = '.'; out[k + 2] = '.'; out[k + 3] = 0;
        GetTextExtentExPointW(hdc, out, k + 3, 0, NULL, NULL, &sz);
        if (sz.cx <= max_w) return k + 3;
    }
    out[0] = 0;
    return 0;
}

BOOL Shell_LaunchExe(LPCWSTR fullPath)
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    memset(&pi, 0, sizeof pi);
    return CreateProcessW(fullPath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
}
