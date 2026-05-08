#include "internal/FreeApiDiagnostics.hpp"
#include "internal/FreeApiMessageQueue.hpp"
#include "internal/FreeApiTimers.hpp"

#include <SDL3/SDL.h>

#include <cstring>
#include <cstdio>

#include "platform/PlatformProcessInfo.hpp"

namespace FreeApi::Internal {

std::atomic<uint64_t> g_diagUpdateCoalesced{0};
std::atomic<uint64_t> g_diagMouseMoveCoalesced{0};
std::atomic<uint64_t> g_diagTimerCoalesced{0};
std::atomic<uint64_t> g_diagQueueHighWater{0};

std::atomic<uint64_t> g_diagMessagesPosted{0};
std::atomic<uint64_t> g_diagMessagesDispatched{0};
std::atomic<uint64_t> g_diagSdlEventsProcessed{0};
std::atomic<uint64_t> g_diagWmUpdatePosted{0};
std::atomic<uint64_t> g_diagWmUpdateDispatched{0};
std::atomic<int64_t>  g_diagWmUpdatePending{0};
std::atomic<int64_t>  g_diagSdlSurfaces{0};
std::atomic<int64_t>  g_diagSdlSurfacesEver{0};
std::atomic<int64_t>  g_diagSdlSurfacesDestroyed{0};
std::atomic<int64_t>  g_diagCompatBitmaps{0};
std::atomic<int64_t>  g_diagCompatBitmapsEver{0};
std::atomic<int64_t>  g_diagCompatBitmapsDestroyed{0};
std::atomic<int64_t>  g_diagCompatDcs{0};
std::atomic<int64_t>  g_diagCompatDcsEver{0};
std::atomic<int64_t>  g_diagCompatDcsDestroyed{0};
std::atomic<int64_t>  g_diagCompatBitmapPixelCapacityBytes{0};
std::atomic<int64_t>  g_diagCompatBitmapPixelCapacityHighWaterBytes{0};

void AdjustDiagLiveBytes(std::atomic<int64_t>& liveCounter,
                         std::atomic<int64_t>& highWaterCounter,
                         const int64_t delta)
{
    const int64_t value = liveCounter.fetch_add(delta, std::memory_order_relaxed) + delta;
    if (delta <= 0) return;

    int64_t highWater = highWaterCounter.load(std::memory_order_relaxed);
    while (value > highWater &&
           !highWaterCounter.compare_exchange_weak(highWater, value, std::memory_order_relaxed)) {
    }
}

void FreeApiDiagSnapshot(const char* tag);

bool FreeApiDiagnosticsEnabled()
{
    static int cached = -1;
    if (cached < 0) {
        const char* direct = SDL_getenv("FREE_DIRECT_DIAGNOSTICS");
        const char* api    = SDL_getenv("FREE_API_DIAGNOSTICS");
        cached = ((direct && *direct && std::strcmp(direct, "0") != 0)
               || (api && *api && std::strcmp(api, "0") != 0)) ? 1 : 0;
        if (cached) {
            std::atexit([]() { FreeApiDiagSnapshot("atexit"); });
            FreeApiDiagSnapshot("startup");
        }
    }
    return cached != 0;
}

bool FreeApiDiagnosticsFastEnabled()
{
    return FreeApiDiagnosticsEnabled();
}

bool FreeApiGdiDebugEnabled()
{
    static int cached = -1;
    if (cached < 0) {
        const char* v = SDL_getenv("FREE_API_DEBUG_GDI");
        cached = (v && v[0] == '1') ? 1 : 0;
    }
    return cached != 0;
}

long FreeApiReadRssKB()
{
    return FreeApi::Platform::ReadRssKB();
}

void FreeApiDiagSnapshot(const char* tag)
{
    if (!FreeApiDiagnosticsEnabled()) return;

    size_t queueSize = 0;
    {
        std::lock_guard<std::mutex> lock(g_messageQueueMutex);
        queueSize = g_messageQueue.size();
    }

    size_t winTimerCount = 0;
    {
        std::lock_guard<std::mutex> lock(g_winTimerMutex);
        winTimerCount = g_winTimers.size();
    }

    size_t mmTimerCount = 0;
    {
        std::lock_guard<std::mutex> lock(g_mmTimerMutex);
        mmTimerCount = g_mmTimers.size();
    }

    const long rssKb = FreeApiReadRssKB();
    SDL_Log("[FREE_API_DIAG][%s] rss=%ldKB rssMB=%.1f queue=%zu queueHW=%llu coalesced=upd:%llu mm:%llu tm:%llu activeTimers=%zu winTimers=%zu mmTimers=%zu "
            "wmUpdate=posted:%llu dispatched:%llu pending:%lld messages=posted:%llu dispatched:%llu sdlEvents=%llu "
            "sdlSurface=%lld/%lld/%lld compatBitmap=%lld/%lld/%lld compatDC=%lld/%lld/%lld "
            "bytes: compatBitmapPixels=%lldKB(hw=%lldKB) cache: freeApi=0",
            tag ? tag : "snapshot",
            rssKb,
            static_cast<double>(rssKb) / 1024.0,
            queueSize,
            static_cast<unsigned long long>(g_diagQueueHighWater.load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_diagUpdateCoalesced.load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_diagMouseMoveCoalesced.load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_diagTimerCoalesced.load(std::memory_order_relaxed)),
            winTimerCount + mmTimerCount,
            winTimerCount,
            mmTimerCount,
            static_cast<unsigned long long>(g_diagWmUpdatePosted.load()),
            static_cast<unsigned long long>(g_diagWmUpdateDispatched.load()),
            static_cast<long long>(g_diagWmUpdatePending.load()),
            static_cast<unsigned long long>(g_diagMessagesPosted.load()),
            static_cast<unsigned long long>(g_diagMessagesDispatched.load()),
            static_cast<unsigned long long>(g_diagSdlEventsProcessed.load()),
            static_cast<long long>(g_diagSdlSurfaces.load()),
            static_cast<long long>(g_diagSdlSurfacesEver.load()),
            static_cast<long long>(g_diagSdlSurfacesDestroyed.load()),
            static_cast<long long>(g_diagCompatBitmaps.load()),
            static_cast<long long>(g_diagCompatBitmapsEver.load()),
            static_cast<long long>(g_diagCompatBitmapsDestroyed.load()),
            static_cast<long long>(g_diagCompatDcs.load()),
            static_cast<long long>(g_diagCompatDcsEver.load()),
            static_cast<long long>(g_diagCompatDcsDestroyed.load()),
            static_cast<long long>(g_diagCompatBitmapPixelCapacityBytes.load() / 1024),
            static_cast<long long>(g_diagCompatBitmapPixelCapacityHighWaterBytes.load() / 1024));
}

void FreeApiDiagTick()
{
    if (!FreeApiDiagnosticsEnabled()) return;
    static std::atomic<uint64_t> lastNs{0};
    const uint64_t now = SDL_GetTicksNS();
    uint64_t prev = lastNs.load(std::memory_order_relaxed);
    if (prev != 0 && now - prev < 5000000000ULL) return;
    if (!lastNs.compare_exchange_strong(prev, now)) return;
    FreeApiDiagSnapshot("periodic5s");
}

} // namespace FreeApi::Internal
