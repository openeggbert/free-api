#include "windows.h"
#include "internal/FreeApiTimers.hpp"
#include "internal/FreeApiDiagnostics.hpp"

#include <SDL3/SDL.h>

using namespace FreeApi::Internal;

extern "C" {

UINT_PTR WINAPI SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, void* lpTimerFunc)
{
    (void)lpTimerFunc;

    if (nIDEvent == 0) {
        // TASK-24H-1250: relaxed, matching every other counter atomic in
        // the codebase -- this only needs to hand out a unique value, not
        // synchronize any other data.
        nIDEvent = g_nextTimerId.fetch_add(1, std::memory_order_relaxed);
    }

    WinTimer wt;
    wt.hwnd        = hWnd;
    wt.id          = nIDEvent;
    wt.intervalMs  = (uElapse > 0) ? uElapse : 1;
    wt.lastFireTick = SDL_GetTicks();

    {
        std::lock_guard<std::mutex> lock(g_winTimerMutex);
        g_winTimers[nIDEvent] = wt;
    }
    if (FreeApiDiagnosticsEnabled()) {
        SDL_Log("free-api SetTimer: hwnd=%p id=%lu elapse=%u ms",
                static_cast<void*>(hWnd),
                static_cast<unsigned long>(nIDEvent),
                static_cast<unsigned>(uElapse));
    }
    FreeApiDiagSnapshot("timer-set");
    return nIDEvent;
}

BOOL WINAPI KillTimer(HWND hWnd, UINT_PTR uIDEvent)
{
    (void)hWnd;
    {
        std::lock_guard<std::mutex> lock(g_winTimerMutex);
        g_winTimers.erase(uIDEvent);
    }
    if (FreeApiDiagnosticsEnabled()) {
        SDL_Log("free-api KillTimer: hwnd=%p id=%lu",
                static_cast<void*>(hWnd),
                static_cast<unsigned long>(uIDEvent));
    }
    FreeApiDiagSnapshot("timer-kill");
    // TASK-24H-0509: always returns TRUE, even for an unknown/already-removed
    // timer ID -- real Win32 returns FALSE (0) in that case. Confirmed both
    // target games' every KillTimer call site is fire-and-forget (bare
    // statement, return value discarded, from their WM_DESTROY handlers).
    // Do not change to return FALSE for unknown IDs without new evidence a
    // real caller needs it.
    return TRUE;
}

} // extern "C"
