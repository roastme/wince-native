/* First "desktop" on the bottom screen: taskbar, a movable window, Start menu. Driven by the vblank IST + KEYINPUT. */
#include <windows.h>
#include <nkintr.h>

extern void NKDbgPrintfW(const wchar_t *, ...);
extern void DsiFill(volatile unsigned short *fb, int x, int y, int w, int h, unsigned short c);
extern void DsiDrawText(volatile unsigned short *fb, int px, int py, const char *s, unsigned short fg, unsigned short bg);

#define FB      ((volatile unsigned short *)0x06200000)
#define KEYS    (*(volatile unsigned short *)0x04000130)
#define RGB(r, g, b) (0x8000 | ((r) >> 3) | (((g) >> 3) << 5) | (((b) >> 3) << 10))
#define SYSINTR_VBLANK (SYSINTR_FIRMWARE + 0)

#define MAILBOX 0x02FFD000u

/* ARM7 publishes keys/touch here each vblank (arm7/source/main.c); drop our cached copy before reading */
static void read_mailbox(unsigned *keys, int *pen, int *x, int *y, unsigned *cnt)
{
    volatile DWORD *m = (volatile DWORD *)MAILBOX;

    __asm__ volatile("mcr p15, 0, %0, c7, c6, 1" :: "r"(MAILBOX));
    *cnt = m[0]; *keys = m[1]; *pen = m[2]; *x = m[3]; *y = m[4];
}

enum { K_A = 1, K_B = 2, K_SELECT = 4, K_START = 8, K_RIGHT = 16, K_LEFT = 32, K_UP = 64, K_DOWN = 128 };

static void itoa_(DWORD v, char *s, int pad)
{
    char t[12];
    int n = 0, i;

    do { t[n++] = '0' + v % 10; v /= 10; } while (v);
    while (n < pad) t[n++] = '0';
    for (i = 0; i < n; i++) s[i] = t[n - 1 - i];
    s[n] = 0;
}

static void draw_clock(void)
{
    char b[16], m[8], s[8];
    DWORD sec = GetTickCount() / 1000;

    itoa_(sec / 60, m, 2);
    itoa_(sec % 60, s, 2);
    b[0] = 0;
    {
        char *p = b; const char *q;
        for (q = m; *q; ) *p++ = *q++;
        *p++ = ':';
        for (q = s; *q; ) *p++ = *q++;
        *p = 0;
    }
    DsiDrawText(FB, 256 - 48, 180, b, RGB(0, 0, 0), RGB(192, 192, 192));
}

static void draw_all(int wx, int wy, int menu, int frames)
{
    char b[12];

    DsiFill(FB, 0, 0, 256, 176, RGB(0, 80, 80));
    /* window */
    DsiFill(FB, wx, wy, 140, 80, RGB(192, 192, 192));
    DsiFill(FB, wx + 2, wy + 2, 136, 12, RGB(0, 0, 128));
    DsiDrawText(FB, wx + 6, wy + 4, "Windows CE", RGB(255, 255, 255), RGB(0, 0, 128));
    DsiDrawText(FB, wx + 6, wy + 22, "running natively", RGB(0, 0, 0), RGB(192, 192, 192));
    DsiDrawText(FB, wx + 6, wy + 34, "on the ARM946E-S", RGB(0, 0, 0), RGB(192, 192, 192));
    DsiDrawText(FB, wx + 6, wy + 50, "frames:", RGB(0, 0, 0), RGB(192, 192, 192));
    itoa_(frames, b, 1);
    DsiDrawText(FB, wx + 62, wy + 50, b, RGB(0, 0, 0), RGB(192, 192, 192));
    /* taskbar */
    DsiFill(FB, 0, 176, 256, 16, RGB(192, 192, 192));
    DsiFill(FB, 0, 176, 256, 1, RGB(255, 255, 255));
    DsiFill(FB, 2, 178, 44, 12, menu ? RGB(128, 128, 128) : RGB(224, 224, 224));
    DsiDrawText(FB, 6, 180, "Start", RGB(0, 0, 0), menu ? RGB(128, 128, 128) : RGB(224, 224, 224));
    if (menu) {
        DsiFill(FB, 2, 120, 90, 56, RGB(192, 192, 192));
        DsiDrawText(FB, 8, 126, "Programs", RGB(0, 0, 0), RGB(192, 192, 192));
        DsiDrawText(FB, 8, 140, "Settings", RGB(0, 0, 0), RGB(192, 192, 192));
        DsiDrawText(FB, 8, 154, "Shut Down", RGB(0, 0, 0), RGB(192, 192, 192));
    }
    draw_clock();
}

DWORD UiMain(LPVOID unused)
{
    HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    int wx = 60, wy = 40, menu = 0, frames = 0, dirty = 1;
    unsigned prev = 0;
    int ppen = 0, drag = 0, dx = 0, dy = 0;
    DWORD lastsec = 0;

    if (!InterruptInitialize(SYSINTR_VBLANK, ev, NULL, 0)) {
        NKDbgPrintfW(L"ui: InterruptInitialize failed\r\n");
        return 1;
    }
    NKDbgPrintfW(L"ui: vblank IST running\r\n");
    for (;;) {
        unsigned k;
        DWORD sec;

        if (WaitForSingleObject(ev, 1000) != WAIT_OBJECT_0) {
            NKDbgPrintfW(L"ui: vblank timeout\r\n");
            continue;
        }
        InterruptDone(SYSINTR_VBLANK);
        frames++;
        k = ~KEYS & 0xFF;
        {
            unsigned mk, cnt;
            int pen, tx, ty;

            read_mailbox(&mk, &pen, &tx, &ty, &cnt);
            k |= mk & 0xFF;
            if (pen && !ppen) {
                if (tx < 48 && ty >= 176) { menu = !menu; dirty = 1; }
                else if (menu && tx < 92 && ty >= 120 && ty < 176) { menu = 0; dirty = 1; }
                else if (tx >= wx && tx < wx + 140 && ty >= wy && ty < wy + 14) { drag = 1; dx = tx - wx; dy = ty - wy; }
                else if (menu) { menu = 0; dirty = 1; }
            }
            if (pen && drag) { wx = tx - dx; wy = ty - dy; dirty = 1; }
            if (!pen) drag = 0;
            ppen = pen;
            if (frames % 60 == 1)
                NKDbgPrintfW(L"ui: arm7 frame %d keys %x pen %d (%d,%d)\r\n", cnt, mk, pen, tx, ty);
        }
        if (k & K_LEFT)  { wx -= 2; dirty = 1; }
        if (k & K_RIGHT) { wx += 2; dirty = 1; }
        if (k & K_UP)    { wy -= 2; dirty = 1; }
        if (k & K_DOWN)  { wy += 2; dirty = 1; }
        if ((k & K_A) && !(prev & K_A)) { menu = !menu; dirty = 1; }
        if (wx < 0) wx = 0;
        if (wx > 116) wx = 116;
        if (wy < 0) wy = 0;
        if (wy > 96) wy = 96;
        prev = k;
        sec = GetTickCount() / 1000;
        if (sec != lastsec) {
            lastsec = sec;
            dirty = 1;
            if (sec % 5 == 0)
                NKDbgPrintfW(L"ui: %d frames, win=%d,%d\r\n", frames, wx, wy);
        }
        if (dirty) {
            draw_all(wx, wy, menu, frames);
            dirty = 0;
        }
    }
}
