/*
 * iconview.h - Icon grid shared by the desktop and the file browser (from ThroatyMumbo's iconview.h).
 *
 * Touch changes from the original: one tap opens an item (no double-click), cells are 64x52 so a finger hits them,
 * and a ".." entry walks up one folder.  One IconView per host window, kept in GWL_USERDATA.
 */
#ifndef ICONVIEW_H
#define ICONVIEW_H

#include <windows.h>

#define ICONVIEW_MAX_ITEMS  64

typedef enum {
    ICONVIEW_ITEM_VIRTUAL = 0,
    ICONVIEW_ITEM_REAL    = 1
} IconItemKind;

typedef enum {
    ICONVIEW_LAYOUT_COLUMN = 0,   /* desktop: one column on the left */
    ICONVIEW_LAYOUT_GRID   = 1    /* file browser: tiles filling the window */
} IconViewLayout;

typedef void (*IconView_OpenFn)(HWND hwndOwner);

typedef struct {
    IconItemKind     kind;
    WCHAR            name[MAX_PATH];
    DWORD            attrs;            /* FILE_ATTRIBUTE_* (REAL only) */
    HICON            hIcon;            /* shared icon, never freed */
    IconView_OpenFn  on_open;          /* VIRTUAL only */
} IconItem;

typedef struct IconView IconView;      /* opaque */

IconView *IconView_Create(HWND hwndOwner, IconViewLayout layout, int top);
void      IconView_Destroy(IconView *iv);
BOOL      IconView_AddVirtualItem(IconView *iv, LPCWSTR label, HICON hIcon, IconView_OpenFn on_open);
BOOL      IconView_SetPath(IconView *iv, LPCWSTR path);
void      IconView_OnPaint(IconView *iv, HDC hdc, const RECT *rcClient);
void      IconView_OnLButtonDown(IconView *iv, int x, int y);

#endif /* ICONVIEW_H */
