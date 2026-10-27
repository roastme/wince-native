#ifndef GWES_PRIV_H
#define GWES_PRIV_H
#include "gwes.h"

extern void NKDbgPrintfW(const wchar_t *, ...);
LPWSTR WINAPI lstrcpynW_(LPWSTR d, LPCWSTR s, int n);
#define GW_NCDESTROY 0x82
#define GW_NCHITTEST 0x84
#define GW_MOUSEACTIVATE 0x21
extern int NKwvsprintfW(LPWSTR, LPCWSTR, va_list, int);

/* ---- GDI objects ---- */
enum { OBJ_BRUSH_ = 1, OBJ_PEN_, OBJ_FONT_, OBJ_ICON_, OBJ_DC_ };

typedef struct GObj { int type; int stock; } GObj;
typedef struct { GObj o; COLORREF color; int hollow; } GBrush;
typedef struct { GObj o; COLORREF color; int width; int null; } GPen;
typedef struct { GObj o; int scale; int bold; int height; } GFont;
typedef struct { GObj o; int w, h; const unsigned short *px; unsigned short *own; } GIcon;
typedef struct GDC {
    GObj o;
    RECT clip;                 /* screen coords */
    int ox, oy;                /* window origin in screen coords */
    GBrush *brush; GPen *pen; GFont *font;
    COLORREF tcolor, bcolor; int bkmode;
    struct WND *wnd;
} GDC;

/* ---- framebuffer primitives (screen coords, 15-bit colour) ---- */
unsigned short gx_color(COLORREF c);
void gx_fill(const RECT *clip, int x, int y, int w, int h, unsigned short c);
void gx_pixel(const RECT *clip, int x, int y, unsigned short c);
void gx_glyph(const RECT *clip, int x, int y, int ch, int scale, int bold, unsigned short fg, int has_bg, unsigned short bg);
int  gx_char_w(const GFont *f);
int  gx_line_h(const GFont *f);
void gx_blit_icon(const RECT *clip, int x, int y, const GIcon *ic, int w, int h);

/* ---- window manager hooks used by the DC code ---- */
void gdi_init(void);
extern GBrush g_stock_brush[6];
extern GPen g_stock_pen[3];
extern GFont g_font_sys;
COLORREF Gwes_SysColor(int idx);

#endif
