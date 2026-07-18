/**
 * @file test_winuser_regressions.cpp
 * @brief Focused P0 regression tests for WinUser behavior both target games
 * (../free-eggbert, ../planetblupi) depend on. See plan.md sections 3/6/7.
 *
 * Covers:
 *  - AdjustWindowRect preserves the requested client-area size for the
 *    window styles both games use.
 *  - DefWindowProcA handles WM_CLOSE by destroying the window and causing
 *    the normal GetMessageA-returns-FALSE quit path.
 *  - DestroyWindow synchronously dispatches WM_DESTROY to the window's own
 *    WndProc before tearing the window down (both target games rely on this
 *    to run their own WM_DESTROY handler -- killing their frame-pump timer
 *    and other cleanup -- before quitting).
 *  - PeekMessageA(PM_NOREMOVE) does not remove the message from the queue.
 *  - PeekMessageA(PM_REMOVE) removes the message from the queue.
 *  - GetMessageA returns FALSE (0) on WM_QUIT.
 *  - WM_MOUSEMOVE packs x/y into lParam as LOWORD=x, HIWORD=y.
 *  - WM_MOUSEMOVE's wParam carries MK_LBUTTON while the left button is held
 *    during a drag (required by planetblupi's CEvent::TreatEventBase, e.g.
 *    "fwKeys & MK_LBUTTON" checks driven directly by wParam).
 *  - ShowCursor(FALSE)/ShowCursor(TRUE) actually hide/show the real OS
 *    cursor via SDL and return the resulting WinAPI-style display counter
 *    (both target games hide the OS cursor while drawing their own sprite).
 *  - SetCursor stores and returns the previously-active cursor handle.
 *
 * Note on MK_SHIFT/MK_CONTROL: FreeApiMessageQueue.cpp's WM_MOUSEMOVE
 * translation now also ORs in MK_SHIFT/MK_CONTROL from SDL_GetKeyboardState()
 * (previously only button-down/up messages did this; planetblupi's
 * CEvent::PlayMove reads wParam&MK_SHIFT on every WM_MOUSEMOVE, not just on
 * press). This is not covered by an automated test here because
 * SDL_PushEvent-injected key events do not update SDL_GetKeyboardState()'s
 * array (verified empirically -- only real backend-driven input does), so a
 * synthetic "Shift held" test would be unable to distinguish a working fix
 * from a broken one in this headless environment. MK_LBUTTON is used below
 * instead because free-api tracks button state itself (not via
 * SDL_GetKeyboardState), so it is reliably testable via SDL_PushEvent.
 *
 * TASK-24H-0404/0405: the MK_SHIFT/MK_CONTROL OR-logic itself is now
 * factored out into FreeApi::Internal::ApplyKeyboardModifierFlags(), a
 * pure function over an SDL keyboard-state array. That closes the
 * automated-coverage gap for the *mechanism* with a synthetic keystate
 * array below, even though the live end-to-end SDL_GetKeyboardState() path
 * still can't be exercised via SDL_PushEvent in this headless environment
 * (see the note above) -- the end-to-end gameplay feature still needs a
 * human playtest (plan.md TASK-24H-0401/0403).
 */
#include <windows.h>
#include <windowsx.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <thread>

#if !defined(_WIN32)
#include <unistd.h>
#endif

// Internal, non-public helper (src/internal/FreeApiMessageQueue.cpp) --
// forward-declared here purely so this test can exercise its OR-logic
// directly with a synthetic keystate array, matching the established
// pattern of test-local forward declarations into internal state already
// used by tests/test_gdi_regressions.cpp for the diagnostic counters.
namespace FreeApi::Internal {
WPARAM ApplyKeyboardModifierFlags(const bool* keys, WPARAM base);
// TASK-24H-1253: same forward-declaration pattern, for GetActiveWindow's
// "no focus window set yet" fallback branch (src/internal/
// FreeApiWindowRegistry.cpp) -- every keyboard event and any mouse event
// with an unresolved SDL windowID routes through this function.
HWND GetActiveWindow();
extern HWND g_focusWindow;
}

static int g_failures = 0;

static void Check(bool condition, const char* what)
{
    if (condition) {
        printf("[winuser-regressions] PASS: %s\n", what);
    } else {
        printf("[winuser-regressions] FAIL: %s\n", what);
        ++g_failures;
    }
}

static LRESULT WINAPI RegTestWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// Drain up to `limit` queued messages via PeekMessageA(PM_REMOVE), dispatching each.
static void DrainMessages(int limit = 200)
{
    MSG msg{};
    while (limit-- > 0) {
        if (!PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) break;
        if (msg.message == WM_QUIT) break;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

static void TestAdjustWindowRectPreservesClientSize()
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = RegTestWndProc;
    wc.lpszClassName = "RegTest_AdjustRect";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    // Matches both games' windowed-mode style (WS_POPUPWINDOW|WS_CAPTION).
    const DWORD style = WS_POPUPWINDOW | WS_CAPTION | WS_VISIBLE;

    RECT rc = {0, 0, 640, 480};
    BOOL ok = AdjustWindowRect(&rc, style, FALSE);
    Check(ok == TRUE, "AdjustWindowRect returns TRUE for a non-null rect");

    const int width  = rc.right - rc.left;
    const int height = rc.bottom - rc.top;

    HWND hwnd = CreateWindowA("RegTest_AdjustRect", "Test", style,
                              100, 100, width, height,
                              HWND_DESKTOP, nullptr, (HINSTANCE)1, nullptr);
    Check(hwnd != nullptr, "CreateWindowA succeeds using AdjustWindowRect's resulting size");
    if (!hwnd) return;

    DrainMessages();

    RECT client{};
    BOOL gotClient = GetClientRect(hwnd, &client);
    Check(gotClient == TRUE, "GetClientRect succeeds after CreateWindowA");

    const int clientWidth  = client.right - client.left;
    const int clientHeight = client.bottom - client.top;
    Check(clientWidth == 640 && clientHeight == 480,
          "AdjustWindowRect preserves the requested 640x480 client-area size");

    DestroyWindow(hwnd);
    DrainMessages();
}

// TASK-0027 (plan.md): both games populate the full WNDCLASSA field set
// (free-eggbert blupi.cpp:707-728, planetblupi blupi.cpp:593,609-619) but
// the other tests in this file only set 3 of the 10 fields. This locks in
// that RegisterClassA/CreateWindowA handle every field without crashing or
// silently dropping behavior.
static void TestRegisterClassAWithFullFieldSet()
{
    WNDCLASSA wc{};
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = RegTestWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = (HINSTANCE)1;
    wc.hIcon         = LoadIconA((HINSTANCE)1, "IDR_MAINFRAME");
    wc.hCursor       = LoadCursorA((HINSTANCE)1, "IDC_POINTER");
    wc.hbrBackground = GetStockBrush(BLACK_BRUSH);
    wc.lpszMenuName  = "RegTest_FullFieldSet_Menu";
    wc.lpszClassName = "RegTest_FullFieldSet";

    ATOM registered = RegisterClassA(&wc);
    Check(registered != 0, "RegisterClassA succeeds with every WNDCLASSA field populated");

    HWND hwnd = CreateWindowA("RegTest_FullFieldSet", "Test",
                              WS_POPUPWINDOW | WS_VISIBLE,
                              0, 0, 320, 240,
                              HWND_DESKTOP, nullptr, (HINSTANCE)1, nullptr);
    Check(hwnd != nullptr, "CreateWindowA succeeds against a class registered with the full field set");

    if (hwnd) {
        DrainMessages();
        DestroyWindow(hwnd);
        DrainMessages();
    }
}

// TASK-0028 (plan.md): both games' fullscreen path calls CreateWindowExA
// with WS_EX_TOPMOST/WS_POPUP and a size taken directly from
// GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN) (free-eggbert blupi.cpp:733-746,
// planetblupi blupi.cpp:625-638). This locks in that exact call shape.
static void TestCreateWindowExAFullscreenPath()
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = RegTestWndProc;
    wc.lpszClassName = "RegTest_Fullscreen";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    const int screenWidth  = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    Check(screenWidth > 0 && screenHeight > 0,
          "GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN) returns sane positive screen dimensions");

    HWND hwnd = CreateWindowExA(WS_EX_TOPMOST, "RegTest_Fullscreen", "Test",
                                 WS_POPUP,
                                 0, 0, screenWidth, screenHeight,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    Check(hwnd != nullptr, "CreateWindowExA succeeds for the exact fullscreen call shape (WS_EX_TOPMOST/WS_POPUP)");
    if (!hwnd) return;

    DrainMessages();

    RECT client{};
    BOOL gotClient = GetClientRect(hwnd, &client);
    Check(gotClient == TRUE, "GetClientRect succeeds for the fullscreen window");
    Check(client.right - client.left == screenWidth && client.bottom - client.top == screenHeight,
          "Fullscreen window's client size matches the requested screen dimensions");

    DestroyWindow(hwnd);
    DrainMessages();
}

// TASK-24H-0316: CreateWindowExA deliberately never sets SDL_WINDOW_RESIZABLE
// -- on some Wayland/X11 compositors a resizable popup window immediately
// receives a spurious WM_CLOSE from the compositor. This is a load-bearing
// decision (src/winuser_window.cpp) that previously had zero test coverage;
// silently reintroducing SDL_WINDOW_RESIZABLE would reintroduce a
// startup-killing bug on affected compositors without any test failing.
static void TestCreateWindowExANeverSetsResizableFlag()
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = RegTestWndProc;
    wc.lpszClassName = "RegTest_NonResizable";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "RegTest_NonResizable", "Test",
                                 WS_POPUPWINDOW | WS_CAPTION,
                                 0, 0, 640, 480,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    Check(hwnd != nullptr, "CreateWindowExA succeeds for the window creation call shape used by this test");
    if (!hwnd) return;

    DrainMessages();

    auto* sdlWin = reinterpret_cast<SDL_Window*>(hwnd);
    SDL_WindowFlags flags = SDL_GetWindowFlags(sdlWin);
    Check((flags & SDL_WINDOW_RESIZABLE) == 0,
          "CreateWindowExA never sets SDL_WINDOW_RESIZABLE (compositor WM_CLOSE workaround, see docs/out-of-scope.md)");

    DestroyWindow(hwnd);
    DrainMessages();
}

// TASK-24H-0308: GetSystemMetrics only ever implements three real indices
// (SM_CXSCREEN/SM_CYSCREEN/SM_CYCAPTION); everything else falls through to
// a hardcoded 0 return. SM_CYCAPTION itself previously had zero direct
// assertions anywhere in the suite, and the fallback branch was untested.
static void TestGetSystemMetricsCyCaptionAndUnqueriedIndexFallback()
{
    Check(GetSystemMetrics(SM_CYCAPTION) == 24,
          "GetSystemMetrics(SM_CYCAPTION) returns the documented fixed value 24");

    // 12345 is not SM_CXSCREEN/SM_CYSCREEN/SM_CYCAPTION and not a real
    // Win32 SM_* value free-api implements -- exercises the "any other
    // index" fallback branch (src/winuser_misc.cpp).
    Check(GetSystemMetrics(12345) == 0,
          "GetSystemMetrics returns 0 for an index none of the three implemented constants match");
}

// TASK-24H-1252: GlobalMemoryStatus (src/winbase.cpp) is a real, live,
// documented call site (docs/supported-apis.md: "Required by free-eggbert:
// Yes") -- free-eggbert calls it twice, once at startup (blupi.cpp:234) to
// gate g_bTrueColorBack/g_bTrueColorDecor on `mem.dwTotalPhys < 32000000`,
// and once for a diagnostic time.blp log (blupi.cpp:693). A coverage sweep
// (2026-07-09) found this documented-as-live function had zero test
// coverage anywhere in the suite. Locks in both the null-safety contract
// and the exact hardcoded dwTotalPhys value the real TrueColor gate
// depends on.
static void TestGlobalMemoryStatusPopulatesPlausibleValues()
{
    GlobalMemoryStatus(nullptr);
    Check(true, "GlobalMemoryStatus(nullptr) does not crash");

    MEMORYSTATUS mem{};
    mem.dwLength = sizeof(MEMORYSTATUS);
    GlobalMemoryStatus(&mem);

    Check(mem.dwLength == sizeof(MEMORYSTATUS), "GlobalMemoryStatus sets dwLength to sizeof(MEMORYSTATUS)");
    Check(mem.dwTotalPhys == 512u * 1024u * 1024u,
          "GlobalMemoryStatus reports the documented fixed dwTotalPhys (512MB)");
    Check(mem.dwTotalPhys >= 32000000u,
          "dwTotalPhys clears free-eggbert's real 32,000,000-byte TrueColor benchmark threshold "
          "(blupi.cpp:236) -- the exact real gameplay effect this function's value controls");
    Check(mem.dwAvailPhys > 0 && mem.dwTotalPageFile > 0 && mem.dwAvailPageFile > 0 &&
          mem.dwTotalVirtual > 0 && mem.dwAvailVirtual > 0,
          "GlobalMemoryStatus populates all remaining MEMORYSTATUS fields with plausible non-zero values");
}

// TASK-24H-1253: GetActiveWindow (src/internal/FreeApiWindowRegistry.cpp)
// falls back to the first registered window when g_focusWindow hasn't been
// set yet -- reached in the real window between CreateWindowExA and the
// first SDL_EVENT_WINDOW_FOCUS_GAINED-triggered focus assignment, since
// every WM_KEYDOWN/WM_KEYUP and any mouse event with an unresolved SDL
// windowID route through this function (src/internal/
// FreeApiMessageQueue.cpp). A coverage sweep (2026-07-09) found this
// fallback branch had zero test coverage. Does not assert exact HWND
// equality -- other tests earlier in this same binary may leave additional
// windows registered depending on execution order and cleanup, so only the
// real, meaningful contract is checked: the fallback returns a live,
// non-null window rather than silently returning NULL (which would drop
// the keyboard/mouse event) whenever at least one window is registered.
static void TestGetActiveWindowFallsBackBeforeFocusIsSet()
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = RegTestWndProc;
    wc.lpszClassName = "RegTest_ActiveWindowFallback";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA("RegTest_ActiveWindowFallback", "Test", WS_POPUPWINDOW | WS_VISIBLE,
                              0, 0, 320, 240, HWND_DESKTOP, nullptr, (HINSTANCE)1, nullptr);
    Check(hwnd != nullptr, "CreateWindowA succeeds for the GetActiveWindow-fallback test");
    if (!hwnd) return;

    // Force the "no focus window set yet" state this test targets -- do
    // not drain messages first, which could process a real SDL focus
    // event and mask the fallback path this test specifically exercises.
    FreeApi::Internal::g_focusWindow = nullptr;

    HWND active = FreeApi::Internal::GetActiveWindow();
    Check(active != nullptr,
          "GetActiveWindow returns a live registered window (not NULL) when g_focusWindow is not yet set");

    DrainMessages();
    DestroyWindow(hwnd);
}

// TASK-24H-0210: the original version of this test only observed the
// downstream WM_QUIT, a correct but indirect proxy -- a regression that
// broke the WM_CLOSE->DestroyWindow call specifically (while some other
// path still happened to produce WM_QUIT) would not have been caught.
// This dedicated WndProc lets the test assert WM_DESTROY was directly
// delivered to the closed window's own procedure as a consequence of
// WM_CLOSE processing.
static bool g_wmCloseDestroyReceived = false;
static HWND g_wmCloseDestroyReceivedHwnd = nullptr;

static LRESULT WINAPI WmCloseTrackingWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_DESTROY) {
        g_wmCloseDestroyReceived = true;
        g_wmCloseDestroyReceivedHwnd = hwnd;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static void TestDefWindowProcHandlesWmClose()
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = WmCloseTrackingWndProc;
    wc.lpszClassName = "RegTest_WmClose";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "RegTest_WmClose", "Test",
                                 WS_POPUPWINDOW | WS_VISIBLE,
                                 0, 0, 320, 240,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    Check(hwnd != nullptr, "CreateWindowExA succeeds for WM_CLOSE test window");
    if (!hwnd) return;

    // Drain startup messages so they don't interfere with the assertions below.
    DrainMessages();

    g_wmCloseDestroyReceived = false;
    g_wmCloseDestroyReceivedHwnd = nullptr;

    // Both target games post WM_CLOSE and never handle it explicitly --
    // they rely entirely on the default window procedure to destroy the
    // window and quit.
    PostMessageA(hwnd, WM_CLOSE, 0, 0);

    bool sawQuit = false;
    MSG msg{};
    for (int i = 0; i < 200 && !sawQuit; ++i) {
        if (!PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) break;
        if (msg.message == WM_QUIT) {
            sawQuit = true;
            break;
        }
        DispatchMessageA(&msg);
    }

    Check(sawQuit, "WM_CLOSE's default handling leads to WM_QUIT via DefWindowProcA");
    Check(g_wmCloseDestroyReceived,
          "WM_CLOSE's default handling directly delivers WM_DESTROY to the window's own WndProc "
          "(not just an eventual WM_QUIT via some other path)");
    Check(g_wmCloseDestroyReceivedHwnd == hwnd, "WM_DESTROY from WM_CLOSE handling carries the correct HWND");
}

static bool g_destroyReceived = false;
static HWND g_destroyReceivedHwnd = nullptr;

static LRESULT WINAPI DestroyTrackingWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_DESTROY) {
        g_destroyReceived = true;
        g_destroyReceivedHwnd = hwnd;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static void TestDestroyWindowDispatchesWmDestroySynchronously()
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = DestroyTrackingWndProc;
    wc.lpszClassName = "RegTest_DestroyDispatch";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "RegTest_DestroyDispatch", "Test",
                                 WS_POPUPWINDOW | WS_VISIBLE,
                                 0, 0, 320, 240,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    Check(hwnd != nullptr, "CreateWindowExA succeeds for DestroyWindow dispatch test");
    if (!hwnd) return;

    DrainMessages();

    g_destroyReceived = false;
    g_destroyReceivedHwnd = nullptr;

    // Both target games rely on DestroyWindow synchronously delivering
    // WM_DESTROY to their own WndProc (to kill their frame-pump timer via
    // KillTimer/timeKillEvent and run cleanup) before the window is actually
    // torn down -- previously this never happened, so that cleanup code
    // never ran on a normal WM_CLOSE-driven quit.
    BOOL destroyed = DestroyWindow(hwnd);
    Check(destroyed == TRUE, "DestroyWindow returns TRUE");
    Check(g_destroyReceived,
          "DestroyWindow synchronously dispatches WM_DESTROY to the window's own WndProc");
    Check(g_destroyReceivedHwnd == hwnd, "WM_DESTROY is dispatched with the correct HWND");

    DrainMessages();
}

// DestroyWindow previously had no guard against being called twice on the
// same (now-stale, but still non-null) hwnd -- found by this session's
// memory-safety audit. A real, ordinary play sequence can trigger exactly
// this: planetblupi's WM_PHASE_BYE quit-confirmation screen posts WM_CLOSE
// from more than one place without changing phase first (event.cpp), so a
// single VK_ESCAPE keydown there posts WM_CLOSE twice for the same hwnd,
// and WM_CLOSE is not coalesced (only WM_MOUSEMOVE/WM_TIMER are). The first
// DestroyWindow call really tears the window down; the second, stale call
// used to fall through to SDL_GetWindowID/SDL_DestroyWindow on an
// already-freed SDL_Window -- masked only by SDL3's own object-validity
// registry, not by anything free-api did. Verifies the second call is now
// rejected before touching SDL at all.
static void TestDestroyWindowRejectsSecondCallOnSameHwnd()
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = DestroyTrackingWndProc;
    wc.lpszClassName = "RegTest_DoubleDestroy";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "RegTest_DoubleDestroy", "Test",
                                 WS_POPUPWINDOW | WS_VISIBLE,
                                 0, 0, 320, 240,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    Check(hwnd != nullptr, "CreateWindowExA succeeds for the double-destroy test");
    if (!hwnd) return;

    DrainMessages();

    g_destroyReceived = false;
    BOOL firstDestroy = DestroyWindow(hwnd);
    Check(firstDestroy == TRUE, "first DestroyWindow call succeeds and tears the window down");
    Check(g_destroyReceived, "first DestroyWindow call dispatches WM_DESTROY");

    g_destroyReceived = false;
    BOOL secondDestroy = DestroyWindow(hwnd);
    Check(secondDestroy == FALSE,
          "second DestroyWindow call on the same (now-stale) hwnd is rejected, not a double-destroy");
    Check(!g_destroyReceived,
          "second DestroyWindow call does not re-dispatch WM_DESTROY (the window procedure is already gone)");
}

// TASK-24H-0301: positive proof that only lpfnWndProc is retained by
// RegisterClassA -- re-registers the SAME class name with a full,
// differently-populated WNDCLASSA (different hIcon/hCursor/hbrBackground/
// style/lpszMenuName, and critically a DIFFERENT lpfnWndProc) and confirms
// the second registration's WndProc is the one that actually runs. If any
// non-WNDPROC field were retained/consulted, or if the first registration's
// WndProc somehow lingered, this would either fail to compile (the
// registry's value type is literally WNDPROC, see
// src/internal/FreeApiWindowRegistry.hpp) or DestroyTrackingWndProc's
// WM_DESTROY flag would never be set.
static void TestRegisterClassADiscardsNonWndprocFields()
{
    const char* kClassName = "RegTest_ReRegister";

    WNDCLASSA first{};
    first.style         = CS_HREDRAW | CS_VREDRAW;
    first.lpfnWndProc    = RegTestWndProc;
    first.hInstance      = (HINSTANCE)1;
    first.hIcon          = LoadIconA((HINSTANCE)1, "IDR_MAINFRAME");
    first.hCursor        = LoadCursorA((HINSTANCE)1, "IDC_ARROW");
    first.hbrBackground  = GetStockBrush(BLACK_BRUSH);
    first.lpszMenuName   = "RegTest_ReRegister_MenuA";
    first.lpszClassName  = kClassName;
    Check(RegisterClassA(&first) != 0, "first RegisterClassA call for the re-register test succeeds");

    g_destroyReceived = false;

    WNDCLASSA second{};
    second.style         = 0;
    second.lpfnWndProc    = DestroyTrackingWndProc; // deliberately different from `first`
    second.hInstance      = (HINSTANCE)1;
    second.hIcon          = nullptr;
    second.hCursor        = nullptr;
    second.hbrBackground  = nullptr;
    second.lpszMenuName   = "RegTest_ReRegister_MenuB";
    second.lpszClassName  = kClassName; // same class name as `first`
    Check(RegisterClassA(&second) != 0,
          "re-registering the same class name with an entirely different (and sparser) field set still succeeds");

    HWND hwnd = CreateWindowA(kClassName, "Test",
                               WS_POPUPWINDOW | WS_VISIBLE,
                               0, 0, 320, 240,
                               HWND_DESKTOP, nullptr, (HINSTANCE)1, nullptr);
    Check(hwnd != nullptr, "CreateWindowA succeeds against the re-registered class");

    if (hwnd) {
        DrainMessages();
        DestroyWindow(hwnd);
        Check(g_destroyReceived,
              "the SECOND registration's lpfnWndProc (not the first's) handles the window -- "
              "proves only lpfnWndProc is retained/updated by RegisterClassA, nothing else lingers or matters");
        DrainMessages();
    }
}

// TASK-24H-0209: DispatchMessageA's null-hwnd single-window fallback
// (src/winuser_message.cpp:191-197) is real, reachable code -- WM_CLOSE is
// pushed with whatever FindWindowById returns, which can be NULL -- but had
// no direct test constructing a NULL-hwnd message and verifying it's
// delivered to the sole registered window's own WndProc rather than falling
// through to the generic DefWindowProcA(NULL, ...) path.
static bool g_nullHwndFallbackReceived = false;
static HWND g_nullHwndFallbackReceivedHwnd = nullptr;

static LRESULT WINAPI NullHwndFallbackTrackingWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_USER + 77) {
        g_nullHwndFallbackReceived = true;
        g_nullHwndFallbackReceivedHwnd = hwnd;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static void TestDispatchMessageARoutesNullHwndToSoleRegisteredWindow()
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = NullHwndFallbackTrackingWndProc;
    wc.lpszClassName = "RegTest_NullHwndFallback";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "RegTest_NullHwndFallback", "Test",
                                 WS_POPUPWINDOW | WS_VISIBLE, 0, 0, 320, 240,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    Check(hwnd != nullptr, "CreateWindowExA succeeds for the null-hwnd fallback test window");
    if (!hwnd) return;

    DrainMessages();
    g_nullHwndFallbackReceived = false;
    g_nullHwndFallbackReceivedHwnd = nullptr;

    Check(PostMessageA(nullptr, WM_USER + 77, 0xABCD, 0) == TRUE,
          "PostMessageA(NULL hwnd, ...) succeeds (matches real WM_CLOSE-with-unresolved-hwnd shape)");

    DrainMessages();

    Check(g_nullHwndFallbackReceived,
          "DispatchMessageA's null-hwnd fallback delivers to the sole registered window's own WndProc "
          "(not silently dropped or misrouted to DefWindowProcA(NULL, ...))");
    Check(g_nullHwndFallbackReceivedHwnd == hwnd,
          "the fallback passes the real window's HWND to the WndProc, not NULL");

    DestroyWindow(hwnd);
    DrainMessages();
}

static void TestPeekMessageNoRemoveAndRemove()
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = RegTestWndProc;
    wc.lpszClassName = "RegTest_Peek";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "RegTest_Peek", "Test",
                                 WS_POPUPWINDOW | WS_VISIBLE,
                                 0, 0, 320, 240,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    Check(hwnd != nullptr, "CreateWindowExA succeeds for PeekMessage test window");
    if (!hwnd) return;

    DrainMessages();

    const UINT kCustomMsg = WM_USER + 1;
    PostMessageA(hwnd, kCustomMsg, 42, 0);

    MSG msg{};
    BOOL got1 = PeekMessageA(&msg, hwnd, 0, 0, PM_NOREMOVE);
    Check(got1 == TRUE && msg.message == kCustomMsg,
          "PeekMessageA(PM_NOREMOVE) finds the posted message");

    MSG msg2{};
    BOOL got2 = PeekMessageA(&msg2, hwnd, 0, 0, PM_NOREMOVE);
    Check(got2 == TRUE && msg2.message == kCustomMsg,
          "PeekMessageA(PM_NOREMOVE) does not remove the message (still found on 2nd call)");

    MSG msg3{};
    BOOL got3 = PeekMessageA(&msg3, hwnd, 0, 0, PM_REMOVE);
    Check(got3 == TRUE && msg3.message == kCustomMsg,
          "PeekMessageA(PM_REMOVE) finds and removes the message");

    MSG msg4{};
    BOOL got4 = PeekMessageA(&msg4, hwnd, kCustomMsg, kCustomMsg, PM_NOREMOVE);
    Check(got4 == FALSE, "PeekMessageA(PM_REMOVE) actually removed the message (not found afterward)");

    DestroyWindow(hwnd);
    DrainMessages();
}

// TASK-24H-0202: positive-assertion companion to TestPeekMessageNoRemoveAndRemove.
// That test only ever passes filter args that either match the posted
// message or hit an already-empty queue -- it never proves PeekMessageA
// ignores a filter that, under REAL Win32 semantics, would exclude the
// posted message. This test posts a message with one ID, then peeks with a
// deliberately mismatched hWnd and a wMsgFilterMin/wMsgFilterMax range that
// excludes that message ID, and asserts it's still returned -- a genuine
// regression guard for the documented TASK-24H-0201 decision (see
// src/winuser_message.cpp and include/winuser.h's PeekMessageA doc comments).
static void TestPeekMessageAIgnoresNonZeroFilterArguments()
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = RegTestWndProc;
    wc.lpszClassName = "RegTest_PeekFilterIgnored";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "RegTest_PeekFilterIgnored", "Test",
                                 WS_POPUPWINDOW | WS_VISIBLE,
                                 0, 0, 320, 240,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    Check(hwnd != nullptr, "CreateWindowExA succeeds for PeekMessageA filter-ignoring test window");
    if (!hwnd) return;

    DrainMessages();

    const UINT kPostedMsg = WM_USER + 5;
    PostMessageA(hwnd, kPostedMsg, 7, 0);

    // A real Win32 PeekMessageA with this hWnd/filter combination would
    // find nothing: a different, never-created HWND, and a filter range
    // that excludes kPostedMsg entirely.
    HWND mismatchedHwnd = reinterpret_cast<HWND>(0x1234);
    MSG msg{};
    BOOL got = PeekMessageA(&msg, mismatchedHwnd, kPostedMsg + 100, kPostedMsg + 200, PM_REMOVE);
    Check(got == TRUE && msg.message == kPostedMsg && msg.wParam == 7,
          "PeekMessageA ignores hWnd/wMsgFilterMin/wMsgFilterMax and still returns a message a real Win32 filter would have excluded");

    DestroyWindow(hwnd);
    DrainMessages();
}

// TASK-24H-0203: src/winuser_message.cpp documents, in a code comment, that
// PeekMessageA "must NEVER sleep or block" -- a prior regression that added
// a per-call SDL_Delay(1) produced a real, player-visible "rubbery/laggy
// feel" in both games' tight main loops. This is a real historical
// regression class with no dedicated timing regression guard until now:
// a tight empty-queue polling loop must complete near-instantly, not take
// ~1ms per call.
static void TestPeekMessageANeverSleepsOnEmptyQueue()
{
    MSG msg{};
    const int kIterations = 20000;
    const Uint64 start = SDL_GetTicks();
    for (int i = 0; i < kIterations; ++i) {
        PeekMessageA(&msg, nullptr, 0, 0, PM_NOREMOVE);
    }
    const Uint64 elapsedMs = SDL_GetTicks() - start;

    // A regression that reintroduces SDL_Delay(1) per call would take at
    // least ~20000ms for 20000 iterations; a correct zero-sleep
    // implementation completes in a handful of milliseconds. Use a generous
    // threshold well below the "no-sleep" ceiling to avoid flakiness while
    // still catching any reintroduced per-call sleep.
    Check(elapsedMs < 2000,
          "PeekMessageA never sleeps on an empty queue (20000 calls completed well under "
          "what a reintroduced per-call SDL_Delay(1) would take)");
}

static void TestGetMessageReturnsFalseOnQuit()
{
    // Post WM_QUIT directly so it is already queued -- GetMessageA must not
    // block, and must return FALSE (0) for it, per both games' shutdown
    // contract (planetblupi's WinMain checks only GetMessageA's return
    // value, never msg.message == WM_QUIT explicitly).
    PostQuitMessage(0);

    MSG msg{};
    BOOL result = GetMessageA(&msg, nullptr, 0, 0);
    Check(result == FALSE, "GetMessageA returns FALSE (0) when WM_QUIT is queued");
}

// TASK-24H-0405: synthetic-keystate unit test for the extracted
// ApplyKeyboardModifierFlags() helper (TASK-24H-0404). Directly exercises
// the OR-logic with a hand-built keystate array covering every combination
// of Shift/Ctrl held, since SDL_PushEvent-injected key events don't update
// the real SDL_GetKeyboardState() array in this headless environment (see
// the file-level doc comment above).
static void TestApplyKeyboardModifierFlagsHelperWithSyntheticKeystate()
{
    bool keys[SDL_SCANCODE_COUNT] = {};

    // Baseline: no modifiers held, base bits (e.g. a button-down flag) pass through untouched.
    WPARAM none = FreeApi::Internal::ApplyKeyboardModifierFlags(keys, MK_LBUTTON);
    Check((none & MK_SHIFT) == 0 && (none & MK_CONTROL) == 0 && (none & MK_LBUTTON) != 0,
          "ApplyKeyboardModifierFlags: no modifiers held -> no MK_SHIFT/MK_CONTROL, base bits preserved");

    // Left Shift only.
    keys[SDL_SCANCODE_LSHIFT] = true;
    WPARAM leftShift = FreeApi::Internal::ApplyKeyboardModifierFlags(keys, 0);
    Check((leftShift & MK_SHIFT) != 0 && (leftShift & MK_CONTROL) == 0,
          "ApplyKeyboardModifierFlags: SDL_SCANCODE_LSHIFT alone sets MK_SHIFT only");
    keys[SDL_SCANCODE_LSHIFT] = false;

    // Right Shift only (both L/R variants must collapse to the same generic MK_SHIFT).
    keys[SDL_SCANCODE_RSHIFT] = true;
    WPARAM rightShift = FreeApi::Internal::ApplyKeyboardModifierFlags(keys, 0);
    Check((rightShift & MK_SHIFT) != 0 && (rightShift & MK_CONTROL) == 0,
          "ApplyKeyboardModifierFlags: SDL_SCANCODE_RSHIFT alone also sets MK_SHIFT");
    keys[SDL_SCANCODE_RSHIFT] = false;

    // Left Ctrl only.
    keys[SDL_SCANCODE_LCTRL] = true;
    WPARAM leftCtrl = FreeApi::Internal::ApplyKeyboardModifierFlags(keys, 0);
    Check((leftCtrl & MK_CONTROL) != 0 && (leftCtrl & MK_SHIFT) == 0,
          "ApplyKeyboardModifierFlags: SDL_SCANCODE_LCTRL alone sets MK_CONTROL only");
    keys[SDL_SCANCODE_LCTRL] = false;

    // Right Ctrl only.
    keys[SDL_SCANCODE_RCTRL] = true;
    WPARAM rightCtrl = FreeApi::Internal::ApplyKeyboardModifierFlags(keys, 0);
    Check((rightCtrl & MK_CONTROL) != 0 && (rightCtrl & MK_SHIFT) == 0,
          "ApplyKeyboardModifierFlags: SDL_SCANCODE_RCTRL alone also sets MK_CONTROL");
    keys[SDL_SCANCODE_RCTRL] = false;

    // Both Shift and Ctrl held together, plus a pre-existing button bit.
    keys[SDL_SCANCODE_LSHIFT] = true;
    keys[SDL_SCANCODE_LCTRL]  = true;
    WPARAM both = FreeApi::Internal::ApplyKeyboardModifierFlags(keys, MK_RBUTTON);
    Check((both & MK_SHIFT) != 0 && (both & MK_CONTROL) != 0 && (both & MK_RBUTTON) != 0,
          "ApplyKeyboardModifierFlags: Shift+Ctrl held together sets both flags and preserves base bits");
    keys[SDL_SCANCODE_LSHIFT] = false;
    keys[SDL_SCANCODE_LCTRL]  = false;

    // A null keys pointer (defensive case) must not crash and must not set any modifier flags.
    WPARAM nullKeys = FreeApi::Internal::ApplyKeyboardModifierFlags(nullptr, MK_LBUTTON);
    Check((nullKeys & MK_SHIFT) == 0 && (nullKeys & MK_CONTROL) == 0 && (nullKeys & MK_LBUTTON) != 0,
          "ApplyKeyboardModifierFlags: a null keys pointer is handled safely (no modifier flags set, no crash)");
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

static void InjectMouseButton(SDL_Window* sdlWin, bool down, Uint8 button, float x, float y)
{
    SDL_Event ev{};
    ev.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    ev.button.windowID = SDL_GetWindowID(sdlWin);
    ev.button.button = button;
    ev.button.x = x;
    ev.button.y = y;
    SDL_PushEvent(&ev);
}

static void TestMouseMoveLParamPackingAndModifierFlags()
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = RegTestWndProc;
    wc.lpszClassName = "RegTest_MouseMove";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "RegTest_MouseMove", "Test",
                                 WS_POPUPWINDOW | WS_VISIBLE,
                                 0, 0, 320, 240,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    Check(hwnd != nullptr, "CreateWindowExA succeeds for mouse-move test window");
    if (!hwnd) return;

    DrainMessages();

    auto* sdlWin = reinterpret_cast<SDL_Window*>(hwnd);

    // Case 1: plain move -- verify LOWORD=x, HIWORD=y packing.
    InjectMouseMotion(sdlWin, 111.0f, 222.0f);

    bool sawPlainMove = false;
    LPARAM plainLParam = 0;
    {
        MSG msg{};
        int limit = 200;
        while (limit-- > 0) {
            if (!PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE)) break;
            if (msg.message == WM_MOUSEMOVE) {
                sawPlainMove = true;
                plainLParam = msg.lParam;
                break;
            }
        }
    }
    Check(sawPlainMove, "WM_MOUSEMOVE is delivered for an injected motion event");
    if (sawPlainMove) {
        const int rx = (int)(plainLParam & 0xFFFF);
        const int ry = (int)((plainLParam >> 16) & 0xFFFF);
        Check(rx == 111 && ry == 222, "WM_MOUSEMOVE lParam packs LOWORD=x, HIWORD=y correctly");
    }

    // Case 2: MK_LBUTTON must persist into WM_MOUSEMOVE while the left
    // button is held during a drag -- required by planetblupi's
    // CEvent::TreatEventBase ("fwKeys = wParam") and the Play*() handlers
    // that then test "fwKeys & MK_LBUTTON" while dragging.
    InjectMouseButton(sdlWin, /*down=*/true, SDL_BUTTON_LEFT, 10.0f, 10.0f);
    DrainMessages(); // consume WM_LBUTTONDOWN so it doesn't confuse the check below

    InjectMouseMotion(sdlWin, 77.0f, 88.0f);

    bool sawDragMove = false;
    WPARAM dragWParam = 0;
    {
        MSG msg{};
        int limit = 200;
        while (limit-- > 0) {
            if (!PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE)) break;
            if (msg.message == WM_MOUSEMOVE) {
                sawDragMove = true;
                dragWParam = msg.wParam;
                break;
            }
        }
    }
    Check(sawDragMove, "WM_MOUSEMOVE is delivered while the left button is held");
    if (sawDragMove) {
        Check((dragWParam & MK_LBUTTON) != 0,
              "WM_MOUSEMOVE wParam carries MK_LBUTTON while the left button is held during a drag");
    }

    // Release the button so state doesn't leak into subsequent tests.
    InjectMouseButton(sdlWin, /*down=*/false, SDL_BUTTON_LEFT, 77.0f, 88.0f);
    DrainMessages();

    // Case 3 (TASK-0048): MK_RBUTTON must likewise persist into
    // WM_MOUSEMOVE while the right button is held during a drag
    // (planetblupi event.cpp:3413,3440,3472,3504).
    InjectMouseButton(sdlWin, /*down=*/true, SDL_BUTTON_RIGHT, 20.0f, 20.0f);
    DrainMessages(); // consume WM_RBUTTONDOWN

    InjectMouseMotion(sdlWin, 99.0f, 55.0f);

    bool sawRightDragMove = false;
    WPARAM rightDragWParam = 0;
    {
        MSG msg{};
        int limit = 200;
        while (limit-- > 0) {
            if (!PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE)) break;
            if (msg.message == WM_MOUSEMOVE) {
                sawRightDragMove = true;
                rightDragWParam = msg.wParam;
                break;
            }
        }
    }
    Check(sawRightDragMove, "WM_MOUSEMOVE is delivered while the right button is held");
    if (sawRightDragMove) {
        Check((rightDragWParam & MK_RBUTTON) != 0,
              "WM_MOUSEMOVE wParam carries MK_RBUTTON while the right button is held during a drag");
    }

    InjectMouseButton(sdlWin, /*down=*/false, SDL_BUTTON_RIGHT, 99.0f, 55.0f);
    DrainMessages();

    DestroyWindow(hwnd);
    DrainMessages();
}

// TASK-24H-0214: WM_MOUSEMOVE coalescing (src/internal/FreeApiMessageQueue.cpp:73-86)
// updates an already-queued WM_MOUSEMOVE for the same hwnd in place instead
// of appending a second one -- mouse motion fires far faster than the game
// can consume it, so without this every real click/keypress would queue up
// behind stale motion. Existing tests exercise lParam packing/modifier
// flags via a single injected motion; this directly and minimally proves
// the coalescing invariant itself: two motions injected back-to-back
// (before either is drained) must leave exactly one WM_MOUSEMOVE queued,
// carrying the SECOND (latest) position, not the first.
static void TestWmMouseMoveCoalescingKeepsOnlyLatestPosition()
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = RegTestWndProc;
    wc.lpszClassName = "RegTest_MouseMoveCoalesce";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "RegTest_MouseMoveCoalesce", "Test",
                                 WS_POPUPWINDOW | WS_VISIBLE,
                                 0, 0, 320, 240,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    Check(hwnd != nullptr, "CreateWindowExA succeeds for the mouse-move coalescing test window");
    if (!hwnd) return;

    DrainMessages();

    auto* sdlWin = reinterpret_cast<SDL_Window*>(hwnd);

    // Both injected before any drain -- PumpSdlEvents (triggered by the
    // first PeekMessageA call below) processes both raw SDL motion events
    // in the same pass, so the second PushMessage(WM_MOUSEMOVE, ...) call
    // finds the first one already queued and coalesces into it.
    InjectMouseMotion(sdlWin, 10.0f, 20.0f);
    InjectMouseMotion(sdlWin, 30.0f, 40.0f);

    int mouseMoveCount = 0;
    LPARAM lastLParam = 0;
    MSG msg{};
    int limit = 200;
    while (limit-- > 0) {
        if (!PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE)) break;
        if (msg.message == WM_MOUSEMOVE) {
            ++mouseMoveCount;
            lastLParam = msg.lParam;
        }
    }

    Check(mouseMoveCount == 1,
          "two WM_MOUSEMOVE events queued back-to-back for the same hwnd coalesce into exactly one queued message");
    if (mouseMoveCount == 1) {
        const int rx = (int)(lastLParam & 0xFFFF);
        const int ry = (int)((lastLParam >> 16) & 0xFFFF);
        Check(rx == 30 && ry == 40,
              "the coalesced WM_MOUSEMOVE carries the SECOND (latest) position, not the first");
    }

    DestroyWindow(hwnd);
    DrainMessages();
}

// TASK-24H-0213: WM_TIMER coalescing (src/internal/FreeApiMessageQueue.cpp:91-101)
// drops a duplicate WM_TIMER for the same hwnd+timer-id (wParam) when one is
// already queued -- WM_TIMER is not a hard real-time ticker on real Win32
// either, so piling up stale timer messages just produces jitter. Existing
// timer tests exercise SetTimer/WM_TIMER delivery broadly but don't isolate
// this specific invariant directly.
static void TestWmTimerCoalescingKeepsOnlyOneQueuedMessagePerHwndAndId()
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = RegTestWndProc;
    wc.lpszClassName = "RegTest_TimerCoalesce";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "RegTest_TimerCoalesce", "Test",
                                 WS_POPUPWINDOW | WS_VISIBLE,
                                 0, 0, 320, 240,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    Check(hwnd != nullptr, "CreateWindowExA succeeds for the WM_TIMER coalescing test window");
    if (!hwnd) return;

    DrainMessages();

    const UINT_PTR kTimerId = 4242;
    // Posted directly (not via SetTimer) -- coalescing lives in PushMessage
    // itself, so this exercises the exact same code path a real SetTimer-
    // generated WM_TIMER would, without needing to wait on real elapsed time.
    PostMessageA(hwnd, WM_TIMER, kTimerId, 0);
    PostMessageA(hwnd, WM_TIMER, kTimerId, 0);

    int timerCount = 0;
    MSG msg{};
    int limit = 200;
    while (limit-- > 0) {
        if (!PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE)) break;
        if (msg.message == WM_TIMER && msg.wParam == kTimerId) {
            ++timerCount;
        }
    }

    Check(timerCount == 1,
          "two WM_TIMER messages posted back-to-back for the same hwnd+timer-id coalesce into exactly one queued message");

    DestroyWindow(hwnd);
    DrainMessages();
}

// TASK-0055 (plan.md): planetblupi calls GetClientRect+ClientToScreen
// (twice, treating a RECT as two POINTs) every single displayed frame
// (pixmap.cpp CPixmap::Display():975-990, MouseQuickDraw:1100-1123) -- a
// hot path, not a startup-only call. This verifies the screen coordinates
// track the window's real, current position (not a cached/stale value)
// across a position change, round-trips via ScreenToClient, and includes a
// basic performance sanity bound to catch an accidental O(n) regression.
static void TestClientToScreenTracksWindowPositionNotStale()
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = RegTestWndProc;
    wc.lpszClassName = "RegTest_ClientToScreen";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "RegTest_ClientToScreen", "Test",
                                 WS_POPUPWINDOW | WS_VISIBLE,
                                 50, 60, 200, 150,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    Check(hwnd != nullptr, "CreateWindowExA succeeds for the ClientToScreen position test window");
    if (!hwnd) return;
    DrainMessages();

    RECT client{};
    GetClientRect(hwnd, &client);

    POINT topLeft{client.left, client.top};
    POINT bottomRight{client.right, client.bottom};
    ClientToScreen(hwnd, &topLeft);
    ClientToScreen(hwnd, &bottomRight);
    Check(topLeft.x == 50 && topLeft.y == 60,
          "ClientToScreen maps the client origin to the window's real screen position");
    Check(bottomRight.x == 250 && bottomRight.y == 210,
          "ClientToScreen maps the client's far corner using the window's real screen position");

    // Move the window (same size) and verify the mapping tracks the *new*
    // position rather than returning a stale/cached value from creation.
    MoveWindow(hwnd, 300, 400, 200, 150, TRUE);
    DrainMessages();

    RECT clientAfterMove{};
    GetClientRect(hwnd, &clientAfterMove);
    POINT topLeftAfterMove{clientAfterMove.left, clientAfterMove.top};
    POINT bottomRightAfterMove{clientAfterMove.right, clientAfterMove.bottom};
    ClientToScreen(hwnd, &topLeftAfterMove);
    ClientToScreen(hwnd, &bottomRightAfterMove);
    Check(topLeftAfterMove.x == 300 && topLeftAfterMove.y == 400,
          "ClientToScreen tracks the window's new position after MoveWindow (not stale)");
    Check(bottomRightAfterMove.x == 500 && bottomRightAfterMove.y == 550,
          "ClientToScreen's far-corner mapping also tracks the window's new position");

    // Round-trip via ScreenToClient.
    POINT roundTrip{500, 550};
    ScreenToClient(hwnd, &roundTrip);
    Check(roundTrip.x == 200 && roundTrip.y == 150,
          "ScreenToClient is the exact inverse of ClientToScreen after the move");

    // Basic performance sanity check: this is a per-frame hot path
    // (Display() calls it twice every frame), so N repeated calls must
    // complete well within a generous bound, catching an accidental O(n)
    // (e.g. linear scan) regression.
    const Uint64 start = SDL_GetTicks();
    for (int i = 0; i < 20000; ++i) {
        POINT p{0, 0};
        ClientToScreen(hwnd, &p);
    }
    const Uint64 elapsedMs = SDL_GetTicks() - start;
    Check(elapsedMs < 2000, "20000 ClientToScreen calls complete well within a generous time bound (no O(n) regression)");

    DestroyWindow(hwnd);
    DrainMessages();
}

// TASK-0055: SetCursorPos/GetCursorPos round-trip, matching
// MouseQuickDraw-adjacent mouse-position usage.
//
// Note: both are thin wrappers over SDL_WarpMouseGlobal/SDL_GetGlobalMouseState,
// which SDL's "dummy" video driver (used in this headless test environment)
// explicitly does not support ("That operation is not supported") -- a real
// backend (X11/Wayland/Windows) supports it. The round-trip is therefore
// only asserted when the warp actually reports success; otherwise this
// records that the environment limitation was hit rather than silently
// skipping (and still proves both functions return the correct *type* of
// result and don't crash).
static void TestSetCursorPosAndGetCursorPosRoundTrip()
{
    BOOL warped = SetCursorPos(123, 45);

    POINT pos{};
    BOOL got = GetCursorPos(&pos);
    Check(got == TRUE, "GetCursorPos returns TRUE");

    if (warped) {
        Check(pos.x == 123 && pos.y == 45, "GetCursorPos reflects the position set by SetCursorPos");
    } else {
        printf("[winuser-regressions] SKIP: SetCursorPos round-trip not verifiable -- "
               "SDL's dummy video driver does not support global mouse warp in this headless environment\n");
    }
}

// TASK-0032 (plan.md): both games' startup sequence calls exactly
// ShowWindow(hwnd, SW_SHOW) -> UpdateWindow(hwnd) -> SetFocus(hwnd)
// (free-eggbert blupi.cpp:784-786; planetblupi blupi.cpp:677-679).
static void TestShowWindowUpdateWindowSetFocusSequence()
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = RegTestWndProc;
    wc.lpszClassName = "RegTest_ShowUpdateFocus";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "RegTest_ShowUpdateFocus", "Test",
                                 WS_POPUPWINDOW, 0, 0, 320, 240,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    Check(hwnd != nullptr, "CreateWindowExA succeeds for the startup-sequence test");
    if (!hwnd) return;

    BOOL shown = ShowWindow(hwnd, SW_SHOW);
    (void)shown; // real Win32 return value here is "was previously visible", not a success code
    BOOL updated = UpdateWindow(hwnd);
    Check(updated == TRUE, "UpdateWindow succeeds immediately after ShowWindow(SW_SHOW)");
    HWND previousFocus = SetFocus(hwnd);
    (void)previousFocus;
    Check(true, "ShowWindow->UpdateWindow->SetFocus sequence completes without crashing");

    DrainMessages();
    DestroyWindow(hwnd);
    DrainMessages();
}

// TASK-24H-1104: minimal, file-local SDL_Log capture helper. Counts lines
// so TASK-24H-1101's "quiet by default" fix has a regression guard --
// without this, a future edit could silently reintroduce an ungated
// SDL_Log in the window-creation path with nothing to catch it.
static int g_capturedLogLineCount = 0;

static void SDLCALL CountAllLogLines(void* userdata, int category, SDL_LogPriority priority, const char* message)
{
    (void)userdata;
    (void)category;
    (void)priority;
    (void)message;
    ++g_capturedLogLineCount;
}

// TASK-24H-1104: with no diagnostics env var set (this test's normal
// default state -- see docs/headers.md's test-authoring notes), the real
// CreateWindowExA->ShowWindow->UpdateWindow->SetFocus sequence both games
// run at startup must produce zero SDL_Log output (TASK-24H-1101).
static void TestWindowStartupSequenceIsQuietByDefault()
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = RegTestWndProc;
    wc.lpszClassName = "RegTest_QuietStartup";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    g_capturedLogLineCount = 0;
    SDL_SetLogOutputFunction(CountAllLogLines, nullptr);

    HWND hwnd = CreateWindowExA(0, "RegTest_QuietStartup", "Test",
                                 WS_POPUPWINDOW, 0, 0, 320, 240,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetFocus(hwnd);

    SDL_SetLogOutputFunction(nullptr, nullptr);

    Check(hwnd != nullptr, "CreateWindowExA succeeds for the quiet-startup test");
    Check(g_capturedLogLineCount == 0,
          "CreateWindowExA->ShowWindow->UpdateWindow->SetFocus produces zero SDL_Log output with no diagnostics env var set");

    DrainMessages();
    if (hwnd) DestroyWindow(hwnd);
    DrainMessages();
}

// TASK-0034 (plan.md): both games read exactly CREATESTRUCT.hInstance on
// WM_CREATE (free-eggbert blupi.cpp:508; planetblupi blupi.cpp:428-430).
static HINSTANCE g_capturedCreateHInstance = nullptr;
static bool g_capturedWmCreate = false;

static LRESULT WINAPI WmCreateCaptureWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_CREATE) {
        g_capturedWmCreate = true;
        auto* cs = reinterpret_cast<LPCREATESTRUCTA>(lParam);
        if (cs) g_capturedCreateHInstance = cs->hInstance;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static void TestWmCreateDeliversCorrectHInstance()
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = WmCreateCaptureWndProc;
    wc.lpszClassName = "RegTest_WmCreateHInstance";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    const HINSTANCE kTestInstance = reinterpret_cast<HINSTANCE>(static_cast<uintptr_t>(0x1234));
    g_capturedWmCreate = false;
    g_capturedCreateHInstance = nullptr;

    HWND hwnd = CreateWindowExA(0, "RegTest_WmCreateHInstance", "Test",
                                 WS_POPUPWINDOW, 0, 0, 320, 240,
                                 nullptr, nullptr, kTestInstance, nullptr);
    Check(hwnd != nullptr, "CreateWindowExA succeeds for the WM_CREATE hInstance test");
    Check(g_capturedWmCreate, "WM_CREATE is dispatched synchronously during CreateWindowExA");
    Check(g_capturedCreateHInstance == kTestInstance,
          "CREATESTRUCT.hInstance delivered via WM_CREATE matches the hInstance passed to CreateWindowExA");

    if (hwnd) {
        DrainMessages();
        DestroyWindow(hwnd);
        DrainMessages();
    }
}

// TASK-0038 (plan.md): both games call WaitMessage only when idle
// (!g_bActive), expecting it to idle without busy-spinning until a new
// message arrives (free-eggbert blupi.cpp:921; planetblupi blupi.cpp:919).
static void TestWaitMessageDoesNotBusySpin()
{
    // With no message ever posted, count how many WaitMessage calls occur
    // in a fixed wall-clock window. A proper (sleep-based) implementation
    // yields roughly one call per ~1ms tick; a busy-spinning implementation
    // would instead execute many thousands/millions of calls in the same
    // window. This distinguishes the two without needing an OS CPU-time API.
    const Uint64 windowMs = 50;
    const Uint64 deadline = SDL_GetTicks() + windowMs;
    long long iterations = 0;
    bool allReturnedTrue = true;
    while (SDL_GetTicks() < deadline) {
        if (!WaitMessage()) allReturnedTrue = false;
        ++iterations;
    }
    Check(iterations < 1000,
          "WaitMessage idles (sleeps) rather than busy-spinning when no message is available "
          "(iteration count stayed low over a fixed wall-clock window)");
    // TASK-24H-0205: WaitMessage is not a true OS-level blocking wait -- it
    // checks the queue twice around a single ~1ms delay and then returns
    // TRUE *unconditionally*, even if the queue is still empty afterward.
    // Both games' idle loops depend on exactly this contract; a future
    // refactor that "fixed" this into a real block or a FALSE-on-timeout
    // return could hang or busy-loop them. Confirmed here across every
    // iteration above, where the queue is guaranteed to stay empty the
    // entire window (nothing posts to it).
    Check(allReturnedTrue,
          "WaitMessage returns TRUE unconditionally on every call, even while the queue is confirmed still empty");

    // Now verify it still returns promptly once a message actually arrives,
    // posted from a background thread after a short delay.
    WNDCLASSA wc{};
    wc.lpfnWndProc   = RegTestWndProc;
    wc.lpszClassName = "RegTest_WaitMessagePost";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, "RegTest_WaitMessagePost", "Test",
                                 WS_POPUPWINDOW, 0, 0, 320, 240,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    DrainMessages();

    std::atomic<bool> posted{false};
    std::thread poster([&]() {
        SDL_Delay(20);
        PostMessageA(hwnd, WM_USER + 1, 0, 0);
        posted.store(true);
    });

    const Uint64 waitStart = SDL_GetTicks();
    bool sawMessage = false;
    MSG msg{};
    while (SDL_GetTicks() - waitStart < 2000) {
        WaitMessage();
        if (PeekMessageA(&msg, hwnd, 0, 0, PM_NOREMOVE)) {
            sawMessage = true;
            break;
        }
    }
    poster.join();

    Check(sawMessage, "WaitMessage-driven polling observes a message posted from another thread after a short delay");

    DrainMessages();
    DestroyWindow(hwnd);
    DrainMessages();
}

// TASK-0033 (plan.md): both games call DestroyWindow only on an
// unrecoverable init failure (free-eggbert blupi.cpp:664; planetblupi
// blupi.cpp:585) -- create-then-immediately-destroy, with no other window
// operations in between (the real games exit shortly after). Repeated
// cycles also verify the internal window registry doesn't grow unbounded
// (todo/free-api-performance-todo.md "Verify Window Lookup Map
// Maintenance").
static void TestDestroyWindowFatalInitFailurePathDoesNotCrash()
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = RegTestWndProc;
    wc.lpszClassName = "RegTest_DestroyInitFailure";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    const int kIterations = 500;
    bool allOk = true;
    for (int i = 0; i < kIterations; ++i) {
        HWND hwnd = CreateWindowExA(0, "RegTest_DestroyInitFailure", "Test",
                                     WS_POPUPWINDOW, 0, 0, 320, 240,
                                     nullptr, nullptr, (HINSTANCE)1, nullptr);
        if (!hwnd) { allOk = false; break; }
        BOOL destroyed = DestroyWindow(hwnd);
        if (destroyed != TRUE) { allOk = false; break; }
    }
    Check(allOk, "repeated create-then-immediately-destroy cycles (matching both games' fatal-init-failure path) complete cleanly, no crash");

    DrainMessages();
}

static void TestShowCursorHidesAndShowsRealCursor()
{
    // SDL_CursorVisible() reflects SDL_ShowCursor()/SDL_HideCursor() even
    // under the "dummy" video driver used in headless test environments,
    // so this can be verified directly rather than only smoke-tested.
    const int afterHide = ShowCursor(FALSE);
    Check(afterHide < 0, "ShowCursor(FALSE) decrements the display counter below zero");
    Check(!SDL_CursorVisible(), "ShowCursor(FALSE) actually hides the real OS cursor");

    const int afterShow = ShowCursor(TRUE);
    Check(afterShow >= 0, "ShowCursor(TRUE) restores the display counter to >= 0");
    Check(SDL_CursorVisible(), "ShowCursor(TRUE) actually shows the real OS cursor again");
}

// TASK-24H-0311: ShowCursor's signed counter has no clamp (matching real
// Win32) -- several TRUE-in-a-row calls must keep incrementing above 0, and
// several FALSE-in-a-row calls must keep decrementing below -1, with a
// single opposite call only partially recovering a deeply-shifted counter.
// Relative to whatever baseline count the counter happens to be at when
// this test runs (it's process-global and shared with the test above), not
// assumed to start at exactly 0.
static void TestShowCursorCounterAccumulatesWithoutClamping()
{
    ShowCursor(TRUE);
    const int baseline = ShowCursor(FALSE); // net-zero pair; reads the counter's value before this test's excursion

    int afterFiveShows = baseline;
    for (int i = 0; i < 5; ++i) {
        afterFiveShows = ShowCursor(TRUE);
    }
    Check(afterFiveShows == baseline + 5,
          "five consecutive ShowCursor(TRUE) calls each increment the counter by one, unclamped");
    Check(SDL_CursorVisible(), "cursor stays visible after repeated ShowCursor(TRUE)");

    // Walk back to baseline, then go deeply negative.
    for (int i = 0; i < 5; ++i) {
        ShowCursor(FALSE);
    }
    int afterSevenHides = baseline;
    for (int i = 0; i < 7; ++i) {
        afterSevenHides = ShowCursor(FALSE);
    }
    Check(afterSevenHides == baseline - 7,
          "seven consecutive ShowCursor(FALSE) calls each decrement the counter by one, unclamped");
    Check(!SDL_CursorVisible(), "cursor stays hidden after repeated ShowCursor(FALSE)");

    const int afterOneShow = ShowCursor(TRUE);
    Check(afterOneShow == baseline - 6,
          "a single ShowCursor(TRUE) only partially recovers a deeply-negative counter");
    Check(!SDL_CursorVisible(), "cursor stays hidden when the counter is still negative after one partial recovery");

    // Restore the counter to baseline so later tests in this binary aren't
    // affected by this test's excursion.
    for (int i = 0; i < 6; ++i) {
        ShowCursor(TRUE);
    }
}

static void TestSetCursorReturnsPreviousHandle()
{
    HCURSOR first = LoadCursorA(nullptr, "IDC_ARROW");
    HCURSOR second = LoadCursorA(nullptr, "IDC_POINTER");
    Check(first != nullptr && second != nullptr, "LoadCursorA returns non-null handles for SetCursor test");

    // TASK-24H-0315: this is the only SetCursor call site in this test
    // binary, so it's genuinely the first-ever SetCursor call in this
    // process -- g_currentCursor starts at nullptr (src/winuser_cursor.cpp),
    // so the very first call's returned "previous" handle must be NULL.
    HCURSOR previousBeforeAny = SetCursor(first);
    Check(previousBeforeAny == nullptr, "SetCursor's very first call in a process returns a NULL previous handle");

    HCURSOR previous = SetCursor(second);
    Check(previous == first, "SetCursor returns the previously-active cursor handle");
}

// TASK-0056 (plan.md): both games call LoadCursorA with a fixed set of
// named cursor IDs on every sprite-cursor swap (free-eggbert misc.cpp:48-59,
// 12 names; planetblupi misc.cpp:56-69, 13 names, union below); every one
// must return a non-null handle to avoid a null-handle edge case in game
// code that doesn't null-check. LoadIconA is called with exactly one name
// (both games' WNDCLASSA.hIcon, blupi.cpp:723/615).
static void TestLoadCursorAAndLoadIconAReturnNonNullForAllRealNames()
{
    static const char* kCursorNames[] = {
        "IDC_ARROW", "IDC_POINTER", "IDC_MAP", "IDC_ARROWU", "IDC_ARROWD",
        "IDC_ARROWL", "IDC_ARROWR", "IDC_ARROWUL", "IDC_ARROWUR",
        "IDC_ARROWDL", "IDC_ARROWDR", "IDC_WAIT", "IDC_EMPTY", "IDC_FILL",
    };
    bool allNonNull = true;
    for (const char* name : kCursorNames) {
        if (LoadCursorA((HINSTANCE)1, name) == nullptr) {
            printf("[winuser-regressions] FAIL: LoadCursorA(\"%s\") returned NULL\n", name);
            allNonNull = false;
        }
    }
    Check(allNonNull, "LoadCursorA returns a non-null handle for every cursor name either game requests");

    HICON icon = LoadIconA((HINSTANCE)1, "IDR_MAINFRAME");
    Check(icon != nullptr, "LoadIconA(\"IDR_MAINFRAME\") returns a non-null handle (both games' WNDCLASSA.hIcon)");
}

// TASK-24H-1224: wsprintfA (src/winuser_message.cpp) is a real variadic
// vsnprintf wrapper into a fixed 1024-byte buffer -- real, live logic (not
// a stub) on both games' sound-diagnostic paths (free-eggbert
// soundbass.cpp:142/sound.cpp:117, planetblupi sound.cpp:99, all the exact
// same shape: three %d integer specifiers), but had zero test coverage and
// wasn't even listed in docs/supported-apis.md or docs/out-of-scope.md
// (found by a session-4 strict test-coverage audit fork).
static void TestWsprintfAFormatsAndHandlesEdgeCases()
{
    char buf[1024];

    // Exact real-usage shape from both games (three %d integers).
    int written = wsprintfA(buf, "Data1 : %d, dwdata: %d, pFile: %d", 7, 1024, 42);
    Check(written == (int)strlen(buf), "wsprintfA's return value matches the actual formatted length");
    Check(strcmp(buf, "Data1 : 7, dwdata: 1024, pFile: 42") == 0,
          "wsprintfA formats multiple %d specifiers exactly like both games' real call shape");

    Check(wsprintfA(nullptr, "x") == 0, "wsprintfA(NULL output buffer, ...) returns 0 instead of crashing");
    Check(wsprintfA(buf, nullptr) == 0, "wsprintfA(..., NULL format) returns 0 instead of crashing");

    // Buffer-edge case: a format string producing output longer than the
    // internal 1024-byte limit must truncate safely (vsnprintf's own
    // bounded-write contract), not overflow.
    char longBuf[2048];
    int longWritten = wsprintfA(longBuf, "%02000d", 1);
    Check(longWritten > 1024,
          "wsprintfA's return value reports the untruncated would-be length (vsnprintf contract), even though the write itself was bounded");
    Check(strlen(longBuf) < 1024, "wsprintfA's actual written buffer content is bounded to the internal 1024-byte limit, not overflowed");
}

// TASK-24H-1225: OutputDebugStringA (src/winbase.cpp) is real, live logic
// (a null-checked printf to stdout, not a stub) on both games' DirectSound-
// failure diagnostic paths (free-eggbert misc.cpp:32; planetblupi
// wave.cpp:234,264,268, called with plain string literals, no format
// specifiers), but had zero test coverage and was completely unclassified
// in both docs/supported-apis.md and docs/out-of-scope.md (found by a
// session-4 strict test-coverage audit fork). Captures real stdout output
// via dup2 (POSIX-only, matching this test suite's existing convention of
// skipping POSIX-specific coverage on _WIN32) to verify the actual printed
// content, not just "doesn't crash".
//
// IMPORTANT: the temp file is opened by a RELATIVE name, not an absolute
// "/tmp/..." path -- this test file #includes <windows.h>, which globally
// redefines `fopen` to `free_api_fopen` (include/windows.h,
// docs/headers.md's "Why windows.h globally redefines fopen"). That
// wrapper normalizes every path through NormalizeFilesystemPath, which
// deliberately strips a leading slash so a game's rooted-looking path
// (e.g. "\User\save.xch") stays relative to CWD instead of escaping to the
// real filesystem root (TASK-24H-0708). An earlier version of this test
// used an absolute "/tmp/..." path for the read-back fopen() call, which
// got silently rewritten into a relative lookup that could never find the
// file -- `mkstemp`/`open`/`access` (none of which go through the fopen
// macro) all correctly saw the real file the whole time, only the
// macro-redirected `fopen()` call was affected. Fixed by using a bare
// relative filename throughout, which free_api_fopen resolves identically
// to the real fopen (no leading slash to strip).
#if !defined(_WIN32)
static void TestOutputDebugStringAPrintsToStdoutAndIsNullSafe()
{
    Check(true, "OutputDebugStringA(nullptr) does not crash (exercised below, before any assertion could run if it did)");
    OutputDebugStringA(nullptr);

    char tmpPath[] = "free-api-odsa-test-XXXXXX"; // relative to CWD -- see comment above
    int tmpFd = mkstemp(tmpPath);
    Check(tmpFd >= 0, "temp file created to capture OutputDebugStringA's real stdout output");
    if (tmpFd < 0) return;

    fflush(stdout);
    int savedStdoutFd = dup(STDOUT_FILENO);
    Check(savedStdoutFd >= 0, "original stdout fd saved for restoration");

    // Flush again, immediately before redirecting: when stdout isn't a
    // TTY it's fully buffered, so the Check() call directly above may
    // still have unflushed text sitting in stdout's user-space buffer.
    // dup2 only changes what the *file descriptor* points to -- it has no
    // effect on that already-buffered text, which would otherwise get
    // flushed into this test's temp file ahead of (and instead of, since
    // fgets below only reads one line) OutputDebugStringA's own output.
    fflush(stdout);
    dup2(tmpFd, STDOUT_FILENO);
    OutputDebugStringA("Failed to create sound buffer\n");
    fflush(stdout);

    // Restore stdout before making any further Check()/printf() calls --
    // every other test in this binary depends on real stdout working.
    dup2(savedStdoutFd, STDOUT_FILENO);
    close(savedStdoutFd);
    close(tmpFd);

    FILE* readBack = fopen(tmpPath, "r");
    char captured[256] = {0};
    if (readBack) {
        fgets(captured, sizeof(captured), readBack);
        fclose(readBack);
    }
    remove(tmpPath);

    // Uses strstr rather than strcmp: this redirect window is on the main
    // thread with nothing else running between dup2 and fflush, so in
    // practice the capture is exact, but asserting "contains" rather than
    // "equals" keeps this test robust against any incidental extra output
    // rather than being needlessly brittle about exact byte-for-byte
    // buffer contents, which isn't the property under test.
    Check(strstr(captured, "Failed to create sound buffer\n") != nullptr,
          "OutputDebugStringA actually prints its argument's exact text to stdout, matching both games' real call shape");
}

// OutputDebugStringW (src/winbase.cpp) was fixed to perform a real UTF-16
// -> UTF-8 conversion (previously it truncated each wchar_t to char) but
// had zero test coverage -- found by this session's gcov sweep. Reuses
// TestOutputDebugStringAPrintsToStdoutAndIsNullSafe's dup2-based stdout
// capture technique; see that test's comment for why the temp file path
// must be relative, not absolute.
static void TestOutputDebugStringWConvertsUtf16ToUtf8AndIsNullSafe()
{
    Check(true, "OutputDebugStringW(nullptr) does not crash (exercised below, before any assertion could run if it did)");
    OutputDebugStringW(nullptr);

    // "Hi <euro-sign><slightly-smiling-face>" -- covers a plain-ASCII run, a
    // BMP character requiring a 3-byte UTF-8 encoding (U+20AC), and an
    // astral character requiring a UTF-16 surrogate pair decoded into a
    // 4-byte UTF-8 encoding (U+1F642).
    const WCHAR text[] = {'H', 'i', ' ', 0x20AC, 0xD83D, 0xDE42, 0};

    char tmpPath[] = "free-api-odsw-test-XXXXXX"; // relative to CWD -- see comment above
    int tmpFd = mkstemp(tmpPath);
    Check(tmpFd >= 0, "temp file created to capture OutputDebugStringW's real stdout output");
    if (tmpFd < 0) return;

    fflush(stdout);
    int savedStdoutFd = dup(STDOUT_FILENO);
    Check(savedStdoutFd >= 0, "original stdout fd saved for restoration");

    fflush(stdout);
    dup2(tmpFd, STDOUT_FILENO);
    OutputDebugStringW(text);
    fflush(stdout);

    dup2(savedStdoutFd, STDOUT_FILENO);
    close(savedStdoutFd);
    close(tmpFd);

    FILE* readBack = fopen(tmpPath, "r");
    char captured[256] = {0};
    if (readBack) {
        fgets(captured, sizeof(captured), readBack);
        fclose(readBack);
    }
    remove(tmpPath);

    // Expected UTF-8 bytes for "Hi €\U0001F642": 'H' 'i' ' ' then the
    // 3-byte encoding of U+20AC (0xE2 0x82 0xAC) then the 4-byte encoding
    // of U+1F642 (0xF0 0x9F 0x99 0x82).
    const char expected[] = "Hi \xE2\x82\xAC\xF0\x9F\x99\x82";
    Check(strcmp(captured, expected) == 0,
          "OutputDebugStringW converts UTF-16 to UTF-8 correctly, including a BMP character and a surrogate-pair-encoded astral character");
}
#endif

// TASK-24H-1226: SetWindowTextA (src/winuser_window.cpp) is real, live
// logic (a genuine SDL_SetWindowTitle call, not a stub) used by both games
// to set their window title (free-eggbert blupi.cpp:532,541; planetblupi
// blupi.cpp:453,462), but had zero test coverage and no
// docs/supported-apis.md row (found by a session-4 strict test-coverage
// audit fork; TASK-24H-0313 tracked only the missing doc row until now).
static void TestSetWindowTextASetsRealWindowTitle()
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = RegTestWndProc;
    wc.lpszClassName = "RegTest_SetWindowText";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "RegTest_SetWindowText", "Initial Title",
                                 WS_POPUPWINDOW | WS_VISIBLE, 0, 0, 320, 240,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    Check(hwnd != nullptr, "CreateWindowExA succeeds for the SetWindowTextA test window");
    if (!hwnd) return;

    Check(SetWindowTextA(hwnd, "New Title") == TRUE, "SetWindowTextA returns TRUE for a valid window");

    auto* sdlWindow = reinterpret_cast<SDL_Window*>(hwnd);
    const char* actualTitle = SDL_GetWindowTitle(sdlWindow);
    Check(actualTitle != nullptr && strcmp(actualTitle, "New Title") == 0,
          "SetWindowTextA actually changes the real SDL window's title, matching both games' real usage (setting the title bar text)");

    Check(SetWindowTextA(nullptr, "x") == FALSE, "SetWindowTextA(NULL hWnd, ...) returns FALSE instead of crashing");

    Check(SetWindowTextA(hwnd, nullptr) == TRUE, "SetWindowTextA(hwnd, NULL) returns TRUE (treated as an empty title, not an error)");
    const char* emptyTitle = SDL_GetWindowTitle(sdlWindow);
    Check(emptyTitle != nullptr && emptyTitle[0] == '\0', "SetWindowTextA(hwnd, NULL) sets an empty title, not a crash or stale leftover");

    DestroyWindow(hwnd);
    DrainMessages();
}

int main()
{
    printf("[winuser-regressions] Starting\n");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("[winuser-regressions] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    TestAdjustWindowRectPreservesClientSize();
    TestRegisterClassAWithFullFieldSet();
    TestCreateWindowExAFullscreenPath();
    TestCreateWindowExANeverSetsResizableFlag();
    TestGetSystemMetricsCyCaptionAndUnqueriedIndexFallback();
    TestGlobalMemoryStatusPopulatesPlausibleValues();
    TestGetActiveWindowFallsBackBeforeFocusIsSet();
    TestDefWindowProcHandlesWmClose();
    TestDestroyWindowDispatchesWmDestroySynchronously();
    TestDestroyWindowRejectsSecondCallOnSameHwnd();
    TestRegisterClassADiscardsNonWndprocFields();
    TestDispatchMessageARoutesNullHwndToSoleRegisteredWindow();
    TestPeekMessageNoRemoveAndRemove();
    TestPeekMessageAIgnoresNonZeroFilterArguments();
    TestPeekMessageANeverSleepsOnEmptyQueue();
    TestGetMessageReturnsFalseOnQuit();
    TestMouseMoveLParamPackingAndModifierFlags();
    TestWmMouseMoveCoalescingKeepsOnlyLatestPosition();
    TestWmTimerCoalescingKeepsOnlyOneQueuedMessagePerHwndAndId();
    TestApplyKeyboardModifierFlagsHelperWithSyntheticKeystate();
    TestClientToScreenTracksWindowPositionNotStale();
    TestSetCursorPosAndGetCursorPosRoundTrip();
    TestShowWindowUpdateWindowSetFocusSequence();
    TestWindowStartupSequenceIsQuietByDefault();
    TestWmCreateDeliversCorrectHInstance();
    TestWaitMessageDoesNotBusySpin();
    TestDestroyWindowFatalInitFailurePathDoesNotCrash();
    TestShowCursorHidesAndShowsRealCursor();
    TestShowCursorCounterAccumulatesWithoutClamping();
    TestSetCursorReturnsPreviousHandle();
    TestLoadCursorAAndLoadIconAReturnNonNullForAllRealNames();
    TestWsprintfAFormatsAndHandlesEdgeCases();
#if !defined(_WIN32)
    TestOutputDebugStringAPrintsToStdoutAndIsNullSafe();
    TestOutputDebugStringWConvertsUtf16ToUtf8AndIsNullSafe();
#endif
    TestSetWindowTextASetsRealWindowTitle();

    SDL_Quit();

    if (g_failures > 0) {
        printf("[winuser-regressions] %d FAILURE(S)\n", g_failures);
        return 1;
    }

    printf("[winuser-regressions] ALL TESTS PASSED\n");
    return 0;
}
