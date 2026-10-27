/*
 * cfile.c - the stdio calls the Frotz core makes, served by the Win32 file API.
 *
 * On the DSi build the WM6 coredll's fread returns 0 for a story file that CreateFileW/ReadFile read fine, so the
 * core's FILE calls are defined here instead. Only the functions the core uses exist.
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct CFile {
    HANDLE h;
    int    err;
} CFile;

static void to_wide(WCHAR *w, const char *s, int max)
{
    int i = 0;
    while (s[i] && i < max - 1) {
        w[i] = (unsigned char)s[i];
        i++;
    }
    w[i] = 0;
}

FILE *fopen(const char *name, const char *mode)
{
    WCHAR wname[MAX_PATH];
    DWORD access = GENERIC_READ, disp = OPEN_EXISTING;
    CFile *f;
    HANDLE h;

    if (mode[0] == 'w') { access = GENERIC_WRITE; disp = CREATE_ALWAYS; }
    else if (mode[0] == 'a') { access = GENERIC_WRITE; disp = OPEN_ALWAYS; }
    if (strchr(mode, '+')) access |= GENERIC_READ | GENERIC_WRITE;

    to_wide(wname, name, MAX_PATH);
    h = CreateFileW(wname, access, FILE_SHARE_READ, NULL, disp, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    if (mode[0] == 'a') SetFilePointer(h, 0, NULL, FILE_END);

    f = malloc(sizeof *f);
    if (!f) {
        CloseHandle(h);
        return NULL;
    }
    f->h = h;
    f->err = 0;
    return (FILE *)f;
}

int fclose(FILE *fp)
{
    CFile *f = (CFile *)fp;
    BOOL ok;

    if (!f) return EOF;
    ok = CloseHandle(f->h);
    free(f);
    return ok ? 0 : EOF;
}

size_t fread(void *ptr, size_t size, size_t n, FILE *fp)
{
    CFile *f = (CFile *)fp;
    DWORD got = 0;

    if (!size || !n) return 0;
    if (!ReadFile(f->h, ptr, (DWORD)(size * n), &got, NULL)) {
        f->err = 1;
        return 0;
    }
    return got / size;
}

size_t fwrite(const void *ptr, size_t size, size_t n, FILE *fp)
{
    CFile *f = (CFile *)fp;
    DWORD put = 0;

    if (!size || !n) return 0;
    if (!WriteFile(f->h, ptr, (DWORD)(size * n), &put, NULL)) {
        f->err = 1;
        return 0;
    }
    return put / size;
}

int fseek(FILE *fp, long off, int whence)
{
    CFile *f = (CFile *)fp;
    DWORD pos = SetFilePointer(f->h, off, NULL, (DWORD)whence);

    return pos == INVALID_SET_FILE_POINTER ? -1 : 0;
}

long ftell(FILE *fp)
{
    CFile *f = (CFile *)fp;
    DWORD pos = SetFilePointer(f->h, 0, NULL, FILE_CURRENT);

    return pos == INVALID_SET_FILE_POINTER ? -1 : (long)pos;
}

int fgetc(FILE *fp)
{
    CFile *f = (CFile *)fp;
    unsigned char c;
    DWORD got = 0;

    if (!ReadFile(f->h, &c, 1, &got, NULL)) {
        f->err = 1;
        return EOF;
    }
    return got == 1 ? c : EOF;
}

int fputc(int c, FILE *fp)
{
    CFile *f = (CFile *)fp;
    unsigned char b = (unsigned char)c;
    DWORD put = 0;

    if (!WriteFile(f->h, &b, 1, &put, NULL) || put != 1) {
        f->err = 1;
        return EOF;
    }
    return c;
}

int ferror(FILE *fp)
{
    return ((CFile *)fp)->err;
}

int remove(const char *name)
{
    WCHAR wname[MAX_PATH];

    to_wide(wname, name, MAX_PATH);
    return DeleteFileW(wname) ? 0 : -1;
}
