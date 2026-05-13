#include "windows.h"
#include "internal/FreeApiGdi.hpp"
#include "internal/FreeApiDiagnostics.hpp"
#include "internal/FreeApiWindowRegistry.hpp"

#include <SDL3/SDL.h>

using namespace FreeApi::Internal;

extern "C" {

HDC FreeApiCreateSurfaceDC(void* pixels, int width, int height, int pitch, int bitsPerPixel)
{
    if (!pixels || width <= 0 || height <= 0 || pitch <= 0 || bitsPerPixel != 32) {
        return NULL;
    }

    auto* dc = new CompatDC{};
    g_diagCompatDcs.fetch_add(1, std::memory_order_relaxed);
    g_diagCompatDcsEver.fetch_add(1, std::memory_order_relaxed);
    dc->kind               = CompatDcKind::Surface;
    dc->surfacePixels      = static_cast<uint8_t*>(pixels);
    dc->surfaceWidth       = width;
    dc->surfaceHeight      = height;
    dc->surfacePitch       = pitch;
    dc->surfaceBitsPerPixel = bitsPerPixel;
    return reinterpret_cast<HDC>(dc);
}

BOOL FreeApiDestroySurfaceDC(HDC hdc)
{
    auto* dc = AsCompatDC(reinterpret_cast<void*>(hdc));
    if (!dc) {
        return FALSE;
    }

    delete dc;
    g_diagCompatDcs.fetch_sub(1, std::memory_order_relaxed);
    g_diagCompatDcsDestroyed.fetch_add(1, std::memory_order_relaxed);
    return TRUE;
}


void FreeApiSetWindowFullscreen(HWND hwnd, bool fullscreen)
{
    if (!hwnd) return;
    auto* sdlWindow = reinterpret_cast<SDL_Window*>(hwnd);

    const bool diag = FreeApiDiagnosticsEnabled();
    if (diag) SDL_Log("free-api: FreeApiSetWindowFullscreen(hwnd=%p, fullscreen=%d)", static_cast<void*>(hwnd), fullscreen);

    // We need the window states map - include it via windowregistry
    // Note: FreeApiSetWindowFullscreen doesn't use FreeApi::Internal state directly
    // but g_freeApiWindowStates is needed; we include it via the SDL_Window pointer
    // stored as HWND and update via the public registry.
    if (SDL_SetWindowFullscreen(sdlWindow, fullscreen)) {
        if (diag) SDL_Log("free-api: SDL_SetWindowFullscreen(%s) success", fullscreen ? "true" : "false");
        auto it = g_freeApiWindowStates.find(hwnd);
        if (it != g_freeApiWindowStates.end()) {
            it->second.isFullscreen = fullscreen;
        }
    } else {
        SDL_Log("free-api: SDL_SetWindowFullscreen(%s) failed: %s", fullscreen ? "true" : "false", SDL_GetError());
    }
}

HDC WINAPI CreateCompatibleDC(HDC hdc)
{
    (void)hdc;
    auto* dc = new CompatDC{};
    g_diagCompatDcs.fetch_add(1, std::memory_order_relaxed);
    g_diagCompatDcsEver.fetch_add(1, std::memory_order_relaxed);
    dc->kind = CompatDcKind::Memory;
    return reinterpret_cast<HDC>(dc);
}

HGDIOBJ WINAPI SelectObject(HDC hdc, HGDIOBJ h)
{
    CompatDC* dc = AsCompatDC(reinterpret_cast<void*>(hdc));
    if (!dc) {
        return NULL;
    }

    if (CompatBitmap* bitmap = AsCompatBitmap(reinterpret_cast<void*>(h))) {
        HGDIOBJ previous = reinterpret_cast<HGDIOBJ>(dc->selectedBitmap);
        dc->selectedBitmap = bitmap;
        return previous;
    }

    return NULL;
}

BOOL WINAPI DeleteDC(HDC hdc)
{
    CompatDC* dc = AsCompatDC(reinterpret_cast<void*>(hdc));
    if (!dc) {
        return FALSE;
    }

    delete dc;
    g_diagCompatDcs.fetch_sub(1, std::memory_order_relaxed);
    g_diagCompatDcsDestroyed.fetch_add(1, std::memory_order_relaxed);
    return TRUE;
}

} // extern "C"
