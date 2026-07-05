/**
 * @file 03_input_and_cursor.cpp
 * @brief Demonstrates mouse/keyboard message translation and cursor
 * control, matching both target games' usage.
 *
 * Shows:
 *  - WM_MOUSEMOVE with bit-exact lParam packing (LOWORD=x, HIWORD=y) and
 *    MK_LBUTTON/MK_RBUTTON in wParam while dragging (Planet Blupi's
 *    CEvent::TreatEventBase pattern).
 *  - WM_LBUTTONDOWN/UP, WM_RBUTTONDOWN/UP.
 *  - VK_* keyboard codes, including the WM_SYSKEYDOWN/UP + VK_F10 quirk
 *    both games share.
 *  - ShowCursor/SetCursor/LoadCursorA (press Space to toggle the OS cursor
 *    on/off, matching both games hiding it while drawing their own sprite
 *    cursor).
 *
 * Move the mouse, click, and press keys to see live console output. Press
 * Escape or close the window to exit.
 */
#include <windows.h>
#include <windowsx.h>
#include <cstdio>

static bool g_running = true;
static bool g_cursorShown = true;

static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_MOUSEMOVE: {
            const int x = LOWORD(lParam);
            const int y = HIWORD(lParam);
            printf("WM_MOUSEMOVE x=%d y=%d  MK_LBUTTON=%d MK_RBUTTON=%d\n",
                   x, y, (wParam & MK_LBUTTON) != 0, (wParam & MK_RBUTTON) != 0);
            return 0;
        }
        case WM_LBUTTONDOWN: printf("WM_LBUTTONDOWN\n"); return 0;
        case WM_LBUTTONUP:   printf("WM_LBUTTONUP\n");   return 0;
        case WM_RBUTTONDOWN: printf("WM_RBUTTONDOWN\n"); return 0;
        case WM_RBUTTONUP:   printf("WM_RBUTTONUP\n");   return 0;
        case WM_KEYDOWN:
            printf("WM_KEYDOWN vk=0x%02X\n", (unsigned)wParam);
            if (wParam == VK_ESCAPE) {
                PostMessageA(hwnd, WM_CLOSE, 0, 0);
            } else if (wParam == VK_SPACE) {
                g_cursorShown = !g_cursorShown;
                ShowCursor(g_cursorShown ? TRUE : FALSE);
                printf("  -> ShowCursor(%s)\n", g_cursorShown ? "TRUE" : "FALSE");
            }
            return 0;
        case WM_KEYUP:
            printf("WM_KEYUP vk=0x%02X\n", (unsigned)wParam);
            return 0;
        // Both games test for WM_SYSKEYDOWN/UP with wParam==VK_F10
        // specifically (real Win32's F10-opens-system-menu quirk) and
        // treat it like a normal key event.
        case WM_SYSKEYDOWN:
            printf("WM_SYSKEYDOWN vk=0x%02X%s\n", (unsigned)wParam,
                   wParam == VK_F10 ? " (F10 quirk)" : "");
            return 0;
        case WM_SYSKEYUP:
            printf("WM_SYSKEYUP vk=0x%02X%s\n", (unsigned)wParam,
                   wParam == VK_F10 ? " (F10 quirk)" : "");
            return 0;
        case WM_DESTROY:
            g_running = false;
            return 0;
        default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}

int main()
{
    printf("=== 03_input_and_cursor ===\n");
    printf("Move the mouse, click, press keys in the window. Space toggles\n");
    printf("the OS cursor (ShowCursor). Escape or close the window to exit.\n\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = (HINSTANCE)1;
    wc.hCursor       = LoadCursorA(nullptr, "IDC_ARROW");
    wc.hbrBackground = GetStockBrush(BLACK_BRUSH);
    wc.lpszClassName = "FreeApiExample_InputAndCursor";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "FreeApiExample_InputAndCursor",
                                 "Free API Example: Input & Cursor (see console)",
                                 WS_POPUPWINDOW | WS_CAPTION | WS_VISIBLE,
                                 100, 100, 640, 480,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    if (!hwnd) {
        printf("CreateWindowExA failed.\n");
        return 1;
    }
    SetFocus(hwnd);

    MSG msg{};
    while (g_running) {
        if (PeekMessageA(&msg, nullptr, 0, 0, PM_NOREMOVE)) {
            if (!GetMessageA(&msg, nullptr, 0, 0)) break;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        } else {
            WaitMessage();
        }
    }

    printf("Exiting cleanly.\n");
    return 0;
}
