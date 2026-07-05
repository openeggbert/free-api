/**
 * @file 02_timers.cpp
 * @brief Demonstrates both target games' live frame-pump timer mechanisms
 * side by side -- Planet Blupi's SetTimer/KillTimer/WM_TIMER, and Free
 * Eggbert's timeSetEvent/timeKillEvent (a WinMM multimedia timer whose
 * callback runs on a background thread and safely posts into the message
 * queue via PostMessageA).
 *
 * Prints a tick count for each mechanism to the console. Close the window
 * to exit.
 */
#include <windows.h>
#include <windowsx.h>
#include <cstdio>

static bool g_running = true;
static int g_wmTimerTicks = 0;
static const UINT kMmTimerTickMsg = WM_USER + 1;
static int g_mmTimerTicks = 0;

static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_TIMER:
            ++g_wmTimerTicks;
            printf("[SetTimer/WM_TIMER]      tick #%d\n", g_wmTimerTicks);
            return 0;
        case kMmTimerTickMsg:
            ++g_mmTimerTicks;
            printf("[timeSetEvent, from bg thread] tick #%d\n", g_mmTimerTicks);
            return 0;
        case WM_DESTROY:
            g_running = false;
            return 0;
        default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}

static HWND g_hwnd = nullptr;

// Runs on a background WinMM/SDL timer thread -- must only touch the
// message queue via PostMessageA, which is cross-thread-safe.
static void CALLBACK MmTimerCallback(UINT uTimerID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
    (void)uTimerID; (void)uMsg; (void)dwUser; (void)dw1; (void)dw2;
    if (g_hwnd) PostMessageA(g_hwnd, kMmTimerTickMsg, 0, 0);
}

int main()
{
    printf("=== 02_timers ===\n");
    printf("Watch the console for interleaved ticks from both timer\n");
    printf("mechanisms. Close the window to exit.\n\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = (HINSTANCE)1;
    wc.hCursor       = LoadCursorA(nullptr, "IDC_ARROW");
    wc.hbrBackground = GetStockBrush(BLACK_BRUSH);
    wc.lpszClassName = "FreeApiExample_Timers";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "FreeApiExample_Timers",
                                 "Free API Example: Timers (see console)",
                                 WS_POPUPWINDOW | WS_CAPTION | WS_VISIBLE,
                                 100, 100, 400, 150,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    if (!hwnd) {
        printf("CreateWindowExA failed.\n");
        return 1;
    }
    g_hwnd = hwnd;

    // Planet Blupi's mechanism: a ~500ms game-tick timer via the message loop.
    SetTimer(hwnd, 1, 500, nullptr);

    // Free Eggbert's mechanism: a ~333ms multimedia timer on a background
    // thread, posting into the queue.
    MMRESULT mmTimerId = timeSetEvent(333, 333 / 4, MmTimerCallback, 0, TIME_PERIODIC);

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

    KillTimer(hwnd, 1);
    timeKillEvent(static_cast<UINT>(mmTimerId));

    printf("Exiting cleanly. WM_TIMER ticks=%d, timeSetEvent ticks=%d\n",
           g_wmTimerTicks, g_mmTimerTicks);
    return 0;
}
