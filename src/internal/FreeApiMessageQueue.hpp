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

} // namespace FreeApi::Internal
