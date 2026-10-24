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

HWND ResolveWindowHandle(const DWORD_PTR value)
{
    const HWND exact = reinterpret_cast<HWND>(value);
    if (value == 0 || g_windowProcedures.count(exact) != 0) return exact;
    if ((static_cast<uint64_t>(value) >> 32) == 0) {
        for (const auto& entry : g_windowProcedures) {
            if ((reinterpret_cast<uintptr_t>(entry.first) & 0xFFFFFFFFu) == value) {
                return entry.first;
            }
        }
    }
    return exact;
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
