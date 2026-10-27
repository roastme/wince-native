/* Window manager for the mini GWES: classes, window tree + z-order, message queue, timers, painting and DSi input. */
#include "priv.h"

extern HDC gwes_new_dc(struct WND *w, const RECT *clip, int ox, int oy);
extern void gwes_free_dc(HDC h);
extern HANDLE WINAPI CreateEventW(LPSECURITY_ATTRIBUTES, BOOL, BOOL, LPCWSTR);
extern BOOL WINAPI EventModify(HANDLE, DWORD);
extern DWORD WINAPI WaitForSingleObject(HANDLE, DWORD);
extern DWORD WINAPI GetTickCount(void);
extern HANDLE WINAPI CreateThread(LPSECURITY_ATTRIBUTES, DWORD, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
extern BOOL WINAPI InterruptInitialize(DWORD, HANDLE, LPVOID, DWORD);
extern void WINAPI InterruptDone(DWORD);
extern BOOL OEMGetRealTime(LPSYSTEMTIME);

#define SYSINTR_VBLANK_ID (8 + 8 + 0)       /* SYSINTR_FIRMWARE + 0, see dsi/oal.c */
#define MAILBOX 0x02FFD000u
#define KEYS_REG (*(volatile unsigned short *)0x04000130)

volatile unsigned Gwes_Frames;

typedef struct CLS {
    struct CLS *next;
    WNDPROC proc; DWORD style; HBRUSH bg; HINSTANCE inst; HCURSOR cur;
    WCHAR name[32];
} CLS;

typedef struct WND {
    struct WND *parent, *first, *last, *prev, *next;     /* children bottom (first) -> top (last) */
    CLS *cls; WNDPROC proc;
    DWORD style, exstyle;
    RECT r;                                              /* screen coordinates */
    WCHAR title[48];
    LONG user, extra[4];
    int visible, enabled, alive, id;
    RECT inval; int has_inval;
    struct WND *owner;
} WND;

#define MAX_WND 64
static WND g_wnd[MAX_WND];
static WND g_root;                                       /* the desktop: parent of all top-level windows */
static CLS g_cls_pool[24];
static CLS *g_cls;
static int g_ncls;

static WND *g_capture, *g_focus, *g_fore;
static HANDLE g_evt;                                     /* signalled when the queue gets a message */

/* ---- tiny critical section: interrupts off ---- */
static unsigned lock(void)
{
    unsigned old;
    __asm__ volatile("mrs %0, cpsr\n\torr r1, %0, #0x80\n\tmsr cpsr_c, r1" : "=r"(old) :: "r1", "memory");
    return old;
}
static void unlock(unsigned old) { __asm__ volatile("msr cpsr_c, %0" :: "r"(old) : "memory"); }

/* ---- rect helpers ---- */
BOOL WINAPI SetRect(LPRECT r, int l, int t, int rr, int b) { r->left = l; r->top = t; r->right = rr; r->bottom = b; return TRUE; }
BOOL WINAPI CopyRect(LPRECT d, const RECT *s) { *d = *s; return TRUE; }
BOOL WINAPI IsRectEmpty(const RECT *r) { return r->right <= r->left || r->bottom <= r->top; }
BOOL WINAPI PtInRect(const RECT *r, POINT p) { return p.x >= r->left && p.x < r->right && p.y >= r->top && p.y < r->bottom; }
BOOL WINAPI OffsetRect(LPRECT r, int dx, int dy) { r->left += dx; r->right += dx; r->top += dy; r->bottom += dy; return TRUE; }
BOOL WINAPI InflateRect(LPRECT r, int dx, int dy) { r->left -= dx; r->right += dx; r->top -= dy; r->bottom += dy; return TRUE; }
BOOL WINAPI IntersectRect(LPRECT o, const RECT *a, const RECT *b)
{
    RECT t;
    t.left = a->left > b->left ? a->left : b->left;
    t.top = a->top > b->top ? a->top : b->top;
    t.right = a->right < b->right ? a->right : b->right;
    t.bottom = a->bottom < b->bottom ? a->bottom : b->bottom;
    if (IsRectEmpty(&t)) { SetRect(o, 0, 0, 0, 0); return FALSE; }
    *o = t;
    return TRUE;
}
BOOL WINAPI UnionRect(LPRECT o, const RECT *a, const RECT *b)
{
    RECT t;
    if (IsRectEmpty(a)) { *o = *b; return !IsRectEmpty(b); }
    if (IsRectEmpty(b)) { *o = *a; return TRUE; }
    t.left = a->left < b->left ? a->left : b->left;
    t.top = a->top < b->top ? a->top : b->top;
    t.right = a->right > b->right ? a->right : b->right;
    t.bottom = a->bottom > b->bottom ? a->bottom : b->bottom;
    *o = t;
    return TRUE;
}

/* ---- window tree ---- */
static int valid(HWND h) { return h && (WND *)h >= g_wnd && (WND *)h < g_wnd + MAX_WND && ((WND *)h)->alive; }
#define W(h) ((WND *)(h))

static int vis_all(WND *w)
{
    for (; w && w != &g_root; w = w->parent)
        if (!w->visible) return 0;
    return 1;
}

static void clip_of(WND *w, RECT *c)
{
    *c = w->r;
    for (w = w->parent; w && w != &g_root; w = w->parent)
        IntersectRect(c, c, &w->r);
}

static WND *top_of(WND *w)
{
    while (w->parent && w->parent != &g_root) w = w->parent;
    return w;
}

static void unlink_wnd(WND *w)
{
    WND *p = w->parent;
    if (w->prev) w->prev->next = w->next; else p->first = w->next;
    if (w->next) w->next->prev = w->prev; else p->last = w->prev;
    w->prev = w->next = NULL;
}

static void link_top(WND *w, WND *p)
{
    w->parent = p;
    w->prev = p->last; w->next = NULL;
    if (p->last) p->last->next = w; else p->first = w;
    p->last = w;
}

static void link_bottom(WND *w, WND *p)
{
    w->parent = p;
    w->next = p->first; w->prev = NULL;
    if (p->first) p->first->prev = w; else p->last = w;
    p->first = w;
}

/* raise w among its siblings: above everything, except a non-topmost window stays below topmost ones */
static void raise_wnd(WND *w, int topmost)
{
    WND *p = w->parent, *s;
    if (p->last == w) return;
    unlink_wnd(w);
    if (!topmost && p == &g_root) {
        for (s = p->last; s && (s->exstyle & WS_EX_TOPMOST); s = s->prev) ;
        if (s) { w->parent = p; w->prev = s; w->next = s->next; if (s->next) s->next->prev = w; else p->last = w; s->next = w; return; }
        link_bottom(w, p);
        return;
    }
    link_top(w, p);
}

/* ---- painting ---- */
static void inval_add(WND *w, const RECT *screen_rc)
{
    RECT c, i;
    clip_of(w, &c);
    if (!IntersectRect(&i, &c, screen_rc)) return;
    if (w->has_inval) UnionRect(&w->inval, &w->inval, &i);
    else { w->inval = i; w->has_inval = 1; }
}

static void inval_screen(const RECT *rc, WND *except)
{
    WND *t, *c;
    for (t = g_root.first; t; t = t->next) {
        WND *stack[8]; int sp = 0;
        stack[sp++] = t;
        while (sp) {                                     /* iterative DFS */
            WND *w = stack[--sp];
            if (!w->visible) continue;
            if (w != except) inval_add(w, rc);
            for (c = w->last; c && sp < 8; c = c->prev) stack[sp++] = c;
        }
    }
}

static void desktop_fill(const RECT *rc)
{
    RECT clip = { 0, 0, GW_SCREEN_W, GW_SCREEN_H }, r;
    if (!IntersectRect(&r, rc, &clip)) return;
    gx_fill(&clip, r.left, r.top, r.right - r.left, r.bottom - r.top, gx_color(0x000000));
}

static WND *g_pw;            /* window being painted */
static RECT g_prc;           /* its paint rect (screen) */
static WND *g_order[MAX_WND];
static int g_norder;
static volatile int g_paint_pending;

static void collect(WND *w)
{
    WND *c;
    if (!w->visible || g_norder >= MAX_WND) return;
    g_order[g_norder++] = w;
    for (c = w->first; c; c = c->next) collect(c);
}

LRESULT WINAPI DefWindowProcW(HWND, UINT, WPARAM, LPARAM);

static void paint_pass(void)
{
    int i, j;

    g_paint_pending = 0;
    g_norder = 0;
    {
        WND *t;
        for (t = g_root.first; t; t = t->next) collect(t);
    }
    for (i = 0; i < g_norder; i++) {
        WND *w = g_order[i];
        RECT rc, clip;
        HDC dc;

        if (!w->has_inval || !w->alive) continue;
        clip_of(w, &clip);
        w->has_inval = 0;
        if (!IntersectRect(&rc, &w->inval, &clip)) continue;
        for (j = i + 1; j < g_norder; j++) {              /* windows later in paint order overlap -> they repaint too */
            RECT x;
            if (IntersectRect(&x, &rc, &g_order[j]->r))
                inval_add(g_order[j], &x);
        }
        g_pw = w; g_prc = rc;
        dc = gwes_new_dc(w, &rc, w->r.left, w->r.top);
        if (dc) {
            w->proc((HWND)w, WM_ERASEBKGND, (WPARAM)dc, 0);
            gwes_free_dc(dc);
        }
        w->proc((HWND)w, WM_PAINT, 0, 0);
        g_pw = NULL;
    }
}

BOOL WINAPI InvalidateRect(HWND h, const RECT *rc, BOOL erase)
{
    RECT s;
    WND *w = h ? W(h) : &g_root;
    if (!valid(h)) return FALSE;
    if (rc) { s.left = rc->left + w->r.left; s.right = rc->right + w->r.left; s.top = rc->top + w->r.top; s.bottom = rc->bottom + w->r.top; }
    else s = w->r;
    inval_add(w, &s);
    g_paint_pending = 1;
    EventModify(g_evt, EVENT_SET);
    return TRUE;
}

BOOL WINAPI ValidateRect(HWND h, const RECT *rc) { if (valid(h)) W(h)->has_inval = 0; return TRUE; }

BOOL WINAPI UpdateWindow(HWND h) { if (g_paint_pending) paint_pass(); return TRUE; }

HDC WINAPI BeginPaint(HWND h, LPPAINTSTRUCT ps)
{
    WND *w = W(h);
    RECT rc = (g_pw == w) ? g_prc : w->r;
    HDC dc = gwes_new_dc(w, &rc, w->r.left, w->r.top);

    memset(ps, 0, sizeof *ps);
    ps->hdc = dc;
    ps->rcPaint.left = rc.left - w->r.left; ps->rcPaint.right = rc.right - w->r.left;
    ps->rcPaint.top = rc.top - w->r.top; ps->rcPaint.bottom = rc.bottom - w->r.top;
    return dc;
}

BOOL WINAPI EndPaint(HWND h, LPPAINTSTRUCT ps) { gwes_free_dc(ps->hdc); return TRUE; }

HDC WINAPI GetDC(HWND h)
{
    RECT c;
    WND *w = h ? W(h) : &g_root;
    if (!h) { SetRect(&c, 0, 0, GW_SCREEN_W, GW_SCREEN_H); return gwes_new_dc(&g_root, &c, 0, 0); }
    if (!valid(h)) return NULL;
    clip_of(w, &c);
    return gwes_new_dc(w, &c, w->r.left, w->r.top);
}
int WINAPI ReleaseDC(HWND h, HDC dc) { gwes_free_dc(dc); return 1; }

/* ---- classes ---- */
ATOM WINAPI RegisterClassW(const WNDCLASSW *wc)
{
    CLS *c;
    if (g_ncls >= 24) return 0;
    c = &g_cls_pool[g_ncls++];
    c->proc = wc->lpfnWndProc; c->style = wc->style; c->bg = wc->hbrBackground; c->inst = wc->hInstance; c->cur = wc->hCursor;
    lstrcpynW_(c->name, wc->lpszClassName, 32);
    c->next = g_cls; g_cls = c;
    return (ATOM)g_ncls;
}

static CLS *find_class(LPCWSTR n)
{
    CLS *c;
    for (c = g_cls; c; c = c->next)
        if (wcscmp(c->name, n) == 0) return c;
    return NULL;
}

/* ---- message queue ---- */
#define QN 64
static MSG g_q[QN];
static volatile int g_qh, g_qt;

BOOL WINAPI PostMessageW(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    unsigned f = lock();
    int nt = (g_qt + 1) % QN;
    if (nt == g_qh) { unlock(f); return FALSE; }
    g_q[g_qt].hwnd = h; g_q[g_qt].message = m; g_q[g_qt].wParam = wp; g_q[g_qt].lParam = lp;
    g_q[g_qt].time = GetTickCount();
    g_qt = nt;
    unlock(f);
    EventModify(g_evt, EVENT_SET);
    return TRUE;
}

static int q_pop(MSG *m)
{
    unsigned f = lock();
    if (g_qh == g_qt) { unlock(f); return 0; }
    *m = g_q[g_qh];
    g_qh = (g_qh + 1) % QN;
    unlock(f);
    return 1;
}

LRESULT WINAPI SendMessageW(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    if (!valid(h)) return 0;
    return W(h)->proc(h, m, wp, lp);
}

void WINAPI PostQuitMessage(int code) { PostMessageW(NULL, WM_QUIT, code, 0); }

/* ---- timers ---- */
typedef struct { HWND h; UINT id; DWORD interval, due; TIMERPROC proc; int used; } TMR;
static TMR g_tm[16];

UINT WINAPI SetTimer(HWND h, UINT id, UINT ms, TIMERPROC proc)
{
    int i, slot = -1;
    for (i = 0; i < 16; i++)
        if (g_tm[i].used && g_tm[i].h == h && g_tm[i].id == id) { slot = i; break; }
    if (slot < 0)
        for (i = 0; i < 16; i++) if (!g_tm[i].used) { slot = i; break; }
    if (slot < 0) return 0;
    if (!h && !id) id = slot + 1000;
    g_tm[slot].h = h; g_tm[slot].id = id; g_tm[slot].interval = ms; g_tm[slot].proc = proc;
    g_tm[slot].due = GetTickCount() + ms; g_tm[slot].used = 1;
    EventModify(g_evt, EVENT_SET);
    return id;
}

BOOL WINAPI KillTimer(HWND h, UINT id)
{
    int i;
    for (i = 0; i < 16; i++)
        if (g_tm[i].used && g_tm[i].h == h && g_tm[i].id == id) { g_tm[i].used = 0; return TRUE; }
    return FALSE;
}

static int timer_due(MSG *m, DWORD *wait)
{
    int i;
    DWORD now = GetTickCount(), best = 0xFFFFFFFF;

    for (i = 0; i < 16; i++) {
        if (!g_tm[i].used) continue;
        if ((int)(now - g_tm[i].due) >= 0) {
            m->hwnd = g_tm[i].h; m->message = WM_TIMER; m->wParam = g_tm[i].id; m->lParam = (LPARAM)g_tm[i].proc;
            g_tm[i].due += g_tm[i].interval;
            if ((int)(now - g_tm[i].due) >= 0) g_tm[i].due = now + g_tm[i].interval;
            return 1;
        }
        if (g_tm[i].due - now < best) best = g_tm[i].due - now;
    }
    *wait = best;
    return 0;
}

static int next_msg(MSG *m, int block)
{
    for (;;) {
        DWORD wait = 0xFFFFFFFF;
        if (q_pop(m)) return 1;
        if (timer_due(m, &wait)) return 1;
        if (g_paint_pending) { paint_pass(); continue; }
        if (!block) return 0;
        WaitForSingleObject(g_evt, wait);
    }
}

BOOL WINAPI GetMessageW(LPMSG m, HWND h, UINT lo, UINT hi)
{
    next_msg(m, 1);
    return m->message != WM_QUIT;
}

BOOL WINAPI PeekMessageW(LPMSG m, HWND h, UINT lo, UINT hi, UINT remove) { return next_msg(m, 0); }

BOOL WINAPI TranslateMessage(const MSG *m)
{
    if (m->message == WM_KEYDOWN && (m->wParam == VK_RETURN || m->wParam == VK_BACK || m->wParam == VK_ESCAPE || m->wParam == VK_TAB))
        PostMessageW(m->hwnd, WM_CHAR, m->wParam, m->lParam);
    return TRUE;
}

LRESULT WINAPI DispatchMessageW(const MSG *m)
{
    if (m->message == WM_TIMER && m->lParam) {
        ((TIMERPROC)m->lParam)(m->hwnd, WM_TIMER, m->wParam, GetTickCount());
        return 0;
    }
    if (!valid(m->hwnd)) return 0;
    return W(m->hwnd)->proc(m->hwnd, m->message, m->wParam, m->lParam);
}

/* ---- windows ---- */
HWND WINAPI CreateWindowExW(DWORD ex, LPCWSTR cls, LPCWSTR title, DWORD style, int x, int y, int cx, int cy,
                            HWND parent, HMENU menu, HINSTANCE inst, LPVOID param)
{
    CLS *c = find_class(cls);
    WND *w = NULL, *p;
    int i;
    CREATESTRUCTW cs;

    if (!c) return NULL;
    for (i = 0; i < MAX_WND; i++) if (!g_wnd[i].alive) { w = &g_wnd[i]; break; }
    if (!w) return NULL;
    memset(w, 0, sizeof *w);
    w->alive = 1; w->cls = c; w->proc = c->proc; w->style = style; w->exstyle = ex; w->enabled = 1;
    w->id = (int)menu;
    if (title) lstrcpynW_(w->title, title, 48);
    p = ((style & WS_CHILD) && valid(parent)) ? W(parent) : &g_root;
    w->owner = (!(style & WS_CHILD) && valid(parent)) ? W(parent) : NULL;
    if (x == (int)CW_USEDEFAULT) x = 0;
    if (y == (int)CW_USEDEFAULT) y = 0;
    if (cx == (int)CW_USEDEFAULT) cx = GW_SCREEN_W;
    if (cy == (int)CW_USEDEFAULT) cy = GW_SCREEN_H;
    w->r.left = x + (p == &g_root ? 0 : p->r.left); w->r.top = y + (p == &g_root ? 0 : p->r.top);
    w->r.right = w->r.left + cx; w->r.bottom = w->r.top + cy;
    if (ex & WS_EX_TOPMOST) link_top(w, p); else { link_top(w, p); raise_wnd(w, 0); }
    memset(&cs, 0, sizeof cs);
    cs.lpCreateParams = param; cs.hInstance = inst; cs.hMenu = menu; cs.hwndParent = parent; cs.cy = cy; cs.cx = cx;
    cs.y = y; cs.x = x; cs.style = style; cs.lpszName = title; cs.lpszClass = cls; cs.dwExStyle = ex;
    if (w->proc((HWND)w, WM_CREATE, 0, (LPARAM)&cs) == (LRESULT)-1) {
        DestroyWindow((HWND)w);
        return NULL;
    }
    if (style & WS_VISIBLE) ShowWindow((HWND)w, SW_SHOW);
    return (HWND)w;
}

BOOL WINAPI DestroyWindow(HWND h)
{
    WND *w = W(h), *c;
    int i;
    RECT r;

    if (!valid(h)) return FALSE;
    while ((c = w->last) != NULL) DestroyWindow((HWND)c);
    w->proc(h, WM_DESTROY, 0, 0);
    w->proc(h, GW_NCDESTROY, 0, 0);
    for (i = 0; i < 16; i++) if (g_tm[i].used && g_tm[i].h == h) g_tm[i].used = 0;
    if (g_capture == w) g_capture = NULL;
    if (g_focus == w) g_focus = NULL;
    if (g_fore == w) g_fore = NULL;
    r = w->r;
    w->alive = 0;
    unlink_wnd(w);
    desktop_fill(&r);
    inval_screen(&r, NULL);
    g_paint_pending = 1;
    EventModify(g_evt, EVENT_SET);
    return TRUE;
}

BOOL WINAPI ShowWindow(HWND h, int cmd)
{
    WND *w = W(h);
    int was, now;

    if (!valid(h)) return FALSE;
    was = w->visible;
    now = (cmd != SW_HIDE);
    w->visible = now;
    if (was != now) {
        w->proc(h, WM_SHOWWINDOW, now, 0);
        if (now) InvalidateRect(h, NULL, TRUE);
        else { RECT r = w->r; desktop_fill(&r); inval_screen(&r, NULL); g_paint_pending = 1; EventModify(g_evt, EVENT_SET); }
    }
    return was;
}

BOOL WINAPI IsWindowVisible(HWND h) { return valid(h) && vis_all(W(h)); }
BOOL WINAPI IsWindow(HWND h) { return valid(h); }
BOOL WINAPI EnableWindow(HWND h, BOOL e) { BOOL o; if (!valid(h)) return FALSE; o = !W(h)->enabled; W(h)->enabled = e; return o; }
BOOL WINAPI IsWindowEnabled(HWND h) { return valid(h) && W(h)->enabled; }

BOOL WINAPI GetWindowRect(HWND h, LPRECT r) { if (!valid(h)) return FALSE; *r = W(h)->r; return TRUE; }
BOOL WINAPI GetClientRect(HWND h, LPRECT r)
{
    if (!valid(h)) return FALSE;
    r->left = r->top = 0; r->right = W(h)->r.right - W(h)->r.left; r->bottom = W(h)->r.bottom - W(h)->r.top;
    return TRUE;
}
BOOL WINAPI ClientToScreen(HWND h, LPPOINT p) { if (!valid(h)) return FALSE; p->x += W(h)->r.left; p->y += W(h)->r.top; return TRUE; }
BOOL WINAPI ScreenToClient(HWND h, LPPOINT p) { if (!valid(h)) return FALSE; p->x -= W(h)->r.left; p->y -= W(h)->r.top; return TRUE; }

static void move_tree(WND *w, int dx, int dy)
{
    WND *c;
    OffsetRect(&w->r, dx, dy);
    for (c = w->first; c; c = c->next) move_tree(c, dx, dy);
}

BOOL WINAPI SetWindowPos(HWND h, HWND after, int x, int y, int cx, int cy, UINT fl)
{
    WND *w = W(h);
    RECT old;

    if (!valid(h)) return FALSE;
    old = w->r;
    if (!(fl & SWP_NOMOVE)) {
        int px = (w->parent && w->parent != &g_root) ? w->parent->r.left : 0;
        int py = (w->parent && w->parent != &g_root) ? w->parent->r.top : 0;
        move_tree(w, x + px - w->r.left, y + py - w->r.top);
    }
    if (!(fl & SWP_NOSIZE)) { w->r.right = w->r.left + cx; w->r.bottom = w->r.top + cy; }
    if (!(fl & SWP_NOZORDER)) {
        if (after == HWND_TOPMOST) { w->exstyle |= WS_EX_TOPMOST; raise_wnd(w, 1); }
        else if (after == HWND_TOP) raise_wnd(w, w->exstyle & WS_EX_TOPMOST);
        else if (after == HWND_BOTTOM) { unlink_wnd(w); link_bottom(w, w->parent); }
        else if (after == HWND_NOTOPMOST) { w->exstyle &= ~WS_EX_TOPMOST; raise_wnd(w, 0); }
    }
    if (fl & SWP_SHOWWINDOW) w->visible = 1;
    if (fl & SWP_HIDEWINDOW) w->visible = 0;
    if (w->visible) {
        RECT u;
        UnionRect(&u, &old, &w->r);
        desktop_fill(&old);
        inval_screen(&u, NULL);
        InvalidateRect(h, NULL, TRUE);
    } else {
        desktop_fill(&old);
        inval_screen(&old, NULL);
        g_paint_pending = 1;
    }
    EventModify(g_evt, EVENT_SET);
    return TRUE;
}

BOOL WINAPI MoveWindow(HWND h, int x, int y, int cx, int cy, BOOL repaint)
{
    return SetWindowPos(h, NULL, x, y, cx, cy, SWP_NOZORDER);
}

LONG WINAPI GetWindowLongW(HWND h, int i)
{
    WND *w = W(h);
    if (!valid(h)) return 0;
    switch (i) {
    case GWL_STYLE: return w->style;
    case GWL_EXSTYLE: return w->exstyle;
    case GWL_USERDATA: return w->user;
    case GWL_WNDPROC: return (LONG)w->proc;
    case GWL_ID: return w->id;
    default: return (i >= 0 && i < 16) ? w->extra[i / 4] : 0;
    }
}

LONG WINAPI SetWindowLongW(HWND h, int i, LONG v)
{
    WND *w = W(h);
    LONG o = 0;
    if (!valid(h)) return 0;
    switch (i) {
    case GWL_STYLE: o = w->style; w->style = v; break;
    case GWL_EXSTYLE: o = w->exstyle; w->exstyle = v; break;
    case GWL_USERDATA: o = w->user; w->user = v; break;
    case GWL_WNDPROC: o = (LONG)w->proc; w->proc = (WNDPROC)v; break;
    case GWL_ID: o = w->id; w->id = v; break;
    default: if (i >= 0 && i < 16) { o = w->extra[i / 4]; w->extra[i / 4] = v; }
    }
    return o;
}

int WINAPI GetWindowTextW(HWND h, LPWSTR s, int n)
{
    if (!valid(h) || n <= 0) return 0;
    lstrcpynW_(s, W(h)->title, n);
    return wcslen(s);
}

BOOL WINAPI SetWindowTextW(HWND h, LPCWSTR s)
{
    if (!valid(h)) return FALSE;
    lstrcpynW_(W(h)->title, s, 48);
    W(h)->proc(h, WM_SETTEXT, 0, (LPARAM)s);
    InvalidateRect(h, NULL, TRUE);
    return TRUE;
}

HWND WINAPI GetParent(HWND h) { return (valid(h) && W(h)->parent != &g_root) ? (HWND)W(h)->parent : NULL; }

HWND WINAPI GetWindow(HWND h, UINT cmd)
{
    WND *w = W(h), *r = NULL;
    if (!valid(h)) return NULL;
    switch (cmd) {
    case GW_HWNDFIRST: r = w->parent->first; break;
    case GW_HWNDLAST: r = w->parent->last; break;
    case GW_HWNDNEXT: r = w->next; break;
    case GW_HWNDPREV: r = w->prev; break;
    case GW_OWNER: r = w->owner; break;
    case GW_CHILD: r = w->last; break;
    }
    return (r && r != &g_root) ? (HWND)r : NULL;
}

BOOL WINAPI RegisterTaskBar(HWND h) { return TRUE; }

/* ---- hit testing, focus, capture ---- */
static WND *hit(WND *p, int x, int y)
{
    WND *c, *r;
    for (c = p->last; c; c = c->prev) {
        POINT pt; pt.x = x; pt.y = y;
        if (!c->visible || !c->enabled || !PtInRect(&c->r, pt)) continue;
        r = hit(c, x, y);
        return r ? r : c;
    }
    return NULL;
}

HWND WINAPI WindowFromPoint(POINT p) { return (HWND)hit(&g_root, p.x, p.y); }

HWND WINAPI SetCapture(HWND h) { HWND o = (HWND)g_capture; g_capture = valid(h) ? W(h) : NULL; return o; }
BOOL WINAPI ReleaseCapture(void) { g_capture = NULL; return TRUE; }
HWND WINAPI GetCapture(void) { return (HWND)g_capture; }
HWND WINAPI SetFocus(HWND h) { HWND o = (HWND)g_focus; g_focus = valid(h) ? W(h) : NULL; return o; }
HWND WINAPI GetFocus(void) { return (HWND)g_focus; }
HWND WINAPI GetForegroundWindow(void) { return (HWND)g_fore; }
HWND WINAPI GetActiveWindow(void) { return (HWND)g_fore; }

BOOL WINAPI SetForegroundWindow(HWND h)
{
    WND *w, *old = g_fore;
    if (!valid(h)) return FALSE;
    w = top_of(W(h));
    g_fore = w;
    raise_wnd(w, w->exstyle & WS_EX_TOPMOST);
    if (old != w) {
        if (old) old->proc((HWND)old, WM_ACTIVATE, WA_INACTIVE, (LPARAM)w);
        w->proc((HWND)w, WM_ACTIVATE, WA_ACTIVE, (LPARAM)old);
        InvalidateRect((HWND)w, NULL, FALSE);
    }
    return TRUE;
}
HWND WINAPI SetActiveWindow(HWND h) { HWND o = (HWND)g_fore; SetForegroundWindow(h); return o; }

/* ---- DefWindowProc ---- */
LRESULT WINAPI DefWindowProcW(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    WND *w = W(h);
    switch (m) {
    case WM_ERASEBKGND:
        if (valid(h) && w->cls->bg) {
            RECT rc;
            GDC *d = (GDC *)wp;
            rc = d->clip;
            rc.left -= d->ox; rc.right -= d->ox; rc.top -= d->oy; rc.bottom -= d->oy;
            FillRect((HDC)wp, &rc, w->cls->bg);
        }
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(h, &ps);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_CLOSE: DestroyWindow(h); return 0;
    case GW_NCHITTEST: return HTCLIENT;
    case GW_MOUSEACTIVATE: return 1;
    case WM_GETTEXTLENGTH: return valid(h) ? wcslen(w->title) : 0;
    case WM_GETTEXT: return GetWindowTextW(h, (LPWSTR)lp, (int)wp);
    }
    return 0;
}

/* ---- misc system ---- */
int WINAPI GetSystemMetrics(int i)
{
    switch (i) {
    case SM_CXSCREEN: return GW_SCREEN_W;
    case SM_CYSCREEN: return GW_SCREEN_H;
    case SM_CXVSCROLL: return 13;
    case SM_CYVSCROLL: return 13;
    case SM_CXICON: case SM_CYICON: return 32;
    case SM_CXSMICON: case SM_CYSMICON: return 16;
    default: return 0;
    }
}

BOOL WINAPI SystemParametersInfoW(UINT a, UINT b, PVOID c, UINT d) { return TRUE; }
HCURSOR WINAPI LoadCursorW(HINSTANCE i, LPCWSTR n) { return (HCURSOR)1; }
HCURSOR WINAPI SetCursor(HCURSOR c) { return NULL; }
BOOL WINAPI GetCursorPos(LPPOINT p) { p->x = p->y = 0; return TRUE; }
BOOL WINAPI SetCursorPos(int x, int y) { return TRUE; }

void WINAPI GetLocalTime(LPSYSTEMTIME st)
{
    DWORD sec = GetTickCount() / 1000;
    OEMGetRealTime(st);
    st->wHour = 9 + sec / 3600 % 12;
    st->wMinute = (30 + sec / 60) % 60;
    st->wSecond = sec % 60;
    st->wMilliseconds = 0;
}

/* ---- input thread: vblank IST reads keys + stylus from the ARM7 mailbox ---- */
static const unsigned char keymap[12] = {
    VK_RETURN, VK_ESCAPE, VK_F3, VK_LWIN, VK_RIGHT, VK_LEFT, VK_UP, VK_DOWN, VK_F2, VK_F1, VK_TAB, VK_BACK,
};

static POINT g_pen;
static int g_pen_down;
static DWORD g_last_click_t;
static POINT g_last_click_p;
static WND *g_last_click_w;

static void post_mouse(UINT msg, int sx, int sy)
{
    WND *w = g_capture ? g_capture : hit(&g_root, sx, sy);
    int x, y;

    if (!w) return;
    x = sx - w->r.left; y = sy - w->r.top;
    if (msg == WM_LBUTTONDOWN) {
        WND *t = top_of(w);
        if (t != g_fore && !(t->exstyle & WS_EX_NOACTIVATE) && !g_capture)
            SetForegroundWindow((HWND)t);
        if (g_last_click_w == w && GetTickCount() - g_last_click_t < 450 &&
            sx - g_last_click_p.x < 8 && g_last_click_p.x - sx < 8 && sy - g_last_click_p.y < 8 && g_last_click_p.y - sy < 8 &&
            (w->cls->style & CS_DBLCLKS)) {
            msg = WM_LBUTTONDBLCLK;
            g_last_click_w = NULL;
        } else {
            g_last_click_w = w; g_last_click_t = GetTickCount(); g_last_click_p.x = sx; g_last_click_p.y = sy;
        }
    }
    PostMessageW((HWND)w, msg, (msg == WM_LBUTTONUP) ? 0 : MK_LBUTTON, MAKELPARAM(x & 0xffff, y & 0xffff));
}

static DWORD WINAPI input_thread(LPVOID unused)
{
    HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    unsigned prev = 0;
    int rep[12];
    int i;

    memset(rep, 0, sizeof rep);
    if (!InterruptInitialize(SYSINTR_VBLANK_ID, ev, NULL, 0)) {
        NKDbgPrintfW(L"gwes: vblank InterruptInitialize failed\r\n");
        return 1;
    }
    for (;;) {
        volatile DWORD *mb = (volatile DWORD *)MAILBOX;
        unsigned k, pen, x, y, mk;

        if (WaitForSingleObject(ev, 1000) != WAIT_OBJECT_0) continue;
        InterruptDone(SYSINTR_VBLANK_ID);
        Gwes_Frames++;
        __asm__ volatile("mcr p15, 0, %0, c7, c6, 1" :: "r"(MAILBOX));
        mk = mb[1]; pen = mb[2]; x = mb[3]; y = mb[4];
        k = (~KEYS_REG & 0x3FF) | (mk & 0xC00);
        for (i = 0; i < 12; i++) {
            unsigned bit = 1u << i;
            if ((k & bit) && !(prev & bit)) { PostMessageW((HWND)(g_focus ? g_focus : g_fore), WM_KEYDOWN, keymap[i], 1); rep[i] = 0; }
            else if ((k & bit) && i >= 4 && i <= 7 && ++rep[i] > 20 && rep[i] % 4 == 0) PostMessageW((HWND)(g_focus ? g_focus : g_fore), WM_KEYDOWN, keymap[i], 1);
            if (!(k & bit) && (prev & bit)) PostMessageW((HWND)(g_focus ? g_focus : g_fore), WM_KEYUP, keymap[i], 1);
        }
        prev = k;
        if (pen) {
            int sx = x, sy = y + GW_TOP_H;
            if (!g_pen_down) post_mouse(WM_LBUTTONDOWN, sx, sy);
            else if (sx != g_pen.x || sy != g_pen.y) post_mouse(WM_MOUSEMOVE, sx, sy);
            g_pen.x = sx; g_pen.y = sy; g_pen_down = 1;
        } else if (g_pen_down) {
            post_mouse(WM_LBUTTONUP, g_pen.x, g_pen.y);
            g_pen_down = 0;
        }
    }
}

void Gwes_Init(void)
{
    RECT all = { 0, 0, GW_SCREEN_W, GW_SCREEN_H };

    g_evt = CreateEventW(NULL, FALSE, FALSE, NULL);
    gdi_init();
    g_root.visible = 1; g_root.alive = 1; g_root.r = all;
    desktop_fill(&all);
    CreateThread(NULL, 0, input_thread, NULL, 0, NULL);
}
