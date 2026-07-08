#include "internal/FreeApiMessageQueue.hpp"
#include "internal/FreeApiDiagnostics.hpp"
#include "internal/FreeApiWindowRegistry.hpp"

#include <SDL3/SDL.h>
#include <cstdarg>
#include <cstdio>

namespace FreeApi::Internal {

std::deque<MSG>  g_messageQueue;
std::mutex       g_messageQueueMutex;
std::atomic_bool g_updateMessagePending{false};
WPARAM           g_mouseButtons = 0;
std::atomic_bool g_debugInput{false};

void InputLog(const char* fmt, ...)
{
    if (!g_debugInput) return;
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    SDL_Log("[free-api input] %s", buf);
}

WPARAM ApplyKeyboardModifierFlags(const bool* keys, WPARAM base)
{
    WPARAM wp = base;
    if (keys && (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT])) wp |= MK_SHIFT;
    if (keys && (keys[SDL_SCANCODE_LCTRL]  || keys[SDL_SCANCODE_RCTRL]))  wp |= MK_CONTROL;
    return wp;
}

void PushMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    const bool diagEnabled = FreeApiDiagnosticsFastEnabled();

    // ---- Coalescing for the internal update wake-up message (WM_USER + 1).
    // This is a Free API-internal redraw signal, not a real WinAPI event.
    // If one is already pending, drop the new one to avoid backlog and
    // input-lag (real input events would queue up behind redundant updates).
    if (message == kDiagWmUpdate) {
        bool expected = false;
        if (!g_updateMessagePending.compare_exchange_strong(
                expected, true, std::memory_order_acq_rel)) {
            if (diagEnabled) {
                g_diagUpdateCoalesced.fetch_add(1, std::memory_order_relaxed);
            }
            return; // already one pending — coalesce
        }
    }

    if (diagEnabled) {
        g_diagMessagesPosted.fetch_add(1, std::memory_order_relaxed);
        if (message == kDiagWmUpdate) {
            g_diagWmUpdatePosted.fetch_add(1, std::memory_order_relaxed);
            // pending count = number of update msgs in queue (always 0 or 1
            // after coalescing, but kept compatible with previous semantics).
            g_diagWmUpdatePending.fetch_add(1, std::memory_order_relaxed);
        }
    }

    MSG msg{};
    msg.hwnd    = hwnd;
    msg.message = message;
    msg.wParam  = wParam;
    msg.lParam  = lParam;
    msg.time    = static_cast<DWORD>(SDL_GetTicks());
    msg.pt      = {0, 0};
    size_t qsize;
    {
        std::lock_guard<std::mutex> lock(g_messageQueueMutex);

        // ---- Coalescing for WM_MOUSEMOVE: if there is already a pending
        // WM_MOUSEMOVE for the same hwnd, update it in place rather than
        // appending another one. Mouse motion events fire much faster than
        // the game can consume them, so without coalescing the queue grows
        // and every real click/keypress is delayed behind stale motion.
        if (message == WM_MOUSEMOVE) {
            for (auto it = g_messageQueue.rbegin(); it != g_messageQueue.rend(); ++it) {
                if (it->message == WM_MOUSEMOVE && it->hwnd == hwnd) {
                    it->wParam = wParam;
                    it->lParam = lParam;
                    it->time   = msg.time;
                    if (diagEnabled) {
                        g_diagMouseMoveCoalesced.fetch_add(1, std::memory_order_relaxed);
                    }
                    qsize = g_messageQueue.size();
                    goto coalesced;
                }
            }
        }

        // ---- Coalescing for WM_TIMER with same hwnd+timer id (wParam).
        // WM_TIMER on real Windows is not a hard real-time ticker; piling up
        // stale timer messages just produces jitter.
        if (message == WM_TIMER) {
            for (auto it = g_messageQueue.rbegin(); it != g_messageQueue.rend(); ++it) {
                if (it->message == WM_TIMER && it->hwnd == hwnd && it->wParam == wParam) {
                    if (diagEnabled) {
                        g_diagTimerCoalesced.fetch_add(1, std::memory_order_relaxed);
                    }
                    qsize = g_messageQueue.size();
                    goto coalesced;
                }
            }
        }

        g_messageQueue.push_back(msg);
        qsize = g_messageQueue.size();
    coalesced:;
    }
    if (diagEnabled) {
        uint64_t hw = g_diagQueueHighWater.load(std::memory_order_relaxed);
        while (qsize > hw &&
               !g_diagQueueHighWater.compare_exchange_weak(
                   hw, qsize, std::memory_order_relaxed)) {
        }
    }
    InputLog("ENQUEUE hwnd=%p msg=0x%04X wParam=0x%X lParam=0x%X qsize=%d",
        (void*)hwnd, message, (unsigned)wParam, (unsigned)lParam, (int)qsize);
    FreeApiDiagTick();
}

// Minimal SDL scancode -> WinAPI VK_* mapping (local to this translation unit)
static WPARAM SdlScancodeToVK(SDL_Scancode sc)
{
    switch (sc) {
        // Letters: SDL_SCANCODE_A=4 .. Z=29, WinAPI 'A'=0x41 .. 'Z'=0x5A
        case SDL_SCANCODE_A: return 'A';
        case SDL_SCANCODE_B: return 'B';
        case SDL_SCANCODE_C: return 'C';
        case SDL_SCANCODE_D: return 'D';
        case SDL_SCANCODE_E: return 'E';
        case SDL_SCANCODE_F: return 'F';
        case SDL_SCANCODE_G: return 'G';
        case SDL_SCANCODE_H: return 'H';
        case SDL_SCANCODE_I: return 'I';
        case SDL_SCANCODE_J: return 'J';
        case SDL_SCANCODE_K: return 'K';
        case SDL_SCANCODE_L: return 'L';
        case SDL_SCANCODE_M: return 'M';
        case SDL_SCANCODE_N: return 'N';
        case SDL_SCANCODE_O: return 'O';
        case SDL_SCANCODE_P: return 'P';
        case SDL_SCANCODE_Q: return 'Q';
        case SDL_SCANCODE_R: return 'R';
        case SDL_SCANCODE_S: return 'S';
        case SDL_SCANCODE_T: return 'T';
        case SDL_SCANCODE_U: return 'U';
        case SDL_SCANCODE_V: return 'V';
        case SDL_SCANCODE_W: return 'W';
        case SDL_SCANCODE_X: return 'X';
        case SDL_SCANCODE_Y: return 'Y';
        case SDL_SCANCODE_Z: return 'Z';
        // Digits
        case SDL_SCANCODE_0: return '0';
        case SDL_SCANCODE_1: return '1';
        case SDL_SCANCODE_2: return '2';
        case SDL_SCANCODE_3: return '3';
        case SDL_SCANCODE_4: return '4';
        case SDL_SCANCODE_5: return '5';
        case SDL_SCANCODE_6: return '6';
        case SDL_SCANCODE_7: return '7';
        case SDL_SCANCODE_8: return '8';
        case SDL_SCANCODE_9: return '9';
        // Navigation
        case SDL_SCANCODE_LEFT:      return 0x25; // VK_LEFT
        case SDL_SCANCODE_UP:        return 0x26; // VK_UP
        case SDL_SCANCODE_RIGHT:     return 0x27; // VK_RIGHT
        case SDL_SCANCODE_DOWN:      return 0x28; // VK_DOWN
        case SDL_SCANCODE_HOME:      return 0x24; // VK_HOME
        case SDL_SCANCODE_END:       return 0x23; // VK_END
        case SDL_SCANCODE_PAGEUP:    return 0x21; // VK_PRIOR
        case SDL_SCANCODE_PAGEDOWN:  return 0x22; // VK_NEXT
        case SDL_SCANCODE_INSERT:    return 0x2D; // VK_INSERT
        case SDL_SCANCODE_DELETE:    return 0x2E; // VK_DELETE
        // Control keys
        case SDL_SCANCODE_RETURN:    return 0x0D; // VK_RETURN
        case SDL_SCANCODE_KP_ENTER:  return 0x0D; // VK_RETURN
        case SDL_SCANCODE_ESCAPE:    return 0x1B; // VK_ESCAPE
        case SDL_SCANCODE_SPACE:     return 0x20; // VK_SPACE
        case SDL_SCANCODE_TAB:       return 0x09; // VK_TAB
        case SDL_SCANCODE_BACKSPACE: return 0x08; // VK_BACK
        case SDL_SCANCODE_LSHIFT:
        case SDL_SCANCODE_RSHIFT:    return 0x10; // VK_SHIFT
        case SDL_SCANCODE_LCTRL:
        case SDL_SCANCODE_RCTRL:     return 0x11; // VK_CONTROL
        case SDL_SCANCODE_LALT:
        case SDL_SCANCODE_RALT:      return 0x12; // VK_MENU
        case SDL_SCANCODE_PAUSE:     return 0x13; // VK_PAUSE
        // Function keys
        case SDL_SCANCODE_F1:  return 0x70;
        case SDL_SCANCODE_F2:  return 0x71;
        case SDL_SCANCODE_F3:  return 0x72;
        case SDL_SCANCODE_F4:  return 0x73;
        case SDL_SCANCODE_F5:  return 0x74;
        case SDL_SCANCODE_F6:  return 0x75;
        case SDL_SCANCODE_F7:  return 0x76;
        case SDL_SCANCODE_F8:  return 0x77;
        case SDL_SCANCODE_F9:  return 0x78;
        case SDL_SCANCODE_F10: return 0x79;
        case SDL_SCANCODE_F11: return 0x7A;
        case SDL_SCANCODE_F12: return 0x7B;
        default: return 0;
    }
}

void PumpSdlEvents()
{
    // Make sure SDL has consumed pending OS events before we poll.
    SDL_PumpEvents();
    SDL_Event event;
    const bool diagEnabled = FreeApiDiagnosticsFastEnabled();
    while (SDL_PollEvent(&event)) {
        if (diagEnabled) g_diagSdlEventsProcessed.fetch_add(1, std::memory_order_relaxed);

        // Convert mouse/touch coordinates from window space to renderer logical
        // space. When SDL_SetRenderLogicalPresentation is active (e.g. letterbox
        // mode), this remaps window pixels to the game's logical resolution.
        if (event.type == SDL_EVENT_MOUSE_MOTION ||
            event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
            event.type == SDL_EVENT_MOUSE_BUTTON_UP ||
            event.type == SDL_EVENT_FINGER_DOWN ||
            event.type == SDL_EVENT_FINGER_UP ||
            event.type == SDL_EVENT_FINGER_MOTION) {
            // Capture raw coordinates before conversion for diagnostic logging.
            float rawX = 0, rawY = 0;
            static int inputDiagCount = 0;
            if (event.type == SDL_EVENT_MOUSE_MOTION) { rawX = event.motion.x; rawY = event.motion.y; }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) { rawX = event.button.x; rawY = event.button.y; }
            // Find the renderer associated with this event's window.
            SDL_Window* evtWin = SDL_GetWindowFromEvent(&event);
            if (evtWin) {
                SDL_Renderer* ren = SDL_GetRenderer(evtWin);
                if (ren) {
                    SDL_ConvertEventToRenderCoordinates(ren, &event);
                }
            }
            // FREE_DIRECT_INPUT diagnostic for first 20 events. Gated behind
            // g_debugInput (FREE_API_DEBUG_INPUT=1) so normal gameplay does
            // not log on every mouse/touch event.
            if (g_debugInput && inputDiagCount < 20) {
                float mappedX = 0, mappedY = 0;
                if (event.type == SDL_EVENT_MOUSE_MOTION) { mappedX = event.motion.x; mappedY = event.motion.y; }
                else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) { mappedX = event.button.x; mappedY = event.button.y; }
                SDL_Log("FREE_DIRECT_INPUT: raw=%.1f,%.1f mapped=%.1f,%.1f evtType=0x%X evtWin=%p ren=%p",
                        rawX, rawY, mappedX, mappedY, event.type,
                        (void*)evtWin, evtWin ? (void*)SDL_GetRenderer(evtWin) : nullptr);
                inputDiagCount++;
            }
        }
        switch (event.type) {
            case SDL_EVENT_QUIT:
                InputLog("SDL_EVENT_QUIT -> WM_QUIT");
#if defined(__ANDROID__)
                SDL_Log("FREEAPI_ANDROID: SDL event SDL_EVENT_QUIT translated to WM_QUIT");
#endif
                PushMessage(NULL, WM_QUIT, 0, 0);
                break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
                HWND hwnd = FindWindowById(event.window.windowID);
                InputLog("SDL_EVENT_WINDOW_CLOSE_REQUESTED hwnd=%p -> WM_CLOSE", (void*)hwnd);
#if defined(__ANDROID__)
                SDL_Log("FREEAPI_ANDROID: SDL event SDL_EVENT_WINDOW_CLOSE_REQUESTED windowID=%u hwnd=%p translated to WM_CLOSE",
                        (unsigned)event.window.windowID, (void*)hwnd);
#endif
                PushMessage(hwnd, WM_CLOSE, 0, 0);
                break;
            }
            case SDL_EVENT_WINDOW_FOCUS_GAINED: {
                HWND hwnd = FindWindowById(event.window.windowID);
                if (hwnd) g_focusWindow = hwnd;
                InputLog("SDL_EVENT_WINDOW_FOCUS_GAINED hwnd=%p -> WM_ACTIVATEAPP(1)", (void*)hwnd);
#if defined(__ANDROID__)
                SDL_Log("FREEAPI_ANDROID: SDL event SDL_EVENT_WINDOW_FOCUS_GAINED windowID=%u hwnd=%p translated to WM_ACTIVATEAPP(1)",
                        (unsigned)event.window.windowID, (void*)hwnd);
#endif
                PushMessage(hwnd, WM_ACTIVATEAPP, 1, 0);
                break;
            }
            case SDL_EVENT_WINDOW_FOCUS_LOST: {
                // Do NOT send WM_ACTIVATEAPP(0) — sending it causes the game to set
                // g_bActive=FALSE which stops rendering and input processing.
                // SDL focus-lost events may fire spuriously on startup or in certain
                // desktop environments. The game window should remain active as long as
                // it is visible. If real deactivation is needed in the future, add a
                // configurable delay or a user-controlled flag.
                HWND hwnd = FindWindowById(event.window.windowID);
                InputLog("SDL_EVENT_WINDOW_FOCUS_LOST hwnd=%p -> suppressed WM_ACTIVATEAPP(0)", (void*)hwnd);
#if defined(__ANDROID__)
                SDL_Log("FREEAPI_ANDROID: SDL event SDL_EVENT_WINDOW_FOCUS_LOST windowID=%u hwnd=%p suppressed (WM_ACTIVATEAPP(0) not sent)",
                        (unsigned)event.window.windowID, (void*)hwnd);
#endif
                (void)hwnd;
                break;
            }
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP: {
                // Ignore repeat keydowns (SDL sends repeat events; pass them through
                // as WM_KEYDOWN repeats matching Windows behavior)
                HWND hwnd = GetActiveWindow();
                if (!hwnd) break;
                WPARAM vk = SdlScancodeToVK(event.key.scancode);
                if (vk == 0) break; // unmapped key
                UINT msg = (event.type == SDL_EVENT_KEY_DOWN) ? WM_KEYDOWN : WM_KEYUP;
                // lParam: simplified — repeat count=1, scan code in bits 16-23
                LPARAM lp = (LPARAM)(event.key.scancode << 16) | 1;
                if (event.type == SDL_EVENT_KEY_UP) {
                    lp |= (1u << 30) | (1u << 31); // transition/previous-state bits
                }
                // Map F10 as WM_SYSKEYDOWN/UP to match Windows behavior
                if (vk == 0x79 /* VK_F10 */) {
                    msg = (event.type == SDL_EVENT_KEY_DOWN) ? WM_SYSKEYDOWN : WM_SYSKEYUP;
                }
                InputLog("%s vk=0x%02X hwnd=%p -> msg=0x%04X",
                    event.type == SDL_EVENT_KEY_DOWN ? "KEY_DOWN" : "KEY_UP",
                    (unsigned)vk, (void*)hwnd, msg);
                PushMessage(hwnd, msg, vk, lp);
                break;
            }
            case SDL_EVENT_TEXT_INPUT: {
                // Feed WM_CHAR for text input (name entry screens)
                HWND hwnd = GetActiveWindow();
                if (!hwnd) break;
                const char* text = event.text.text;
                while (*text) {
                    unsigned char ch = (unsigned char)*text++;
                    if (ch < 128) {
                        InputLog("TEXT_INPUT char=0x%02X hwnd=%p -> WM_CHAR", ch, (void*)hwnd);
                        PushMessage(hwnd, WM_CHAR, (WPARAM)ch, 1);
                    }
                }
                break;
            }
            case SDL_EVENT_MOUSE_MOTION: {
                HWND hwnd = FindWindowById(event.motion.windowID);
                if (!hwnd) hwnd = GetActiveWindow();
                InputLog("SDL_EVENT_MOUSE_MOTION windowID=%u resolvedHwnd=%p",
                    (unsigned)event.motion.windowID, (void*)hwnd);
                if (!hwnd) break;

                // Coordinates are already in logical space after SDL_ConvertEventToRenderCoordinates.
                int x = (int)event.motion.x;
                int y = (int)event.motion.y;

                // lParam encodes client-area x/y
                LPARAM lp = (LPARAM)(((WORD)(DWORD_PTR)y << 16) | ((WORD)(DWORD_PTR)x));

                // wParam must carry live MK_SHIFT/MK_CONTROL modifier state in
                // addition to button-down state: planetblupi's WM_MOUSEMOVE
                // handler (CEvent::PlayMove) reads wParam&MK_SHIFT every move
                // to support shift-drag cell highlighting, not just at the
                // initial button press.
                WPARAM wp = ApplyKeyboardModifierFlags(SDL_GetKeyboardState(nullptr), g_mouseButtons);

                InputLog("MOUSE_MOTION x=%d y=%d wParam=0x%X -> WM_MOUSEMOVE",
                    x, y, (unsigned)wp);
                PushMessage(hwnd, WM_MOUSEMOVE, wp, lp);
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                HWND hwnd = FindWindowById(event.button.windowID);
                if (!hwnd) hwnd = GetActiveWindow();
                InputLog("SDL_EVENT_MOUSE_BUTTON windowID=%u resolvedHwnd=%p",
                    (unsigned)event.button.windowID, (void*)hwnd);
                if (!hwnd) break;

                // Coordinates are already in logical space after SDL_ConvertEventToRenderCoordinates.
                int x = (int)event.button.x;
                int y = (int)event.button.y;

                LPARAM lp = (LPARAM)(((WORD)(DWORD_PTR)y << 16) | ((WORD)(DWORD_PTR)x));
                bool isDown = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                UINT msg = 0;
                WPARAM mk = 0;
                switch (event.button.button) {
                    case SDL_BUTTON_LEFT:
                        msg = isDown ? WM_LBUTTONDOWN : WM_LBUTTONUP;
                        mk = MK_LBUTTON;
                        break;
                    case SDL_BUTTON_RIGHT:
                        msg = isDown ? WM_RBUTTONDOWN : WM_RBUTTONUP;
                        mk = MK_RBUTTON;
                        break;
                    case SDL_BUTTON_MIDDLE:
                        msg = isDown ? WM_MBUTTONDOWN : WM_MBUTTONUP;
                        mk = MK_MBUTTON;
                        break;
                    default:
                        break;
                }
                if (!msg) break;
                if (isDown) {
                    g_mouseButtons |= mk;
                } else {
                    g_mouseButtons &= ~mk;
                }
                // Build wParam: current button state + keyboard modifiers
                WPARAM wp = ApplyKeyboardModifierFlags(SDL_GetKeyboardState(nullptr), g_mouseButtons);
                InputLog("MOUSE_BTN %s btn=%d x=%d y=%d wParam=0x%X -> msg=0x%04X",
                    isDown ? "DOWN" : "UP", event.button.button, x, y, (unsigned)wp, msg);
                PushMessage(hwnd, msg, wp, lp);
                break;
            }
            default:
                if (g_debugInput) {
                    InputLog("SDL_EVENT type=0x%X (unhandled)", event.type);
                }
                break;
        }
    }
}

} // namespace FreeApi::Internal
