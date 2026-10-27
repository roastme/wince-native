/* API set 20 (SH_FILESYS_APIS) for the flat DSi kernel: a stand-in for filesys.exe, implemented against the documented method indices
 * (OAK/INC/mwinbase.h, mwinreg.h).  Files come from the SD card (FAT, under \wm6) with a RAM overlay for anything written; the registry is
 * built from the blob that tools/wm6/mkreg.py makes out of the ROM's .reg/.rgu sources. */
#include <windows.h>
#include <kernel.h>
#include <string.h>
#include "fat.h"

extern void NKDbgPrintfW(const wchar_t *, ...);

int g_fsTrace;   /* set to log every CreateFile */

#define MAGIC_KEY 0x4B455931u

/* ---- arena allocator: chunks from the VirtualAlloc pool, never freed ------------------------------------------------------------ */
static BYTE *ar_cur, *ar_end;

static void *fs_alloc(DWORD n)
{
    void *p;

    n = (n + 7) & ~7u;
    if (ar_cur + n > ar_end) {
        DWORD chunk = n > 0x10000 ? (n + 0xffff) & ~0xffffu : 0x10000;
        BYTE *b = VirtualAlloc(NULL, chunk, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!b) return NULL;
        ar_cur = b; ar_end = b + chunk;
    }
    p = ar_cur; ar_cur += n;
    return p;
}

static int wlen(const WCHAR *s) { int n = 0; while (s[n]) n++; return n; }
static WCHAR lcw(WCHAR c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int wieq(const WCHAR *a, const WCHAR *b)
{
    for (; *a && *b; a++, b++) if (lcw(*a) != lcw(*b)) return 0;
    return *a == *b;
}
static int wineq(const WCHAR *a, int n, const WCHAR *b)
{
    int i;
    for (i = 0; i < n; i++) if (!b[i] || lcw(a[i]) != lcw(b[i])) return 0;
    return !b[n];
}

/* ---- registry ------------------------------------------------------------------------------------------------------------------- */
typedef struct RVal { const WCHAR *name; DWORD type; const BYTE *data; DWORD size; } RVal;
typedef struct RNode {
    const WCHAR *name;
    struct RNode *parent, *child, *sibling;
    RVal *vals; DWORD nvals, cap;
} RNode;
typedef struct { DWORD magic; RNode *n; } RKey;

static RNode *reg_root;

static const BYTE *g_regblob;

static int reg_load(void)
{
    FATENT e;
    BYTE *b;
    DWORD i, nn, nv, o_n, o_v, o_s, o_d, got = 0, total;
    RNode *nodes; RVal *vals;
    const DWORD *hd;

    if (fat_lookup(L"\\wm6\\boot.reg", &e)) { NKDbgPrintfW(L"fs: no \\wm6\\boot.reg\r\n"); return -1; }
    b = VirtualAlloc(NULL, e.size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!b) return -2;
    while (got < e.size) {
        int r = fat_read(&e, got, b + got, e.size - got > 0x10000 ? 0x10000 : e.size - got);
        if (r <= 0) break;
        got += r;
    }
    if (got != e.size || memcmp(b, "DREG", 4)) { NKDbgPrintfW(L"fs: bad boot.reg (%d of %d)\r\n", got, e.size); return -3; }
    hd = (const DWORD *)b;
    nn = hd[1]; nv = hd[2]; o_n = hd[3]; o_v = hd[4]; o_s = hd[5]; o_d = hd[6]; total = hd[7];
    (void)total;
    nodes = fs_alloc(nn * sizeof(RNode)); vals = fs_alloc(nv * sizeof(RVal));
    if (!nodes || !vals) return -4;
    for (i = 0; i < nv; i++) {
        const DWORD *v = (const DWORD *)(b + o_v + 16 * i);
        vals[i].name = (const WCHAR *)(b + o_s + v[0]);
        vals[i].type = v[1];
        vals[i].data = b + o_d + v[2];
        vals[i].size = v[3];
    }
    for (i = 0; i < nn; i++) {
        const DWORD *n = (const DWORD *)(b + o_n + 24 * i);
        nodes[i].name = (const WCHAR *)(b + o_s + n[0]);
        nodes[i].parent = n[1] == 0xFFFFFFFFu ? NULL : &nodes[n[1]];
        nodes[i].child = n[2] ? &nodes[n[2]] : NULL;
        nodes[i].sibling = n[3] ? &nodes[n[3]] : NULL;
        nodes[i].vals = &vals[n[4]];
        nodes[i].nvals = nodes[i].cap = n[5];
    }
    reg_root = &nodes[0];
    g_regblob = b;
    NKDbgPrintfW(L"fs: registry %d keys %d values\r\n", nn, nv);
    return 0;
}

static RNode *rn_child(RNode *n, const WCHAR *name, int len)
{
    RNode *c;
    for (c = n->child; c; c = c->sibling)
        if (wineq(name, len, c->name)) return c;
    return NULL;
}

static RNode *rn_new(RNode *parent, const WCHAR *name, int len)
{
    RNode *n = fs_alloc(sizeof(RNode));
    WCHAR *nm = fs_alloc((len + 1) * 2);

    if (!n || !nm) return NULL;
    memcpy(nm, name, len * 2); nm[len] = 0;
    memset(n, 0, sizeof *n);
    n->name = nm; n->parent = parent;
    if (!parent->child) parent->child = n;
    else {
        RNode *c = parent->child;
        while (c->sibling) c = c->sibling;
        c->sibling = n;
    }
    return n;
}

/* Walk `path` below `n`; with create, missing keys are made.  *created is set when at least one key was made. */
static RNode *rn_walk(RNode *n, const WCHAR *path, int create, int *created)
{
    while (n) {
        int len = 0;
        RNode *c;
        while (*path == '\\') path++;
        if (!*path) return n;
        while (path[len] && path[len] != '\\') len++;
        c = rn_child(n, path, len);
        if (!c) {
            if (!create) return NULL;
            c = rn_new(n, path, len);
            if (created) *created = 1;
        }
        n = c;
        path += len;
    }
    return NULL;
}

static RNode *hive(DWORD h)
{
    static const WCHAR *names[4] = { L"HKEY_CLASSES_ROOT", L"HKEY_CURRENT_USER", L"HKEY_LOCAL_MACHINE", L"HKEY_USERS" };
    RNode *n;
    if (!reg_root) return NULL;
    n = rn_child(reg_root, names[h & 3], wlen(names[h & 3]));
    return n ? n : rn_new(reg_root, names[h & 3], wlen(names[h & 3]));
}

static RNode *hk2node(HKEY h)
{
    DWORD v = (DWORD)h;
    if (v >= 0x80000000u && v <= 0x80000003u) return hive(v);
    if (v >= 0x02000000u && v < 0x03000000u && !(v & 3) && ((RKey *)v)->magic == MAGIC_KEY) return ((RKey *)v)->n;
    return NULL;
}

static HKEY newkey(RNode *n)
{
    RKey *k = fs_alloc(sizeof(RKey));
    if (!k) return NULL;
    k->magic = MAGIC_KEY; k->n = n;
    return (HKEY)k;
}

static LONG fs_RegCloseKey(HKEY h)
{
    DWORD v = (DWORD)h;
    if (v >= 0x80000000u && v <= 0x80000003u) return ERROR_SUCCESS;
    if (!hk2node(h)) return ERROR_INVALID_HANDLE;
    ((RKey *)v)->magic = 0;
    return ERROR_SUCCESS;
}

static LONG fs_RegOpenKeyExW(HKEY h, LPCWSTR sub, DWORD opt, REGSAM sam, PHKEY phk)
{
    RNode *n = hk2node(h), *r;
    if (!n) return ERROR_INVALID_HANDLE;
    r = sub ? rn_walk(n, sub, 0, NULL) : n;
    if (!r) return ERROR_FILE_NOT_FOUND;
    *phk = newkey(r);
    return *phk ? ERROR_SUCCESS : ERROR_OUTOFMEMORY;
}

static LONG fs_RegCreateKeyExW(HKEY h, LPCWSTR sub, DWORD res, LPWSTR cls, DWORD opts, REGSAM sam, LPSECURITY_ATTRIBUTES sa, PHKEY phk, LPDWORD disp)
{
    RNode *n = hk2node(h), *r;
    int created = 0;
    if (!n) return ERROR_INVALID_HANDLE;
    r = sub ? rn_walk(n, sub, 1, &created) : n;
    if (!r) return ERROR_OUTOFMEMORY;
    *phk = newkey(r);
    if (disp) *disp = created ? REG_CREATED_NEW_KEY : REG_OPENED_EXISTING_KEY;
    return *phk ? ERROR_SUCCESS : ERROR_OUTOFMEMORY;
}

static LONG fs_RegDeleteKeyW(HKEY h, LPCWSTR sub)
{
    RNode *n = hk2node(h), *r, *c;
    if (!n) return ERROR_INVALID_HANDLE;
    r = rn_walk(n, sub, 0, NULL);
    if (!r || !r->parent) return ERROR_FILE_NOT_FOUND;
    c = r->parent->child;
    if (c == r) r->parent->child = r->sibling;
    else {
        while (c->sibling != r) c = c->sibling;
        c->sibling = r->sibling;
    }
    return ERROR_SUCCESS;
}

static RVal *rn_val(RNode *n, LPCWSTR name)
{
    DWORD i;
    if (!name) name = L"";
    for (i = 0; i < n->nvals; i++)
        if (wieq(n->vals[i].name, name)) return &n->vals[i];
    return NULL;
}

static LONG fs_RegDeleteValueW(HKEY h, LPCWSTR name)
{
    RNode *n = hk2node(h);
    RVal *v;
    if (!n) return ERROR_INVALID_HANDLE;
    if (!(v = rn_val(n, name))) return ERROR_FILE_NOT_FOUND;
    memmove(v, v + 1, (n->vals + n->nvals - v - 1) * sizeof(RVal));
    n->nvals--;
    return ERROR_SUCCESS;
}

static LONG fs_RegSetValueExW(HKEY h, LPCWSTR name, DWORD res, DWORD type, const BYTE *data, DWORD cb)
{
    RNode *n = hk2node(h);
    RVal *v;
    BYTE *d;
    WCHAR *nm;
    if (!n) return ERROR_INVALID_HANDLE;
    if (!name) name = L"";
    v = rn_val(n, name);
    if (!v) {
        if (n->nvals == n->cap) {
            DWORD nc = n->cap ? n->cap * 2 : 4;
            RVal *nv = fs_alloc(nc * sizeof(RVal));
            if (!nv) return ERROR_OUTOFMEMORY;
            memcpy(nv, n->vals, n->nvals * sizeof(RVal));
            n->vals = nv; n->cap = nc;
        }
        nm = fs_alloc((wlen(name) + 1) * 2);
        if (!nm) return ERROR_OUTOFMEMORY;
        memcpy(nm, name, (wlen(name) + 1) * 2);
        v = &n->vals[n->nvals++];
        v->name = nm;
    }
    d = fs_alloc(cb ? cb : 4);
    if (!d) return ERROR_OUTOFMEMORY;
    if (cb) memcpy(d, data, cb);
    v->type = type; v->data = d; v->size = cb;
    return ERROR_SUCCESS;
}

static LONG fs_RegQueryValueExW(HKEY h, LPCWSTR name, LPDWORD res, LPDWORD ptype, LPBYTE data, LPDWORD pcb)
{
    RNode *n = hk2node(h);
    RVal *v;
    if (!n) return ERROR_INVALID_HANDLE;
    if (!(v = rn_val(n, name))) return ERROR_FILE_NOT_FOUND;
    if (ptype) *ptype = v->type;
    if (!pcb) return data ? ERROR_INVALID_PARAMETER : ERROR_SUCCESS;
    if (!data) { *pcb = v->size; return ERROR_SUCCESS; }
    if (*pcb < v->size) { *pcb = v->size; return ERROR_MORE_DATA; }
    memcpy(data, v->data, v->size);
    *pcb = v->size;
    return ERROR_SUCCESS;
}

static LONG fs_RegEnumValueW(HKEY h, DWORD idx, LPWSTR name, LPDWORD pcbName, LPDWORD res, LPDWORD ptype, LPBYTE data, LPDWORD pcbData)
{
    RNode *n = hk2node(h);
    RVal *v;
    DWORD len;
    if (!n) return ERROR_INVALID_HANDLE;
    if (idx >= n->nvals) return ERROR_NO_MORE_ITEMS;
    v = &n->vals[idx];
    len = wlen(v->name);
    if (*pcbName < len + 1) { *pcbName = len + 1; return ERROR_MORE_DATA; }
    if (data && pcbData && *pcbData < v->size) { *pcbData = v->size; return ERROR_MORE_DATA; }
    memcpy(name, v->name, (len + 1) * 2);
    *pcbName = len;
    if (ptype) *ptype = v->type;
    if (pcbData) {
        if (data) memcpy(data, v->data, v->size);
        *pcbData = v->size;
    }
    return ERROR_SUCCESS;
}

static LONG fs_RegEnumKeyExW(HKEY h, DWORD idx, LPWSTR name, LPDWORD pcbName, LPDWORD res, LPWSTR cls, LPDWORD pcbCls, PFILETIME pft)
{
    RNode *n = hk2node(h), *c;
    DWORD len;
    if (!n) return ERROR_INVALID_HANDLE;
    for (c = n->child; c && idx; c = c->sibling) idx--;
    if (!c) return ERROR_NO_MORE_ITEMS;
    len = wlen(c->name);
    if (*pcbName < len + 1) { *pcbName = len + 1; return ERROR_MORE_DATA; }
    memcpy(name, c->name, (len + 1) * 2);
    *pcbName = len;
    if (cls && pcbCls && *pcbCls) cls[0] = 0;
    if (pcbCls) *pcbCls = 0;
    if (pft) pft->dwLowDateTime = pft->dwHighDateTime = 0;
    return ERROR_SUCCESS;
}

static LONG fs_RegQueryInfoKeyW(HKEY h, LPWSTR cls, LPDWORD pcbCls, LPDWORD res, LPDWORD pcSub, LPDWORD pcbMaxSub, LPDWORD pcbMaxCls,
                                LPDWORD pcVals, LPDWORD pcbMaxName, LPDWORD pcbMaxData, LPDWORD pcbSec, PFILETIME pft)
{
    RNode *n = hk2node(h), *c;
    DWORD i, nsub = 0, maxsub = 0, maxname = 0, maxdata = 0;
    if (!n) return ERROR_INVALID_HANDLE;
    for (c = n->child; c; c = c->sibling) { DWORD l = wlen(c->name); nsub++; if (l > maxsub) maxsub = l; }
    for (i = 0; i < n->nvals; i++) {
        DWORD l = wlen(n->vals[i].name);
        if (l > maxname) maxname = l;
        if (n->vals[i].size > maxdata) maxdata = n->vals[i].size;
    }
    if (cls && pcbCls && *pcbCls) cls[0] = 0;
    if (pcbCls) *pcbCls = 0;
    if (pcSub) *pcSub = nsub;
    if (pcbMaxSub) *pcbMaxSub = maxsub;
    if (pcbMaxCls) *pcbMaxCls = 0;
    if (pcVals) *pcVals = n->nvals;
    if (pcbMaxName) *pcbMaxName = maxname;
    if (pcbMaxData) *pcbMaxData = maxdata;
    if (pcbSec) *pcbSec = 0;
    if (pft) pft->dwLowDateTime = pft->dwHighDateTime = 0;
    return ERROR_SUCCESS;
}

static LONG fs_RegFlushKey(HKEY h) { return hk2node(h) ? ERROR_SUCCESS : ERROR_INVALID_HANDLE; }

/* ---- files ---------------------------------------------------------------------------------------------------------------------- */
#define MAXPATHW 260
#define ROOTDIR L"\\wm6"

typedef struct RFile {              /* RAM overlay entry: a file or directory created at run time, or a tombstone for a deleted ROM file */
    WCHAR path[MAXPATHW];
    BYTE *data; DWORD size, cap;
    DWORD attr; FILETIME ft;
    int used, dir, deleted;
} RFile;
#define MAX_RFILES 128
static RFile rfiles[MAX_RFILES];

typedef struct FileObj {
    int used;
    FATENT e;                       /* when !ram */
    RFile *rf;                      /* RAM-backed */
    DWORD pos, access;
    WCHAR path[MAXPATHW];
} FileObj;
#define MAX_FILES 96
static FileObj files[MAX_FILES];

typedef struct FindObj {
    int used;
    WCHAR dir[MAXPATHW], pat[MAXPATHW];
    FATENT d; int have_d;
    DWORD idx;                      /* index into the SD directory, then MAX_RFILES entries after it */
    DWORD ridx;
    int in_ram;
} FindObj;
#define MAX_FINDS 16
static FindObj finds[MAX_FINDS];

static const CINFO cinfFile, cinfFind;
extern const CINFO cinfMap;

/* Normalise a CE path: backslashes, rooted at \, no trailing \ (except the root itself). */
static int norm_path(LPCWSTR in, WCHAR *out)
{
    int n = 0;
    if (!in) return -1;
    if (in[0] != '\\' && in[0] != '/') out[n++] = '\\';
    for (; *in && n < MAXPATHW - 2; in++) {
        WCHAR c = *in == '/' ? '\\' : *in;
        if (c == '\\' && n && out[n - 1] == '\\') continue;
        out[n++] = c;
    }
    if (*in) return -1;
    if (n > 1 && out[n - 1] == '\\') n--;
    out[n] = 0;
    return n;
}

static void sd_path(const WCHAR *p, WCHAR *out)
{
    int n = wlen(ROOTDIR);
    memcpy(out, ROOTDIR, n * 2);
    memcpy(out + n, p, (wlen(p) + 1) * 2);
}

static RFile *rf_find(const WCHAR *p)
{
    int i;
    for (i = 0; i < MAX_RFILES; i++)
        if (rfiles[i].used && wieq(rfiles[i].path, p)) return &rfiles[i];
    return NULL;
}

static RFile *rf_new(const WCHAR *p)
{
    int i;
    for (i = 0; i < MAX_RFILES; i++)
        if (!rfiles[i].used) {
            memset(&rfiles[i], 0, sizeof(RFile));
            memcpy(rfiles[i].path, p, (wlen(p) + 1) * 2);
            rfiles[i].used = 1;
            return &rfiles[i];
        }
    return NULL;
}

static int rf_reserve(RFile *rf, DWORD n)
{
    if (n > rf->cap) {
        DWORD nc = rf->cap ? rf->cap : 4096;
        BYTE *nd;
        while (nc < n) nc *= 2;
        nd = VirtualAlloc(NULL, nc, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!nd) return 0;
        if (rf->size) memcpy(nd, rf->data, rf->size);
        if (rf->data) VirtualFree(rf->data, 0, MEM_RELEASE);
        rf->data = nd; rf->cap = nc;
    }
    return 1;
}

static long days_from_civil(long y, long m, long d)
{
    long era, yoe, doy, doe;
    y -= m <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static void fat_ft(unsigned date, unsigned time, FILETIME *ft)
{
    unsigned long long secs, t;
    long y = 1980 + (date >> 9), mo = (date >> 5) & 15, d = date & 31;
    if (!mo) mo = 1;
    if (!d) d = 1;
    secs = (unsigned long long)((long long)days_from_civil(y, mo, d) * 86400LL) + (time >> 11) * 3600u + ((time >> 5) & 63) * 60u + (time & 31) * 2u + 11644473600ULL;
    t = secs * 10000000ULL;
    ft->dwLowDateTime = (DWORD)t; ft->dwHighDateTime = (DWORD)(t >> 32);
}

static DWORD fat_attr_to_win(unsigned char a)
{
    DWORD r = 0;
    if (a & FAT_ATTR_RO) r |= FILE_ATTRIBUTE_READONLY;
    if (a & FAT_ATTR_HIDDEN) r |= FILE_ATTRIBUTE_HIDDEN;
    if (a & FAT_ATTR_SYSTEM) r |= FILE_ATTRIBUTE_SYSTEM;
    if (a & FAT_ATTR_DIR) r |= FILE_ATTRIBUTE_DIRECTORY;
    if (a & FAT_ATTR_ARCH) r |= FILE_ATTRIBUTE_ARCHIVE;
    return r ? r : FILE_ATTRIBUTE_NORMAL;
}

/* Resolve a normalised CE path: RAM overlay first, then the SD tree.  Returns 0 if found (rf or e filled), else 1. */
static int resolve(const WCHAR *np, RFile **rf, FATENT *e)
{
    WCHAR sp[MAXPATHW + 8];
    RFile *r = rf_find(np);

    *rf = NULL;
    if (r) {
        if (r->deleted) return 1;
        *rf = r;
        return 0;
    }
    if (np[0] == '\\' && !np[1]) { sd_path(np, sp); return fat_lookup(sp, e); }
    sd_path(np, sp);
    return fat_lookup(sp, e);
}

static FileObj *file_alloc(HANDLE *ph)
{
    int i;
    for (i = 0; i < MAX_FILES; i++)
        if (!files[i].used) {
            memset(&files[i], 0, sizeof(FileObj));
            files[i].used = 1;
            *ph = AllocHandle(&cinfFile, &files[i], pCurProc);
            if (!*ph) { files[i].used = 0; return NULL; }
            return &files[i];
        }
    return NULL;
}

static FileObj *h2file(HANDLE h)
{
    FileObj *f = (FileObj *)GetObjectPtrByType(h, HT_FILE);
    return f && f->used ? f : NULL;
}

static const WCHAR *devnames[] = { L"COM", L"LPT", L"NUL", L"CON", L"$device", L"\\$device", L"\\\\.", NULL };

static HANDLE fs_CreateFileW(LPCWSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disp, DWORD flags, HANDLE tmpl)
{
    WCHAR np[MAXPATHW];
    RFile *rf;
    FATENT e;
    FileObj *f;
    HANDLE h = INVALID_HANDLE_VALUE;
    int found, i, write = (access & GENERIC_WRITE) != 0;

    if (norm_path(name, np) < 0) { KSetLastError(pCurThread, ERROR_INVALID_NAME); return INVALID_HANDLE_VALUE; }
    for (i = 0; devnames[i]; i++) {
        int l = wlen(devnames[i]);
        if (wineq(np + (np[0] == '\\' && devnames[i][0] != '\\' ? 1 : 0), l, devnames[i]) || wineq(np, l, devnames[i])) {
            KSetLastError(pCurThread, ERROR_FILE_NOT_FOUND);
            return INVALID_HANDLE_VALUE;
        }
    }
    found = !resolve(np, &rf, &e);
    if (g_fsTrace) NKDbgPrintfW(L"fs: CreateFile(%s, acc=%x, disp=%d) found=%d\r\n", np, access, disp, found);
    if (found && ((rf && rf->dir) || (!rf && (e.attr & FAT_ATTR_DIR)))) {
        /* opening a directory: only allowed for attribute queries (CreateFileForMapping etc. never do this) */
        KSetLastError(pCurThread, ERROR_ACCESS_DENIED);
        return INVALID_HANDLE_VALUE;
    }
    switch (disp) {
    case CREATE_NEW: if (found) { KSetLastError(pCurThread, ERROR_FILE_EXISTS); return INVALID_HANDLE_VALUE; } break;
    case OPEN_EXISTING: case TRUNCATE_EXISTING: if (!found) { KSetLastError(pCurThread, ERROR_FILE_NOT_FOUND); return INVALID_HANDLE_VALUE; } break;
    }
    if (!found) {
        /* create in RAM: the parent has to exist */
        WCHAR parent[MAXPATHW];
        RFile *prf; FATENT pe;
        int n = wlen(np);
        memcpy(parent, np, (n + 1) * 2);
        while (n > 0 && parent[n - 1] != '\\') n--;
        if (n > 1) parent[n - 1] = 0; else parent[1] = 0;
        if (resolve(parent, &prf, &pe) || (prf ? !prf->dir : !(pe.attr & FAT_ATTR_DIR))) {
            KSetLastError(pCurThread, ERROR_PATH_NOT_FOUND);
            return INVALID_HANDLE_VALUE;
        }
        rf = rf_new(np);
        if (!rf) { KSetLastError(pCurThread, ERROR_NOT_ENOUGH_MEMORY); return INVALID_HANDLE_VALUE; }
        rf->attr = FILE_ATTRIBUTE_ARCHIVE;
    } else if (!rf && (write || disp == TRUNCATE_EXISTING || disp == CREATE_ALWAYS)) {
        /* copy-up: a ROM/SD file that is written to gets a RAM copy */
        rf = rf_new(np);
        if (!rf) { KSetLastError(pCurThread, ERROR_NOT_ENOUGH_MEMORY); return INVALID_HANDLE_VALUE; }
        rf->attr = fat_attr_to_win(e.attr) & ~FILE_ATTRIBUTE_READONLY;
        fat_ft(e.wdate, e.wtime, &rf->ft);
        if (disp != TRUNCATE_EXISTING && disp != CREATE_ALWAYS && e.size) {
            if (!rf_reserve(rf, e.size) || fat_read(&e, 0, rf->data, e.size) != (int)e.size) {
                rf->used = 0;
                KSetLastError(pCurThread, ERROR_NOT_ENOUGH_MEMORY);
                return INVALID_HANDLE_VALUE;
            }
            rf->size = e.size;
        }
    } else if (rf && (disp == TRUNCATE_EXISTING || disp == CREATE_ALWAYS)) {
        rf->size = 0;
    }
    f = file_alloc(&h);
    if (!f) { KSetLastError(pCurThread, ERROR_TOO_MANY_OPEN_FILES); return INVALID_HANDLE_VALUE; }
    f->rf = rf; f->access = access;
    if (!rf) f->e = e;
    memcpy(f->path, np, (wlen(np) + 1) * 2);
    if (disp == CREATE_ALWAYS || disp == OPEN_ALWAYS) KSetLastError(pCurThread, found ? ERROR_ALREADY_EXISTS : 0);
    else KSetLastError(pCurThread, 0);
    return h;
}

static BOOL fs_file_close(HANDLE h)
{
    FileObj *f = h2file(h);
    if (!f) { KSetLastError(pCurThread, ERROR_INVALID_HANDLE); return FALSE; }
    f->used = 0;
    FreeHandle(h);
    return TRUE;
}

static DWORD file_size(FileObj *f) { return f->rf ? f->rf->size : f->e.size; }

static BOOL file_read_at(FileObj *f, DWORD off, void *buf, DWORD n, DWORD *got)
{
    DWORD sz = file_size(f), r = 0;
    if (off < sz) {
        if (n > sz - off) n = sz - off;
        if (f->rf) { memcpy(buf, f->rf->data + off, n); r = n; }
        else {
            /* the FAT driver needs 32-byte aligned buffers for its direct sector reads, but handles anything else via its cache */
            int k = fat_read(&f->e, off, buf, n);
            r = k < 0 ? 0 : k;
        }
    }
    if (got) *got = r;
    return TRUE;
}

static BOOL fs_ReadFile(HANDLE h, LPVOID buf, DWORD n, LPDWORD got, LPOVERLAPPED ov)
{
    FileObj *f = h2file(h);
    DWORD r;
    if (!f) { KSetLastError(pCurThread, ERROR_INVALID_HANDLE); return FALSE; }
    file_read_at(f, f->pos, buf, n, &r);
    f->pos += r;
    if (got) *got = r;
    return TRUE;
}

static BOOL fs_ReadFileWithSeek(HANDLE h, LPVOID buf, DWORD n, LPDWORD got, LPOVERLAPPED ov, DWORD lo, DWORD hi)
{
    FileObj *f = h2file(h);
    DWORD r;
    if (!f) { KSetLastError(pCurThread, ERROR_INVALID_HANDLE); return FALSE; }
    file_read_at(f, lo, buf, n, &r);
    if (got) *got = r;
    return TRUE;
}

static BOOL file_write_at(FileObj *f, DWORD off, const void *buf, DWORD n, DWORD *wrote)
{
    RFile *rf = f->rf;
    if (!rf || !(f->access & GENERIC_WRITE)) { KSetLastError(pCurThread, ERROR_ACCESS_DENIED); return FALSE; }
    if (!rf_reserve(rf, off + n)) { KSetLastError(pCurThread, ERROR_DISK_FULL); return FALSE; }
    if (off > rf->size) memset(rf->data + rf->size, 0, off - rf->size);
    memcpy(rf->data + off, buf, n);
    if (off + n > rf->size) rf->size = off + n;
    if (wrote) *wrote = n;
    return TRUE;
}

static BOOL fs_WriteFile(HANDLE h, LPCVOID buf, DWORD n, LPDWORD wrote, LPOVERLAPPED ov)
{
    FileObj *f = h2file(h);
    if (!f) { KSetLastError(pCurThread, ERROR_INVALID_HANDLE); return FALSE; }
    if (!file_write_at(f, f->pos, buf, n, wrote)) return FALSE;
    f->pos += n;
    return TRUE;
}

static BOOL fs_WriteFileWithSeek(HANDLE h, LPCVOID buf, DWORD n, LPDWORD wrote, LPOVERLAPPED ov, DWORD lo, DWORD hi)
{
    FileObj *f = h2file(h);
    if (!f) { KSetLastError(pCurThread, ERROR_INVALID_HANDLE); return FALSE; }
    return file_write_at(f, lo, buf, n, wrote);
}

static DWORD fs_GetFileSize(HANDLE h, LPDWORD hi)
{
    FileObj *f = h2file(h);
    if (!f) { KSetLastError(pCurThread, ERROR_INVALID_HANDLE); return 0xFFFFFFFF; }
    if (hi) *hi = 0;
    return file_size(f);
}

static DWORD fs_SetFilePointer(HANDLE h, LONG dist, PLONG hi, DWORD method)
{
    FileObj *f = h2file(h);
    long long p;
    if (!f) { KSetLastError(pCurThread, ERROR_INVALID_HANDLE); return 0xFFFFFFFF; }
    switch (method) {
    case FILE_BEGIN: p = (unsigned)dist; if (dist < 0) p = dist; break;
    case FILE_CURRENT: p = (long long)f->pos + dist; break;
    default: p = (long long)file_size(f) + dist; break;
    }
    if (p < 0) { KSetLastError(pCurThread, ERROR_NEGATIVE_SEEK); return 0xFFFFFFFF; }
    f->pos = (DWORD)p;
    if (hi) *hi = 0;
    return f->pos;
}

static BOOL fs_GetFileInformationByHandle(HANDLE h, LPBY_HANDLE_FILE_INFORMATION fi)
{
    FileObj *f = h2file(h);
    if (!f) { KSetLastError(pCurThread, ERROR_INVALID_HANDLE); return FALSE; }
    memset(fi, 0, sizeof *fi);
    if (f->rf) { fi->dwFileAttributes = f->rf->attr; fi->ftLastWriteTime = fi->ftCreationTime = fi->ftLastAccessTime = f->rf->ft; }
    else {
        fi->dwFileAttributes = fat_attr_to_win(f->e.attr);
        fat_ft(f->e.cdate, f->e.ctime, &fi->ftCreationTime);
        fat_ft(f->e.wdate, f->e.wtime, &fi->ftLastWriteTime);
        fi->ftLastAccessTime = fi->ftLastWriteTime;
    }
    fi->nFileSizeLow = file_size(f);
    fi->nNumberOfLinks = 1;
    fi->nFileIndexLow = f->rf ? (DWORD)f->rf : f->e.first;
    return TRUE;
}

static BOOL fs_FlushFileBuffers(HANDLE h) { return h2file(h) != NULL; }

static BOOL fs_GetFileTime(HANDLE h, LPFILETIME c, LPFILETIME a, LPFILETIME w)
{
    FileObj *f = h2file(h);
    FILETIME t;
    if (!f) { KSetLastError(pCurThread, ERROR_INVALID_HANDLE); return FALSE; }
    if (f->rf) t = f->rf->ft; else fat_ft(f->e.wdate, f->e.wtime, &t);
    if (c) *c = t;
    if (a) *a = t;
    if (w) *w = t;
    return TRUE;
}

static BOOL fs_SetFileTime(HANDLE h, const FILETIME *c, const FILETIME *a, const FILETIME *w)
{
    FileObj *f = h2file(h);
    if (!f) { KSetLastError(pCurThread, ERROR_INVALID_HANDLE); return FALSE; }
    if (f->rf && w) f->rf->ft = *w;
    return TRUE;
}

static BOOL fs_SetEndOfFile(HANDLE h)
{
    FileObj *f = h2file(h);
    if (!f || !f->rf) { KSetLastError(pCurThread, f ? ERROR_ACCESS_DENIED : ERROR_INVALID_HANDLE); return FALSE; }
    if (!rf_reserve(f->rf, f->pos)) return FALSE;
    if (f->pos > f->rf->size) memset(f->rf->data + f->rf->size, 0, f->pos - f->rf->size);
    f->rf->size = f->pos;
    return TRUE;
}

static BOOL fs_DeviceIoControl(HANDLE h, DWORD code, LPVOID in, DWORD nin, LPVOID out, DWORD nout, LPDWORD ret, LPOVERLAPPED ov)
{
    KSetLastError(pCurThread, ERROR_NOT_SUPPORTED);
    return FALSE;
}

static BOOL fs_lock_nop(HANDLE h) { return h2file(h) != NULL; }

static DWORD fs_GetFileAttributesW(LPCWSTR name)
{
    WCHAR np[MAXPATHW];
    RFile *rf;
    FATENT e;
    if (norm_path(name, np) < 0) { KSetLastError(pCurThread, ERROR_INVALID_NAME); return 0xFFFFFFFF; }
    if (resolve(np, &rf, &e)) {
        KSetLastError(pCurThread, wlen(np) > 1 ? ERROR_FILE_NOT_FOUND : ERROR_PATH_NOT_FOUND);
        return 0xFFFFFFFF;
    }
    return rf ? rf->attr | (rf->dir ? FILE_ATTRIBUTE_DIRECTORY : 0) : fat_attr_to_win(e.attr);
}

static BOOL fs_SetFileAttributesW(LPCWSTR name, DWORD attr)
{
    WCHAR np[MAXPATHW];
    RFile *rf;
    FATENT e;
    if (norm_path(name, np) < 0 || resolve(np, &rf, &e)) { KSetLastError(pCurThread, ERROR_FILE_NOT_FOUND); return FALSE; }
    if (rf) rf->attr = attr & ~FILE_ATTRIBUTE_DIRECTORY;
    return TRUE;
}

static BOOL fs_CreateDirectoryW(LPCWSTR name, LPSECURITY_ATTRIBUTES sa)
{
    WCHAR np[MAXPATHW], parent[MAXPATHW];
    RFile *rf, *prf; FATENT e, pe;
    int n;
    if (norm_path(name, np) < 0) { KSetLastError(pCurThread, ERROR_INVALID_NAME); return FALSE; }
    if (!resolve(np, &rf, &e)) { KSetLastError(pCurThread, ERROR_ALREADY_EXISTS); return FALSE; }
    n = wlen(np);
    memcpy(parent, np, (n + 1) * 2);
    while (n > 0 && parent[n - 1] != '\\') n--;
    if (n > 1) parent[n - 1] = 0; else parent[1] = 0;
    if (resolve(parent, &prf, &pe)) { KSetLastError(pCurThread, ERROR_PATH_NOT_FOUND); return FALSE; }
    rf = rf_new(np);
    if (!rf) { KSetLastError(pCurThread, ERROR_NOT_ENOUGH_MEMORY); return FALSE; }
    rf->dir = 1; rf->attr = FILE_ATTRIBUTE_DIRECTORY;
    return TRUE;
}

static BOOL fs_RemoveDirectoryW(LPCWSTR name)
{
    WCHAR np[MAXPATHW];
    RFile *rf; FATENT e;
    if (norm_path(name, np) < 0 || resolve(np, &rf, &e)) { KSetLastError(pCurThread, ERROR_PATH_NOT_FOUND); return FALSE; }
    if (rf) { rf->used = 0; return TRUE; }
    rf = rf_new(np);                     /* tombstone over a ROM/SD directory */
    if (!rf) return FALSE;
    rf->deleted = 1;
    return TRUE;
}

static BOOL fs_DeleteFileW(LPCWSTR name)
{
    WCHAR np[MAXPATHW];
    RFile *rf; FATENT e;
    if (norm_path(name, np) < 0 || resolve(np, &rf, &e)) { KSetLastError(pCurThread, ERROR_FILE_NOT_FOUND); return FALSE; }
    if (rf && !rf->deleted) {
        RFile *sd_shadow;
        if (rf->data) VirtualFree(rf->data, 0, MEM_RELEASE);
        rf->data = NULL; rf->size = rf->cap = 0;
        /* a copy-up of an SD file needs a tombstone, a pure RAM file just disappears */
        { WCHAR sp[MAXPATHW + 8]; FATENT se; sd_path(np, sp); sd_shadow = fat_lookup(sp, &se) ? NULL : rf; }
        if (sd_shadow) rf->deleted = 1; else rf->used = 0;
        return TRUE;
    }
    rf = rf_new(np);
    if (!rf) return FALSE;
    rf->deleted = 1;
    return TRUE;
}

static BOOL fs_MoveFileW(LPCWSTR from, LPCWSTR to)
{
    WCHAR a[MAXPATHW], b[MAXPATHW];
    RFile *rf, *nf; FATENT e;
    if (norm_path(from, a) < 0 || norm_path(to, b) < 0) { KSetLastError(pCurThread, ERROR_INVALID_NAME); return FALSE; }
    if (resolve(a, &rf, &e)) { KSetLastError(pCurThread, ERROR_FILE_NOT_FOUND); return FALSE; }
    if (!resolve(b, &nf, &e)) { KSetLastError(pCurThread, ERROR_ALREADY_EXISTS); return FALSE; }
    if (rf) { memcpy(rf->path, b, (wlen(b) + 1) * 2); return TRUE; }
    KSetLastError(pCurThread, ERROR_ACCESS_DENIED);       /* moving ROM files is not supported */
    return FALSE;
}

/* AFS method 12, PrestoChangoFileName (now DeleteAndRenameFileW): rename over an existing target. */
static BOOL fs_DeleteAndRenameFileW(LPCWSTR from, LPCWSTR to)
{
    WCHAR b[MAXPATHW]; RFile *nf; FATENT e;
    if (norm_path(to, b) >= 0 && !resolve(b, &nf, &e)) fs_DeleteFileW(to);
    return fs_MoveFileW(from, to);
}

static BOOL fs_CopyFileW(LPCWSTR from, LPCWSTR to, BOOL failIfExists)
{
    HANDLE hs, hd;
    FileObj *fs_, *fd;
    BYTE *buf;
    DWORD n;
    hs = fs_CreateFileW(from, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hs == INVALID_HANDLE_VALUE) return FALSE;
    hd = fs_CreateFileW(to, GENERIC_WRITE, 0, NULL, failIfExists ? CREATE_NEW : CREATE_ALWAYS, 0, NULL);
    if (hd == INVALID_HANDLE_VALUE) { fs_file_close(hs); return FALSE; }
    fs_ = h2file(hs); fd = h2file(hd);
    n = file_size(fs_);
    if (n && (!rf_reserve(fd->rf, n) || !(buf = fd->rf->data))) { fs_file_close(hs); fs_file_close(hd); return FALSE; }
    if (n) file_read_at(fs_, 0, fd->rf->data, n, &n);
    fd->rf->size = n;
    fs_file_close(hs); fs_file_close(hd);
    return TRUE;
}

static int wild_match(const WCHAR *p, const WCHAR *s)
{
    for (; *p; p++) {
        if (*p == '*') {
            while (p[1] == '*') p++;
            if (!p[1]) return 1;
            for (; *s; s++) if (wild_match(p + 1, s)) return 1;
            return wild_match(p + 1, s);
        }
        if (!*s) return 0;
        if (*p != '?' && lcw(*p) != lcw(*s)) return 0;
        s++;
    }
    return !*s;
}

static void fill_find(WIN32_FIND_DATAW *fd, const WCHAR *name, DWORD attr, DWORD size, const FILETIME *ct, const FILETIME *wt)
{
    memset(fd, 0, sizeof *fd);
    fd->dwFileAttributes = attr;
    fd->ftCreationTime = *ct; fd->ftLastAccessTime = *wt; fd->ftLastWriteTime = *wt;
    fd->nFileSizeLow = size;
    {
        int n = wlen(name);
        if (n > MAX_PATH - 1) n = MAX_PATH - 1;
        memcpy(fd->cFileName, name, n * 2);
        fd->cFileName[n] = 0;
    }
}

static int find_next(FindObj *fo, WIN32_FIND_DATAW *fd)
{
    WCHAR nm[260];
    FATENT e;
    if (fo->have_d) {
        while (!fat_readdir(&fo->d, fo->idx++, nm, &e)) {
            WCHAR full[MAXPATHW];
            int dl = wlen(fo->dir);
            RFile *sh;
            if (!wild_match(fo->pat, nm)) continue;
            memcpy(full, fo->dir, dl * 2);
            if (dl > 1) full[dl++] = '\\';
            memcpy(full + dl, nm, (wlen(nm) + 1) * 2);
            sh = rf_find(full);
            if (sh) continue;           /* shadowed (listed from RAM below) or deleted */
            {
                FILETIME ct, wt;
                fat_ft(e.cdate, e.ctime, &ct); fat_ft(e.wdate, e.wtime, &wt);
                fill_find(fd, nm, fat_attr_to_win(e.attr), e.size, &ct, &wt);
            }
            return 0;
        }
        fo->have_d = 0;
    }
    for (; fo->ridx < MAX_RFILES; fo->ridx++) {
        RFile *r = &rfiles[fo->ridx];
        int dl = wlen(fo->dir), l;
        const WCHAR *leaf;
        if (!r->used || r->deleted) continue;
        l = wlen(r->path);
        if (l <= dl + (dl > 1)) continue;
        if (!wineq(fo->dir, dl, r->path)) continue;
        leaf = r->path + dl + (dl > 1);
        if (dl > 1 && r->path[dl] != '\\') continue;
        { const WCHAR *q; for (q = leaf; *q; q++) if (*q == '\\') break; if (*q) continue; }
        if (!wild_match(fo->pat, leaf)) continue;
        fill_find(fd, leaf, r->attr | (r->dir ? FILE_ATTRIBUTE_DIRECTORY : 0), r->size, &r->ft, &r->ft);
        fo->ridx++;
        return 0;
    }
    return 1;
}

static HANDLE fs_FindFirstFileW(LPCWSTR pattern, LPWIN32_FIND_DATAW fd)
{
    WCHAR np[MAXPATHW], sp[MAXPATHW + 8];
    FindObj *fo = NULL;
    HANDLE h;
    int i, n;
    RFile *rf;

    if (norm_path(pattern, np) < 0) { KSetLastError(pCurThread, ERROR_INVALID_NAME); return INVALID_HANDLE_VALUE; }
    for (i = 0; i < MAX_FINDS; i++) if (!finds[i].used) { fo = &finds[i]; break; }
    if (!fo) { KSetLastError(pCurThread, ERROR_TOO_MANY_OPEN_FILES); return INVALID_HANDLE_VALUE; }
    memset(fo, 0, sizeof *fo);
    n = wlen(np);
    while (n > 0 && np[n - 1] != '\\') n--;
    memcpy(fo->pat, np + n, (wlen(np + n) + 1) * 2);
    memcpy(fo->dir, np, n * 2);
    if (n > 1) fo->dir[n - 1] = 0; else { fo->dir[1] = 0; }
    fo->dir[n ? (n > 1 ? n - 1 : 1) : 0] = 0;
    if (!n) { fo->dir[0] = '\\'; fo->dir[1] = 0; }
    /* a pattern without wildcards naming a directory lists the directory itself in CE; model that as a single-entry match */
    if (!resolve(fo->dir, &rf, &fo->d) && !rf && (fo->d.attr & FAT_ATTR_DIR)) fo->have_d = 1;
    else if (!rf || !rf->dir) {
        if (rf_find(fo->dir) == NULL && !fo->have_d) { KSetLastError(pCurThread, ERROR_PATH_NOT_FOUND); return INVALID_HANDLE_VALUE; }
    }
    (void)sp;
    if (find_next(fo, fd)) { KSetLastError(pCurThread, ERROR_NO_MORE_FILES); return INVALID_HANDLE_VALUE; }
    fo->used = 1;
    h = AllocHandle(&cinfFind, fo, pCurProc);
    if (!h) { fo->used = 0; return INVALID_HANDLE_VALUE; }
    return h;
}

static HANDLE fs_FindFirstFileExW(LPCWSTR pattern, FINDEX_INFO_LEVELS lvl, LPVOID fd, FINDEX_SEARCH_OPS op, LPVOID filter, DWORD flags)
{
    return fs_FindFirstFileW(pattern, (LPWIN32_FIND_DATAW)fd);
}

static FindObj *h2find(HANDLE h)
{
    FindObj *fo = (FindObj *)GetObjectPtrByType(h, HT_FIND);
    return fo && fo->used ? fo : NULL;
}

static BOOL fs_FindNextFileW(HANDLE h, LPWIN32_FIND_DATAW fd)
{
    FindObj *fo = h2find(h);
    if (!fo) { KSetLastError(pCurThread, ERROR_INVALID_HANDLE); return FALSE; }
    if (find_next(fo, fd)) { KSetLastError(pCurThread, ERROR_NO_MORE_FILES); return FALSE; }
    return TRUE;
}

static BOOL fs_FindClose(HANDLE h)
{
    FindObj *fo = h2find(h);
    if (!fo) { KSetLastError(pCurThread, ERROR_INVALID_HANDLE); return FALSE; }
    fo->used = 0;
    FreeHandle(h);
    return TRUE;
}

/* ---- message queues (methods 79..84, OAK/INC/mwinbase.h).  The shell's notification queues: a ring of fixed-size messages per queue.
 * The queue handle is a kernel event that is set while the ring holds messages, so WaitForSingleObject on it works. */
typedef struct FsMsgOpts { DWORD dwSize, dwFlags, dwMaxMessages, cbMaxMessage; BOOL bReadAccess; } FsMsgOpts;
typedef struct FsMsgInfo { DWORD dwSize, dwFlags, dwMaxMessages, cbMaxMessage, dwCurrentMessages, dwMaxQueueMessages; WORD wNumReaders, wNumWriters; } FsMsgInfo;

#define MAX_MSGQS 8
typedef struct MsgQ {
    HANDLE hev;
    DWORD used, flags, maxmsg, msgsz, head, count, hi;
    BYTE *buf;
    DWORD *lens;
} MsgQ;
static MsgQ msgqs[MAX_MSGQS];

static MsgQ *h2msgq(HANDLE h)
{
    int i;

    for (i = 0; i < MAX_MSGQS; i++) if (msgqs[i].used && msgqs[i].hev == h) return &msgqs[i];
    return NULL;
}

static HANDLE fs_CreateMsgQueue(LPCWSTR name, FsMsgOpts *o)
{
    MsgQ *q = NULL;
    int i;

    if (!o) { KSetLastError(pCurThread, ERROR_INVALID_PARAMETER); return NULL; }
    for (i = 0; i < MAX_MSGQS; i++) if (!msgqs[i].used) { q = &msgqs[i]; break; }
    if (!q) { KSetLastError(pCurThread, ERROR_TOO_MANY_OPEN_FILES); return NULL; }
    q->maxmsg = o->dwMaxMessages ? o->dwMaxMessages : 1;
    q->msgsz = o->cbMaxMessage;
    q->buf = fs_alloc(q->maxmsg * q->msgsz);
    q->lens = fs_alloc(q->maxmsg * sizeof(DWORD));
    if (!q->buf || !q->lens) return NULL;
    q->hev = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!q->hev) return NULL;
    q->flags = o->dwFlags;
    q->head = q->count = q->hi = 0;
    q->used = 1;
    return q->hev;
}

static HANDLE fs_OpenMsgQueue(HANDLE hSrc, HANDLE hMsgQ, FsMsgOpts *o)
{
    if (!h2msgq(hMsgQ)) { KSetLastError(pCurThread, ERROR_INVALID_HANDLE); return NULL; }
    return hMsgQ;
}

static BOOL fs_ReadMsgQueue(HANDLE h, LPVOID buf, DWORD cbBuf, LPDWORD pRead, DWORD timeout, DWORD *pFlags)
{
    MsgQ *q = h2msgq(h);
    DWORD len;

    if (!q) { KSetLastError(pCurThread, ERROR_INVALID_HANDLE); return FALSE; }
    while (q->count == 0)
        if (WaitForMultipleObjects(1, &q->hev, FALSE, timeout) != WAIT_OBJECT_0) { KSetLastError(pCurThread, ERROR_TIMEOUT); return FALSE; }
    len = q->lens[q->head];
    if (len > cbBuf) { KSetLastError(pCurThread, ERROR_INSUFFICIENT_BUFFER); return FALSE; }
    memcpy(buf, q->buf + q->head * q->msgsz, len);
    q->head = (q->head + 1) % q->maxmsg;
    q->count--;
    if (q->count) SetEvent(q->hev);     /* another reader may be waiting for the next message */
    if (pRead) *pRead = len;
    if (pFlags) *pFlags = 0;
    return TRUE;
}

static BOOL fs_WriteMsgQueue(HANDLE h, LPVOID buf, DWORD cbData, DWORD timeout, DWORD flags)
{
    MsgQ *q = h2msgq(h);
    DWORD tail;

    if (!q) { KSetLastError(pCurThread, ERROR_INVALID_HANDLE); return FALSE; }
    if (cbData > q->msgsz) { KSetLastError(pCurThread, ERROR_INSUFFICIENT_BUFFER); return FALSE; }
    if (q->count == q->maxmsg) { KSetLastError(pCurThread, ERROR_TIMEOUT); return FALSE; }
    tail = (q->head + q->count) % q->maxmsg;
    memcpy(q->buf + tail * q->msgsz, buf, cbData);
    q->lens[tail] = cbData;
    q->count++;
    if (q->count > q->hi) q->hi = q->count;
    SetEvent(q->hev);
    return TRUE;
}

static BOOL fs_GetMsgQueueInfo(HANDLE h, FsMsgInfo *info)
{
    MsgQ *q = h2msgq(h);

    if (!q) { KSetLastError(pCurThread, ERROR_INVALID_HANDLE); return FALSE; }
    info->dwFlags = q->flags;
    info->dwMaxMessages = q->maxmsg;
    info->cbMaxMessage = q->msgsz;
    info->dwCurrentMessages = q->count;
    info->dwMaxQueueMessages = q->hi;
    info->wNumReaders = 0;
    info->wNumWriters = 0;
    return TRUE;
}

static BOOL fs_CloseMsgQueue(HANDLE h)
{
    MsgQ *q = h2msgq(h);

    if (!q) { KSetLastError(pCurThread, ERROR_INVALID_HANDLE); return FALSE; }
    q->used = 0;
    SC_CloseHandle(h);
    return TRUE;
}

/* Device notifications (methods 92/93): no device ever posts to these queues yet, so the request succeeds and nothing is sent. */
static HANDLE fs_RequestDeviceNotifications(const GUID *devclass, HANDLE hMsgQ, BOOL fAll)
{
    if (!h2msgq(hMsgQ)) { KSetLastError(pCurThread, ERROR_INVALID_HANDLE); return NULL; }
    return hMsgQ;
}

static BOOL fs_StopDeviceNotifications(HANDLE h)
{
    return TRUE;
}

/* Registry change notifications (methods 104..106, CeFindFirstRegChange / CeFindNextRegChange / CeFindCloseRegChange): the handle is an
 * event that never fires, since nothing changes the registry behind the shell's back. */
static HANDLE fs_CeFindFirstRegChange(HKEY hk, BOOL subtree, DWORD filter)
{
    return CreateEventW(NULL, FALSE, FALSE, NULL);
}
static BOOL fs_CeFindNextRegChange(HANDLE h) { return TRUE; }
static BOOL fs_CeFindCloseRegChange(HANDLE h) { SC_CloseHandle(h); return TRUE; }

static DWORD fs_GetTempPathW(DWORD n, LPWSTR buf)
{
    static const WCHAR t[] = L"\\Temp\\";
    if (n < 7) return 7;
    memcpy(buf, t, sizeof t);
    return 6;
}

static BOOL fs_GetDiskFreeSpaceExW(LPCWSTR dir, PULARGE_INTEGER avail, PULARGE_INTEGER total, PULARGE_INTEGER freeb)
{
    if (avail) { avail->LowPart = 4u << 20; avail->HighPart = 0; }
    if (total) { total->LowPart = 8u << 20; total->HighPart = 0; }
    if (freeb) { freeb->LowPart = 4u << 20; freeb->HighPart = 0; }
    return TRUE;
}

static LONG fs_SignalStarted(DWORD d) { return 0; }
static BOOL fs_true(void) { return TRUE; }
static BOOL fs_false(void) { return FALSE; }

/* ---- method tables -------------------------------------------------------------------------------------------------------------- */
static DWORD fs_unimpl(int n, DWORD a, DWORD b, DWORD c, DWORD d)
{
    NKDbgPrintfW(L"fs: unimplemented API set 20 method %d (%x,%x,%x,%x)\r\n", n, a, b, c, d);
    KSetLastError(pCurThread, ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}
#include "fs20_stubs.h"

#define S(n) ((PFNVOID)fs_stub_##n)
#define STUBS_0_15 S(0),S(1),S(2),S(3),S(4),S(5),S(6),S(7),S(8),S(9),S(10),S(11),S(12),S(13),S(14),S(15)
static PFNVOID fsMethods[128] = {
    STUBS_0_15,
    S(16),S(17),S(18),S(19),S(20),S(21),S(22),S(23),S(24),S(25),S(26),S(27),S(28),S(29),S(30),S(31),
    S(32),S(33),S(34),S(35),S(36),S(37),S(38),S(39),S(40),S(41),S(42),S(43),S(44),S(45),S(46),S(47),
    S(48),S(49),S(50),S(51),S(52),S(53),S(54),S(55),S(56),S(57),S(58),S(59),S(60),S(61),S(62),S(63),
    S(64),S(65),S(66),S(67),S(68),S(69),S(70),S(71),S(72),S(73),S(74),S(75),S(76),S(77),S(78),S(79),
    S(80),S(81),S(82),S(83),S(84),S(85),S(86),S(87),S(88),S(89),S(90),S(91),S(92),S(93),S(94),S(95),
    S(96),S(97),S(98),S(99),S(100),S(101),S(102),S(103),S(104),S(105),S(106),S(107),S(108),S(109),S(110),S(111),
    S(112),S(113),S(114),S(115),S(116),S(117),S(118),S(119),S(120),S(121),S(122),S(123),S(124),S(125),S(126),S(127),
};

static PFNVOID fileMethods[16] = {
    (PFNVOID)fs_file_close, 0, (PFNVOID)fs_ReadFile, (PFNVOID)fs_WriteFile, (PFNVOID)fs_GetFileSize, (PFNVOID)fs_SetFilePointer,
    (PFNVOID)fs_GetFileInformationByHandle, (PFNVOID)fs_FlushFileBuffers, (PFNVOID)fs_GetFileTime, (PFNVOID)fs_SetFileTime,
    (PFNVOID)fs_SetEndOfFile, (PFNVOID)fs_DeviceIoControl, (PFNVOID)fs_ReadFileWithSeek, (PFNVOID)fs_WriteFileWithSeek,
    (PFNVOID)fs_lock_nop, (PFNVOID)fs_lock_nop,
};
static PFNVOID findMethods[3] = { (PFNVOID)fs_FindClose, (PFNVOID)fs_FindClose, (PFNVOID)fs_FindNextFileW };

static const CINFO cinfFs = { "FSYS", DISPATCH_KERNEL_PSL, 0, 128, fsMethods };
static const CINFO cinfFile = { "FILE", DISPATCH_KERNEL_PSL, HT_FILE, 16, fileMethods };
static const CINFO cinfFind = { "FIND", DISPATCH_KERNEL_PSL, HT_FIND, 3, findMethods };

extern const CINFO *SystemAPISets[];

/* Table coredll reaches through GetRomFileInfo(4): in kernel mode (all-kmode image) a few hot APIs skip the PSL trap and call
 * through these slots (layout from NK/KERNEL/kmodeentries.cpp).  Slots whose service is not up yet log and fail. */
static PFNVOID kme[62];

static DWORD kme_unimpl(DWORD a, DWORD b, DWORD c, DWORD d)
{
    NKDbgPrintfW(L"kmode: unimplemented entry called (%x,%x,%x,%x) ra=%x tab=%x s27=%x\r\n", a, b, c, d, __builtin_return_address(0), (DWORD)kme, (DWORD)kme[27]);
    return 0;
}

/* kmode slots that forward to the API sets gwes registers: SH_GDI (16) through GweApiSet2_t, SH_WMGR (17) through
 * GweApiSet1_t. Member indexes come from INC/gweapiset1.hpp; slot -> SC_ wrapper from NK/KERNEL/kmodeentries.cpp.
 * Eight argument words pass through so stack arguments reach the gwes method unchanged. */
static DWORD gwe_call(int set, int m, DWORD *a)
{
    const CINFO *pci = SystemAPISets[set];
    if (!pci) return 0;
    return ((DWORD (*)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD))((PFNVOID *)pci->ppfnMethods)[m])(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
}
static DWORD kfwd_6(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(16, 53, a);   /* SC_SelectObject */
}
static DWORD kfwd_7(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(16, 36, a);   /* SC_PatBlt */
}
static DWORD kfwd_8(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(16, 38, a);   /* SC_GetTextExtentExPointW */
}
static DWORD kfwd_9(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(16, 62, a);   /* SC_GetSysColorBrush */
}
static DWORD kfwd_10(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(16, 54, a);   /* SC_SetBkColor */
}
static DWORD kfwd_11(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 49, a);   /* SC_GetParent */
}
static DWORD kfwd_12(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 17, a);   /* SC_InvalidateRect */
}
static DWORD kfwd_24(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 32, a);   /* SC_ClientToScreen */
}
static DWORD kfwd_25(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 42, a);   /* SC_DefWindowProcW */
}
static DWORD kfwd_26(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 124, a);   /* SC_GetClipCursor */
}
static DWORD kfwd_27(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 40, a);   /* SC_GetDC */
}
static DWORD kfwd_28(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 52, a);   /* SC_GetFocus */
}
static DWORD kfwd_29(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 8, a);   /* SC_GetMessageW */
}
static DWORD kfwd_30(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 18, a);   /* SC_GetWindow */
}
static DWORD kfwd_31(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 98, a);   /* SC_PeekMessageW */
}
static DWORD kfwd_32(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 41, a);   /* SC_ReleaseDC */
}
static DWORD kfwd_33(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 7, a);   /* SC_SendMessageW */
}
static DWORD kfwd_34(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 94, a);   /* SC_SetScrollInfo */
}
static DWORD kfwd_35(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 36, a);   /* SC_SetWindowLongW */
}
static DWORD kfwd_36(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 14, a);   /* SC_SetWindowPos */
}
static DWORD kfwd_37(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(16, 11, a);   /* SC_CreateSolidBrush */
}
static DWORD kfwd_38(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 77, a);   /* SC_DeleteMenu */
}
static DWORD kfwd_39(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(16, 13, a);   /* SC_DeleteObject */
}
static DWORD kfwd_40(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(16, 16, a);   /* SC_DrawTextW */
}
static DWORD kfwd_41(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(16, 21, a);   /* SC_ExtTextOutW */
}
static DWORD kfwd_42(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(16, 22, a);   /* SC_FillRect */
}
static DWORD kfwd_43(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 81, a);   /* SC_GetAsyncKeyState */
}
static DWORD kfwd_44(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 86, a);   /* SC_GetDlgCtrlID */
}
static DWORD kfwd_45(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(16, 35, a);   /* SC_GetStockObject */
}
static DWORD kfwd_46(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 19, a);   /* SC_GetSystemMetrics */
}
static DWORD kfwd_47(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 2, a);   /* SC_RegisterClassWStub */
}
static DWORD kfwd_48(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 171, a);   /* SC_RegisterClipboardFormatW */
}
static DWORD kfwd_49(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(16, 55, a);   /* SC_SetBkMode */
}
static DWORD kfwd_50(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(16, 58, a);   /* SC_SetTextColor */
}
static DWORD kfwd_51(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(16, 94, a);   /* SC_TransparentImage */
}
static DWORD kfwd_52(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 112, a);   /* SC_IsDialogMessageW */
}
static DWORD kfwd_53(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 5, a);   /* SC_PostMessageW */
}
static DWORD kfwd_54(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 72, a);   /* SC_IsWindowVisible */
}
static DWORD kfwd_55(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 87, a);   /* SC_GetKeyState */
}
static DWORD kfwd_56(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 38, a);   /* SC_BeginPaint */
}
static DWORD kfwd_57(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7)
{
    DWORD a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    return gwe_call(17, 39, a);   /* SC_EndPaint */
}


void *KmodeEntries(void)
{
    static int init;
    if (!init) {
        int i;
        for (i = 0; i < 62; i++) kme[i] = (PFNVOID)kme_unimpl;
        kme[0] = (PFNVOID)SC_EventModify;
        kme[1] = (PFNVOID)SC_ReleaseMutex;
        kme[2] = (PFNVOID)SC_CreateAPIHandle;
        kme[3] = (PFNVOID)SC_MapViewOfFile;
        kme[4] = (PFNVOID)SC_ThreadGetPrio;
        kme[5] = (PFNVOID)SC_ThreadSetPrio;
        kme[13] = (PFNVOID)fs_RegOpenKeyExW;
        kme[14] = (PFNVOID)fs_RegQueryValueExW;
        kme[15] = (PFNVOID)fs_CreateFileW;
        kme[16] = (PFNVOID)fs_ReadFile;
        kme[20] = (PFNVOID)fs_RegCreateKeyExW;
        kme[21] = (PFNVOID)SC_DeviceIoControl;
        kme[22] = (PFNVOID)SC_CloseHandle;
        kme[23] = (PFNVOID)fs_RegCloseKey;
        kme[6] = (PFNVOID)kfwd_6;
        kme[7] = (PFNVOID)kfwd_7;
        kme[8] = (PFNVOID)kfwd_8;
        kme[9] = (PFNVOID)kfwd_9;
        kme[10] = (PFNVOID)kfwd_10;
        kme[11] = (PFNVOID)kfwd_11;
        kme[12] = (PFNVOID)kfwd_12;
        kme[24] = (PFNVOID)kfwd_24;
        kme[25] = (PFNVOID)kfwd_25;
        kme[26] = (PFNVOID)kfwd_26;
        kme[27] = (PFNVOID)kfwd_27;
        kme[28] = (PFNVOID)kfwd_28;
        kme[29] = (PFNVOID)kfwd_29;
        kme[30] = (PFNVOID)kfwd_30;
        kme[31] = (PFNVOID)kfwd_31;
        kme[32] = (PFNVOID)kfwd_32;
        kme[33] = (PFNVOID)kfwd_33;
        kme[34] = (PFNVOID)kfwd_34;
        kme[35] = (PFNVOID)kfwd_35;
        kme[36] = (PFNVOID)kfwd_36;
        kme[37] = (PFNVOID)kfwd_37;
        kme[38] = (PFNVOID)kfwd_38;
        kme[39] = (PFNVOID)kfwd_39;
        kme[40] = (PFNVOID)kfwd_40;
        kme[41] = (PFNVOID)kfwd_41;
        kme[42] = (PFNVOID)kfwd_42;
        kme[43] = (PFNVOID)kfwd_43;
        kme[44] = (PFNVOID)kfwd_44;
        kme[45] = (PFNVOID)kfwd_45;
        kme[46] = (PFNVOID)kfwd_46;
        kme[47] = (PFNVOID)kfwd_47;
        kme[48] = (PFNVOID)kfwd_48;
        kme[49] = (PFNVOID)kfwd_49;
        kme[50] = (PFNVOID)kfwd_50;
        kme[51] = (PFNVOID)kfwd_51;
        kme[52] = (PFNVOID)kfwd_52;
        kme[53] = (PFNVOID)kfwd_53;
        kme[54] = (PFNVOID)kfwd_54;
        kme[55] = (PFNVOID)kfwd_55;
        kme[56] = (PFNVOID)kfwd_56;
        kme[57] = (PFNVOID)kfwd_57;
        kme[60] = (PFNVOID)fs_ReadFileWithSeek;
        kme[61] = (PFNVOID)fs_WriteFileWithSeek;
        init = 1;
    }
    return kme;
}

/* Window-manager entries the kernel-mode table forwards to gwes (GetDC, ReleaseDC): set once gwes is loaded. */
void Fs20SetKmode(int idx, void *fn)
{
    ((PFNVOID *)KmodeEntries())[idx] = (PFNVOID)fn;
}

void Fs20Init(void)
{
    fsMethods[2] = (PFNVOID)fs_CreateDirectoryW;
    fsMethods[3] = (PFNVOID)fs_RemoveDirectoryW;
    fsMethods[4] = (PFNVOID)fs_MoveFileW;
    fsMethods[5] = (PFNVOID)fs_CopyFileW;
    fsMethods[6] = (PFNVOID)fs_DeleteFileW;
    fsMethods[12] = (PFNVOID)fs_DeleteAndRenameFileW;
    fsMethods[7] = (PFNVOID)fs_GetFileAttributesW;
    fsMethods[8] = (PFNVOID)fs_FindFirstFileW;
    fsMethods[9] = (PFNVOID)fs_CreateFileW;
    fsMethods[79] = (PFNVOID)fs_CreateMsgQueue;
    fsMethods[80] = (PFNVOID)fs_OpenMsgQueue;
    fsMethods[81] = (PFNVOID)fs_ReadMsgQueue;
    fsMethods[82] = (PFNVOID)fs_WriteMsgQueue;
    fsMethods[83] = (PFNVOID)fs_GetMsgQueueInfo;
    fsMethods[84] = (PFNVOID)fs_CloseMsgQueue;
    fsMethods[104] = (PFNVOID)fs_CeFindFirstRegChange;
    fsMethods[105] = (PFNVOID)fs_CeFindNextRegChange;
    fsMethods[106] = (PFNVOID)fs_CeFindCloseRegChange;
    fsMethods[92] = (PFNVOID)fs_RequestDeviceNotifications;
    fsMethods[93] = (PFNVOID)fs_StopDeviceNotifications;
    fsMethods[17] = (PFNVOID)fs_RegCloseKey;
    fsMethods[18] = (PFNVOID)fs_RegCreateKeyExW;
    fsMethods[19] = (PFNVOID)fs_RegDeleteKeyW;
    fsMethods[20] = (PFNVOID)fs_RegDeleteValueW;
    fsMethods[21] = (PFNVOID)fs_RegEnumValueW;
    fsMethods[22] = (PFNVOID)fs_RegEnumKeyExW;
    fsMethods[23] = (PFNVOID)fs_RegOpenKeyExW;
    fsMethods[24] = (PFNVOID)fs_RegQueryInfoKeyW;
    fsMethods[25] = (PFNVOID)fs_RegQueryValueExW;
    fsMethods[26] = (PFNVOID)fs_RegSetValueExW;
    fsMethods[27] = (PFNVOID)fs_GetTempPathW;
    fsMethods[31] = (PFNVOID)fs_SetFileAttributesW;
    fsMethods[49] = (PFNVOID)fs_RegFlushKey;
    fsMethods[54] = (PFNVOID)fs_GetDiskFreeSpaceExW;
    fsMethods[63] = (PFNVOID)fs_FindFirstFileExW;
    fsMethods[66] = (PFNVOID)fs_SignalStarted;
    reg_load();
    SystemAPISets[SH_FILESYS_APIS] = &cinfFs;
    SystemAPISets[HT_FILE] = &cinfFile;
    SystemAPISets[HT_FIND] = &cinfFind;
    KInfoTable[KINX_API_MASK] |= 1u << SH_FILESYS_APIS;
    NKDbgPrintfW(L"fs: API set %d registered\r\n", SH_FILESYS_APIS);
}

/* ---- file mappings -------------------------------------------------------------------------------------------------------------- */
/* Flat memory has no demand paging: a mapping is a block of VirtualAlloc pool memory.  Anonymous mappings (hFile == -1) are zeroed and
 * can be shared by name (coredll's locale cache relies on that); file mappings read the file in when created.  Writes to a
 * file-backed view stay in RAM. */
typedef struct MapObj {
    int used, refs;
    BYTE *base; DWORD size;
    WCHAR name[64];
} MapObj;
#define MAX_MAPS 48
static MapObj maps[MAX_MAPS];

HANDLE SC_CreateFileMapping(HANDLE hFile, LPSECURITY_ATTRIBUTES sa, DWORD prot, DWORD hi, DWORD lo, LPCTSTR name)
{
    MapObj *m = NULL;
    HANDLE h;
    DWORD size = lo;
    int i;

    if (name && name[0])
        for (i = 0; i < MAX_MAPS; i++)
            if (maps[i].used && wieq(maps[i].name, name)) {
                h = AllocHandle(&cinfMap, &maps[i], pCurProc);
                if (!h) return NULL;
                maps[i].refs++;
                KSetLastError(pCurThread, ERROR_ALREADY_EXISTS);
                return h;
            }
    for (i = 0; i < MAX_MAPS; i++) if (!maps[i].used) { m = &maps[i]; break; }
    if (!m) { KSetLastError(pCurThread, ERROR_NOT_ENOUGH_MEMORY); return NULL; }
    memset(m, 0, sizeof *m);
    if (hFile != INVALID_HANDLE_VALUE) {
        FileObj *f = h2file(hFile);
        if (!f) { KSetLastError(pCurThread, ERROR_INVALID_HANDLE); return NULL; }
        if (!size) size = file_size(f);
    }
    if (!size) { KSetLastError(pCurThread, ERROR_INVALID_PARAMETER); return NULL; }
    m->size = size;
    m->base = VirtualAlloc(NULL, (size + 0xfff) & ~0xfffu, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!m->base) { KSetLastError(pCurThread, ERROR_NOT_ENOUGH_MEMORY); return NULL; }
    memset(m->base, 0, size);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD got;
        file_read_at(h2file(hFile), 0, m->base, size, &got);
    }
    if (name) { for (i = 0; i < 63 && name[i]; i++) m->name[i] = name[i]; }
    h = AllocHandle(&cinfMap, m, pCurProc);
    if (!h) { VirtualFree(m->base, 0, MEM_RELEASE); return NULL; }
    m->used = 1; m->refs = 1;
    KSetLastError(pCurThread, 0);
    return h;
}

static BOOL fs_map_close(HANDLE h)
{
    MapObj *m = (MapObj *)GetObjectPtrByType(h, HT_FSMAP);
    if (!m) { KSetLastError(pCurThread, ERROR_INVALID_HANDLE); return FALSE; }
    FreeHandle(h);
    if (--m->refs <= 0) { m->used = 0; }       /* the memory stays: views may outlive the handle, and the pool is never fragmented by it */
    return TRUE;
}

LPVOID SC_MapViewOfFile(HANDLE hMap, DWORD access, DWORD offLo, DWORD offHi, DWORD cb)
{
    MapObj *m = (MapObj *)GetObjectPtrByType(hMap, HT_FSMAP);
    if (!m) { KSetLastError(pCurThread, ERROR_INVALID_HANDLE); return NULL; }
    if (offLo >= m->size) { KSetLastError(pCurThread, ERROR_INVALID_PARAMETER); return NULL; }
    return m->base + offLo;
}

BOOL SC_UnmapViewOfFile(LPVOID p) { return TRUE; }

HANDLE SC_CreateFileForMapping(LPCTSTR n, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD d, DWORD f, HANDLE t)
{
    return fs_CreateFileW(n, a, s, sa, d, f, t);
}

static PFNVOID mapMethods[1] = { (PFNVOID)fs_map_close };
const CINFO cinfMap = { "FMAP", DISPATCH_KERNEL_PSL, HT_FSMAP, 1, mapMethods };
