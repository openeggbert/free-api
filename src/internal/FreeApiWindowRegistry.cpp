#include "internal/FreeApiWindowRegistry.hpp"
#include "internal/FreeApiMessageQueue.hpp"

namespace FreeApi::Internal {

std::unordered_map<std::string, WNDPROC>     g_registeredClasses;
std::unordered_map<HWND, WNDPROC>            g_windowProcedures;
std::unordered_map<HWND, FreeApiWindowState> g_freeApiWindowStates;
std::unordered_map<uint32_t, HWND>           g_windowsById;
HWND                                          g_focusWindow = NULL;

HWND FindWindowById(const uint32_t windowId)
{
    const auto it = g_windowsById.find(windowId);
    return (it != g_windowsById.end()) ? it->second : NULL;
}

HWND GetActiveWindow()
{
    if (g_focusWindow) return g_focusWindow;
    // Fallback: use the first (and usually only) registered window.
    if (!g_windowProcedures.empty()) {
        HWND hw = g_windowProcedures.begin()->first;
        InputLog("GetActiveWindow fallback -> hwnd=%p (no focus window set yet)", (void*)hw);
        return hw;
    }
    return NULL;
}

} // namespace FreeApi::Internal
