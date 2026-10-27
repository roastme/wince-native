/* ARM9 side of the SD service: a request block in main RAM that the ARM7 polls (see arm7/source/main.c). */
#include "sd.h"

#define SDREQ ((volatile unsigned *)0x02FFD100)
#define SDREQ_LINE 0x02FFD100u

static void dc_clean(unsigned a, unsigned len)
{
    for (a &= ~31u, len += 31; (int)len > 0; a += 32, len -= 32)
        __asm__ volatile("mcr p15, 0, %0, c7, c10, 1" :: "r"(a));
    __asm__ volatile("mcr p15, 0, %0, c7, c10, 4" :: "r"(0));
}

static void dc_flush_inval(unsigned a, unsigned len)
{
    for (a &= ~31u, len += 31; (int)len > 0; a += 32, len -= 32)
        __asm__ volatile("mcr p15, 0, %0, c7, c14, 1" :: "r"(a));
    __asm__ volatile("mcr p15, 0, %0, c7, c10, 4" :: "r"(0));
}

static void dc_inval(unsigned a, unsigned len)
{
    for (a &= ~31u, len += 31; (int)len > 0; a += 32, len -= 32)
        __asm__ volatile("mcr p15, 0, %0, c7, c6, 1" :: "r"(a));
}


/* Timer 1 counts timer 0 overflows (1 ms each, see OEMInit in oal.c). The spin below runs with IRQs masked, so the tick
 * IRQs pend and collapse into one. The overflows that happened during the spin are added to CurMSec here. */
#define REG_TM1CNT_L (*(volatile unsigned short *)0x04000104)
#define REG_IF_HW    (*(volatile unsigned short *)0x04000214)
extern unsigned CurMSec;

/* The request runs with IRQs masked, so no other thread can use the single mailbox while it is in flight. */
static int sd_req(unsigned cmd, unsigned sector, unsigned count, unsigned addr)
{
    unsigned spin, saved = 0, t1, elapsed;
    int r;

    SDREQ[1] = sector; SDREQ[2] = count; SDREQ[3] = addr; SDREQ[4] = 0xFFFFFFFFu;
    SDREQ[0] = cmd;
    dc_clean(SDREQ_LINE, 32);
    __asm__ volatile("mrs %0, cpsr" : "=r"(saved));
    __asm__ volatile("msr cpsr_c, %0" :: "r"(saved | 0x80u));
    t1 = REG_TM1CNT_L;
    for (spin = 0; spin < 200000000u; spin++) {
        dc_inval(SDREQ_LINE, 32);
        if (!SDREQ[0]) break;
    }
    elapsed = (REG_TM1CNT_L - t1) & 0xffffu;
    /* Any overflow in the spin set IF, and that pending IRQ adds the last ms when the caller's CPSR is restored. */
    if (elapsed && (REG_IF_HW & 0x8)) CurMSec += elapsed - 1;
    r = spin < 200000000u ? (int)SDREQ[4] : -1;
    __asm__ volatile("msr cpsr_c, %0" :: "r"(saved));
    return r;
}

int sd_init(unsigned *sectors)
{
    int r = sd_req(1, 0, 0, 0);

    dc_inval(SDREQ_LINE, 32);
    if (sectors) *sectors = SDREQ[5];
    return r;
}

int sd_read(unsigned sector, unsigned count, void *buf)
{
    int r;

    dc_flush_inval((unsigned)buf, count * 512);
    r = sd_req(2, sector, count, (unsigned)buf);
    dc_inval((unsigned)buf, count * 512);
    return r;
}

int sd_write(unsigned sector, unsigned count, const void *buf)
{
    dc_clean((unsigned)buf, count * 512);
    return sd_req(3, sector, count, (unsigned)buf);
}

