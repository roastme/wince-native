#ifndef DSI_SD_H
#define DSI_SD_H
/* SD card access for the ARM9 kernel: sector requests are served by the ARM7 (arm7/source/main.c). */
int sd_init(unsigned *sectors);
int sd_read(unsigned sector, unsigned count, void *buf);    /* buf: 32-byte aligned main RAM; 0 = ok */
int sd_write(unsigned sector, unsigned count, const void *buf);
#endif
