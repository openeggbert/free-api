/**
 * @file test_eggbert_loop.cpp
 * @brief End-to-end test reproducing free-eggbert's exact main-loop idiom
 * and `timeSetEvent`/`TimerStep`/`PostMessage`-driven game-tick pattern,
 * per plan.md TASK-0037/TASK-0043/TASK-0111 (named-deliverable
 * consolidation).
 *
 * free-eggbert's WinMain loop (blupi.cpp:903-921) is byte-for-byte the same
 * idiom as planetblupi's (see test_planetblupi_loop.cpp), but its tick
 * source is different: `timeSetEvent` starts a background WinMM/SDL timer
 * thread whose callback (`TimerStep`, blupi.cpp:641-648) calls
 * `PostMessage(g_hWnd, WM_UPDATE, 0, 0)`. This test reproduces that exact
 * shape and additionally verifies the loop correctly interleaves a real,
 * SDL-originated event (mouse motion) with directly-posted custom messages
 * in FIFO order -- the "mix of real and posted messages" requirement from
 * TASK-0037 -- since free-eggbert's own real input (mouse/keyboard) arrives
 * this way alongside its TimerStep-posted ticks.
 *
 * Note: real free-eggbert's WM_UPDATE happens to equal WM_USER+1, which
 * free-api's own internal wake-up message (kDiagWmUpdate,
 * FreeApiMessageQueue.hpp) intentionally coalesces (at most one pending at
 * a time). This test deliberately uses a different custom message id for
 * its tick counting so that intentional coalescing behavior doesn't
 * invalidate this test's "every tick observed" assertions -- coalescing
 * itself is an unrelated, already-relied-upon behavior, not part of what
 * TASK-0037/0043/0111 ask this test to prove.
 */
#include <windows.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <atomic>
#include <vector>

static int g_failures = 0;

static void Check(bool condition, const char* what)
{
    if (condition) {
        printf("[eggbert-loop] PASS: %s\n", what);
    } else {
        printf("[eggbert-loop] FAIL: %s\n", what);
        ++g_failures;
    }
}

enum class Seen { Tick, Custom1, Custom2, MouseMove };

static std::vector<Seen> g_order;
static const UINT kTickMsg    = WM_USER + 200; // stand-in for free-eggbert's WM_UPDATE
static const UINT kCustomMsg1 = WM_USER + 201;
static const UINT kCustomMsg2 = WM_USER + 202;

static LRESULT WINAPI LoopTestWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == kTickMsg)         g_order.push_back(Seen::Tick);
    else if (msg == kCustomMsg1) g_order.push_back(Seen::Custom1);
    else if (msg == kCustomMsg2) g_order.push_back(Seen::Custom2);
    else if (msg == WM_MOUSEMOVE) g_order.push_back(Seen::MouseMove);
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static HWND g_hwnd = nullptr;
static std::atomic<int> g_tickCount{0};

// Mirrors free-eggbert's TimerStep (blupi.cpp:641-648): a timeSetEvent
// callback, running on a background thread, that posts into the main
// message queue.
static void CALLBACK TimerStepLike(UINT wTimerID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
    (void)wTimerID; (void)uMsg; (void)dwUser; (void)dw1; (void)dw2;
    g_tickCount.fetch_add(1);
    if (g_hwnd) PostMessageA(g_hwnd, kTickMsg, 0, 0);
}

static void InjectMouseMotion(SDL_Window* sdlWin, float x, float y)
{
    SDL_Event motion{};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.windowID = SDL_GetWindowID(sdlWin);
    motion.motion.x = x;
    motion.motion.y = y;
    SDL_PushEvent(&motion);
}

int main()
{
    printf("[eggbert-loop] Starting\n");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("[eggbert-loop] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    WNDCLASSA wc{};
    wc.lpfnWndProc   = LoopTestWndProc;
    wc.lpszClassName = "RegTest_EggbertLoop";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "RegTest_EggbertLoop", "Test",
                                 WS_POPUPWINDOW | WS_VISIBLE, 0, 0, 320, 240,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    Check(hwnd != nullptr, "CreateWindowExA succeeds for the eggbert-loop test window");
    g_hwnd = hwnd;

    {
        MSG startup{};
        while (PeekMessageA(&startup, nullptr, 0, 0, PM_REMOVE)) {}
    }

    // Queue a deterministic mixed real+posted sequence up front: custom #1,
    // a real SDL mouse-motion event, then custom #2. Because PeekMessageA
    // only pumps SDL events when its internal queue is empty, push the
    // events with the queue empty at each step so the resulting order is
    // exactly the injection order (matches how a real, slower-than-input
    // game loop naturally interleaves them).
    PostMessageA(hwnd, kCustomMsg1, 0, 0);
    {
        MSG drain{};
        while (PeekMessageA(&drain, nullptr, 0, 0, PM_NOREMOVE)) {
            GetMessageA(&drain, nullptr, 0, 0);
            TranslateMessage(&drain);
            DispatchMessageA(&drain);
        }
    }
    InjectMouseMotion(reinterpret_cast<SDL_Window*>(hwnd), 42.0f, 43.0f);
    {
        MSG drain{};
        while (PeekMessageA(&drain, nullptr, 0, 0, PM_NOREMOVE)) {
            GetMessageA(&drain, nullptr, 0, 0);
            TranslateMessage(&drain);
            DispatchMessageA(&drain);
        }
    }
    PostMessageA(hwnd, kCustomMsg2, 0, 0);
    {
        MSG drain{};
        while (PeekMessageA(&drain, nullptr, 0, 0, PM_NOREMOVE)) {
            GetMessageA(&drain, nullptr, 0, 0);
            TranslateMessage(&drain);
            DispatchMessageA(&drain);
        }
    }

    Check(g_order.size() == 3 &&
          g_order[0] == Seen::Custom1 && g_order[1] == Seen::MouseMove && g_order[2] == Seen::Custom2,
          "the PM_NOREMOVE-gated loop delivers a mix of posted and real (SDL-originated) messages in FIFO order");

    // Now drive the loop end-to-end with a real timeSetEvent/TimerStep-style
    // background timer, exactly free-eggbert's frame-pump mechanism.
    g_order.clear();
    const UINT delayMs = 10;
    MMRESULT timerId = timeSetEvent(delayMs, delayMs / 4, TimerStepLike, 0, TIME_PERIODIC);
    Check(timerId != 0, "timeSetEvent returns a non-zero id for the eggbert-loop end-to-end test");

    const int kTargetTicks = 5;
    const int kMaxIterations = 200000; // hard safety cap, never expected to be hit
    LRESULT quitCode = -1;
    bool sawQuit = false;
    int ticksDispatched = 0;

    for (int i = 0; i < kMaxIterations; ++i) {
        MSG msg{};
        // free-eggbert's exact idiom (blupi.cpp:903-921), identical to
        // planetblupi's -- see test_planetblupi_loop.cpp.
        if (PeekMessageA(&msg, nullptr, 0, 0, PM_NOREMOVE)) {
            if (!GetMessageA(&msg, nullptr, 0, 0)) {
                quitCode = static_cast<LRESULT>(msg.wParam);
                sawQuit = true;
                break;
            }
            if (msg.message == kTickMsg) ++ticksDispatched;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        } else {
            // Idle branch: real WinMain calls WaitMessage() here when
            // inactive (TASK-0038 covers WaitMessage's own contract
            // separately).
            if (ticksDispatched >= kTargetTicks) {
                PostQuitMessage(7);
            } else {
                SDL_Delay(1);
            }
        }
    }

    timeKillEvent(static_cast<UINT>(timerId));
    g_hwnd = nullptr;

    Check(sawQuit, "the PM_NOREMOVE-gated loop terminates via GetMessage's FALSE-on-WM_QUIT path");
    Check(quitCode == 7, "the loop returns PostQuitMessage's exact exit code via msg.wParam");
    Check(ticksDispatched >= kTargetTicks,
          "timeSetEvent's TimerStep-style background-thread PostMessageA is delivered end-to-end "
          "through free-eggbert's exact PeekMessage/GetMessage/Dispatch idiom");

    DestroyWindow(hwnd);
    SDL_Quit();

    if (g_failures > 0) {
        printf("[eggbert-loop] %d FAILURE(S)\n", g_failures);
        return 1;
    }

    printf("[eggbert-loop] ALL TESTS PASSED\n");
    return 0;
}
