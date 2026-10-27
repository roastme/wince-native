/* GDI subset for the mini GWES: DCs with a clip rectangle, brushes/pens/fonts/icons, text and a few shapes. */
#include "priv.h"
#include "../dsi/font8x8.h"

#define VRAM_TOP ((volatile unsigned short *)0x06800000)
#define VRAM_BOT ((volatile unsigned short *)0x06200000)

extern void *LocalAlloc_(unsigned long n);

/* tiny bump allocator for GDI objects (freed objects go on a per-type free list only for DCs) */
static unsigned char g_heap[96 * 1024];
static unsigned g_heap_used;
static void *galloc(unsigned n)
{
    void *p;
    n = (n + 7) & ~7u;
    if (g_heap_used + n > sizeof g_heap) return NULL;
    p = g_heap + g_heap_used;
    g_heap_used += n;
    memset(p, 0, n);
    return p;
}

GBrush g_stock_brush[6];
GPen g_stock_pen[3];
GFont g_font_sys = { { OBJ_FONT_, 1 }, 1, 0, 8 };

static const COLORREF sys_colors[] = {
    0xC0C0C0, 0x808000, 0x800000, 0x808080, 0xC0C0C0, 0xFFFFFF, 0x000000, 0x000000,
    0x000000, 0xFFFFFF, 0xC0C0C0, 0xC0C0C0, 0x808080, 0x800000, 0xFFFFFF, 0xC0C0C0,
    0x808080, 0x808080, 0x000000, 0xC0C0C0, 0xFFFFFF, 0x000000, 0xC0C0C0, 0x000000, 0xE1FFFF,
};

COLORREF Gwes_SysColor(int idx)
{
    idx &= 0xff;
    if (idx >= (int)(sizeof sys_colors / sizeof sys_colors[0])) idx = 15;
    return sys_colors[idx];
}

void gdi_init(void)
{
    static const COLORREF bc[6] = { 0xFFFFFF, 0xC0C0C0, 0x808080, 0x404040, 0x000000, 0 };
    int i;
    for (i = 0; i < 6; i++) {
        g_stock_brush[i].o.type = OBJ_BRUSH_; g_stock_brush[i].o.stock = 1;
        g_stock_brush[i].color = bc[i];
        g_stock_brush[i].hollow = (i == 5);
    }
    for (i = 0; i < 3; i++) {
        g_stock_pen[i].o.type = OBJ_PEN_; g_stock_pen[i].o.stock = 1; g_stock_pen[i].width = 1;
    }
    g_stock_pen[0].color = 0xFFFFFF;
    g_stock_pen[1].color = 0x000000;
    g_stock_pen[2].null = 1;
}

/* ---- pixels ---- */
unsigned short gx_color(COLORREF c)
{
    return 0x8000 | ((c & 0xff) >> 3) | (((c >> 8) & 0xff) >> 3 << 5) | (((c >> 16) & 0xff) >> 3 << 10);
}

static volatile unsigned short *row(int y)
{
    return y < GW_TOP_H ? VRAM_TOP + y * 256 : VRAM_BOT + (y - GW_TOP_H) * 256;
}

void gx_fill(const RECT *clip, int x, int y, int w, int h, unsigned short c)
{
    int x1 = x + w, y1 = y + h, xx;
    if (x < clip->left) x = clip->left;
    if (y < clip->top) y = clip->top;
    if (x1 > clip->right) x1 = clip->right;
    if (y1 > clip->bottom) y1 = clip->bottom;
    for (; y < y1; y++) {
        volatile unsigned short *p = row(y);
        for (xx = x; xx < x1; xx++) p[xx] = c;
    }
}

void gx_pixel(const RECT *clip, int x, int y, unsigned short c)
{
    if (x < clip->left || x >= clip->right || y < clip->top || y >= clip->bottom) return;
    row(y)[x] = c;
}

int gx_char_w(const GFont *f) { return 8 * f->scale; }
int gx_line_h(const GFont *f) { return (f->scale == 1 ? 12 : 20); }

void gx_glyph(const RECT *clip, int x, int y, int ch, int scale, int bold, unsigned short fg, int has_bg, unsigned short bg)
{
    const unsigned char *g;
    int gx, gy, sx, sy;
    int top = y + (scale == 1 ? 2 : 2);

    if (ch < 0x20 || ch > 0x7e) ch = '?';
    g = font8x8[ch - 0x20];
    if (has_bg)
        gx_fill(clip, x, y, 8 * scale, scale == 1 ? 12 : 20, bg);
    for (gy = 0; gy < 8; gy++)
        for (gx = 0; gx < 8; gx++) {
            int on = (g[gy] & (0x80 >> gx)) || (bold && gx > 0 && (g[gy] & (0x80 >> (gx - 1))));
            if (!on) continue;
            for (sy = 0; sy < scale; sy++)
                for (sx = 0; sx < scale; sx++)
                    gx_pixel(clip, x + gx * scale + sx, top + gy * scale + sy, fg);
        }
}

void gx_blit_icon(const RECT *clip, int x, int y, const GIcon *ic, int w, int h)
{
    int ix, iy;
    for (iy = 0; iy < h; iy++)
        for (ix = 0; ix < w; ix++) {
            int sx = ix * ic->w / w, sy = iy * ic->h / h;
            unsigned short c = ic->px[sy * ic->w + sx];
            if (c & 0x8000) gx_pixel(clip, x + ix, y + iy, c);
        }
}

/* ---- DC management ---- */
static GDC g_dcs[8];

HDC gwes_new_dc(struct WND *w, const RECT *clip, int ox, int oy)
{
    int i;
    for (i = 0; i < 8; i++)
        if (!g_dcs[i].o.type) {
            GDC *d = &g_dcs[i];
            memset(d, 0, sizeof *d);
            d->o.type = OBJ_DC_;
            d->clip = *clip; d->ox = ox; d->oy = oy; d->wnd = w;
            d->brush = &g_stock_brush[0]; d->pen = &g_stock_pen[1]; d->font = &g_font_sys;
            d->tcolor = 0; d->bcolor = 0xFFFFFF; d->bkmode = OPAQUE;
            return (HDC)d;
        }
    return NULL;
}

void gwes_free_dc(HDC h)
{
    GDC *d = (GDC *)h;
    if (d && d->o.type == OBJ_DC_) d->o.type = 0;
}

HDC WINAPI CreateCompatibleDC(HDC h) { return NULL; }
BOOL WINAPI DeleteDC(HDC h) { gwes_free_dc(h); return TRUE; }

/* ---- objects ---- */
HBRUSH WINAPI CreateSolidBrush(COLORREF c)
{
    GBrush *b = galloc(sizeof *b);
    if (!b) return NULL;
    b->o.type = OBJ_BRUSH_; b->color = c;
    return (HBRUSH)b;
}

HPEN WINAPI CreatePen(int style, int width, COLORREF c)
{
    GPen *p = galloc(sizeof *p);
    if (!p) return NULL;
    p->o.type = OBJ_PEN_; p->color = c; p->width = width ? width : 1; p->null = (style == PS_NULL);
    return (HPEN)p;
}

HFONT WINAPI CreateFontIndirectW(const LOGFONTW *lf)
{
    GFont *f = galloc(sizeof *f);
    int h = lf->lfHeight < 0 ? -lf->lfHeight : lf->lfHeight;
    if (!f) return NULL;
    f->o.type = OBJ_FONT_; f->height = h;
    f->scale = h >= 16 ? 2 : 1;
    f->bold = lf->lfWeight >= FW_BOLD;
    return (HFONT)f;
}

HGDIOBJ WINAPI GetStockObject(int i)
{
    switch (i) {
    case WHITE_BRUSH: return &g_stock_brush[0];
    case LTGRAY_BRUSH: return &g_stock_brush[1];
    case GRAY_BRUSH: return &g_stock_brush[2];
    case DKGRAY_BRUSH: return &g_stock_brush[3];
    case BLACK_BRUSH: return &g_stock_brush[4];
    case NULL_BRUSH: return &g_stock_brush[5];
    case WHITE_PEN: return &g_stock_pen[0];
    case BLACK_PEN: return &g_stock_pen[1];
    case NULL_PEN: return &g_stock_pen[2];
    default: return &g_font_sys;
    }
}

HBRUSH WINAPI GetSysColorBrush(int idx)
{
    return CreateSolidBrush(Gwes_SysColor(idx));      /* leaks a few bytes per call; callers are rare */
}

DWORD WINAPI GetSysColor(int idx) { return Gwes_SysColor(idx); }

HGDIOBJ WINAPI SelectObject(HDC h, HGDIOBJ obj)
{
    GDC *d = (GDC *)h;
    GObj *o = (GObj *)obj;
    HGDIOBJ old = NULL;

    if (!d || !o) return NULL;
    switch (o->type) {
    case OBJ_BRUSH_: old = d->brush; d->brush = (GBrush *)o; break;
    case OBJ_PEN_:   old = d->pen;   d->pen = (GPen *)o; break;
    case OBJ_FONT_:  old = d->font;  d->font = (GFont *)o; break;
    }
    return old;
}

BOOL WINAPI DeleteObject(HGDIOBJ o) { return TRUE; }  /* the arena is never reclaimed */

COLORREF WINAPI SetTextColor(HDC h, COLORREF c) { GDC *d = (GDC *)h; COLORREF o = d->tcolor; d->tcolor = c; return o; }
COLORREF WINAPI SetBkColor(HDC h, COLORREF c) { GDC *d = (GDC *)h; COLORREF o = d->bcolor; d->bcolor = c; return o; }
int WINAPI SetBkMode(HDC h, int m) { GDC *d = (GDC *)h; int o = d->bkmode; d->bkmode = m; return o; }

/* ---- drawing (coordinates are client-relative; DC origin shifts them to the screen) ---- */
static void to_screen(GDC *d, const RECT *r, RECT *o)
{
    o->left = r->left + d->ox; o->right = r->right + d->ox;
    o->top = r->top + d->oy;   o->bottom = r->bottom + d->oy;
}

static unsigned short brush_color(GBrush *b) { return gx_color(b->color); }

int WINAPI FillRect(HDC h, const RECT *rc, HBRUSH hb)
{
    GDC *d = (GDC *)h;
    RECT s;
    GBrush tmp;

    if (!d || !rc) return 0;
    if ((DWORD)hb < 0x100 || ((DWORD)hb & 0x40000000)) {      /* COLOR_xxx + 1 (CE ORs in SYS_COLOR_INDEX_FLAG) */
        tmp.o.type = OBJ_BRUSH_; tmp.hollow = 0;
        tmp.color = Gwes_SysColor((((DWORD)hb - 1) & 0xff)); hb = (HBRUSH)&tmp;
    }
    if (((GBrush *)hb)->hollow) return 1;
    to_screen(d, rc, &s);
    gx_fill(&d->clip, s.left, s.top, s.right - s.left, s.bottom - s.top, brush_color((GBrush *)hb));
    return 1;
}

BOOL WINAPI Rectangle(HDC h, int l, int t, int r, int b)
{
    GDC *d = (GDC *)h;
    RECT rc;
    unsigned short pc = gx_color(d->pen->color);

    rc.left = l; rc.top = t; rc.right = r; rc.bottom = b;
    if (!d->brush->hollow) {
        RECT in = rc;
        in.left++; in.top++; in.right--; in.bottom--;
        FillRect(h, &in, (HBRUSH)d->brush);
    }
    if (!d->pen->null) {
        RECT s;
        to_screen(d, &rc, &s);
        gx_fill(&d->clip, s.left, s.top, s.right - s.left, 1, pc);
        gx_fill(&d->clip, s.left, s.bottom - 1, s.right - s.left, 1, pc);
        gx_fill(&d->clip, s.left, s.top, 1, s.bottom - s.top, pc);
        gx_fill(&d->clip, s.right - 1, s.top, 1, s.bottom - s.top, pc);
    }
    return TRUE;
}

static void line(GDC *d, int x0, int y0, int x1, int y1, unsigned short c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;

    for (;;) {
        gx_pixel(&d->clip, x0 + d->ox, y0 + d->oy, c);
        if (x0 == x1 && y0 == y1) break;
        if (2 * err >= dy) { err += dy; x0 += sx; }
        if (2 * err <= dx) { err += dx; y0 += sy; }
    }
}

BOOL WINAPI Polyline(HDC h, const POINT *p, int n)
{
    GDC *d = (GDC *)h;
    int i;
    if (d->pen->null) return TRUE;
    for (i = 0; i + 1 < n; i++)
        line(d, p[i].x, p[i].y, p[i + 1].x, p[i + 1].y, gx_color(d->pen->color));
    return TRUE;
}

BOOL WINAPI Polygon(HDC h, const POINT *p, int n)
{
    GDC *d = (GDC *)h;
    int y, i, miny = 32767, maxy = -32768;

    for (i = 0; i < n; i++) { if (p[i].y < miny) miny = p[i].y; if (p[i].y > maxy) maxy = p[i].y; }
    if (!d->brush->hollow)
        for (y = miny; y <= maxy; y++) {            /* scanline even-odd fill */
            int xs[16], nx = 0, j;
            for (i = 0; i < n && nx < 16; i++) {
                int a = i, b = (i + 1) % n;
                int ya = p[a].y, yb = p[b].y;
                if ((ya <= y && yb > y) || (yb <= y && ya > y))
                    xs[nx++] = p[a].x + (y - ya) * (p[b].x - p[a].x) / (yb - ya);
            }
            for (i = 0; i < nx; i++)
                for (j = i + 1; j < nx; j++)
                    if (xs[j] < xs[i]) { int t = xs[i]; xs[i] = xs[j]; xs[j] = t; }
            for (i = 0; i + 1 < nx; i += 2)
                gx_fill(&d->clip, xs[i] + d->ox, y + d->oy, xs[i + 1] - xs[i] + 1, 1, brush_color(d->brush));
        }
    for (i = 0; i < n; i++)
        line(d, p[i].x, p[i].y, p[(i + 1) % n].x, p[(i + 1) % n].y, gx_color(d->pen->color));
    return TRUE;
}

BOOL WINAPI DrawEdge(HDC h, LPRECT rc, UINT edge, UINT flags)
{
    GDC *d = (GDC *)h;
    COLORREF hi = 0xFFFFFF, lo = 0x808080, dk = 0x000000, fc = 0xC0C0C0;
    int sunken = (edge & EDGE_SUNKEN) == EDGE_SUNKEN && (edge & EDGE_RAISED) != EDGE_RAISED;
    int w = rc->right - rc->left, ht = rc->bottom - rc->top;
    unsigned short tl, br, tl2, br2;
    RECT s;

    to_screen(d, rc, &s);
    if (sunken) { tl = gx_color(lo); br = gx_color(hi); tl2 = gx_color(dk); br2 = gx_color(fc); }
    else        { tl = gx_color(hi); br = gx_color(dk); tl2 = gx_color(fc); br2 = gx_color(lo); }
    if (flags & BF_LEFT)   { gx_fill(&d->clip, s.left, s.top, 1, ht, tl); gx_fill(&d->clip, s.left + 1, s.top + 1, 1, ht - 2, tl2); }
    if (flags & BF_TOP)    { gx_fill(&d->clip, s.left, s.top, w, 1, tl); gx_fill(&d->clip, s.left + 1, s.top + 1, w - 2, 1, tl2); }
    if (flags & BF_RIGHT)  { gx_fill(&d->clip, s.right - 1, s.top, 1, ht, br); gx_fill(&d->clip, s.right - 2, s.top + 1, 1, ht - 2, br2); }
    if (flags & BF_BOTTOM) { gx_fill(&d->clip, s.left, s.bottom - 1, w, 1, br); gx_fill(&d->clip, s.left + 1, s.bottom - 2, w - 2, 1, br2); }
    if (flags & BF_ADJUST) { rc->left += 2; rc->top += 2; rc->right -= 2; rc->bottom -= 2; }
    return TRUE;
}

BOOL WINAPI DrawFocusRect(HDC h, const RECT *rc)
{
    GDC *d = (GDC *)h;
    RECT s;
    int x, y;
    unsigned short c = gx_color(0);

    to_screen(d, rc, &s);
    for (x = s.left; x < s.right; x += 2) { gx_pixel(&d->clip, x, s.top, c); gx_pixel(&d->clip, x, s.bottom - 1, c); }
    for (y = s.top; y < s.bottom; y += 2) { gx_pixel(&d->clip, s.left, y, c); gx_pixel(&d->clip, s.right - 1, y, c); }
    return TRUE;
}

BOOL WINAPI PatBlt(HDC h, int x, int y, int w, int ht, DWORD rop)
{
    RECT rc;
    rc.left = x; rc.top = y; rc.right = x + w; rc.bottom = y + ht;
    return FillRect(h, &rc, (HBRUSH)((GDC *)h)->brush);
}

/* ---- text ---- */
static int wlen(const WCHAR *s) { int n = 0; while (s[n]) n++; return n; }

BOOL WINAPI GetTextExtentExPointW(HDC h, LPCWSTR s, int n, int maxw, LPINT fit, LPINT dx, LPSIZE sz)
{
    GDC *d = (GDC *)h;
    if (n < 0) n = wlen(s);
    sz->cx = n * gx_char_w(d->font);
    sz->cy = gx_line_h(d->font);
    if (fit) *fit = n;
    return TRUE;
}

BOOL WINAPI GetTextMetricsW(HDC h, LPTEXTMETRICW tm)
{
    GDC *d = (GDC *)h;
    memset(tm, 0, sizeof *tm);
    tm->tmHeight = gx_line_h(d->font);
    tm->tmAscent = tm->tmHeight - 3;
    tm->tmDescent = 3;
    tm->tmAveCharWidth = tm->tmMaxCharWidth = gx_char_w(d->font);
    return TRUE;
}

static void draw_run(GDC *d, int x, int y, const WCHAR *s, int n)
{
    unsigned short fg = gx_color(d->tcolor), bg = gx_color(d->bcolor);
    int i, cw = gx_char_w(d->font);
    for (i = 0; i < n; i++, x += cw)
        gx_glyph(&d->clip, x + d->ox, y + d->oy, s[i], d->font->scale, d->font->bold, fg, d->bkmode == OPAQUE, bg);
}

BOOL WINAPI ExtTextOutW(HDC h, int x, int y, UINT opts, const RECT *rc, LPCWSTR s, UINT n, const INT *dx)
{
    GDC *d = (GDC *)h;
    if (rc && (opts & ETO_OPAQUE)) {
        GBrush b; b.o.type = OBJ_BRUSH_; b.color = d->bcolor; b.hollow = 0;
        FillRect(h, rc, (HBRUSH)&b);
    }
    draw_run(d, x, y, s, (int)n);
    return TRUE;
}

int WINAPI DrawTextW(HDC h, LPCWSTR s, int n, LPRECT rc, UINT fmt)
{
    GDC *d = (GDC *)h;
    int cw = gx_char_w(d->font), lh = gx_line_h(d->font);
    int width = rc->right - rc->left, maxc = width / cw;
    int start[16], len[16], lines = 0, i, pos = 0, y;

    if (n < 0) n = wlen(s);
    if (maxc < 1) maxc = 1;
    if (!(fmt & DT_WORDBREAK)) maxc = 0x7fff;
    while (lines < 16) {                             /* split into lines */
        int e = pos, last_sp = -1;
        if (fmt & DT_SINGLELINE) {
            e = n;
        } else {
            while (e < n && s[e] != L'\n' && e - pos < maxc) {
                if (s[e] == L' ') last_sp = e;
                e++;
            }
            if (e < n && s[e] != L'\n' && last_sp > pos) e = last_sp;
        }
        start[lines] = pos; len[lines] = e - pos;
        if (len[lines] > 0 && s[pos + len[lines] - 1] == L'\r') len[lines]--;
        lines++;
        if (e >= n) break;
        pos = e;
        if (s[pos] == L'\n' || s[pos] == L' ') pos++;
    }
    if (fmt & DT_CALCRECT) {
        int mw = 0;
        for (i = 0; i < lines; i++) if (len[i] * cw > mw) mw = len[i] * cw;
        rc->right = rc->left + ((fmt & DT_SINGLELINE) ? n * cw : mw);
        rc->bottom = rc->top + lines * lh;
        return lines * lh;
    }
    y = rc->top;
    if (fmt & DT_VCENTER) y = rc->top + (rc->bottom - rc->top - lines * lh) / 2;
    else if (fmt & DT_BOTTOM) y = rc->bottom - lines * lh;
    for (i = 0; i < lines; i++, y += lh) {
        int x = rc->left, tw = len[i] * cw;
        if (fmt & DT_CENTER) x = rc->left + (width - tw) / 2;
        else if (fmt & DT_RIGHT) x = rc->right - tw;
        draw_run(d, x, y, s + start[i], len[i]);
    }
    return lines * lh;
}

/* ---- icons ---- */
#define MAX_ICONS 24
static struct { int id, w, h; const unsigned short *px; } g_icons[MAX_ICONS];
static int g_nicons;

void Gwes_RegisterIcon(int id, int w, int h, const unsigned short *px)
{
    if (g_nicons < MAX_ICONS) {
        g_icons[g_nicons].id = id; g_icons[g_nicons].w = w; g_icons[g_nicons].h = h; g_icons[g_nicons].px = px;
        g_nicons++;
    }
}

HICON WINAPI LoadIconW(HINSTANCE inst, LPCWSTR name)
{
    int i, id = (int)name;
    for (i = 0; i < g_nicons; i++)
        if (g_icons[i].id == id) {
            GIcon *ic = galloc(sizeof *ic);
            if (!ic) return NULL;
            ic->o.type = OBJ_ICON_; ic->w = g_icons[i].w; ic->h = g_icons[i].h; ic->px = g_icons[i].px;
            return (HICON)ic;
        }
    return NULL;
}

HANDLE WINAPI LoadImageW(HINSTANCE inst, LPCWSTR name, UINT type, int cx, int cy, UINT fl)
{
    GIcon *ic = (GIcon *)LoadIconW(inst, name);
    if (ic && cx) { ic->o.stock = cx; }           /* remember the requested size */
    return (HANDLE)ic;
}

BOOL WINAPI DestroyIcon(HICON i) { return TRUE; }

BOOL WINAPI DrawIconEx(HDC h, int x, int y, HICON hi, int cx, int cy, UINT step, HBRUSH bg, UINT flags)
{
    GDC *d = (GDC *)h;
    GIcon *ic = (GIcon *)hi;
    if (!ic) return FALSE;
    if (!cx) cx = ic->w;
    if (!cy) cy = ic->h;
    gx_blit_icon(&d->clip, x + d->ox, y + d->oy, ic, cx, cy);
    return TRUE;
}

