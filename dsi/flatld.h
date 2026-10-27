#ifndef FLATLD_H
#define FLATLD_H
#define FL_MAX_MODS 64
typedef struct {
    char name[32];
    DWORD base, size, entry;
    int nmiss;
    int isdll, attached;
} FMOD;
FMOD *FlatLoad(const char *name, const void *img);
FMOD *FlatLoadSD(const char *name);                  /* \\wm6\\Windows\\name, then \\wm6\\name; loads dependencies, runs DllMain(ATTACH) of DLLs */
void FlatAttach(FMOD *m);
FMOD *FlatFind(const char *name);
void *FlatProc(FMOD *m, DWORD ord, const char *name);
FMOD *FlatFindW(LPCWSTR name);
FMOD *FlatLoadSDW(LPCWSTR name);
FMOD *FlatByHandle(HANDLE h);
void *FlatGetProc(HANDLE h, LPCVOID name, BOOL wide);
void FlatSetOverride(void *(*fn)(const char *name));   /* coredll imports this returns non-NULL for bind to it instead */
#endif
