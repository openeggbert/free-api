/**
 * @file free_api_bridge.h
 * @brief The documented free-direct-bridge exception's public surface.
 *
 * These three functions are NOT part of the Win32/WinAPI compatibility
 * surface (they use no WINAPI calling convention, no Hungarian-notation
 * Win32 signature shape) -- they exist solely so `../free-direct` (a
 * sibling project implementing the DirectDraw/DirectSound/DirectPlay
 * subset both target games also need) can render into and manage the
 * window Free API owns, without free-api exposing SDL types anywhere in
 * its public headers. See docs/scope.md's "Boundary with free-direct"
 * section for the full policy this header enforces.
 *
 * Prior to this header's introduction, every consumer (this project's own
 * examples/tests, and ../free-direct/src/directdraw/DirectDraw.cpp)
 * hand-declared its own local `extern "C"` copy of these three signatures
 * -- an unenforced, drift-prone duplication (plan.md TASK-0002 /
 * TASK-24H-0101). This header is now the single authoritative declaration
 * site; do not re-declare these functions elsewhere.
 *
 * @note This header must not expose SDL types, matching windows.h's rule.
 */
#ifndef FREE_API_BRIDGE_H
#define FREE_API_BRIDGE_H

#include "windows.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Wraps an externally-owned pixel buffer (e.g. a DirectDraw
 * surface's own memory) as a GDI-compatible DC, without copying.
 *
 * Returns NULL if `pixels` is null, `width`/`height`/`pitch` are not
 * positive, or `bitsPerPixel` is not 32 -- only 32bpp surfaces are
 * supported. The returned HDC's lifetime is the caller's responsibility
 * via FreeApiDestroySurfaceDC(); it does not take ownership of `pixels`.
 * @note Status: IMPLEMENTED
 */
HDC FreeApiCreateSurfaceDC(void* pixels, int width, int height, int pitch, int bitsPerPixel);

/**
 * @brief Releases a DC created by FreeApiCreateSurfaceDC(). Does not free
 * or otherwise touch the underlying pixel buffer, which the caller still
 * owns. Returns FALSE for a null/invalid handle rather than crashing.
 * @note Status: IMPLEMENTED
 */
BOOL FreeApiDestroySurfaceDC(HDC hdc);

/**
 * @brief Toggles a window's real OS fullscreen state via SDL and updates
 * Free API's own logical window-state tracking to match. A null `hwnd`
 * is a no-op. Logs (gated behind FreeApiDiagnosticsEnabled()) on entry,
 * success, and failure.
 * @note Status: IMPLEMENTED
 */
void FreeApiSetWindowFullscreen(HWND hwnd, bool fullscreen);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // FREE_API_BRIDGE_H
