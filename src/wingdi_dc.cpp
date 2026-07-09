#include "windows.h"
#include "free_api_bridge.h"
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
    // TASK-0004: g_diagCompatDcs/g_diagCompatDcsEver are intentionally NOT
    // gated behind FreeApiDiagnosticsFastEnabled(), unlike the other
    // diagnostic-only counters in this file -- tests/test_gdi_regressions.cpp
    // reads both directly as an always-on leak-detection primitive
    // (independent of whether verbose diagnostics logging is enabled), so
    // gating them would silently turn those tests into no-ops rather than
    // real checks. Verified this the hard way: gating them broke 2 real
    // test assertions before this was reverted to just these two counters.
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

    // TASK-24H-1229: clear the magic tag before delete so a double-delete
    // of the same (now-freed) handle fails AsCompatDC's validation instead
    // of risking a double-free against stale-but-still-tagged memory.
    dc->magic = 0;
    delete dc;
    // TASK-0004: g_diagCompatDcs itself stays ungated (see the matching
    // comment in FreeApiCreateSurfaceDC above); only g_diagCompatDcsDestroyed
    // (diagnostics-snapshot-only, never read by any test) is gated.
    g_diagCompatDcs.fetch_sub(1, std::memory_order_relaxed);
    if (FreeApiDiagnosticsFastEnabled()) {
        g_diagCompatDcsDestroyed.fetch_add(1, std::memory_order_relaxed);
    }
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
        if (diag) SDL_Log("free-api: SDL_SetWindowFullscreen(%s) failed: %s", fullscreen ? "true" : "false", SDL_GetError());
    }
}

HDC WINAPI CreateCompatibleDC(HDC hdc)
{
    (void)hdc;
    auto* dc = new CompatDC{};
    // TASK-0004: see FreeApiCreateSurfaceDC's matching comment above.
    g_diagCompatDcs.fetch_add(1, std::memory_order_relaxed);
    g_diagCompatDcsEver.fetch_add(1, std::memory_order_relaxed);
    dc->kind = CompatDcKind::Memory;
    return reinterpret_cast<HDC>(dc);
}

// TASK-24H-0608: only bitmap objects are supported. Any other/invalid
// handle falls through to the final "return NULL" below -- an intentional
// simplification versus real Win32's no-op/return-current-default
// behavior for other GDI object kinds, safe because neither game ever
// selects anything but a bitmap into a DC.
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

    // TASK-24H-1229: see FreeApiDestroySurfaceDC's matching comment above.
    dc->magic = 0;
    delete dc;
    // TASK-0004: see FreeApiDestroySurfaceDC's matching comment above.
    g_diagCompatDcs.fetch_sub(1, std::memory_order_relaxed);
    if (FreeApiDiagnosticsFastEnabled()) {
        g_diagCompatDcsDestroyed.fetch_add(1, std::memory_order_relaxed);
    }
    return TRUE;
}

} // extern "C"
