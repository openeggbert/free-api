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

extern std::unordered_map<std::string, WNDPROC>     g_registeredClasses;
extern std::unordered_map<HWND, WNDPROC>            g_windowProcedures;
extern std::unordered_map<HWND, FreeApiWindowState> g_freeApiWindowStates;
extern std::unordered_map<uint32_t, HWND>           g_windowsById;
extern HWND                                          g_focusWindow;

HWND FindWindowById(uint32_t windowId);
HWND GetActiveWindow();

} // namespace FreeApi::Internal
