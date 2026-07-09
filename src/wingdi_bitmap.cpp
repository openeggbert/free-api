#include "windows.h"
#include "internal/FreeApiGdi.hpp"
#include "internal/FreeApiDiagnostics.hpp"
#include "internal/FreeApiPath.hpp"

#include <SDL3/SDL.h>
#include <cstring>

using namespace FreeApi::Internal;

extern "C" {

HANDLE WINAPI LoadImageA(HINSTANCE hInst, LPCSTR name, UINT type, int cx, int cy, UINT fuLoad)
{
    (void)hInst;

    if (type != IMAGE_BITMAP || !name) {
        return NULL;
    }

    if ((fuLoad & LR_LOADFROMFILE) == 0) {
        SDL_Log("free-api LoadImageA: resource bitmap loading is not implemented for '%s'", name);
        return NULL;
    }

    // TASK-24H-0605: NormalizeFilesystemPath, not the weaker NormalizePath --
    // every other file-opening entry point in the codebase (_lopen,
    // CreateDirectoryA, _mkdir, _findfirst, ...) already uses the stronger
    // function, which additionally strips a leading drive letter/leading
    // slashes so a Windows-style rooted path (e.g. "\User\foo.bmp") is
    // treated as relative rather than escaping to the real filesystem root.
    std::string normalizedPath = NormalizeFilesystemPath(name);
    SDL_Surface* loaded = SDL_LoadBMP(normalizedPath.c_str());
    if (!loaded) {
        SDL_Log("free-api LoadImageA: SDL_LoadBMP failed for '%s': %s", normalizedPath.c_str(), SDL_GetError());
        return NULL;
    }
    g_diagSdlSurfaces.fetch_add(1, std::memory_order_relaxed);
    g_diagSdlSurfacesEver.fetch_add(1, std::memory_order_relaxed);

    CompatBitmap* bitmap = CreateCompatBitmapFromSurface(loaded);
    SDL_DestroySurface(loaded);
    g_diagSdlSurfaces.fetch_sub(1, std::memory_order_relaxed);
    g_diagSdlSurfacesDestroyed.fetch_add(1, std::memory_order_relaxed);
    if (!bitmap) {
        return NULL;
    }

    if (cx > 0 && cy > 0) {
        ScaleCompatBitmap(*bitmap, cx, cy);
    }

    if (FreeApiGdiDebugEnabled()) {
        SDL_Log("free-api LoadImageA: loaded bitmap '%s' -> %dx%d", normalizedPath.c_str(), bitmap->width, bitmap->height);
    }
    return reinterpret_cast<HANDLE>(bitmap);
}

int WINAPI GetObjectA(HANDLE h, int c, LPVOID pv)
{
    if (!h || !pv || c <= 0) {
        return 0;
    }

    CompatBitmap* bitmap = AsCompatBitmap(reinterpret_cast<void*>(h));
    if (!bitmap) {
        return 0;
    }

    BITMAP info{};
    info.bmType       = 0;
    info.bmWidth      = bitmap->width;
    info.bmHeight     = bitmap->height;
    info.bmWidthBytes = bitmap->pitch;
    info.bmPlanes     = 1;
    info.bmBitsPixel  = static_cast<WORD>(bitmap->bitsPerPixel);
    info.bmBits       = bitmap->pixels.data();

    const int copySize = c < static_cast<int>(sizeof(BITMAP)) ? c : static_cast<int>(sizeof(BITMAP));
    memcpy(pv, &info, static_cast<size_t>(copySize));
    // TASK-24H-1239: return the actual number of bytes copied, matching
    // real Win32 GetObjectA's documented contract -- not unconditionally
    // sizeof(BITMAP) even when the caller passed a smaller c and fewer
    // bytes were actually written.
    return copySize;
}

BOOL WINAPI DeleteObject(HGDIOBJ ho)
{
    auto* bitmap = AsCompatBitmap(reinterpret_cast<void*>(ho));
    if (!bitmap) {
        return FALSE;
    }

    AdjustDiagLiveBytes(g_diagCompatBitmapPixelCapacityBytes,
                        g_diagCompatBitmapPixelCapacityHighWaterBytes,
                        -static_cast<int64_t>(bitmap->pixels.capacity()));
    // TASK-24H-1229: clear the magic tag before delete so a double-delete
    // of the same (now-freed) handle fails AsCompatBitmap's validation
    // instead of risking a double-free against stale-but-still-tagged memory.
    bitmap->magic = 0;
    delete bitmap;
    g_diagCompatBitmaps.fetch_sub(1, std::memory_order_relaxed);
    g_diagCompatBitmapsDestroyed.fetch_add(1, std::memory_order_relaxed);
    return TRUE;
}

// TODO: CreateBitmap creates a GDI-compatible bitmap from raw pixel bits.
// Planet Blupi uses this for the minimap: it writes 8-bit or 16-bit pixels into
// a raw buffer, calls CreateBitmap, then passes the HBITMAP to DDConnectBitmap
// which reads dimensions via GetObject and blits via StretchBlt.
// We convert the source pixels to RGBA32 so the existing StretchBlt path works.
HBITMAP WINAPI CreateBitmap(int nWidth, int nHeight, UINT nPlanes, UINT nBitCount, const void* lpBits)
{
    (void)nPlanes; // always 1 for device-independent bitmaps
    if (nWidth <= 0 || nHeight <= 0) {
        return NULL;
    }

    auto* bitmap = new CompatBitmap{};
    g_diagCompatBitmaps.fetch_add(1, std::memory_order_relaxed);
    g_diagCompatBitmapsEver.fetch_add(1, std::memory_order_relaxed);
    bitmap->width        = nWidth;
    bitmap->height       = nHeight;
    bitmap->bitsPerPixel = 32; // store as RGBA32 internally
    // TASK-24H-1231: cast before multiplying, matching the pixel-buffer size
    // computation two lines below -- a plain `nWidth * 4` int*int
    // multiplication is undefined behavior (signed overflow) once nWidth
    // exceeds roughly 536,870,911. Not reachable by either game's real,
    // small, fixed bitmap dimensions; purely defensive.
    bitmap->pitch        = static_cast<int>(static_cast<int64_t>(nWidth) * 4);
    bitmap->pixels.resize(static_cast<size_t>(nWidth) * static_cast<size_t>(nHeight) * 4u, 0);
    AdjustDiagLiveBytes(g_diagCompatBitmapPixelCapacityBytes,
                        g_diagCompatBitmapPixelCapacityHighWaterBytes,
                        static_cast<int64_t>(bitmap->pixels.capacity()));

    if (lpBits) {
        if (nBitCount == 8) {
            // 8-bit indexed: store index as grey -- confirmed intentional,
            // not a TODO (TASK-24H-0602/0603 investigation). planetblupi's
            // minimap (decmap.cpp:578, the only 8-bit CreateBitmap call site
            // in either target game) only reaches this path when running
            // fullscreen (CPixmap::m_bPalette stays TRUE unconditionally in
            // fullscreen mode -- InitSysPalette()'s GetDeviceCaps(SIZEPALETTE)
            // override only runs when !m_bFullScreen). The shipped
            // data/config.def default is "FullScreen=0" (windowed), which
            // takes the working 16-bit RGB565 path instead -- confirmed
            // unaffected by this limitation in the default configuration.
            // If fullscreen IS enabled, SearchColor()'s fullscreen branch
            // returns real, non-greyscale m_pal[] palette-slot indices
            // (decmap.cpp's m_colors[MAP_*] table), so the minimap would
            // render as a dark greyscale scramble instead of real colors --
            // a real, but low-priority (non-default-config-only), visual
            // defect. Not fixed: free-api's CreateBitmap has no palette
            // parameter (matching real Win32's own CreateBitmap, which
            // doesn't take one either), and the two ways to add one --
            // a general HPALETTE/SetDIBColorTable-style API, or hard-coding
            // planetblupi's specific m_colors table into free-api -- both
            // violate this project's "no general Win32 palette API" and
            // "no unrelated/game-specific special-casing" scope rules.
            const uint8_t* src = static_cast<const uint8_t*>(lpBits);
            uint8_t* dst = bitmap->pixels.data();
            for (int y = 0; y < nHeight; ++y) {
                for (int x = 0; x < nWidth; ++x) {
                    uint8_t idx = src[y * nWidth + x];
                    // Expand to RGBA: treat index as greyscale placeholder
                    dst[(y * nWidth + x) * 4 + 0] = idx;
                    dst[(y * nWidth + x) * 4 + 1] = idx;
                    dst[(y * nWidth + x) * 4 + 2] = idx;
                    dst[(y * nWidth + x) * 4 + 3] = 0xFF;
                }
            }
        } else if (nBitCount == 16) {
            // 16-bit RGB565: convert to RGBA32
            const uint16_t* src = static_cast<const uint16_t*>(lpBits);
            uint8_t* dst = bitmap->pixels.data();
            for (int y = 0; y < nHeight; ++y) {
                for (int x = 0; x < nWidth; ++x) {
                    uint16_t px = src[y * nWidth + x];
                    uint8_t r = static_cast<uint8_t>(((px >> 11) & 0x1F) * 255 / 31);
                    uint8_t g = static_cast<uint8_t>(((px >> 5)  & 0x3F) * 255 / 63);
                    uint8_t b = static_cast<uint8_t>(((px >> 0)  & 0x1F) * 255 / 31);
                    dst[(y * nWidth + x) * 4 + 0] = r;
                    dst[(y * nWidth + x) * 4 + 1] = g;
                    dst[(y * nWidth + x) * 4 + 2] = b;
                    dst[(y * nWidth + x) * 4 + 3] = 0xFF;
                }
            }
        } else if (nBitCount == 32) {
            const size_t byteCount = static_cast<size_t>(nWidth) * static_cast<size_t>(nHeight) * 4u;
            memcpy(bitmap->pixels.data(), lpBits, byteCount);
        } else {
            SDL_Log("free-api CreateBitmap: unsupported bpp=%u, pixels zeroed", nBitCount);
        }
    }

    return reinterpret_cast<HBITMAP>(bitmap);
}

} // extern "C"
