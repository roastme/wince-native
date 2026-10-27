/* Mini GWES for the native DSi CE port: a window manager + GDI subset with the usual Win32 entry points (CreateWindowExW,
 * GetMessageW, BeginPaint, FillRect, DrawTextW ...). The DSi screens are one 256x384 desktop (top screen = rows 0-191,
 * bottom = 192-383). No MMU/PSL involved: it links straight into the image and runs in kernel-mode threads. */
#ifndef GWES_H
#define GWES_H

#include <windows.h>

#define GW_SCREEN_W 256
#define GW_SCREEN_H 384
#define GW_TOP_H    192

/* Start the input thread (vblank IST: keys + stylus from the ARM7 mailbox). Call once before creating windows. */
void Gwes_Init(void);
/* 15-bit icons: px has bit 15 set for opaque pixels. The data is not copied. */
void Gwes_RegisterIcon(int id, int w, int h, const unsigned short *px);
/* frame counter from the vblank thread */
extern volatile unsigned Gwes_Frames;

#endif
