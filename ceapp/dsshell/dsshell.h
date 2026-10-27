/*
 * dsshell.h - Shared declarations for the DSi CE shell.
 *
 * Port of ThroatyMumbo's WinCE64 shell (MIT, see LICENSE-ThroatyMumbo.txt) for the DSi.
 * The shell runs on the bottom screen, where the touch panel is: desktop rows 192-383.
 * Every window is positioned in those desktop coordinates.
 */
#ifndef DSSHELL_H
#define DSSHELL_H

#include <windows.h>

#define SHELL_Y     192                     /* desktop row where the bottom screen starts */
#define SHELL_W     256
#define SHELL_H     192
#define TASKBAR_H   24
#define DESK_H      (SHELL_H - TASKBAR_H)
#define TASKBAR_Y   (SHELL_Y + DESK_H)
#define TITLE_H     16                      /* file browser title strip */

/* Icon ids for Gwes_RegisterIcon / LoadIconW. */
#define IDI_FOLDER  1
#define IDI_FILE    2
#define IDI_EXE     3
#define IDI_DRIVE   4
#define IDI_START   5

/* Where the shell looks for things.  The FAT volume is mounted under \wm6 (dsi/fs20.c ROOTDIR). */
#define STORAGE_ROOT  L"\\"
#define PROGRAMS_DIR  L"\\Apps"

#define DESKTOP_BG  RGB(0, 128, 128)

extern HINSTANCE g_hInst;
extern HWND      g_hwndDesktop;
extern HWND      g_hwndTaskBar;

/* shell_icons.c */
void  Shell_RegisterIcons(void);
HICON Shell_Icon(int id);
BOOL  Shell_JoinPath(WCHAR *out, int max, LPCWSTR parent, LPCWSTR name);
void  Shell_Wcopyn(WCHAR *dst, LPCWSTR src, int max);
int   Shell_Wlen(LPCWSTR s);
BOOL  Shell_Weq(LPCWSTR a, LPCWSTR b);      /* exact match */
int   Shell_HasExtCI(LPCWSTR name, WCHAR a, WCHAR b, WCHAR c);
int   Shell_Ellipsize(HDC hdc, LPCWSTR src, int max_w, WCHAR *out);
BOOL  Shell_LaunchExe(LPCWSTR fullPath);

/* desktop.c */
BOOL  Desktop_Register(HINSTANCE hInst);
HWND  Desktop_Create(void);

/* taskbar.c */
BOOL  TaskBar_Register(HINSTANCE hInst);
HWND  TaskBar_Create(void);
void  TaskBar_AddWindow(HWND hwnd);
void  TaskBar_RemoveWindow(HWND hwnd);

/* filebrowser.c */
BOOL  FileBrowser_Register(HINSTANCE hInst);
HWND  FileBrowser_Open(LPCWSTR path);

/* startmenu.c */
BOOL  StartMenu_Register(HINSTANCE hInst);
void  StartMenu_Open(void);
void  StartMenu_Close(void);
BOOL  StartMenu_IsVisible(void);

#endif /* DSSHELL_H */
