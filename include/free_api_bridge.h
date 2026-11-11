/**
 * @file free_api_bridge.h
 * @brief The documented free-direct-bridge exception's public surface.
 *
 * These functions are NOT part of the Win32/WinAPI compatibility
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
 * @brief FreeApiCreateSurfaceDC() for a pixel buffer with a backing scale:
 * `width`/`height`/`pitch` describe the physical buffer, and GDI calls on
 * the returned DC take logical coordinates (multiplied by `scale`). Used by
 * free-direct's IDirectDrawSurface::GetDC when FREE_DIRECT_BACKING_SCALE
 * enlarges its surfaces, so DDCopyBitmap's StretchBlt and DDColorMatch's
 * SetPixel/GetPixel keep working in the game's coordinates.
 *
 * Returns NULL under the same conditions as FreeApiCreateSurfaceDC(), or if
 * `scale` is not positive. A scale of 1 is FreeApiCreateSurfaceDC().
 * Released with FreeApiDestroySurfaceDC().
 * @note Status: IMPLEMENTED
 */
HDC FreeApiCreateScaledSurfaceDC(void* pixels, int width, int height, int pitch, int bitsPerPixel, int scale);

/**
 * @brief What StretchBlt drew into a surface DC since it was created: 0 no
 * image, 1 only opaque images, 2 an image with an alpha channel (an overlay
 * image, docs/image-overlay.md), whose alpha is then in the 4th byte of the
 * DC's pixels. Free Direct reads it at IDirectDrawSurface::ReleaseDC to keep
 * or drop the surface's alpha plane. Returns 0 for an invalid handle.
 * @note Status: IMPLEMENTED
 */
int FreeApiGetSurfaceDCAlpha(HDC hdc);

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

/**
 * @brief What Free Direct's "FreeDirect server" provider asks the player before Internet play:
 * the server, its port, its password, and the room. In/out: prefilled by the caller, edited by
 * the player. Strings are NUL-terminated UTF-8.
 */
typedef struct FreeApiServerConnection {
    char server[256];    /**< host name, IP address or ws:// / wss:// URL */
    char port[16];       /**< used when `server` has no port */
    char password[128];  /**< the server's password; empty when it has none */
    char roomId[64];     /**< the room to join; ignored when `createRoom` */
    int  createRoom;     /**< nonzero: create a new room instead of joining one */
    char message[512];   /**< in only: why the previous attempt failed, shown in the dialog */
} FreeApiServerConnection;

/**
 * @brief Shows the connection dialog of Free Direct's server provider and waits for the player.
 * Native builds draw it into the game's window (so it shows in full screen too) with SDL's
 * built-in font; the web build shows an HTML form over the canvas, prefilled from the page's
 * `server`, `port` and `room` URL parameters when the caller left those empty, and uses
 * `Module.freeApiServerPrompt` instead when the page defines it. Returns TRUE when the player
 * chose to connect (the fields hold their entries), FALSE when they cancelled.
 * @note Status: IMPLEMENTED
 */
BOOL FreeApiPromptServerConnection(FreeApiServerConnection* connection);

/**
 * @brief Tells the player the secret ID of the room they just created on server `serverUrl`,
 * to give to the other players, and copies it to the clipboard. Returns when they dismiss it.
 * @note Status: IMPLEMENTED
 */
void FreeApiShowRoomCreated(const char* roomId, const char* serverUrl);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // FREE_API_BRIDGE_H
