#pragma once

#include "windows.h"
#include <deque>
#include <mutex>
#include <atomic>
#include <cstdint>

namespace FreeApi::Internal {

// The internal WM update signal (Free API-internal only, not a real WinAPI event)
constexpr UINT kDiagWmUpdate = WM_USER + 1;

extern std::deque<MSG>   g_messageQueue;
extern std::mutex        g_messageQueueMutex;
extern std::atomic_bool  g_updateMessagePending;

// Mouse button state tracked for MK_* wParam in WM_MOUSEMOVE
extern WPARAM g_mouseButtons;

// Optional debug logging for input translation (set FREE_API_DEBUG_INPUT=1 at runtime)
extern bool g_debugInput;

void InputLog(const char* fmt, ...);

void PushMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
void PumpSdlEvents();

// TASK-24H-0404: shared MK_SHIFT/MK_CONTROL modifier-flag helper, extracted
// from what used to be duplicated inline logic in the mouse-motion and
// mouse-button SDL event handlers. `keys` is an SDL keyboard-state array as
// returned by SDL_GetKeyboardState() (may be null); `base` is the button-
// state bits (e.g. g_mouseButtons) to OR the modifier flags into. Exposed
// here (not static in the .cpp) so tests/test_winuser_regressions.cpp can
// exercise it directly with a synthetic keystate array -- SDL_PushEvent-
// injected key events do not update SDL_GetKeyboardState()'s real array in
// this headless test environment, so the live end-to-end mouse+modifier
// path can't be exercised via event injection; this at least unit-tests the
// OR-logic itself deterministically.
WPARAM ApplyKeyboardModifierFlags(const bool* keys, WPARAM base);

} // namespace FreeApi::Internal
