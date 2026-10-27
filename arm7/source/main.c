// ARM7 side of the native CE port: samples keys + touch every vblank and publishes them in a mailbox in main RAM
// (the ARM9 CE kernel has no libnds FIFO driver). Mailbox layout (u32): [0]=frame counter, [1]=keys (KEYINPUT|XY<<10, active high),
// [2]=pen down, [3]=x (0..255), [4]=y (0..191), [5]=lid/misc flags.
#include <nds.h>
#include <nds/arm7/sdmmc.h>

#define MAILBOX ((volatile u32 *)0x02FFD000)

static void vblank_handler(void)
{
    touchPosition t;
    u32 pen = 0;
    u32 xy = REG_KEYXY;

    if (!(xy & KEYXY_TOUCH)) {
        touchReadXY(&t);
        if (t.px || t.py) {
            pen = 1;
            MAILBOX[3] = t.px;
            MAILBOX[4] = t.py;
        }
    }
    MAILBOX[1] = (~REG_KEYINPUT & 0x3FF) | ((~xy & 0x3) << 10);
    MAILBOX[2] = pen;
    MAILBOX[5] = xy;
    MAILBOX[0]++;
}

// SD service for the ARM9 kernel (it has no FIFO driver): request block at 0x02FFD100 (u32): [0]=cmd (ARM9 writes it last; ARM7 clears it
// when done), [1]=sector, [2]=count (<= 128), [3]=main RAM address, [4]=status (0 ok, else SDMMC_ERR_*), [5]=total sectors after init.
// cmd: 1=init, 2=read, 3=write.
#define SDREQ ((volatile u32 *)0x02FFD100)

static void sd_service(void)
{
    u32 cmd = SDREQ[0], r = 0;

    if (!cmd)
        return;
    if (cmd == 1) {
        r = SDMMC_init(SDMMC_DEV_CARD);
        if (r == SDMMC_ERR_INITIALIZED)
            r = 0;
        SDREQ[5] = r ? 0 : SDMMC_getSectors(SDMMC_DEV_CARD);
    } else if (cmd == 2) {
        r = SDMMC_readSectors(SDMMC_DEV_CARD, SDREQ[1], (void *)SDREQ[3], SDREQ[2]);
    } else if (cmd == 3) {
        r = SDMMC_writeSectors(SDMMC_DEV_CARD, SDREQ[1], (const void *)SDREQ[3], SDREQ[2]);
    } else {
        r = 0xFF;
    }
    SDREQ[4] = r;
    SDREQ[0] = 0;
}

int main(void)
{
    readUserSettings();
    ledBlink(0);
    touchInit();
    irqInit();
    fifoInit();
    installSystemFIFO();
    irqSet(IRQ_VBLANK, vblank_handler);
    irqEnable(IRQ_VBLANK);
    for (;;)
        sd_service();
    return 0;
}
