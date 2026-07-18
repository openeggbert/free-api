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
    // NOTE: deliberately NOT a magic-static (`static const bool cached =
    // [](){...}();`) -- that was tried and immediately caused a real,
    // 100%-reproducible startup crash (SIGABRT / __gnu_cxx::recursive_init_error)
    // whenever diagnostics are enabled. The reason: this function's own
    // one-time init work calls FreeApiDiagSnapshot("startup"), and
    // FreeApiDiagSnapshot's very first line calls back into
    // FreeApiDiagnosticsEnabled() -- a same-thread reentrant call into a
    // function-local static that is still being initialized. The C++11
    // thread-safe-init guard treats that as undefined behavior, and
    // libstdc++ concretely throws/terminates on it. A plain atomic doesn't
    // have that "in progress" guard state -- storing `value` into `cached`
    // *before* the recursive call (matching the original plain-int
    // version's ordering) lets the reentrant call see the already-computed
    // result and return immediately, same as before this was ever "fixed"
    // for the (real but far lower severity) data race the previous
    // audit found.
    static std::atomic<int> cached{-1};
    int value = cached.load(std::memory_order_acquire);
    if (value < 0) {
        const char* direct = SDL_getenv("FREE_DIRECT_DIAGNOSTICS");
        const char* api    = SDL_getenv("FREE_API_DIAGNOSTICS");
        const bool enabled = (direct && *direct && std::strcmp(direct, "0") != 0)
                           || (api && *api && std::strcmp(api, "0") != 0);
        value = enabled ? 1 : 0;
        cached.store(value, std::memory_order_release);
        if (enabled) {
            std::atexit([]() { FreeApiDiagSnapshot("atexit"); });
            FreeApiDiagSnapshot("startup");
        }
    }
    return value != 0;
}

// See the doc comment on this function's declaration
// (src/internal/FreeApiDiagnostics.hpp) -- deliberately just a forwarding
// call, not a separate/cheaper implementation.
bool FreeApiDiagnosticsFastEnabled()
{
    return FreeApiDiagnosticsEnabled();
}

bool FreeApiGdiDebugEnabled()
{
    // See FreeApiDiagnosticsEnabled's comment above -- same fix, same reason.
    static const bool cached = [] {
        const char* v = SDL_getenv("FREE_API_DEBUG_GDI");
        return v && v[0] == '1';
    }();
    return cached;
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
