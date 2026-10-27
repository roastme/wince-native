#ifndef DSI_FAT_H
#define DSI_FAT_H
/* Small FAT16/FAT32 driver (long names, read and in-place overwrite) on top of dsi/sd.c. */
#define FAT_ATTR_RO 0x01
#define FAT_ATTR_HIDDEN 0x02
#define FAT_ATTR_SYSTEM 0x04
#define FAT_ATTR_VOLID 0x08
#define FAT_ATTR_DIR 0x10
#define FAT_ATTR_ARCH 0x20

typedef struct {
    unsigned first;         /* first cluster (0 = empty file, or the FAT16 root directory) */
    unsigned size;
    unsigned char attr;
    unsigned char root;     /* this entry is the volume root */
    unsigned short wdate, wtime, cdate, ctime;
    unsigned hint_idx, hint_clus;   /* last cluster-chain position, speeds up sequential access */
} FATENT;

int fat_mount(void);                                            /* 0 = ok */
int fat_lookup(const unsigned short *path, FATENT *e);          /* '\\' or '/' separated, wide chars; 0 = found */
int fat_readdir(const FATENT *dir, unsigned idx, unsigned short *name, FATENT *e);   /* name[260]; 0 = ok, 1 = no more */
int fat_read(FATENT *e, unsigned off, void *buf, unsigned n);   /* bytes read */
unsigned fat_total_sectors(void);
#endif
