#include <string.h>
#include "sd.h"
#include "fat.h"

#define SEC 512
#define NCACHE 32

static struct {
    unsigned part_lba, fat_lba, fat_sectors, root_lba, root_entries, data_lba, clus_secs, clus_count;
    int fat32;
    unsigned root_clus;
} v;

static unsigned char cache_buf[NCACHE][SEC] __attribute__((aligned(32)));
static unsigned cache_sec[NCACHE];
static unsigned char bounce[SEC] __attribute__((aligned(32)));
#define STAGE_SECS 16
static unsigned char stage[STAGE_SECS * SEC] __attribute__((aligned(32)));

/* Read-ahead window for small reads: keyed by physical sector, and the FAT driver is read-only, so it never goes stale. */
#define RA_SECS 32
static unsigned char ra_buf[RA_SECS * SEC] __attribute__((aligned(32)));
static unsigned ra_lba, ra_n;

static unsigned char *readahead(unsigned lba)
{
    if (ra_n && lba >= ra_lba && lba < ra_lba + ra_n) return ra_buf + (lba - ra_lba) * SEC;
    if (!sd_read(lba, RA_SECS, ra_buf)) ra_n = RA_SECS;
    else if (!sd_read(lba, 1, ra_buf)) ra_n = 1;
    else { ra_n = 0; return 0; }
    ra_lba = lba;
    return ra_buf;
}

static unsigned char *getsec(unsigned lba)
{
    unsigned i = (lba ^ (lba >> 5)) % NCACHE;   /* plain lba % NCACHE put the FAT and directory sectors in one slot */

    if (cache_sec[i] != lba + 1) {
        cache_sec[i] = 0;
        if (sd_read(lba, 1, cache_buf[i])) return 0;
        cache_sec[i] = lba + 1;
    }
    return cache_buf[i];
}

static unsigned rd16(const unsigned char *p) { return p[0] | p[1] << 8; }
static unsigned rd32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24; }

int fat_mount(void)
{
    unsigned char *s;
    unsigned secs, sd_secs = 0, tot, nroot_secs, nclus;

    memset(&v, 0, sizeof v);
    memset(cache_sec, 0, sizeof cache_sec);
    if (sd_init(&sd_secs)) return -1;
    if (!(s = getsec(0))) return -2;
    if (s[510] != 0x55 || s[511] != 0xAA) return -3;
    if (s[0] != 0xEB && s[0] != 0xE9) {         /* MBR: take the first FAT partition */
        int i;
        for (i = 0; i < 4; i++) {
            unsigned char *pe = s + 446 + 16 * i;
            if (pe[4] && pe[4] != 5 && pe[4] != 0xF) { v.part_lba = rd32(pe + 8); break; }
        }
        if (i == 4) return -4;
        if (!(s = getsec(v.part_lba))) return -5;
    }
    if (rd16(s + 11) != SEC) return -6;
    v.clus_secs = s[13];
    v.fat_lba = v.part_lba + rd16(s + 14);
    v.root_entries = rd16(s + 17);
    tot = rd16(s + 19) ? rd16(s + 19) : rd32(s + 32);
    v.fat_sectors = rd16(s + 22) ? rd16(s + 22) : rd32(s + 36);
    secs = s[16];
    v.root_lba = v.fat_lba + secs * v.fat_sectors;
    nroot_secs = (v.root_entries * 32 + SEC - 1) / SEC;
    v.data_lba = v.root_lba + nroot_secs;
    nclus = (tot - (v.data_lba - v.part_lba)) / v.clus_secs;
    v.clus_count = nclus;
    v.fat32 = nclus >= 65525;
    if (v.fat32) v.root_clus = rd32(s + 44);
    else if (nclus < 4085) return -7;           /* FAT12 not supported */
    return 0;
}

unsigned fat_total_sectors(void) { return v.data_lba + v.clus_count * v.clus_secs; }

static unsigned next_clus(unsigned c)
{
    unsigned off = v.fat32 ? c * 4 : c * 2, lba = v.fat_lba + off / SEC;
    unsigned char *s = getsec(lba);

    if (!s) return 0x0FFFFFFF;
    s += off % SEC;
    return v.fat32 ? rd32(s) & 0x0FFFFFFF : rd16(s);
}

static int is_eoc(unsigned c) { return c < 2 || c >= (v.fat32 ? 0x0FFFFFF8u : 0xFFF8u); }
static unsigned clus_lba(unsigned c) { return v.data_lba + (c - 2) * v.clus_secs; }

/* Absolute LBA of byte offset `off` in the object `e`; sets *run to the number of contiguous bytes from there. */
static int locate(FATENT *e, unsigned off, unsigned want, unsigned *lba, unsigned *in_sec, unsigned *run)
{
    unsigned cb = v.clus_secs * SEC;

    if (e->root && !v.fat32) {
        unsigned total = v.root_entries * 32;
        if (off >= total) return -1;
        *lba = v.root_lba + off / SEC; *in_sec = off % SEC; *run = total - off;
        return 0;
    } else {
        unsigned idx = off / cb, c, i;
        unsigned first = e->root ? v.root_clus : e->first;

        if (!first) return -1;
        if (e->hint_clus && e->hint_idx <= idx) { c = e->hint_clus; i = e->hint_idx; }
        else { c = first; i = 0; }
        for (; i < idx; i++) {
            c = next_clus(c);
            if (is_eoc(c)) return -1;
        }
        e->hint_idx = idx; e->hint_clus = c;
        {
            unsigned run_c = 1, cc = c;
            while (want > SEC && run_c * cb < 64 * 1024 && run_c * cb < off % cb + want) {       /* coalesce contiguous clusters */
                unsigned n = next_clus(cc);
                if (n != cc + 1) break;
                cc = n; run_c++;
            }
            *lba = clus_lba(c) + (off % cb) / SEC;
            *in_sec = off % SEC;
            *run = run_c * cb - off % cb;
        }
        return 0;
    }
}

int fat_read(FATENT *e, unsigned off, void *buf, unsigned n)
{
    unsigned char *d = buf;
    unsigned done = 0;

    if (!(e->attr & FAT_ATTR_DIR)) {
        if (off >= e->size) return 0;
        if (n > e->size - off) n = e->size - off;
    }
    while (done < n) {
        unsigned lba, in_sec, run, chunk;

        if (locate(e, off + done, n - done, &lba, &in_sec, &run)) break;
        chunk = n - done;
        if (chunk > run) chunk = run;
        if (!in_sec && chunk >= SEC) {
            unsigned secs = chunk / SEC;
            unsigned char *dst = d + done;
            if (secs > 128) secs = 128;
            if ((unsigned)dst & 31) {   /* unaligned caller buffer: batch through the aligned staging buffer */
                if (secs > STAGE_SECS) secs = STAGE_SECS;
                if (sd_read(lba, secs, stage)) break;
                memcpy(dst, stage, secs * SEC);
            } else if (sd_read(lba, secs, dst)) break;
            done += secs * SEC;
        } else {
            unsigned char *s = readahead(lba);
            unsigned c = SEC - in_sec;
            if (!s) break;
            if (c > chunk) c = chunk;
            memcpy(d + done, s + in_sec, c);
            done += c;
        }
    }
    return done;
}

static unsigned lc(unsigned c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static int name_eq(const unsigned short *a, int alen, const unsigned short *b)
{
    int i;
    for (i = 0; i < alen; i++)
        if (!b[i] || lc(a[i]) != lc(b[i])) return 0;
    return !b[alen];
}

static void fill_ent(const unsigned char *d, FATENT *e)
{
    memset(e, 0, sizeof *e);
    e->first = rd16(d + 26) | (v.fat32 ? rd16(d + 20) << 16 : 0);
    e->size = rd32(d + 28);
    e->attr = d[11];
    e->cdate = rd16(d + 16); e->ctime = rd16(d + 14);
    e->wdate = rd16(d + 24); e->wtime = rd16(d + 22);
}

/* Directory iteration: returns 0 and fills name/e for the idx-th visible entry, 1 at the end. */
static struct { unsigned first; int root; unsigned idx, off; } rcache[4];
static int rnext;

int fat_readdir(const FATENT *dir, unsigned idx, unsigned short *name, FATENT *e)
{
    FATENT d = *dir;
    unsigned off = 0, seen = 0;
    int ri;

    for (ri = 0; ri < 4; ri++)
        if (rcache[ri].idx && rcache[ri].first == dir->first && rcache[ri].root == dir->root && rcache[ri].idx <= idx) {
            seen = rcache[ri].idx; off = rcache[ri].off;
            break;
        }
    if (ri == 4) { ri = rnext; rnext = (rnext + 1) & 3; rcache[ri].first = dir->first; rcache[ri].root = dir->root; rcache[ri].idx = 0; rcache[ri].off = 0; }
    unsigned short lfn[260];
    int have_lfn = 0, k;
    unsigned char raw[32];

    d.hint_clus = 0; d.hint_idx = 0;
    for (;; off += 32) {
        if (fat_read(&d, off, raw, 32) != 32) return 1;
        if (!raw[0]) return 1;
        if (raw[0] == 0xE5) { have_lfn = 0; continue; }
        if (raw[11] == 0x0F) {
            unsigned ord = raw[0] & 0x1F;
            static const unsigned char pos[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
            if (raw[0] & 0x40) { memset(lfn, 0, sizeof lfn); have_lfn = 1; }
            if (ord >= 1 && ord <= 20)
                for (k = 0; k < 13; k++) lfn[(ord - 1) * 13 + k] = rd16(raw + pos[k]);
            continue;
        }
        if (raw[11] & FAT_ATTR_VOLID) { have_lfn = 0; continue; }
        if (raw[0] == '.' && (raw[1] == ' ' || (raw[1] == '.' && raw[2] == ' '))) { have_lfn = 0; continue; }
        if (seen++ == idx) {
            rcache[ri].idx = seen; rcache[ri].off = off + 32;
            fill_ent(raw, e);
            if (have_lfn) {
                for (k = 0; k < 259 && lfn[k] && lfn[k] != 0xFFFF; k++) name[k] = lfn[k];
                name[k] = 0;
            } else {
                int n = 0;
                for (k = 0; k < 8 && raw[k] != ' '; k++) name[n++] = (raw[12] & 8) ? lc(raw[k]) : raw[k];
                if (raw[8] != ' ') {
                    name[n++] = '.';
                    for (k = 8; k < 11 && raw[k] != ' '; k++) name[n++] = (raw[12] & 16) ? lc(raw[k]) : raw[k];
                }
                name[n] = 0;
            }
            return 0;
        }
        have_lfn = 0;
    }
}

#define PCACHE 128
static struct { unsigned hash; FATENT e; } pcache[PCACHE];

static unsigned path_hash(const unsigned short *p)
{
    unsigned h = 2166136261u, slash = 1;
    for (; *p; p++) {
        unsigned c = *p == '/' ? '\\' : lc(*p);
        if (c == '\\') { if (slash) continue; slash = 1; } else slash = 0;
        h = (h ^ c) * 16777619u;
    }
    return h | 1;
}

static int fat_lookup_slow(const unsigned short *path, FATENT *e);

int fat_lookup(const unsigned short *path, FATENT *e)
{
    unsigned h = path_hash(path), i = h % PCACHE;
    int r;

    if (pcache[i].hash == h) { *e = pcache[i].e; return 0; }
    r = fat_lookup_slow(path, e);
    if (!r) { pcache[i].hash = h; pcache[i].e = *e; }
    return r;
}

static int fat_lookup_slow(const unsigned short *path, FATENT *e)
{
    FATENT cur;
    unsigned short nm[260];

    memset(&cur, 0, sizeof cur);
    cur.attr = FAT_ATTR_DIR; cur.root = 1;
    for (;;) {
        int len = 0, i;
        while (*path == '\\' || *path == '/') path++;
        if (!*path) { *e = cur; return 0; }
        while (path[len] && path[len] != '\\' && path[len] != '/') len++;
        if (!(cur.attr & FAT_ATTR_DIR)) return 1;
        for (i = 0;; i++) {
            FATENT c;
            if (fat_readdir(&cur, i, nm, &c)) return 1;
            if (name_eq(path, len, nm)) { cur = c; break; }
        }
        path += len;
    }
}
