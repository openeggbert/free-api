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
 */
#include <windows.h>
#include <SDL3/SDL.h>
#include <cstdio>

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

static void TestDefWindowProcHandlesWmClose()
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = RegTestWndProc;
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

    DestroyWindow(hwnd);
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

static void TestSetCursorReturnsPreviousHandle()
{
    HCURSOR first = LoadCursorA(nullptr, "IDC_ARROW");
    HCURSOR second = LoadCursorA(nullptr, "IDC_POINTER");
    Check(first != nullptr && second != nullptr, "LoadCursorA returns non-null handles for SetCursor test");

    HCURSOR previousBeforeAny = SetCursor(first);
    (void)previousBeforeAny;

    HCURSOR previous = SetCursor(second);
    Check(previous == first, "SetCursor returns the previously-active cursor handle");
}

int main()
{
    printf("[winuser-regressions] Starting\n");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("[winuser-regressions] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    TestAdjustWindowRectPreservesClientSize();
    TestDefWindowProcHandlesWmClose();
    TestDestroyWindowDispatchesWmDestroySynchronously();
    TestPeekMessageNoRemoveAndRemove();
    TestGetMessageReturnsFalseOnQuit();
    TestMouseMoveLParamPackingAndModifierFlags();
    TestShowCursorHidesAndShowsRealCursor();
    TestSetCursorReturnsPreviousHandle();

    SDL_Quit();

    if (g_failures > 0) {
        printf("[winuser-regressions] %d FAILURE(S)\n", g_failures);
        return 1;
    }

    printf("[winuser-regressions] ALL TESTS PASSED\n");
    return 0;
}
