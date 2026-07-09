#include "internal/FreeApiGdi.hpp"
#include "internal/FreeApiDiagnostics.hpp"

#include <SDL3/SDL.h>
#include <cstring>

namespace FreeApi::Internal {

CompatBitmap* AsCompatBitmap(void* object)
{
    auto* bitmap = reinterpret_cast<CompatBitmap*>(object);
    if (!bitmap || bitmap->magic != kCompatBitmapMagic) {
        return nullptr;
    }
    return bitmap;
}

CompatDC* AsCompatDC(void* dc)
{
    auto* compatDc = reinterpret_cast<CompatDC*>(dc);
    if (!compatDc || compatDc->magic != kCompatDcMagic) {
        return nullptr;
    }
    return compatDc;
}

CompatBitmap* CreateCompatBitmapFromSurface(SDL_Surface* surface)
{
    if (!surface) {
        return nullptr;
    }

    SDL_Surface* rgbaSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    if (!rgbaSurface) {
        // TASK-24H-1107: intentional, permanent unconditional error log --
        // this is a genuine failure path only (never the success path), and
        // project policy keeps error logs visible by default, unlike
        // success/startup logs which are gated behind FreeApiDiagnosticsEnabled()
        // et al. Do not gate this.
        SDL_Log("free-api LoadImageA: SDL_ConvertSurface failed: %s", SDL_GetError());  // sdl-log-gating: intentional (failure)
        return nullptr;
    }
    // TASK-0004: g_diagSdlSurfaces and the pixel-capacity-bytes tracking
    // below are diagnostics-snapshot-only (never read by any test) and are
    // gated behind FreeApiDiagnosticsFastEnabled() -- cached once per
    // process, so an inc/dec pair is always consistently gated together.
    // g_diagCompatBitmaps/g_diagCompatBitmapsEver just below are
    // deliberately NOT gated: tests/test_gdi_regressions.cpp reads both
    // directly as an always-on leak-detection primitive, independent of
    // whether verbose diagnostics logging is enabled.
    const bool diagEnabled = FreeApiDiagnosticsFastEnabled();
    if (diagEnabled) {
        g_diagSdlSurfaces.fetch_add(1, std::memory_order_relaxed);
        g_diagSdlSurfacesEver.fetch_add(1, std::memory_order_relaxed);
    }

    auto* bitmap = new CompatBitmap{};
    g_diagCompatBitmaps.fetch_add(1, std::memory_order_relaxed);
    g_diagCompatBitmapsEver.fetch_add(1, std::memory_order_relaxed);
    bitmap->width = rgbaSurface->w;
    bitmap->height = rgbaSurface->h;
    bitmap->bitsPerPixel = 32;
    // TASK-24H-1248: cast before multiplying, matching CreateBitmap's
    // (src/wingdi_bitmap.cpp) already-fixed pattern -- a plain int*int
    // multiplication is undefined behavior (signed overflow) once width
    // exceeds roughly 536,870,911. Not reachable by either game's real,
    // small, fixed loaded-bitmap dimensions; purely defensive.
    bitmap->pitch = static_cast<int>(static_cast<int64_t>(bitmap->width) * 4);
    bitmap->pixels.resize(static_cast<size_t>(bitmap->pitch) * static_cast<size_t>(bitmap->height));
    if (diagEnabled) {
        AdjustDiagLiveBytes(g_diagCompatBitmapPixelCapacityBytes,
                            g_diagCompatBitmapPixelCapacityHighWaterBytes,
                            static_cast<int64_t>(bitmap->pixels.capacity()));
    }

    for (int y = 0; y < bitmap->height; ++y) {
        const auto* srcRow = static_cast<const uint8_t*>(rgbaSurface->pixels) + static_cast<size_t>(y) * static_cast<size_t>(rgbaSurface->pitch);
        auto* dstRow = bitmap->pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(bitmap->pitch);
        memcpy(dstRow, srcRow, static_cast<size_t>(bitmap->pitch));
    }

    SDL_DestroySurface(rgbaSurface);
    if (diagEnabled) {
        g_diagSdlSurfaces.fetch_sub(1, std::memory_order_relaxed);
        g_diagSdlSurfacesDestroyed.fetch_add(1, std::memory_order_relaxed);
    }
    return bitmap;
}

void ScaleCompatBitmap(CompatBitmap& bitmap, const int targetWidth, const int targetHeight)
{
    if (targetWidth <= 0 || targetHeight <= 0 || (targetWidth == bitmap.width && targetHeight == bitmap.height)) {
        return;
    }

    const size_t oldCapacity = bitmap.pixels.capacity();
    std::vector<uint8_t> scaled(static_cast<size_t>(targetWidth) * static_cast<size_t>(targetHeight) * 4u, 0);
    const int srcWidth = bitmap.width;
    const int srcHeight = bitmap.height;
    const int srcPitch = bitmap.pitch;
    const uint8_t* srcData = bitmap.pixels.data();

    for (int y = 0; y < targetHeight; ++y) {
        const int srcY = (y * srcHeight) / targetHeight;
        auto* dstRow = scaled.data() + static_cast<size_t>(y) * static_cast<size_t>(targetWidth) * 4u;
        for (int x = 0; x < targetWidth; ++x) {
            const int srcX = (x * srcWidth) / targetWidth;
            const auto* srcPixel = srcData + static_cast<size_t>(srcY) * static_cast<size_t>(srcPitch) + static_cast<size_t>(srcX) * 4u;
            auto* dstPixel = dstRow + static_cast<size_t>(x) * 4u;
            dstPixel[0] = srcPixel[0];
            dstPixel[1] = srcPixel[1];
            dstPixel[2] = srcPixel[2];
            dstPixel[3] = srcPixel[3];
        }
    }

    bitmap.width = targetWidth;
    bitmap.height = targetHeight;
    // TASK-24H-1248: cast before multiplying, same rationale as
    // CreateCompatBitmapFromSurface above / CreateBitmap (wingdi_bitmap.cpp).
    bitmap.pitch = static_cast<int>(static_cast<int64_t>(targetWidth) * 4);
    bitmap.pixels.swap(scaled);
    if (FreeApiDiagnosticsFastEnabled()) { // TASK-0004
        const auto delta = static_cast<int64_t>(bitmap.pixels.capacity()) - static_cast<int64_t>(oldCapacity);
        AdjustDiagLiveBytes(g_diagCompatBitmapPixelCapacityBytes,
                            g_diagCompatBitmapPixelCapacityHighWaterBytes,
                            delta);
    }
}

} // namespace FreeApi::Internal
