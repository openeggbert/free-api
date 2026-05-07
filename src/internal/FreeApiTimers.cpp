#include "internal/FreeApiTimers.hpp"

namespace FreeApi::Internal {

std::unordered_map<UINT_PTR, WinTimer>  g_winTimers;
std::mutex                               g_winTimerMutex;
std::unordered_map<UINT, MmTimerEntry>  g_mmTimers;
std::mutex                               g_mmTimerMutex;
std::unordered_set<UINT>                g_activeTimerIds;
std::atomic<UINT>                       g_nextTimerId{1};

} // namespace FreeApi::Internal
