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

// TASK-24H-0408: text is a plain string-literal pointer here, not memory
// SDL itself allocated -- safe, because SDL only attempts to free an
// SDL_EVENT_TEXT_INPUT event's text pointer if it finds a matching entry
// in its own internal temporary-memory registry (SDL_LinkTemporaryMemoryToEvent);
// a pointer it never registered (e.g. a string literal) is simply left alone.
static void InjectTextInput(SDL_Window* sdlWin, const char* utf8Text)
{
    SDL_Event ev{};
    ev.type = SDL_EVENT_TEXT_INPUT;
    ev.text.windowID = SDL_GetWindowID(sdlWin);
    ev.text.text = utf8Text;
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

    // --- Test 1b (TASK-0047): WM_MOUSEMOVE lParam boundary/edge coordinate
    // values. Both games persist raw (message,wParam,lParam) triples to disk
    // for demo recording/playback (free-eggbert event.cpp:5276-5297;
    // planetblupi include/event.h:70-77's DemoEvent), so any LOWORD/HIWORD
    // packing deviation at boundary values would desync demo files.
    struct BoundaryCase { float x, y; int expectedRx, expectedRy; const char* label; };
    const BoundaryCase boundaryCases[] = {
        {0.0f, 0.0f, 0, 0, "(0,0)"},
        {65535.0f, 65535.0f, 65535, 65535, "near-0xFFFF (65535,65535)"},
        {-1.0f, -2.0f, 65535, 65534, "negative-as-unsigned (-1,-2) -> (0xFFFF,0xFFFE)"},
    };
    for (const auto& bc : boundaryCases) {
        g_received.clear();
        InjectMouseMotion(sdlWin, bc.x, bc.y);
        DrainMessages();
        bool found = false;
        for (auto& m : g_received) {
            if (m.msg == WM_MOUSEMOVE) {
                found = true;
                int rx = (int)(m.lParam & 0xFFFF);
                int ry = (int)((m.lParam >> 16) & 0xFFFF);
                if (rx == bc.expectedRx && ry == bc.expectedRy) {
                    printf("[input-pipeline-test] PASS: WM_MOUSEMOVE lParam packs %s correctly (x=%d y=%d)\n",
                           bc.label, rx, ry);
                } else {
                    printf("[input-pipeline-test] FAIL: WM_MOUSEMOVE lParam for %s: expected x=%d y=%d, got x=%d y=%d\n",
                           bc.label, bc.expectedRx, bc.expectedRy, rx, ry);
                    SDL_Quit();
                    return 1;
                }
            }
        }
        if (!found) {
            printf("[input-pipeline-test] FAIL: WM_MOUSEMOVE not received for %s\n", bc.label);
            SDL_Quit();
            return 1;
        }
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

    g_received.clear();

    // --- Test 7 (TASK-0050): full VK_* set both games test (§3.4), beyond
    // the SPACE/ESCAPE already covered above: F1-F12, RETURN, SHIFT,
    // CONTROL, PAUSE, arrows, HOME, END.
    struct VkCase { SDL_Scancode scancode; int expectedVk; const char* name; };
    const VkCase vkCases[] = {
        {SDL_SCANCODE_F1,     VK_F1,      "VK_F1"},
        {SDL_SCANCODE_F2,     VK_F2,      "VK_F2"},
        {SDL_SCANCODE_F3,     VK_F3,      "VK_F3"},
        {SDL_SCANCODE_F4,     VK_F4,      "VK_F4"},
        {SDL_SCANCODE_F5,     VK_F5,      "VK_F5"},
        {SDL_SCANCODE_F6,     VK_F6,      "VK_F6"},
        {SDL_SCANCODE_F7,     VK_F7,      "VK_F7"},
        {SDL_SCANCODE_F8,     VK_F8,      "VK_F8"},
        {SDL_SCANCODE_F9,     VK_F9,      "VK_F9"},
        {SDL_SCANCODE_F11,    VK_F11,     "VK_F11"},
        {SDL_SCANCODE_F12,    VK_F12,     "VK_F12"},
        {SDL_SCANCODE_RETURN, VK_RETURN,  "VK_RETURN"},
        {SDL_SCANCODE_LSHIFT, VK_SHIFT,   "VK_SHIFT"},
        {SDL_SCANCODE_LCTRL,  VK_CONTROL, "VK_CONTROL"},
        {SDL_SCANCODE_PAUSE,  VK_PAUSE,   "VK_PAUSE"},
        {SDL_SCANCODE_LEFT,   VK_LEFT,    "VK_LEFT"},
        {SDL_SCANCODE_RIGHT,  VK_RIGHT,   "VK_RIGHT"},
        {SDL_SCANCODE_UP,     VK_UP,      "VK_UP"},
        {SDL_SCANCODE_DOWN,   VK_DOWN,    "VK_DOWN"},
        {SDL_SCANCODE_HOME,   VK_HOME,    "VK_HOME"},
        {SDL_SCANCODE_END,    VK_END,     "VK_END"},
    };
    for (const auto& vc : vkCases) {
        g_received.clear();
        InjectKeyEvent(sdlWin, true, vc.scancode);
        DrainMessages();
        bool found = false;
        for (auto& m : g_received) {
            if (m.msg == WM_KEYDOWN) {
                found = true;
                if (static_cast<int>(m.wParam) == vc.expectedVk) {
                    printf("[input-pipeline-test] PASS: WM_KEYDOWN delivers %s (0x%02X)\n", vc.name, (unsigned)m.wParam);
                } else {
                    printf("[input-pipeline-test] FAIL: %s: expected wParam=0x%02X, got 0x%02X\n",
                           vc.name, vc.expectedVk, (unsigned)m.wParam);
                    SDL_Quit();
                    return 1;
                }
            }
        }
        if (!found) {
            printf("[input-pipeline-test] FAIL: WM_KEYDOWN not received for %s\n", vc.name);
            SDL_Quit();
            return 1;
        }
        InjectKeyEvent(sdlWin, false, vc.scancode);
        DrainMessages();
    }

    g_received.clear();

    // --- Test 8 (TASK-0049): F10 is delivered as WM_SYSKEYDOWN/WM_SYSKEYUP
    // (not WM_KEYDOWN/WM_KEYUP), matching both games' identical quirk of
    // testing for WM_SYSKEYDOWN/UP with wParam==VK_F10 and treating it like
    // a normal key event (free-eggbert blupi.cpp:457,461; planetblupi
    // blupi.cpp:402,406).
    InjectKeyEvent(sdlWin, true, SDL_SCANCODE_F10);
    DrainMessages();
    if (HasMsg(WM_SYSKEYDOWN, VK_F10, true)) {
        printf("[input-pipeline-test] PASS: WM_SYSKEYDOWN received with wParam==VK_F10\n");
    } else {
        printf("[input-pipeline-test] FAIL: WM_SYSKEYDOWN with wParam==VK_F10 not received\n");
        SDL_Quit();
        return 1;
    }
    if (HasMsg(WM_KEYDOWN)) {
        printf("[input-pipeline-test] FAIL: F10 incorrectly also delivered as WM_KEYDOWN\n");
        SDL_Quit();
        return 1;
    }

    g_received.clear();
    InjectKeyEvent(sdlWin, false, SDL_SCANCODE_F10);
    DrainMessages();
    if (HasMsg(WM_SYSKEYUP, VK_F10, true)) {
        printf("[input-pipeline-test] PASS: WM_SYSKEYUP received with wParam==VK_F10\n");
    } else {
        printf("[input-pipeline-test] FAIL: WM_SYSKEYUP with wParam==VK_F10 not received\n");
        SDL_Quit();
        return 1;
    }

    g_received.clear();

    // --- Test 9 (TASK-0057): sustained-input performance stress test.
    // Input translation runs continuously during gameplay; verify hundreds
    // of injected mouse/keyboard events processed via the real pipeline
    // complete in reasonable wall-clock time with bounded memory (no
    // per-event allocation growth), matching the performance TODO's general
    // hot-path guidance applied to the input path.
    {
        const int kEvents = 2000;
        const Uint64 start = SDL_GetTicks();
        for (int i = 0; i < kEvents; ++i) {
            InjectMouseMotion(sdlWin, static_cast<float>(i % 640), static_cast<float>(i % 480));
            if (i % 4 == 0) InjectKeyEvent(sdlWin, true, SDL_SCANCODE_SPACE);
            if (i % 4 == 1) InjectKeyEvent(sdlWin, false, SDL_SCANCODE_SPACE);
            DrainMessages();
            g_received.clear(); // bound memory growth, matching a real game's per-frame message consumption
        }
        const Uint64 elapsedMs = SDL_GetTicks() - start;
        printf("[input-pipeline-test] %d sustained input events processed in %llu ms\n",
               kEvents, static_cast<unsigned long long>(elapsedMs));
        if (elapsedMs < 5000) {
            printf("[input-pipeline-test] PASS: sustained input pipeline completes %d events well within a generous time bound\n", kEvents);
        } else {
            printf("[input-pipeline-test] FAIL: sustained input pipeline took too long (%llu ms) for %d events\n",
                   static_cast<unsigned long long>(elapsedMs), kEvents);
            SDL_Quit();
            return 1;
        }
    }

    g_received.clear();

    // --- Test 10 (TASK-24H-0407): planetblupi's cheat-code system reads
    // `wParam >= 'A' && wParam <= 'Z'` directly from WM_KEYDOWN
    // (event.cpp:4633), and these same WM_KEYDOWN events are persisted
    // verbatim to demo files by DemoRecEvent -- so an incorrect letter-key
    // VK mapping would be both a live-gameplay bug and a demo-file-
    // compatibility bug. Despite Test 7's broad VK_* coverage above, it
    // covers zero letter scancodes. Sample across the full A-Z range.
    {
        const VkCase letterCases[] = {
            {SDL_SCANCODE_A, 'A', "VK 'A'"},
            {SDL_SCANCODE_M, 'M', "VK 'M'"},
            {SDL_SCANCODE_Z, 'Z', "VK 'Z'"},
            {SDL_SCANCODE_B, 'B', "VK 'B'"},
            {SDL_SCANCODE_Y, 'Y', "VK 'Y'"},
        };
        for (const auto& lc : letterCases) {
            g_received.clear();
            InjectKeyEvent(sdlWin, true, lc.scancode);
            DrainMessages();
            if (HasMsg(WM_KEYDOWN, static_cast<WPARAM>(lc.expectedVk), true)) {
                printf("[input-pipeline-test] PASS: WM_KEYDOWN delivers %s (0x%02X)\n", lc.name, (unsigned)lc.expectedVk);
            } else {
                printf("[input-pipeline-test] FAIL: %s not delivered with the expected wParam\n", lc.name);
                SDL_Quit();
                return 1;
            }
            InjectKeyEvent(sdlWin, false, lc.scancode);
            DrainMessages();
        }
    }

    g_received.clear();

    // --- Test 11 (TASK-24H-0406): SdlScancodeToVK silently returns 0 for
    // any scancode not in its explicit table, and the caller drops the
    // event entirely rather than forwarding a garbage VK code -- confirmed
    // correct, safe-default behavior, but previously only exercised via
    // the mapped-key cases above, never asserted as a positive claim for
    // an unmapped key. SDL_SCANCODE_CAPSLOCK is confirmed absent from the
    // scancode->VK switch in src/internal/FreeApiMessageQueue.cpp.
    {
        InjectKeyEvent(sdlWin, true, SDL_SCANCODE_CAPSLOCK);
        DrainMessages();
        if (!HasMsg(WM_KEYDOWN) && !HasMsg(WM_SYSKEYDOWN)) {
            printf("[input-pipeline-test] PASS: an unmapped scancode (CAPSLOCK) produces no WM_KEYDOWN/WM_SYSKEYDOWN message\n");
        } else {
            printf("[input-pipeline-test] FAIL: an unmapped scancode (CAPSLOCK) unexpectedly produced a key message\n");
            SDL_Quit();
            return 1;
        }

        g_received.clear();
        InjectKeyEvent(sdlWin, false, SDL_SCANCODE_CAPSLOCK);
        DrainMessages();
        if (!HasMsg(WM_KEYUP) && !HasMsg(WM_SYSKEYUP)) {
            printf("[input-pipeline-test] PASS: an unmapped scancode (CAPSLOCK) produces no WM_KEYUP/WM_SYSKEYUP message\n");
        } else {
            printf("[input-pipeline-test] FAIL: an unmapped scancode (CAPSLOCK) unexpectedly produced a key-up message\n");
            SDL_Quit();
            return 1;
        }
    }

    g_received.clear();

    // --- Test 12 (TASK-24H-0408): WM_CHAR forwards only ASCII bytes
    // (<128) from SDL_EVENT_TEXT_INPUT; non-ASCII UTF-8 bytes are silently
    // dropped, and must not corrupt or skip the surrounding ASCII bytes.
    // "ab" + U+00E9 ('e' with acute accent, 2-byte UTF-8: 0xC3 0xA9) + "cd".
    {
        InjectTextInput(sdlWin, "ab\xC3\xA9""cd");
        DrainMessages();

        std::vector<WPARAM> charWParams;
        for (auto& m : g_received) {
            if (m.msg == WM_CHAR) charWParams.push_back(m.wParam);
        }

        const std::vector<WPARAM> expected = {'a', 'b', 'c', 'd'};
        if (charWParams == expected) {
            printf("[input-pipeline-test] PASS: WM_CHAR forwards exactly the ASCII bytes ('a','b','c','d'), in order, dropping the 2-byte UTF-8 sequence between them\n");
        } else {
            printf("[input-pipeline-test] FAIL: WM_CHAR sequence mismatch -- expected 4 ASCII chars, got %zu messages\n", charWParams.size());
            for (auto w : charWParams) printf("  wParam=0x%02X\n", (unsigned)w);
            SDL_Quit();
            return 1;
        }
    }

    printf("[input-pipeline-test] ALL TESTS PASSED\n");
    SDL_Quit();
    return 0;
}
