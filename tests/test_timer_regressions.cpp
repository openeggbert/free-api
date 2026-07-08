/**
 * @file test_timer_regressions.cpp
 * @brief Focused P0 regression tests for both target games' live frame-pump
 * timer mechanisms, per plan.md TASK-0039/0040/0041/0042/0043/0044.
 *
 * Free API implements two mutually-exclusive frame-pump mechanisms and both
 * must keep working (see NEXT.md architecture notes):
 *  - planetblupi drives its game loop via SetTimer/KillTimer/WM_TIMER.
 *  - free-eggbert drives its game loop via timeSetEvent/timeKillEvent,
 *    posting into the message queue from a real background (WinMM/SDL
 *    timer) thread -- exercising the exact cross-thread PostMessageA path
 *    the P0 "do not hold the queue mutex while sleeping" fix
 *    (src/winuser_message.cpp PeekMessageA) protects.
 *
 * These tests use real wall-clock timing (SDL_GetTicks/SDL_Delay) rather
 * than mocked time, since both timer mechanisms are driven by real elapsed
 * time (SetTimer via PeekMessageA's SDL_GetTicks() check; timeSetEvent via
 * SDL_AddTimer's own background thread) -- there is no seam to mock without
 * changing production code. Intervals/tolerances are chosen to be reliable
 * in a loaded CI/sandboxed environment, not to test tight real-time
 * precision (neither game requires hard real-time timing).
 */
#include <windows.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <atomic>
#include <set>
#include <vector>

static int g_failures = 0;

static void Check(bool condition, const char* what)
{
    if (condition) {
        printf("[timer-regressions] PASS: %s\n", what);
    } else {
        printf("[timer-regressions] FAIL: %s\n", what);
        ++g_failures;
    }
}

static LRESULT WINAPI TimerTestWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static HWND MakeTestWindow(const char* className)
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = TimerTestWndProc;
    wc.lpszClassName = className;
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    return CreateWindowExA(0, className, "Test", WS_POPUPWINDOW | WS_VISIBLE,
                            0, 0, 320, 240,
                            nullptr, nullptr, (HINSTANCE)1, nullptr);
}

// Drain the queue via PeekMessageA(PM_REMOVE) for up to `durationMs`,
// counting WM_TIMER messages matching (hwnd, timerId).
static int CountWmTimerFor(HWND hwnd, UINT_PTR timerId, Uint64 durationMs)
{
    int count = 0;
    const Uint64 deadline = SDL_GetTicks() + durationMs;
    MSG msg{};
    while (SDL_GetTicks() < deadline) {
        if (PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_TIMER && msg.hwnd == hwnd && msg.wParam == timerId) {
                ++count;
            }
        } else {
            SDL_Delay(1);
        }
    }
    return count;
}

// TASK-0041: SetTimer/WM_TIMER fires at approximately the requested
// interval while the message loop polls PeekMessage -- planetblupi's entire
// game-tick mechanism (blupi.cpp:900,562,392,416-426).
static void TestSetTimerFiresWmTimerAtApproximateInterval()
{
    HWND hwnd = MakeTestWindow("RegTest_SetTimer");
    Check(hwnd != nullptr, "CreateWindowExA succeeds for SetTimer test window");
    if (!hwnd) return;

    CountWmTimerFor(hwnd, 1, 0); // drain any startup messages first

    const UINT intervalMs = 30;
    UINT_PTR timerId = SetTimer(hwnd, 1, intervalMs, nullptr);
    Check(timerId == 1, "SetTimer returns the requested timer id");

    // Poll for ~10 intervals' worth of wall time; expect roughly that many
    // WM_TIMER fires (generous tolerance for CI scheduling jitter).
    const Uint64 windowMs = intervalMs * 10;
    int fired = CountWmTimerFor(hwnd, 1, windowMs);

    const int expected = static_cast<int>(windowMs / intervalMs);
    Check(fired >= expected / 3 && fired <= expected * 2,
          "SetTimer's WM_TIMER fires at approximately the requested interval");

    KillTimer(hwnd, 1);
    DestroyWindow(hwnd);
}

// TASK-0042: KillTimer actually stops further WM_TIMER delivery.
static void TestKillTimerStopsWmTimerDelivery()
{
    HWND hwnd = MakeTestWindow("RegTest_KillTimer");
    Check(hwnd != nullptr, "CreateWindowExA succeeds for KillTimer test window");
    if (!hwnd) return;

    CountWmTimerFor(hwnd, 1, 0);

    const UINT intervalMs = 20;
    SetTimer(hwnd, 2, intervalMs, nullptr);

    // Let it fire at least once, then kill it.
    CountWmTimerFor(hwnd, 2, intervalMs * 5);
    BOOL killed = KillTimer(hwnd, 2);
    Check(killed == TRUE, "KillTimer returns TRUE");

    // Drain whatever's already queued so it doesn't count against the
    // "no further delivery" check below.
    CountWmTimerFor(hwnd, 2, 0);

    int firedAfterKill = CountWmTimerFor(hwnd, 2, intervalMs * 5);
    Check(firedAfterKill == 0, "KillTimer stops further WM_TIMER delivery for that timer id");

    DestroyWindow(hwnd);
}

// TASK-24H-0508: SetTimer has two implemented edge-case behaviors with no
// prior test coverage -- passing nIDEvent==0 triggers auto-ID generation
// (via the shared g_nextTimerId counter also used by timeSetEvent), and
// passing uElapse==0 clamps the interval to a minimum of 1ms rather than
// never firing. Every other SetTimer call in this file uses an explicit
// non-zero ID and interval, so neither branch was previously exercised.
static void TestSetTimerAutoIdAndMinimumIntervalClamp()
{
    HWND hwnd = MakeTestWindow("RegTest_SetTimerAutoId");
    Check(hwnd != nullptr, "CreateWindowExA succeeds for SetTimer auto-id/clamp test window");
    if (!hwnd) return;

    UINT_PTR autoId = SetTimer(hwnd, 0, 5, nullptr);
    Check(autoId != 0, "SetTimer(nIDEvent=0) returns a non-zero auto-generated timer id");

    bool sawAutoIdFire = false;
    {
        const Uint64 deadline = SDL_GetTicks() + 200;
        MSG msg{};
        while (SDL_GetTicks() < deadline && !sawAutoIdFire) {
            if (PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_TIMER && msg.wParam == autoId) sawAutoIdFire = true;
            } else {
                SDL_Delay(1);
            }
        }
    }
    Check(sawAutoIdFire, "SetTimer(nIDEvent=0)'s auto-generated id actually fires WM_TIMER carrying that same id");
    KillTimer(hwnd, autoId);
    CountWmTimerFor(hwnd, autoId, 0); // drain any straggler already queued

    UINT_PTR clampedId = SetTimer(hwnd, 99, 0, nullptr);
    Check(clampedId == 99, "SetTimer(uElapse=0) still returns the requested timer id");
    int clampedFireCount = CountWmTimerFor(hwnd, 99, 100);
    Check(clampedFireCount > 0,
          "SetTimer(uElapse=0) still delivers WM_TIMER (clamped to a fast minimum interval) rather than never firing");
    KillTimer(hwnd, 99);

    DestroyWindow(hwnd);
}

// TASK-24H-0505: FreeApiMessageQueue coalesces WM_TIMER (at most one pending
// per hwnd+timer-id) so stale timer messages don't pile up -- but this must
// not come at the cost of delaying/dropping real, concurrently-posted
// non-timer input, which is exactly the scenario both games' frame pumps
// create every frame (a fast game-tick timer running alongside real player
// input). Proves all posted non-timer messages are still received, each
// exactly once, while a fast timer fires concurrently.
static void TestFastTimerDoesNotStarveConcurrentNonTimerMessages()
{
    HWND hwnd = MakeTestWindow("RegTest_TimerStarvation");
    Check(hwnd != nullptr, "CreateWindowExA succeeds for timer-starvation test window");
    if (!hwnd) return;

    CountWmTimerFor(hwnd, 3, 0); // drain any startup messages

    const UINT intervalMs = 2; // deliberately fast, to maximize WM_TIMER pressure
    SetTimer(hwnd, 3, intervalMs, nullptr);

    const UINT kInputMsg = WM_USER + 300;
    const int kInputCount = 10;

    // Post one input message every ~15ms, interleaved with draining, rather
    // than all up front -- WM_TIMER only fires inside PeekMessageA when the
    // queue was already empty on entry (src/winuser_message.cpp), so
    // front-loading every input message would itself starve the *timer*
    // (queue never goes empty) rather than testing whether the timer
    // starves *input*. Interleaving matches a real game loop, where player
    // input and a fast frame-pump timer both compete for the same queue
    // over time, not all at once.
    int timerCount = 0;
    std::set<int> seenInputIndices;
    const Uint64 deadline = SDL_GetTicks() + 400;
    int nextInputToPost = 0;
    Uint64 lastPostTick = SDL_GetTicks();
    MSG msg{};
    while (SDL_GetTicks() < deadline && static_cast<int>(seenInputIndices.size()) < kInputCount) {
        if (nextInputToPost < kInputCount && SDL_GetTicks() - lastPostTick >= 15) {
            PostMessageA(hwnd, kInputMsg, static_cast<WPARAM>(nextInputToPost), 0);
            ++nextInputToPost;
            lastPostTick = SDL_GetTicks();
        }

        if (PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_TIMER && msg.wParam == 3) {
                ++timerCount;
            } else if (msg.message == kInputMsg) {
                seenInputIndices.insert(static_cast<int>(msg.wParam));
            }
        } else {
            SDL_Delay(1);
        }
    }

    Check(static_cast<int>(seenInputIndices.size()) == kInputCount,
          "all posted non-timer input messages are received despite a concurrently fast-firing SetTimer "
          "(WM_TIMER coalescing does not starve them)");
    Check(timerCount > 0, "the fast timer also continues firing WM_TIMER concurrently with input delivery");

    KillTimer(hwnd, 3);
    DestroyWindow(hwnd);
}

// TASK-0043: timeSetEvent's callback fires repeatedly at approximately the
// requested interval, and can safely call PostMessageA (mirroring
// free-eggbert's TimerStep, blupi.cpp:891,628,641-648) without corrupting
// the queue.
static std::atomic<int> g_mmTickCount{0};
static HWND g_mmTestHwnd = nullptr;
static const UINT kMmTickMsg = WM_USER + 100;

static void CALLBACK MmTimerCallback(UINT uTimerID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
    (void)uTimerID; (void)uMsg; (void)dwUser; (void)dw1; (void)dw2;
    int n = g_mmTickCount.fetch_add(1) + 1;
    if (g_mmTestHwnd) {
        PostMessageA(g_mmTestHwnd, kMmTickMsg, static_cast<WPARAM>(n), 0);
    }
}

static void TestTimeSetEventFiresCallbackAtApproximateInterval()
{
    HWND hwnd = MakeTestWindow("RegTest_TimeSetEvent");
    Check(hwnd != nullptr, "CreateWindowExA succeeds for timeSetEvent test window");
    if (!hwnd) return;

    g_mmTestHwnd = hwnd;
    g_mmTickCount.store(0);

    const UINT delayMs = 15;
    MMRESULT id = timeSetEvent(delayMs, delayMs / 4, MmTimerCallback, 0, TIME_PERIODIC);
    Check(id != 0, "timeSetEvent returns a non-zero timer id");

    SDL_Delay(delayMs * 10);

    const int ticks = g_mmTickCount.load();
    const int expected = 10;
    Check(ticks >= expected / 3 && ticks <= expected * 3,
          "timeSetEvent's callback fires at approximately the requested interval");

    // Drain and verify PostMessageA from the background timer thread
    // delivered intact, monotonically-increasing wParam values -- proof the
    // cross-thread post path (TASK-0039/0040) doesn't corrupt the queue.
    std::vector<WPARAM> seen;
    MSG msg{};
    while (PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE)) {
        if (msg.message == kMmTickMsg) {
            seen.push_back(msg.wParam);
        }
    }
    bool monotonic = true;
    for (size_t i = 1; i < seen.size(); ++i) {
        if (seen[i] <= seen[i - 1]) { monotonic = false; break; }
    }
    Check(!seen.empty(), "background timeSetEvent thread's PostMessageA calls are received by the main thread");
    Check(monotonic, "messages posted from the timer thread arrive uncorrupted and in order");

    timeKillEvent(static_cast<UINT>(id));
    g_mmTestHwnd = nullptr;
    DestroyWindow(hwnd);
}

// TASK-0044: timeKillEvent actually stops the periodic callback.
static void TestTimeKillEventStopsCallback()
{
    HWND hwnd = MakeTestWindow("RegTest_TimeKillEvent");
    Check(hwnd != nullptr, "CreateWindowExA succeeds for timeKillEvent test window");
    if (!hwnd) return;

    g_mmTestHwnd = hwnd;
    g_mmTickCount.store(0);

    const UINT delayMs = 10;
    MMRESULT id = timeSetEvent(delayMs, delayMs / 4, MmTimerCallback, 0, TIME_PERIODIC);
    Check(id != 0, "timeSetEvent returns a non-zero timer id for the kill test");

    SDL_Delay(delayMs * 5); // let it tick a few times

    MMRESULT killResult = timeKillEvent(static_cast<UINT>(id));
    Check(killResult == MMSYSERR_NOERROR, "timeKillEvent returns MMSYSERR_NOERROR");

    const int countAtKill = g_mmTickCount.load();
    SDL_Delay(delayMs * 10); // wait long enough that more ticks would have happened if not killed
    const int countAfterWait = g_mmTickCount.load();

    Check(countAfterWait == countAtKill, "timeKillEvent stops further callback invocations");

    // Drain any messages posted before the kill so they don't leak into
    // later tests.
    MSG msg{};
    while (PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE)) {}

    g_mmTestHwnd = nullptr;
    DestroyWindow(hwnd);
}

// TASK-0039/0040: stress-test cross-thread PostMessageA safety. A real
// timeSetEvent background thread posts several hundred messages in rapid
// succession while the main thread concurrently drains the queue via
// PeekMessageA -- verifying no lost/corrupted/duplicated messages and no
// crash, which is exactly the race the P0 "do not hold the queue mutex
// while sleeping" fix (PeekMessageA in src/winuser_message.cpp) guards
// against (todo/free-api-performance-todo.md).
static void TestCrossThreadPostMessageAStressTest()
{
    HWND hwnd = MakeTestWindow("RegTest_CrossThreadStress");
    Check(hwnd != nullptr, "CreateWindowExA succeeds for cross-thread stress test window");
    if (!hwnd) return;

    g_mmTestHwnd = hwnd;
    g_mmTickCount.store(0);

    const int kTargetTicks = 300;
    const UINT delayMs = 2; // fast enough to produce real contention with the drain loop below

    MMRESULT id = timeSetEvent(delayMs, 0, MmTimerCallback, 0, TIME_PERIODIC);
    Check(id != 0, "timeSetEvent returns a non-zero timer id for the stress test");

    std::set<WPARAM> received;
    bool duplicate = false;
    const Uint64 deadline = SDL_GetTicks() + 5000; // hard safety timeout
    while (static_cast<int>(received.size()) < kTargetTicks && SDL_GetTicks() < deadline) {
        MSG msg{};
        if (PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE)) {
            if (msg.message == kMmTickMsg) {
                if (!received.insert(msg.wParam).second) {
                    duplicate = true;
                }
            }
        }
        // Intentionally no sleep here on the fast path: PeekMessageA must
        // never block, and this loop mimics a game's tight main-loop poll
        // racing the background timer thread's posts as hard as possible.
    }

    timeKillEvent(static_cast<UINT>(id));
    g_mmTestHwnd = nullptr;

    // Drain anything still queued after killing the timer.
    MSG msg{};
    while (PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE)) {
        if (msg.message == kMmTickMsg && !received.insert(msg.wParam).second) {
            duplicate = true;
        }
    }

    Check(static_cast<int>(received.size()) >= kTargetTicks,
          "cross-thread PostMessageA stress test: all posted messages were received (none lost)");
    Check(!duplicate, "cross-thread PostMessageA stress test: no message was delivered more than once");

    DestroyWindow(hwnd);
}

// Drain the queue via GetMessageA until it returns FALSE (WM_QUIT reached)
// or `limit` messages have been dispatched, whichever comes first.
static bool DrainUntilQuit(int limit = 200)
{
    MSG msg{};
    while (limit-- > 0) {
        BOOL result = GetMessageA(&msg, nullptr, 0, 0);
        if (result == FALSE) return true;
        DispatchMessageA(&msg);
    }
    return false;
}

// TASK-0035: verify the exact WM_DESTROY -> kill-timer -> PostQuitMessage
// sequence both games' own WndProc runs (planetblupi blupi.cpp:561-565:
// KillTimer+PostQuitMessage; free-eggbert blupi.cpp:626-633:
// timeKillEvent+PostQuitMessage), for both timer mechanisms. A regression
// here would either leak a running timer past window teardown or hang the
// shutdown sequence (GetMessageA never returning FALSE).
static bool g_wmDestroySetTimerKilled = false;

static LRESULT WINAPI WmDestroySetTimerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_DESTROY) {
        KillTimer(hwnd, 1);
        g_wmDestroySetTimerKilled = true;
        PostQuitMessage(0);
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static void TestWmDestroyKillsSetTimerThenPostsQuit()
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = WmDestroySetTimerWndProc;
    wc.lpszClassName = "RegTest_WmDestroySetTimer";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "RegTest_WmDestroySetTimer", "Test",
                                 WS_POPUPWINDOW | WS_VISIBLE, 0, 0, 320, 240,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    Check(hwnd != nullptr, "CreateWindowExA succeeds for WM_DESTROY+SetTimer sequence test");
    if (!hwnd) return;

    const UINT intervalMs = 20;
    SetTimer(hwnd, 1, intervalMs, nullptr);
    CountWmTimerFor(hwnd, 1, intervalMs * 5); // let it tick at least once, drain

    g_wmDestroySetTimerKilled = false;
    BOOL destroyed = DestroyWindow(hwnd);
    Check(destroyed == TRUE, "DestroyWindow returns TRUE for the WM_DESTROY+SetTimer sequence test");
    Check(g_wmDestroySetTimerKilled,
          "WM_DESTROY's handler (KillTimer then PostQuitMessage) runs synchronously during DestroyWindow");

    // Note: deliberately not re-polling for "no further WM_TIMER" here via
    // PeekMessageA -- this implementation's PeekMessageA ignores the hwnd
    // filter entirely (see src/winuser_message.cpp), so any such poll would
    // also silently consume the WM_QUIT we're about to check for below.
    // "KillTimer actually stops delivery" is already covered generically by
    // TestKillTimerStopsWmTimerDelivery (TASK-0042).
    Check(DrainUntilQuit(), "GetMessageA returns FALSE once the WM_DESTROY-posted WM_QUIT is reached");
}

static MMRESULT g_wmDestroyMmTimerId = 0;

static LRESULT WINAPI WmDestroyTimeSetEventWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_DESTROY) {
        timeKillEvent(static_cast<UINT>(g_wmDestroyMmTimerId));
        PostQuitMessage(0);
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static void TestWmDestroyKillsTimeSetEventThenPostsQuit()
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = WmDestroyTimeSetEventWndProc;
    wc.lpszClassName = "RegTest_WmDestroyTimeSetEvent";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "RegTest_WmDestroyTimeSetEvent", "Test",
                                 WS_POPUPWINDOW | WS_VISIBLE, 0, 0, 320, 240,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    Check(hwnd != nullptr, "CreateWindowExA succeeds for WM_DESTROY+timeSetEvent sequence test");
    if (!hwnd) return;

    g_mmTestHwnd = hwnd;
    g_mmTickCount.store(0);

    const UINT delayMs = 10;
    g_wmDestroyMmTimerId = timeSetEvent(delayMs, delayMs / 4, MmTimerCallback, 0, TIME_PERIODIC);
    Check(g_wmDestroyMmTimerId != 0, "timeSetEvent returns a non-zero id for the WM_DESTROY sequence test");

    SDL_Delay(delayMs * 5); // let it tick a few times

    BOOL destroyed = DestroyWindow(hwnd);
    Check(destroyed == TRUE, "DestroyWindow returns TRUE for the WM_DESTROY+timeSetEvent sequence test");

    const int countAtDestroy = g_mmTickCount.load();
    SDL_Delay(delayMs * 10); // long enough that more ticks would occur if not truly killed
    const int countAfterWait = g_mmTickCount.load();
    Check(countAfterWait == countAtDestroy,
          "WM_DESTROY's handler (timeKillEvent then PostQuitMessage) stops the timer synchronously during DestroyWindow");

    g_mmTestHwnd = nullptr;
    Check(DrainUntilQuit(), "GetMessageA returns FALSE once the WM_DESTROY-posted WM_QUIT is reached (timeSetEvent path)");
}

int main()
{
    printf("[timer-regressions] Starting\n");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("[timer-regressions] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    TestSetTimerFiresWmTimerAtApproximateInterval();
    TestKillTimerStopsWmTimerDelivery();
    TestSetTimerAutoIdAndMinimumIntervalClamp();
    TestFastTimerDoesNotStarveConcurrentNonTimerMessages();
    TestTimeSetEventFiresCallbackAtApproximateInterval();
    TestTimeKillEventStopsCallback();
    TestCrossThreadPostMessageAStressTest();
    TestWmDestroyKillsSetTimerThenPostsQuit();
    TestWmDestroyKillsTimeSetEventThenPostsQuit();

    SDL_Quit();

    if (g_failures > 0) {
        printf("[timer-regressions] %d FAILURE(S)\n", g_failures);
        return 1;
    }

    printf("[timer-regressions] ALL TESTS PASSED\n");
    return 0;
}
