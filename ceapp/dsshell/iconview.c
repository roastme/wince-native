/*
 * iconview.c - Icon grid for the desktop and the file browser (from ThroatyMumbo's iconview.c, touch-adapted).
 *
 * COLUMN layout (desktop): one item per row on the left rail.
 * GRID layout (file browser): tiles from the top of the body (below the title strip) down and across.
 * Hit testing recomputes the layout from the client size, so paint and hit test cannot drift apart.
 */
#include "dsshell.h"
#include "iconview.h"

#define CELL_W       64
#define CELL_H       52
#define ICON_SZ      32
#define LABEL_H      14

struct IconView {
    HWND           hwnd;
    IconViewLayout layout;
    int            top;                     /* first body row inside the client area */
    WCHAR          cur_path[MAX_PATH];
    IconItem       items[ICONVIEW_MAX_ITEMS];
    int            count;
};

static WCHAR lc(WCHAR c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

/* Case-insensitive a > b. */
static int name_gt(LPCWSTR a, LPCWSTR b)
{
    while (*a && lc(*a) == lc(*b)) { a++; b++; }
    return lc(*a) > lc(*b);
}

/* Cell rect for item i (client coordinates).  FALSE when i does not fit. */
static BOOL cell_of(const IconView *iv, const RECT *rc, int i, RECT *cell)
{
    int x, y, cols, rows;

    if (iv->layout == ICONVIEW_LAYOUT_COLUMN) {
        y = rc->top + i * CELL_H;
        if (y + CELL_H > rc->bottom) return FALSE;
        SetRect(cell, rc->left, y, rc->left + CELL_W, y + CELL_H);
        return TRUE;
    }
    cols = (rc->right - rc->left) / CELL_W;
    rows = (rc->bottom - iv->top) / CELL_H;
    if (cols <= 0 || rows <= 0 || i >= cols * rows) return FALSE;
    x = rc->left + (i % cols) * CELL_W;
    y = iv->top + (i / cols) * CELL_H;
    SetRect(cell, x, y, x + CELL_W, y + CELL_H);
    return TRUE;
}

IconView *IconView_Create(HWND hwndOwner, IconViewLayout layout, int top)
{
    IconView *iv = (IconView *)LocalAlloc(LPTR, sizeof(IconView));
    if (!iv) return NULL;
    iv->hwnd   = hwndOwner;
    iv->layout = layout;
    iv->top    = top;
    return iv;
}

void IconView_Destroy(IconView *iv)
{
    if (iv) LocalFree((HLOCAL)iv);
}

BOOL IconView_AddVirtualItem(IconView *iv, LPCWSTR label, HICON hIcon, IconView_OpenFn on_open)
{
    IconItem *it;
    if (!iv || iv->count >= ICONVIEW_MAX_ITEMS) return FALSE;
    it = &iv->items[iv->count++];
    it->kind    = ICONVIEW_ITEM_VIRTUAL;
    it->attrs   = 0;
    it->hIcon   = hIcon;
    it->on_open = on_open;
    Shell_Wcopyn(it->name, label, MAX_PATH);
    return TRUE;
}

/* Fill items[] from a folder: "..", then folders first, then files, each group sorted case-insensitively. */
static BOOL enum_folder(LPCWSTR path, IconItem *items, int *out)
{
    WCHAR pat[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    int n = 0, i, j;

    *out = 0;
    if (!Shell_JoinPath(pat, MAX_PATH, path, L"*")) return FALSE;
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    do {
        IconItem *it;
        if (fd.cFileName[0] == '.' && (fd.cFileName[1] == 0 || (fd.cFileName[1] == '.' && fd.cFileName[2] == 0)))
            continue;
        if (n >= ICONVIEW_MAX_ITEMS) break;
        it = &items[n++];
        it->kind  = ICONVIEW_ITEM_REAL;
        it->attrs = fd.dwFileAttributes;
        it->on_open = NULL;
        Shell_Wcopyn(it->name, fd.cFileName, MAX_PATH);
    } while (FindNextFileW(h, &fd));
    FindClose(h);

    /* Insertion sort: directories first, then by name. */
    for (i = 1; i < n; i++) {
        IconItem tmp = items[i];
        j = i;
        while (j > 0) {
            IconItem *p = &items[j - 1];
            int pdir = (p->attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
            int tdir = (tmp.attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
            int gt;
            if (pdir != tdir) gt = !pdir && tdir;
            else gt = name_gt(p->name, tmp.name);
            if (!gt) break;
            items[j] = *p;
            j--;
        }
        items[j] = tmp;
    }
    *out = n;
    return TRUE;
}

BOOL IconView_SetPath(IconView *iv, LPCWSTR path)
{
    IconItem list[ICONVIEW_MAX_ITEMS];
    int n = 0, i;

    if (!iv || !path) return FALSE;
    if (!enum_folder(path, list, &n)) n = 0;

    iv->count = 0;
    Shell_Wcopyn(iv->cur_path, path, MAX_PATH);
    SetWindowTextW(iv->hwnd, iv->cur_path);

    /* ".." goes first whenever the path is below the storage root. */
    if (!Shell_Weq(path, STORAGE_ROOT)) {
        IconItem *up = &iv->items[iv->count++];
        up->kind = ICONVIEW_ITEM_REAL;
        up->attrs = FILE_ATTRIBUTE_DIRECTORY;
        up->on_open = NULL;
        Shell_Wcopyn(up->name, L"..", MAX_PATH);
        up->hIcon = Shell_Icon(IDI_FOLDER);
    }
    for (i = 0; i < n && iv->count < ICONVIEW_MAX_ITEMS; i++) {
        IconItem *it = &iv->items[iv->count++];
        *it = list[i];
        it->hIcon = Shell_Icon((it->attrs & FILE_ATTRIBUTE_DIRECTORY) ? IDI_FOLDER
                             : Shell_HasExtCI(it->name, 'e', 'x', 'e') ? IDI_EXE : IDI_FILE);
    }
    InvalidateRect(iv->hwnd, NULL, TRUE);
    return TRUE;
}

void IconView_OnPaint(IconView *iv, HDC hdc, const RECT *rcClient)
{
    RECT rc = *rcClient;
    int i;

    if (!iv) return;
    rc.top = iv->top;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, iv->layout == ICONVIEW_LAYOUT_COLUMN ? RGB(255, 255, 255) : RGB(0, 0, 0));

    for (i = 0; i < iv->count; i++) {
        RECT cell, icon, label;
        WCHAR text[MAX_PATH + 4];
        int len;
        IconItem *it = &iv->items[i];

        if (!cell_of(iv, &rc, i, &cell)) break;
        icon.left = cell.left + (CELL_W - ICON_SZ) / 2;
        icon.top  = cell.top + 2;
        if (it->hIcon) DrawIconEx(hdc, icon.left, icon.top, it->hIcon, ICON_SZ, ICON_SZ, 0, NULL, DI_NORMAL);

        SetRect(&label, cell.left, cell.top + ICON_SZ + 2, cell.right, cell.top + ICON_SZ + 2 + LABEL_H);
        len = Shell_Ellipsize(hdc, it->name, CELL_W - 4, text);
        DrawTextW(hdc, text, len, &label, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
}

/* Open the item at index: virtual items call back, folders navigate, ".." walks up, .exe files launch. */
static void open_item(IconView *iv, int index)
{
    IconItem *it = &iv->items[index];
    WCHAR child[MAX_PATH];

    if (it->kind == ICONVIEW_ITEM_VIRTUAL) {
        if (it->on_open) it->on_open(iv->hwnd);
        return;
    }
    if (Shell_Weq(it->name, L"..")) {
        WCHAR parent[MAX_PATH];
        int k = Shell_Wlen(iv->cur_path);
        while (k > 0 && iv->cur_path[k - 1] != '\\') k--;
        k--;                                  /* index of the last separator */
        Shell_Wcopyn(parent, iv->cur_path, MAX_PATH);
        if (k <= Shell_Wlen(STORAGE_ROOT)) Shell_Wcopyn(parent, STORAGE_ROOT, MAX_PATH);
        else parent[k] = 0;
        IconView_SetPath(iv, parent);
        return;
    }
    if (!Shell_JoinPath(child, MAX_PATH, iv->cur_path, it->name)) return;
    if (it->attrs & FILE_ATTRIBUTE_DIRECTORY) {
        IconView_SetPath(iv, child);
        return;
    }
    if (Shell_HasExtCI(it->name, 'e', 'x', 'e')) Shell_LaunchExe(child);
}

void IconView_OnLButtonDown(IconView *iv, int x, int y)
{
    RECT rc;
    int i;
    POINT pt;

    if (!iv) return;
    GetClientRect(iv->hwnd, &rc);
    rc.top = iv->top;
    pt.x = x; pt.y = y;
    for (i = 0; i < iv->count; i++) {
        RECT cell;
        if (!cell_of(iv, &rc, i, &cell)) break;
        if (PtInRect(&cell, pt)) {
            open_item(iv, i);            /* a tap opens; there is no double-click on a touch panel */
            return;
        }
    }
}
