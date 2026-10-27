/* DSi video: top screen = VRAM A in "VRAM display" mode (direct 15-bit colour), bottom screen = sub-engine BG3 bitmap in VRAM C.
 * A text console sits on the top screen and receives the kernel debug output. */
#include <windows.h>
#include "font8x8.h"

#define REG16(a) (*(volatile unsigned short *)(a))
#define REG8(a)  (*(volatile unsigned char *)(a))

#define VRAM_A   ((volatile unsigned short *)0x06800000)
#define VRAM_SUB ((volatile unsigned short *)0x06200000)

#define COLS 32
#define ROWS 24

static int cx, cy;
static int video_up;
static int console_muted = 1;   /* start muted: the scroll loop cost most of the guest CPU during boot */

#define RGB(r, g, b) (0x8000 | ((r) >> 3) | (((g) >> 3) << 5) | (((b) >> 3) << 10))

void DsiFill(volatile unsigned short *fb, int x0, int y0, int w, int h, unsigned short c)
{
    int x, y;

    for (y = y0; y < y0 + h; y++)
        for (x = x0; x < x0 + w; x++)
            fb[y * 256 + x] = c;
}

void DsiDrawChar(volatile unsigned short *fb, int px, int py, int ch, unsigned short fg, unsigned short bg)
{
    const unsigned char *g;
    int x, y;

    if (ch < 0x20 || ch > 0x7e)
        ch = '?';
    g = font8x8[ch - 0x20];
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++)
            fb[(py + y) * 256 + px + x] = (g[y] & (0x80 >> x)) ? fg : bg;
}

void DsiDrawText(volatile unsigned short *fb, int px, int py, const char *s, unsigned short fg, unsigned short bg)
{
    for (; *s; s++, px += 8)
        DsiDrawChar(fb, px, py, *s, fg, bg);
}

void DsiVideoInit(void)
{
    REG16(0x04000304) = 0x8203;         /* POWCNT1: both LCDs, engine A + B, A on the top screen */
    REG8(0x04000240) = 0x80;            /* VRAMCNT_A: LCDC mode, 0x06800000 */
    REG8(0x04000242) = 0x84;            /* VRAMCNT_C: sub-engine BG, 0x06200000 */
    *(volatile unsigned long *)0x04000000 = 0x00020000;     /* engine A: VRAM display, block A */
    *(volatile unsigned long *)0x04001000 = 0x00010805;     /* engine B: mode 5, BG3, normal display */
    REG16(0x0400100E) = 0x4084;         /* BG3CNT: 256x256 direct-colour bitmap at the start of sub-BG VRAM */
    REG16(0x04001030) = 0x100;          /* BG3PA..PD: identity */
    REG16(0x04001032) = 0;
    REG16(0x04001034) = 0;
    REG16(0x04001036) = 0x100;
    DsiFill(VRAM_A, 0, 0, 256, 192, RGB(0, 0, 32));
    DsiFill(VRAM_SUB, 0, 0, 256, 192, RGB(0, 80, 80));
    video_up = 1;
}

static void console_scroll(void)
{
    int x, y;

    for (y = 0; y < 192 - 8; y++)
        for (x = 0; x < 256; x++)
            VRAM_A[y * 256 + x] = VRAM_A[(y + 8) * 256 + x];
    DsiFill(VRAM_A, 0, 192 - 8, 256, 8, RGB(0, 0, 32));
}

/* Stop the debug console from writing the top screen, so the WM6 display owns it. */
void DsiConsoleMute(void)
{
    console_muted = 1;
    DsiFill(VRAM_A, 0, 0, 256, 192, RGB(32, 0, 0));
}

void DsiConsolePutc(int c)
{
    if (!video_up || console_muted)
        return;
    if (c == '\r') {
        cx = 0;
        return;
    }
    if (c == '\n') {
        cx = 0;
        if (++cy >= ROWS) {
            console_scroll();
            cy = ROWS - 1;
        }
        return;
    }
    DsiDrawChar(VRAM_A, cx * 8, cy * 8, c, RGB(255, 255, 255), RGB(0, 0, 32));
    if (++cx >= COLS)
        DsiConsolePutc('\n');
}
