#pragma once

#include "windows.h"
#include <unordered_map>
#include <string>
#include <cstdint>

struct SDL_Window;

namespace FreeApi::Internal {

struct FreeApiWindowState {
    int  width        = 0;
    int  height       = 0;
    bool isFullscreen = false;
};

// TASK-0004 (maintainability sweep, 2026-07-09): these five globals are
// deliberately unsynchronized (no mutex) -- safe ONLY because this
// project's documented single-live-window assumption ("Single live window
// assumption" section, docs/out-of-scope.md) means they're only ever
// touched from the single main/game thread. Do not read/write any of these
// from a new background thread (the way src/MidiMusic.cpp's mixer thread
// was added) without first adding real synchronization -- there is no
// local warning if you do, only a ThreadSanitizer report after the fact.
extern std::unordered_map<std::string, WNDPROC>     g_registeredClasses;
extern std::unordered_map<HWND, WNDPROC>            g_windowProcedures;
extern std::unordered_map<HWND, FreeApiWindowState> g_freeApiWindowStates;
extern std::unordered_map<uint32_t, HWND>           g_windowsById;
extern HWND                                          g_focusWindow;

HWND FindWindowById(uint32_t windowId);
HWND GetActiveWindow();

} // namespace FreeApi::Internal
