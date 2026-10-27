/* Flat-memory PE loader: the WM6 coredll (made relocatable by tools/wm6/mkpe.py) and CE applications built with relocations. One copy
 * per load, placed in the VirtualAlloc pool (VA == PA), relocated by (base - ImageBase), imports bound against already-loaded modules. */
#include <windows.h>
#include <kernel.h>
#ifndef IMAGE_FIRST_SECTION
#define IMAGE_FIRST_SECTION(nt) ((PIMAGE_SECTION_HEADER)((DWORD)(nt) + 4 + sizeof(IMAGE_FILE_HEADER) + (nt)->FileHeader.SizeOfOptionalHeader))
#endif
#include <string.h>
#include "flatld.h"
#include "fat.h"

extern void NKDbgPrintfW(const wchar_t *, ...);

static FMOD g_mods[FL_MAX_MODS];

/* VirtualAlloc(NULL) is not usable from the kernel process yet; reserve+commit an explicit 64K-aligned address instead. Blocks that
 * the kernel already holds (DLL data, thread stacks) make the explicit reserve fail with ERROR_INVALID_PARAMETER, so step down past them.
 * First fit from the top of the pool, so a block released later (a per-load scratch buffer) is reused. */
static BYTE *flat_alloc(DWORD size)
{
    DWORD len = (size + 0xffff) & ~0xffffu, a;
    BYTE *p;

    for (a = FLAT_POOL_HI - len; a >= FLAT_POOL_LO; a -= 0x10000) {
        p = (BYTE *)VirtualAlloc((LPVOID)a, len, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (p) return p;
    }
    NKDbgPrintfW(L"flat: no pool block for %x bytes\r\n", len);
    return NULL;
}
static int g_nmods;
FMOD *FlatLoadSD(const char *name);

static int ieq(const char *a, const char *b)
{
    for (;; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return 0;
        if (!x) return 1;
    }
}

FMOD *FlatFind(const char *name)
{
    int i;

    for (i = 0; i < g_nmods; i++)
        if (ieq(g_mods[i].name, name)) return &g_mods[i];
    return NULL;
}

/* export lookup by ordinal (name == NULL) or by name */
void *FlatProc(FMOD *m, DWORD ord, const char *name)
{
    BYTE *b = (BYTE *)m->base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    IMAGE_DATA_DIRECTORY *dd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    IMAGE_EXPORT_DIRECTORY *ex;
    DWORD *fn, *nm; WORD *no; DWORD i, idx;

    if (!dd->VirtualAddress) return NULL;
    ex = (IMAGE_EXPORT_DIRECTORY *)(b + dd->VirtualAddress);
    fn = (DWORD *)(b + ex->AddressOfFunctions);
    if (name) {
        nm = (DWORD *)(b + ex->AddressOfNames); no = (WORD *)(b + ex->AddressOfNameOrdinals);
        for (i = 0; i < ex->NumberOfNames; i++)
            if (!strcmp((char *)(b + nm[i]), name)) { idx = no[i]; goto found; }
        return NULL;
    }
    if (ord < ex->Base || ord - ex->Base >= ex->NumberOfFunctions) return NULL;
    idx = ord - ex->Base;
found:
    return fn[idx] ? b + fn[idx] : NULL;
}

/* Kernel-side module lookups (hooked into SC_LoadLibraryExW / SC_GetModuleHandleW / GetOneProcAddress). HMODULE == image base. */
static int w2name(LPCWSTR w, char *nm, int cap)
{
    LPCWSTR p, plain = w;
    int n = 0, dot = 0;

    for (p = w; *p; p++)
        if (*p == '\\' || *p == '/') plain = p + 1;
    for (p = plain; *p && n < cap - 5; p++, n++) {
        nm[n] = (char)*p;
        if (*p == '.') dot = 1;
    }
    if (*p) return 0;
    if (!dot) { strcpy(nm + n, ".dll"); n += 4; }
    nm[n] = 0;
    return 1;
}

FMOD *FlatLoadSDW(LPCWSTR w)
{
    char nm[48];

    if (!w || !w2name(w, nm, sizeof nm)) return NULL;
    return FlatLoadSD(nm);
}

FMOD *FlatFindW(LPCWSTR w)
{
    char nm[sizeof(g_mods[0].name)];
    LPCWSTR p, plain = w;
    int n = 0, dot = 0;

    for (p = w; *p; p++)
        if (*p == '\\' || *p == '/') plain = p + 1;
    for (p = plain; *p && n < (int)sizeof(nm) - 5; p++, n++) {
        nm[n] = (char)*p;
        if (*p == '.') dot = 1;
    }
    if (*p) return NULL;
    if (!dot) { strcpy(nm + n, ".dll"); n += 4; }
    nm[n] = 0;
    return FlatFind(nm);
}

FMOD *FlatByHandle(HANDLE h)
{
    int i;

    for (i = 0; i < g_nmods; i++)
        if (g_mods[i].base == (DWORD)h) return &g_mods[i];
    return NULL;
}

void *FlatGetProc(HANDLE h, LPCVOID name, BOOL wide)
{
    FMOD *m = FlatByHandle(h);
    char buf[128];
    int i;

    if (!m) return NULL;
    if ((DWORD)name < 0x10000) return FlatProc(m, (DWORD)name, NULL);
    for (i = 0; i < (int)sizeof(buf) - 1; i++) {
        char c = wide ? (char)((const WCHAR *)name)[i] : ((const char *)name)[i];
        buf[i] = c;
        if (!c) {
            return FlatProc(m, 0, buf);
        }
    }
    return NULL;
}

/* Imports from coredll.dll can be served by our own code first (the mini GWES window manager and GDI): FlatSetOverride installs
 * the lookup. Ordinal imports are turned into names through coredll's export name table, so apps linked against coredll.lib
 * (ordinals) and GCC-built apps (names) take the same path. */
static void *(*g_override)(const char *name);

void FlatSetOverride(void *(*fn)(const char *name)) { g_override = fn; }

static const char *export_name(FMOD *m, DWORD ord)
{
    BYTE *b = (BYTE *)m->base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    IMAGE_DATA_DIRECTORY *dd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    IMAGE_EXPORT_DIRECTORY *ex;
    DWORD *nm; WORD *no; DWORD i;

    if (!dd->VirtualAddress) return NULL;
    ex = (IMAGE_EXPORT_DIRECTORY *)(b + dd->VirtualAddress);
    nm = (DWORD *)(b + ex->AddressOfNames); no = (WORD *)(b + ex->AddressOfNameOrdinals);
    for (i = 0; i < ex->NumberOfNames; i++)
        if (no[i] == ord - ex->Base) return (const char *)(b + nm[i]);
    return NULL;
}

static int bind_imports(FMOD *m, BYTE *b, IMAGE_NT_HEADERS *nt)
{
    IMAGE_DATA_DIRECTORY *dd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *id;
    int miss = 0;

    if (!dd->VirtualAddress) return 0;
    for (id = (IMAGE_IMPORT_DESCRIPTOR *)(b + dd->VirtualAddress); id->Name; id++) {
        const char *dll = (const char *)(b + id->Name);
        FMOD *t = FlatFind(dll);
        if (!t) t = FlatLoadSD(dll);
        DWORD *ilt = (DWORD *)(b + (id->OriginalFirstThunk ? id->OriginalFirstThunk : id->FirstThunk));
        DWORD *iat = (DWORD *)(b + id->FirstThunk);
        int core = ieq(dll, "coredll.dll") && g_override;
        for (; *ilt; ilt++, iat++) {
            void *p = NULL;
            if (t) {
                const char *nm = (*ilt & 0x80000000) ? (core ? export_name(t, *ilt & 0xffff) : NULL) : (const char *)(b + *ilt + 2);
                if (core && nm) p = g_override(nm);
                if (!p) p = (*ilt & 0x80000000) ? FlatProc(t, *ilt & 0xffff, NULL) : FlatProc(t, 0, nm);
            }
            if (!p) {
                if (miss < 8) NKDbgPrintfW(L"flat: %S imports %S!%x unresolved\r\n", m->name, dll, *ilt);
                miss++;
                p = (void *)0xBAD00000;
            }
            *iat = (DWORD)p;
        }
    }
    return miss;
}

FMOD *FlatLoad(const char *name, const void *img)
{
    const BYTE *s = (const BYTE *)img;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(s + ((IMAGE_DOS_HEADER *)s)->e_lfanew);
    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    DWORD size = nt->OptionalHeader.SizeOfImage, i;
    BYTE *b;
    FMOD *m;
    long delta;
    IMAGE_DATA_DIRECTORY *rd;

    if (g_nmods >= FL_MAX_MODS) return NULL;
    b = flat_alloc(size);
    if (!b) { NKDbgPrintfW(L"flat: no memory for %S (%x) err=%d\r\n", name, size, pCurThread->dwLastError); return NULL; }
    memcpy(b, s, nt->OptionalHeader.SizeOfHeaders);
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        DWORD raw = sec[i].SizeOfRawData, vs = sec[i].Misc.VirtualSize;
        if (raw > vs && vs) raw = vs;
        memcpy(b + sec[i].VirtualAddress, s + sec[i].PointerToRawData, raw);
    }
    delta = (long)b - (long)nt->OptionalHeader.ImageBase;
    rd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (rd->VirtualAddress && delta) {
        BYTE *p = b + rd->VirtualAddress, *e = p + rd->Size;
        while (p < e) {
            DWORD page = ((DWORD *)p)[0], blk = ((DWORD *)p)[1];
            WORD *w = (WORD *)(p + 8), *we = (WORD *)(p + blk);
            if (!blk) break;
            for (; w < we; w++)
                if ((*w >> 12) == IMAGE_REL_BASED_HIGHLOW)
                    *(long *)(b + page + (*w & 0xfff)) += delta;
            p += blk;
        }
    }
    /* An EXE without base relocations cannot be moved off its link address, so refuse it before its imports load (and their DllMain runs). */
    if (!(nt->FileHeader.Characteristics & IMAGE_FILE_DLL) && !rd->VirtualAddress) {
        NKDbgPrintfW(L"flat: %S has no relocations, refused\r\n", name);
        return NULL;
    }
    m = &g_mods[g_nmods++];
    strncpy(m->name, name, sizeof(m->name) - 1);
    m->base = (DWORD)b;
    m->size = size;
    m->isdll = (nt->FileHeader.Characteristics & IMAGE_FILE_DLL) != 0;
    m->entry = nt->OptionalHeader.AddressOfEntryPoint ? (DWORD)b + nt->OptionalHeader.AddressOfEntryPoint : 0;
    m->nmiss = bind_imports(m, b, (IMAGE_NT_HEADERS *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew));
    NKDbgPrintfW(L"flat: %S at %x size %x entry %x (%d unresolved imports)\r\n", name, m->base, size, m->entry, m->nmiss);
    return m;
}

void FlatAttach(FMOD *m)
{
    if (m->attached || !m->isdll || !m->entry) return;
    m->attached = 1;
    ((BOOL (*)(HANDLE, DWORD, LPVOID))m->entry)((HANDLE)m->base, DLL_PROCESS_ATTACH, NULL);
}

FMOD *FlatLoadSD(const char *name)
{
    static const char *dirs[] = { "\\wm6\\Windows\\", "\\wm6\\Apps\\", "\\wm6\\", NULL };
    FATENT e;
    unsigned short path[96];
    FMOD *m;
    BYTE *buf;
    DWORD got = 0;
    int d, n, i;

    if ((m = FlatFind(name))) return m;
    for (d = 0; dirs[d]; d++) {
        n = 0;
        for (i = 0; dirs[d][i]; i++) path[n++] = dirs[d][i];
        for (i = 0; name[i] && n < 94; i++) path[n++] = name[i];
        path[n] = 0;
        if (!fat_lookup(path, &e) && !(e.attr & FAT_ATTR_DIR)) break;
    }
    if (!dirs[d]) return NULL;
    /* Scratch copy of the file, exact size, released once FlatLoad has copied the image out of it. A nested load (import) gets its own. */
    buf = flat_alloc(e.size);
    if (!buf) return NULL;
    while (got < e.size) {
        int r = fat_read(&e, got, buf + got, e.size - got > 0x10000 ? 0x10000 : e.size - got);
        if (r <= 0) break;
        got += r;
    }
    m = got == e.size ? FlatLoad(name, buf) : NULL;
    VirtualFree(buf, 0, MEM_RELEASE);
    if (m) FlatAttach(m);
    return m;
}
