/* coredll.dll exports the mini GWES serves itself. The flat loader binds an app's coredll import here first (dsi/flatld.c
 * FlatSetOverride); everything else (heap, files, strings, registry) stays with the real WM6 coredll. No headers: the SDK
 * macros would turn some of these names into traps, and only the addresses matter here. */
#define GW_EXPORTS(X) \
    X(BeginPaint) \
    X(ClientToScreen) \
    X(CopyRect) \
    X(CreateCompatibleDC) \
    X(CreateFontIndirectW) \
    X(CreatePen) \
    X(CreateSolidBrush) \
    X(CreateWindowExW) \
    X(DefWindowProcW) \
    X(DeleteDC) \
    X(DeleteObject) \
    X(DestroyIcon) \
    X(DestroyWindow) \
    X(DispatchMessageW) \
    X(DrawEdge) \
    X(DrawFocusRect) \
    X(DrawIconEx) \
    X(DrawTextW) \
    X(EnableWindow) \
    X(EndPaint) \
    X(ExtTextOutW) \
    X(FillRect) \
    X(GetActiveWindow) \
    X(GetCapture) \
    X(GetClientRect) \
    X(GetCursorPos) \
    X(GetDC) \
    X(GetFocus) \
    X(GetForegroundWindow) \
    X(GetMessageW) \
    X(GetParent) \
    X(GetStockObject) \
    X(GetSysColor) \
    X(GetSysColorBrush) \
    X(GetSystemMetrics) \
    X(GetTextExtentExPointW) \
    X(GetTextMetricsW) \
    X(GetWindow) \
    X(Gwes_RegisterIcon) \
    X(GetWindowLongW) \
    X(GetWindowRect) \
    X(GetWindowTextW) \
    X(InflateRect) \
    X(IntersectRect) \
    X(InvalidateRect) \
    X(IsRectEmpty) \
    X(IsWindow) \
    X(IsWindowEnabled) \
    X(IsWindowVisible) \
    X(KillTimer) \
    X(LoadCursorW) \
    X(LoadIconW) \
    X(LoadImageW) \
    X(MoveWindow) \
    X(OffsetRect) \
    X(PatBlt) \
    X(PeekMessageW) \
    X(Polygon) \
    X(Polyline) \
    X(PostMessageW) \
    X(PostQuitMessage) \
    X(PtInRect) \
    X(Rectangle) \
    X(RegisterClassW) \
    X(RegisterTaskBar) \
    X(ReleaseCapture) \
    X(ReleaseDC) \
    X(ScreenToClient) \
    X(SelectObject) \
    X(SendMessageW) \
    X(SetActiveWindow) \
    X(SetBkColor) \
    X(SetBkMode) \
    X(SetCapture) \
    X(SetCursor) \
    X(SetCursorPos) \
    X(SetFocus) \
    X(SetForegroundWindow) \
    X(SetRect) \
    X(SetTextColor) \
    X(SetTimer) \
    X(SetWindowLongW) \
    X(SetWindowPos) \
    X(SetWindowTextW) \
    X(ShowWindow) \
    X(SystemParametersInfoW) \
    X(TranslateMessage) \
    X(UnionRect) \
    X(UpdateWindow) \
    X(ValidateRect) \
    X(WindowFromPoint) \

#define DECL(n) extern void n(void);
GW_EXPORTS(DECL)

static const struct { const char *name; void (*fn)(void); } g_exports[] = {
#define ENT(n) { #n, n },
    GW_EXPORTS(ENT)
};

extern int strcmp(const char *, const char *);

/* Starts an app from the SD card (dsi/demo.c): the shell's CreateProcessW call lands here, not in the WM6 coredll. */
extern void Dsi_CreateProcessW(void);

void *Gwes_Export(const char *name)
{
    unsigned i;

    if (!strcmp(name, "CreateProcessW")) return (void *)Dsi_CreateProcessW;
    for (i = 0; i < sizeof g_exports / sizeof g_exports[0]; i++)
        if (!strcmp(g_exports[i].name, name)) return (void *)g_exports[i].fn;
    return 0;
}
