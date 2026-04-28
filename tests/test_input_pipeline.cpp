/**
 * @file test_input_pipeline.cpp
 * @brief End-to-end test: SDL mouse/keyboard events → WinAPI WndProc pipeline.
 *
 * Injects SDL events programmatically and verifies that the free-api translation
 * produces WM_MOUSEMOVE, WM_LBUTTONDOWN, WM_KEYDOWN etc. in WndProc.
 */
#include <windows.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <cassert>
#include <vector>
#include <string>

// Received WinAPI messages captured by test WndProc
struct CapturedMsg {
    UINT msg;
    WPARAM wParam;
    LPARAM lParam;
};
static std::vector<CapturedMsg> g_received;
static HWND g_testHwnd = nullptr;

static LRESULT WINAPI TestWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    g_received.push_back({msg, wParam, lParam});
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// Inject an SDL mouse-motion event at window-relative (x, y)
static void InjectMouseMotion(SDL_Window* sdlWin, float x, float y)
{
    SDL_Event ev{};
    ev.type = SDL_EVENT_MOUSE_MOTION;
    ev.motion.windowID = SDL_GetWindowID(sdlWin);
    ev.motion.x = x;
    ev.motion.y = y;
    ev.motion.xrel = 0;
    ev.motion.yrel = 0;
    SDL_PushEvent(&ev);
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

static void InjectKeyEvent(SDL_Window* sdlWin, bool down, SDL_Scancode sc)
{
    SDL_Event ev{};
    ev.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    ev.key.windowID = SDL_GetWindowID(sdlWin);
    ev.key.scancode = sc;
    ev.key.key = SDL_GetKeyFromScancode(sc, SDL_KMOD_NONE, false);
    ev.key.repeat = 0;
    SDL_PushEvent(&ev);
}

// Drain all messages via PeekMessage/DispatchMessage
static void DrainMessages()
{
    MSG wmsg{};
    int limit = 100;
    while (limit-- > 0) {
        BOOL got = PeekMessageA(&wmsg, nullptr, 0, 0, PM_REMOVE);
        if (!got) break;
        if (wmsg.message == WM_QUIT) break;
        TranslateMessage(&wmsg);
        DispatchMessageA(&wmsg);
    }
}

// Helper: count messages of a given type in g_received
static int CountMsg(UINT msg)
{
    int n = 0;
    for (auto& m : g_received) {
        if (m.msg == msg) ++n;
    }
    return n;
}

static bool HasMsg(UINT msg, WPARAM wParam = 0, bool checkWParam = false)
{
    for (auto& m : g_received) {
        if (m.msg == msg) {
            if (!checkWParam || m.wParam == wParam) return true;
        }
    }
    return false;
}

int main()
{
    printf("[input-pipeline-test] Starting\n");

    // Initialize SDL (free-api does this lazily, but we need it for SDL_PushEvent)
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("[input-pipeline-test] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // Register window class and create window via free-api
    WNDCLASSA wc{};
    wc.lpfnWndProc   = TestWndProc;
    wc.lpszClassName = "TestClass";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);

    g_testHwnd = CreateWindowExA(
        0, "TestClass", "InputTest",
        WS_OVERLAPPEDWINDOW,
        100, 100, 640, 480,
        nullptr, nullptr, (HINSTANCE)1, nullptr
    );
    if (!g_testHwnd) {
        printf("[input-pipeline-test] FAIL: CreateWindowExA returned NULL\n");
        SDL_Quit();
        return 1;
    }
    printf("[input-pipeline-test] Window created: hwnd=%p\n", (void*)g_testHwnd);

    // The SDL window handle is the same as our HWND in free-api
    auto* sdlWin = reinterpret_cast<SDL_Window*>(g_testHwnd);

    // Drain any startup events
    DrainMessages();
    g_received.clear();

    // --- Test 1: Mouse motion ---
    InjectMouseMotion(sdlWin, 123.0f, 456.0f);
    DrainMessages();
    if (HasMsg(WM_MOUSEMOVE)) {
        // Check lParam encodes x=123, y=456
        for (auto& m : g_received) {
            if (m.msg == WM_MOUSEMOVE) {
                int rx = (int)(m.lParam & 0xFFFF);
                int ry = (int)((m.lParam >> 16) & 0xFFFF);
                printf("[input-pipeline-test] PASS: WM_MOUSEMOVE received x=%d y=%d\n", rx, ry);
                assert(rx == 123);
                assert(ry == 456);
            }
        }
    } else {
        printf("[input-pipeline-test] FAIL: WM_MOUSEMOVE not received\n");
        SDL_Quit();
        return 1;
    }

    g_received.clear();

    // --- Test 2: Left mouse button down ---
    InjectMouseButton(sdlWin, true, SDL_BUTTON_LEFT, 50.0f, 60.0f);
    DrainMessages();
    if (HasMsg(WM_LBUTTONDOWN)) {
        printf("[input-pipeline-test] PASS: WM_LBUTTONDOWN received\n");
    } else {
        printf("[input-pipeline-test] FAIL: WM_LBUTTONDOWN not received\n");
        SDL_Quit();
        return 1;
    }

    g_received.clear();

    // --- Test 3: Left mouse button up ---
    InjectMouseButton(sdlWin, false, SDL_BUTTON_LEFT, 50.0f, 60.0f);
    DrainMessages();
    if (HasMsg(WM_LBUTTONUP)) {
        printf("[input-pipeline-test] PASS: WM_LBUTTONUP received\n");
    } else {
        printf("[input-pipeline-test] FAIL: WM_LBUTTONUP not received\n");
        SDL_Quit();
        return 1;
    }

    g_received.clear();

    // --- Test 4: Key down (VK_SPACE = SDL_SCANCODE_SPACE) ---
    InjectKeyEvent(sdlWin, true, SDL_SCANCODE_SPACE);
    DrainMessages();
    if (HasMsg(WM_KEYDOWN)) {
        for (auto& m : g_received) {
            if (m.msg == WM_KEYDOWN) {
                printf("[input-pipeline-test] PASS: WM_KEYDOWN received vk=0x%02X\n", (unsigned)m.wParam);
                assert(m.wParam == VK_SPACE);
            }
        }
    } else {
        printf("[input-pipeline-test] FAIL: WM_KEYDOWN not received\n");
        SDL_Quit();
        return 1;
    }

    g_received.clear();

    // --- Test 5: Key up ---
    InjectKeyEvent(sdlWin, false, SDL_SCANCODE_ESCAPE);
    DrainMessages();
    if (HasMsg(WM_KEYUP)) {
        for (auto& m : g_received) {
            if (m.msg == WM_KEYUP) {
                printf("[input-pipeline-test] PASS: WM_KEYUP received vk=0x%02X\n", (unsigned)m.wParam);
                assert(m.wParam == VK_ESCAPE);
            }
        }
    } else {
        printf("[input-pipeline-test] FAIL: WM_KEYUP not received\n");
        SDL_Quit();
        return 1;
    }

    g_received.clear();

    // --- Test 6: Right mouse button ---
    InjectMouseButton(sdlWin, true, SDL_BUTTON_RIGHT, 10.0f, 20.0f);
    DrainMessages();
    if (HasMsg(WM_RBUTTONDOWN)) {
        printf("[input-pipeline-test] PASS: WM_RBUTTONDOWN received\n");
    } else {
        printf("[input-pipeline-test] FAIL: WM_RBUTTONDOWN not received\n");
        SDL_Quit();
        return 1;
    }

    printf("[input-pipeline-test] ALL TESTS PASSED\n");
    SDL_Quit();
    return 0;
}
