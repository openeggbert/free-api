/**
 * @file test_planetblupi_loop.cpp
 * @brief End-to-end test reproducing planetblupi's exact main-loop idiom
 * and `SetTimer`-driven game-tick pattern, per plan.md TASK-0037/TASK-0041/
 * TASK-0110 (named-deliverable consolidation).
 *
 * planetblupi's WinMain loop (blupi.cpp:904-919) is exactly:
 *
 *   SetTimer(g_hWnd, 1, g_timerInterval, NULL);
 *   while (TRUE) {
 *       if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
 *           if (!GetMessage(&msg, NULL, 0, 0)) return msg.wParam;
 *           TranslateMessage(&msg);
 *           DispatchMessage(&msg);
 *       } else {
 *           if (!g_bActive) WaitMessage();
 *       }
 *   }
 *
 * This test reproduces that idiom verbatim (the idle branch is bounded
 * instead of blocking indefinitely on WaitMessage -- WaitMessage's own
 * non-busy-spin contract is covered separately by TASK-0038) and drives it
 * with a real SetTimer, verifying WM_TIMER ticks are delivered through the
 * exact PM_NOREMOVE-gated path and that GetMessage's FALSE-on-WM_QUIT
 * return correctly unwinds the loop with the right exit code.
 */
#include <windows.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <vector>

static int g_failures = 0;

static void Check(bool condition, const char* what)
{
    if (condition) {
        printf("[planetblupi-loop] PASS: %s\n", what);
    } else {
        printf("[planetblupi-loop] FAIL: %s\n", what);
        ++g_failures;
    }
}

static std::vector<UINT> g_dispatched;

static LRESULT WINAPI LoopTestWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_TIMER) {
        g_dispatched.push_back(msg);
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

int main()
{
    printf("[planetblupi-loop] Starting\n");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("[planetblupi-loop] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    WNDCLASSA wc{};
    wc.lpfnWndProc   = LoopTestWndProc;
    wc.lpszClassName = "RegTest_PlanetblupiLoop";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "RegTest_PlanetblupiLoop", "Test",
                                 WS_POPUPWINDOW | WS_VISIBLE, 0, 0, 320, 240,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    Check(hwnd != nullptr, "CreateWindowExA succeeds for the planetblupi-loop test window");

    // Drain window-creation startup messages before the loop starts, same as
    // real WinMain does implicitly (nothing is dispatched before the loop).
    {
        MSG startup{};
        while (PeekMessageA(&startup, nullptr, 0, 0, PM_REMOVE)) {}
    }

    const UINT timerInterval = 15; // matches planetblupi's g_timerInterval shape
    SetTimer(hwnd, 1, timerInterval, nullptr);

    const int kTargetTicks = 5;
    const int kMaxIterations = 200000; // hard safety cap, never expected to be hit
    LRESULT quitCode = -1;
    bool sawQuit = false;

    for (int i = 0; i < kMaxIterations; ++i) {
        MSG msg{};
        // This is planetblupi's exact idiom: a non-blocking PM_NOREMOVE gate
        // before the blocking GetMessage call.
        if (PeekMessageA(&msg, nullptr, 0, 0, PM_NOREMOVE)) {
            if (!GetMessageA(&msg, nullptr, 0, 0)) {
                quitCode = static_cast<LRESULT>(msg.wParam);
                sawQuit = true;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        } else {
            // Idle branch: real WinMain calls WaitMessage() here when
            // inactive (TASK-0038 covers WaitMessage's own contract
            // separately). This test only needs forward progress, so it
            // requests shutdown once enough ticks have been observed.
            if (static_cast<int>(g_dispatched.size()) >= kTargetTicks) {
                PostQuitMessage(42);
            } else {
                SDL_Delay(1);
            }
        }
    }

    Check(sawQuit, "the PM_NOREMOVE-gated loop terminates via GetMessage's FALSE-on-WM_QUIT path");
    Check(quitCode == 42, "the loop returns PostQuitMessage's exact exit code via msg.wParam");
    Check(static_cast<int>(g_dispatched.size()) >= kTargetTicks,
          "SetTimer's WM_TIMER is delivered end-to-end through planetblupi's exact PeekMessage/GetMessage/Dispatch idiom");

    KillTimer(hwnd, 1);
    DestroyWindow(hwnd);
    SDL_Quit();

    if (g_failures > 0) {
        printf("[planetblupi-loop] %d FAILURE(S)\n", g_failures);
        return 1;
    }

    printf("[planetblupi-loop] ALL TESTS PASSED\n");
    return 0;
}
