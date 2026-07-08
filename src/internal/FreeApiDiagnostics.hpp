#pragma once

#include <atomic>
#include <cstdint>

namespace FreeApi::Internal {

// Diagnostic counters (only meaningful when diagnostics are enabled)
extern std::atomic<uint64_t> g_diagUpdateCoalesced;
extern std::atomic<uint64_t> g_diagMouseMoveCoalesced;
extern std::atomic<uint64_t> g_diagTimerCoalesced;
extern std::atomic<uint64_t> g_diagQueueHighWater;

extern std::atomic<uint64_t> g_diagMessagesPosted;
extern std::atomic<uint64_t> g_diagMessagesDispatched;
extern std::atomic<uint64_t> g_diagSdlEventsProcessed;
extern std::atomic<uint64_t> g_diagWmUpdatePosted;
extern std::atomic<uint64_t> g_diagWmUpdateDispatched;
extern std::atomic<int64_t>  g_diagWmUpdatePending;
extern std::atomic<int64_t>  g_diagSdlSurfaces;
extern std::atomic<int64_t>  g_diagSdlSurfacesEver;
extern std::atomic<int64_t>  g_diagSdlSurfacesDestroyed;
extern std::atomic<int64_t>  g_diagCompatBitmaps;
extern std::atomic<int64_t>  g_diagCompatBitmapsEver;
extern std::atomic<int64_t>  g_diagCompatBitmapsDestroyed;
extern std::atomic<int64_t>  g_diagCompatDcs;
extern std::atomic<int64_t>  g_diagCompatDcsEver;
extern std::atomic<int64_t>  g_diagCompatDcsDestroyed;
extern std::atomic<int64_t>  g_diagCompatBitmapPixelCapacityBytes;
extern std::atomic<int64_t>  g_diagCompatBitmapPixelCapacityHighWaterBytes;

void AdjustDiagLiveBytes(std::atomic<int64_t>& liveCounter,
                         std::atomic<int64_t>& highWaterCounter,
                         int64_t delta);

bool FreeApiDiagnosticsEnabled();

// TASK-24H-1105: intentionally identical to FreeApiDiagnosticsEnabled()
// above -- same cached O(1) static-int check, not a distinct or cheaper
// code path despite the "Fast" name. Exists only to mark, semantically,
// that a call site (message dispatch, message-queue coalescing -- see
// src/winuser_message.cpp and src/internal/FreeApiMessageQueue.cpp) is on a
// genuinely hot path and was deliberately checked for cost, not to signal a
// different implementation. Do not "fix" this by making it actually
// faster/differently cached without an evidenced need, and do not remove it
// or its call sites without a separate, explicitly-approved cleanup task.
bool FreeApiDiagnosticsFastEnabled();
bool FreeApiGdiDebugEnabled();
long FreeApiReadRssKB();
void FreeApiDiagSnapshot(const char* tag);
void FreeApiDiagTick();

} // namespace FreeApi::Internal
