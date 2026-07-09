#pragma once

#include "windows.h"
#include "mmsystem.h"
#include <SDL3/SDL.h>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <cstdint>

namespace FreeApi::Internal {

// WinAPI-style timer registered via SetTimer()
struct WinTimer {
    HWND     hwnd         = NULL;
    UINT_PTR id           = 0;
    UINT     intervalMs   = 0;
    uint64_t lastFireTick = 0; // SDL_GetTicks() when last fired
};

// Multimedia timer registered via timeSetEvent()
struct MmTimerEntry {
    SDL_TimerID     sdlId    = 0;
    LPTIMECALLBACK  callback = nullptr;
    DWORD_PTR       user     = 0;
    UINT            mmId     = 0;
};

extern std::unordered_map<UINT_PTR, WinTimer>  g_winTimers;
extern std::mutex                               g_winTimerMutex;
extern std::unordered_map<UINT, MmTimerEntry>  g_mmTimers;
extern std::mutex                               g_mmTimerMutex;
extern std::unordered_set<UINT>                g_activeTimerIds;
// TASK-24H-0502: deliberately shared between SetTimer (WinUser) and
// timeSetEvent (WinMM) -- two otherwise fully independent timer mechanisms
// with their own separate maps/mutexes above. Sharing this one counter
// guarantees ID uniqueness if a caller ever mixed both APIs; it is the
// only coupling between them. Do not split into two independent
// per-mechanism counters -- confirmed intentional, not an accidental coupling.
extern std::atomic<UINT>                       g_nextTimerId;

} // namespace FreeApi::Internal
