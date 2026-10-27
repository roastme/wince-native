/* hello.c - smallest windowed CE app: checks that a GCC-built PE runs on coredll + gwes like a third-party EXE. */
#include <windows.h>

static LRESULT CALLBACK HelloProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        RECT rc;
        HDC hdc = BeginPaint(hwnd, &ps);
        RETAILMSG(1, (L"hello: WM_PAINT hdc=%x\r\n", hdc));
        HBRUSH br = CreateSolidBrush(RGB(0, 128, 128));
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, br);
        DeleteObject(br);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        DrawTextW(hdc, L"Hello from a PE app", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
        RETAILMSG(1, (L"hello: tap %d,%d\r\n", LOWORD(lp), HIWORD(lp)));
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR cmd, int show)
{
    WNDCLASSW wc;
    HWND hwnd;
    MSG msg;

    OutputDebugStringW(L"hello: WinMain\r\n");
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = HelloProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"HelloWindow";
    if (!RegisterClassW(&wc)) { OutputDebugStringW(L"hello: RegisterClass failed\r\n"); return 1; }
    hwnd = CreateWindowExW(0, L"HelloWindow", L"", WS_POPUP, 0, 0, 256, 192, NULL, NULL, hInst, NULL);
    RETAILMSG(1, (L"hello: hwnd=%x\r\n", hwnd));
    if (!hwnd) return 1;
    ShowWindow(hwnd, SW_SHOW);
    OutputDebugStringW(L"hello: shown\r\n");
    UpdateWindow(hwnd);
    OutputDebugStringW(L"hello: updated\r\n");
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
