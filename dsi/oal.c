/* Minimal DSi OAL for the native CE kernel (ARM946E-S, no MMU). First milestone: banner + KernelInit. */
#include <windows.h>
#include <nkintr.h>

#define DLOG_ADDR 0x02FF8000u   /* matches tools/dsirun's -l ring: u32 write counter, then 4092 bytes of text */
#define RING_SIZE 4092

#define REG_IME (*(volatile DWORD *)0x04000208)
#define REG_IE  (*(volatile DWORD *)0x04000210)
#define REG_IF  (*(volatile DWORD *)0x04000214)

extern void DsiConsolePutc(int c);

static void dlog_putc(int c)
{
    DsiConsolePutc(c);
    volatile DWORD *ring = (volatile DWORD *)DLOG_ADDR;
    volatile BYTE *buf = (volatile BYTE *)DLOG_ADDR + 4;
    DWORD wr = ring[0];

    buf[wr % RING_SIZE] = (BYTE)c;
    ring[0] = wr + 1;
    /* the headless runner reads RAM directly, so push it out of the write-back D cache */
    __asm__ volatile("mcr p15, 0, %0, c7, c14, 1" :: "r"(DLOG_ADDR));
    __asm__ volatile("mcr p15, 0, %0, c7, c14, 1" :: "r"((DLOG_ADDR + (wr % RING_SIZE) + 4) & ~31u));
    __asm__ volatile("mcr p15, 0, %0, c7, c10, 4" :: "r"(0));
}

extern void DsiVideoInit(void);
void OEMInitDebugSerial(void) { DsiVideoInit(); }
void OEMWriteDebugByte(BYTE c) { dlog_putc(c); }
int  OEMReadDebugByte(void) { return -1; }

void OEMWriteDebugString(unsigned short *s)
{
    while (*s)
        dlog_putc(*s++);
}

#define REG_TM0CNT_L (*(volatile unsigned short *)0x04000100)
#define REG_TM0CNT_H (*(volatile unsigned short *)0x04000102)
#define IRQ_TIMER0   (1u << 3)

extern DWORD CurMSec, dwReschedTime;

/* 1 ms system tick: timer 0 on 33.514 MHz / 64, reload 65536 - 524 (1.0007 ms) */
void OEMInit(void)
{
    REG_IME = 0;
    REG_TM0CNT_H = 0;
    REG_TM0CNT_L = 65536 - 524;
    REG_IF = 0xFFFFFFFF;
    REG_IE = IRQ_TIMER0;
    REG_TM0CNT_H = 0x00C1;      /* enable, irq, prescaler /64 */
    /* timer 1: count timer 0 overflows (cascade, no IRQ), so sd_req can see the ticks it masked (sd.c) */
    *(volatile unsigned short *)0x04000104 = 0;
    *(volatile unsigned short *)0x04000106 = 0x0084;   /* enable, count-up (cascade) */
    REG_IME = 1;
}
void InitClock(void) {}

/* SYSINTR_FIRMWARE+n <-> DSi IRQ number */
#define SYSINTR_VBLANK  (SYSINTR_FIRMWARE + 0)
#define IRQ_VBLANK      (1u << 0)
#define REG_DISPSTAT    (*(volatile unsigned short *)0x04000004)

ULONG OEMInterruptHandler(ULONG ra)
{
    DWORD pending = REG_IF & REG_IE;

    if (pending & IRQ_TIMER0) {
        REG_IF = IRQ_TIMER0;
        CurMSec++;
        if ((int)(CurMSec - dwReschedTime) >= 0)
            return SYSINTR_RESCHED;
        return SYSINTR_NOP;
    }
    if (pending & IRQ_VBLANK) {
        REG_IF = IRQ_VBLANK;
        return SYSINTR_VBLANK;      /* edge interrupt: nothing to mask until the IST calls InterruptDone */
    }
    REG_IF = pending;           /* nothing else is routed yet */
    return SYSINTR_NOP;
}
ULONG OEMInterruptHandlerFIQ(ULONG ra) { return SYSINTR_NOP; }

BOOL OEMInterruptEnable(DWORD idInt, LPVOID pvData, DWORD cbData)
{
    if (idInt == SYSINTR_VBLANK) {
        REG_DISPSTAT |= 0x0008;     /* vblank irq enable */
        REG_IE |= IRQ_VBLANK;
        return TRUE;
    }
    return FALSE;
}
void OEMInterruptDisable(DWORD idInt)
{
    if (idInt == SYSINTR_VBLANK)
        REG_IE &= ~IRQ_VBLANK;
}
void OEMInterruptDone(DWORD idInt) {}

void OEMIdle(void)
{
    __asm__ volatile("mcr p15, 0, %0, c7, c0, 4" :: "r"(0));   /* wait for interrupt */
}

void OEMPowerOff(void)
{
    for (;;)
        OEMIdle();
}

BOOL OEMGetRealTime(LPSYSTEMTIME t)
{
    t->wYear = 2026; t->wMonth = 10; t->wDayOfWeek = 4; t->wDay = 22;
    t->wHour = t->wMinute = t->wSecond = t->wMilliseconds = 0;
    return TRUE;
}
BOOL OEMSetRealTime(LPSYSTEMTIME t) { return TRUE; }
BOOL OEMSetAlarmTime(LPSYSTEMTIME t) { return TRUE; }

BOOL OEMGetExtensionDRAM(LPDWORD lpMemStart, LPDWORD lpMemLen) { return FALSE; }
BOOL OEMIoControl(DWORD code, LPVOID in, DWORD insz, LPVOID out, DWORD outsz, LPDWORD actual) { return FALSE; }
BOOL KernelIoControl(DWORD code, LPVOID in, DWORD insz, LPVOID out, DWORD outsz, LPDWORD actual) { return FALSE; }
BOOL KernelLibIoControl(HANDLE h, DWORD code, LPVOID in, DWORD insz, LPVOID out, DWORD outsz, LPDWORD actual) { return FALSE; }

DWORD OEMARMCacheMode(void) { return 0; }

/* flags from pkfuncs.h: CACHE_SYNC_DISCARD 1, WRITEBACK 2, INSTRUCTIONS 4 */
void OEMCacheRangeFlush(LPVOID pAddr, DWORD len, DWORD flags)
{
    DWORD a, end;

    if (pAddr == NULL) {
        DWORD seg, idx;
        for (seg = 0; seg < 4; seg++)
            for (idx = 0; idx < 32; idx++)
                __asm__ volatile("mcr p15, 0, %0, c7, c14, 2" :: "r"((seg << 30) | (idx << 5)));
    } else {
        a = (DWORD)pAddr & ~31u;
        end = (DWORD)pAddr + len;
        for (; a < end; a += 32)
            __asm__ volatile("mcr p15, 0, %0, c7, c14, 1" :: "r"(a));
    }
    __asm__ volatile("mcr p15, 0, %0, c7, c10, 4" :: "r"(0));
    if (flags & 4)
        __asm__ volatile("mcr p15, 0, %0, c7, c5, 0" :: "r"(0));
}

static void dlog_hex(DWORD v)
{
    int i;
    for (i = 28; i >= 0; i -= 4)
        dlog_putc("0123456789abcdef"[(v >> i) & 15]);
}

/* regs: r0-r12, lr (return address + offset); spsr; id 1=undef 3=prefetch 4=data. Never returns. */
void DsiExcDump(DWORD *regs, DWORD spsr, DWORD id)
{
    static const char *nm[] = { "", "UNDEF", "", "PABT", "DABT" };
    const char *s = nm[id];
    int i;

    while (*s) dlog_putc(*s++);
    dlog_putc(' '); dlog_putc('l'); dlog_putc('r'); dlog_putc('='); dlog_hex(regs[13]);
    dlog_putc(' '); dlog_putc('s'); dlog_putc('p'); dlog_putc('s'); dlog_putc('r'); dlog_putc('='); dlog_hex(spsr);
    for (i = 0; i < 13; i++) {
        dlog_putc(i % 4 ? ' ' : '\n'); dlog_putc('r'); dlog_putc('0' + i / 10); dlog_putc('0' + i % 10); dlog_putc('=');
        dlog_hex(regs[i]);
    }
    dlog_putc('\n');
    for (;;)
        ;
}
