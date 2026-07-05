/**
 * @file 01_window_and_message_loop.cpp
 * @brief Demonstrates the exact window-creation and message-loop pattern
 * both target games (Free Eggbert, Planet Blupi) use.
 *
 * Shows:
 *  - RegisterClassA with a full WNDCLASSA (matching both games' shape).
 *  - CreateWindowExA (windowed mode) + AdjustWindowRect.
 *  - ShowWindow / UpdateWindow / SetFocus startup sequence.
 *  - The PeekMessage(PM_NOREMOVE) -> GetMessage -> Translate/Dispatch loop
 *    idiom both games use verbatim.
 *  - DefWindowProcA's default WM_CLOSE -> DestroyWindow -> WM_DESTROY ->
 *    PostQuitMessage(0) -> GetMessageA returns FALSE chain.
 *
 * Close the window (or press Escape) to exit.
 */
#include <windows.h>
#include <windowsx.h>
#include <cstdio>

static bool g_running = true;

static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                PostMessageA(hwnd, WM_CLOSE, 0, 0);
            }
            return 0;
        case WM_DESTROY:
            printf("WM_DESTROY received -- cleaning up before quit.\n");
            g_running = false;
            return 0;
        default:
            // Both games rely on DefWindowProcA's default handling for
            // WM_CLOSE (destroy the window) and any message they don't
            // handle themselves.
            return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}

int main()
{
    printf("=== 01_window_and_message_loop ===\n");
    printf("A window should appear. Press Escape or close it to exit.\n\n");

    WNDCLASSA wc{};
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = (HINSTANCE)1;
    wc.hCursor       = LoadCursorA(nullptr, "IDC_ARROW");
    wc.hbrBackground = GetStockBrush(BLACK_BRUSH);
    wc.lpszClassName = "FreeApiExample_WindowAndLoop";
    RegisterClassA(&wc);

    // Windowed mode: exactly both games' style (WS_POPUPWINDOW|WS_CAPTION),
    // sized via AdjustWindowRect so the *client area* ends up 640x480 --
    // free-api's AdjustWindowRect is a deliberate identity transform (see
    // include/winuser.h), since this library's "window size" already IS the
    // client-area size.
    const DWORD style = WS_POPUPWINDOW | WS_CAPTION;
    RECT rc = {0, 0, 640, 480};
    AdjustWindowRect(&rc, style, FALSE);

    HWND hwnd = CreateWindowA("FreeApiExample_WindowAndLoop",
                              "Free API Example: Window & Message Loop",
                              style,
                              100, 100, rc.right - rc.left, rc.bottom - rc.top,
                              HWND_DESKTOP, nullptr, (HINSTANCE)1, nullptr);
    if (!hwnd) {
        printf("CreateWindowA failed.\n");
        return 1;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetFocus(hwnd);

    MSG msg{};
    while (g_running) {
        if (PeekMessageA(&msg, nullptr, 0, 0, PM_NOREMOVE)) {
            if (!GetMessageA(&msg, nullptr, 0, 0)) {
                break; // WM_QUIT
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        } else {
            WaitMessage();
        }
    }

    printf("Exiting cleanly.\n");
    return 0;
}
