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

    std::string normalizedPath = NormalizePath(name);
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
    return static_cast<int>(sizeof(BITMAP));
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
    bitmap->pitch        = nWidth * 4;
    bitmap->pixels.resize(static_cast<size_t>(nWidth) * static_cast<size_t>(nHeight) * 4u, 0);
    AdjustDiagLiveBytes(g_diagCompatBitmapPixelCapacityBytes,
                        g_diagCompatBitmapPixelCapacityHighWaterBytes,
                        static_cast<int64_t>(bitmap->pixels.capacity()));

    if (lpBits) {
        if (nBitCount == 8) {
            // 8-bit indexed: store index as grey (palette expansion not yet supported)
            // TODO: apply palette if one is set
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
