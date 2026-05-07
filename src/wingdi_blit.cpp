#include "windows.h"
#include "internal/FreeApiGdi.hpp"
#include "internal/FreeApiDiagnostics.hpp"

#include <SDL3/SDL.h>
#include <vector>
#include <cstring>

using namespace FreeApi::Internal;

extern "C" {

BOOL WINAPI StretchBlt(HDC hdcDest,
                       int xDest,
                       int yDest,
                       int wDest,
                       int hDest,
                       HDC hdcSrc,
                       int xSrc,
                       int ySrc,
                       int wSrc,
                       int hSrc,
                       DWORD rop)
{
    if (rop != SRCCOPY) {
        if (FreeApiGdiDebugEnabled()) {
            SDL_Log("free-api StretchBlt: unsupported ROP=0x%08lx", static_cast<unsigned long>(rop));
        }
        return FALSE;
    }

    CompatDC* dst = AsCompatDC(reinterpret_cast<void*>(hdcDest));
    CompatDC* src = AsCompatDC(reinterpret_cast<void*>(hdcSrc));
    if (!dst || !src || dst->kind != CompatDcKind::Surface || dst->surfaceBitsPerPixel != 32 || !dst->surfacePixels || !src->selectedBitmap) {
        if (FreeApiGdiDebugEnabled()) {
            SDL_Log("free-api StretchBlt: unsupported DC pair dst=%p src=%p", reinterpret_cast<void*>(hdcDest), reinterpret_cast<void*>(hdcSrc));
        }
        return FALSE;
    }

    if (wDest <= 0 || hDest <= 0 || wSrc <= 0 || hSrc <= 0) {
        return FALSE;
    }

    CompatBitmap* srcBitmap = src->selectedBitmap;
    const uint8_t* srcPixels = srcBitmap->pixels.data();

    // Fast path: 1:1 copy — clip once and copy rows with memcpy.
    // This avoids per-pixel scaling math for the common case.
    if (wDest == wSrc && hDest == hSrc) {
        // Clip against destination surface
        int dx0 = xDest, dy0 = yDest;
        int sx0 = xSrc,  sy0 = ySrc;
        int cw  = wDest, ch  = hDest;

        // Clip left
        if (dx0 < 0) { sx0 -= dx0; cw += dx0; dx0 = 0; }
        if (sx0 < 0) { dx0 -= sx0; cw += sx0; sx0 = 0; }
        // Clip right
        if (dx0 + cw > dst->surfaceWidth)  cw = dst->surfaceWidth  - dx0;
        if (sx0 + cw > srcBitmap->width)   cw = srcBitmap->width   - sx0;
        // Clip top
        if (dy0 < 0) { sy0 -= dy0; ch += dy0; dy0 = 0; }
        if (sy0 < 0) { dy0 -= sy0; ch += sy0; sy0 = 0; }
        // Clip bottom
        if (dy0 + ch > dst->surfaceHeight) ch = dst->surfaceHeight - dy0;
        if (sy0 + ch > srcBitmap->height)  ch = srcBitmap->height  - sy0;

        if (cw <= 0 || ch <= 0) return TRUE;

        // Fast path: bulk-copy each row with memcpy. The source bitmaps used
        // by the game are produced inside the compatibility layer with alpha
        // already set to 255 (RGBA32 surfaces created via SDL), so we no
        // longer need a per-pixel alpha fix-up here.
        // TODO: if a future code path introduces bitmaps with non-255 alpha,
        //       fix that at bitmap creation/load time, NOT in this hot blit
        //       loop — this is the inner frame-pacing path.
        const size_t rowBytes = static_cast<size_t>(cw) * 4u;
        for (int row = 0; row < ch; ++row) {
            const uint8_t* srcRow = srcPixels
                + static_cast<size_t>(sy0 + row) * static_cast<size_t>(srcBitmap->pitch)
                + static_cast<size_t>(sx0) * 4u;
            uint8_t* dstRow = dst->surfacePixels
                + static_cast<size_t>(dy0 + row) * static_cast<size_t>(dst->surfacePitch)
                + static_cast<size_t>(dx0) * 4u;
            std::memcpy(dstRow, srcRow, rowBytes);
        }

        if (FreeApiGdiDebugEnabled()) {
            SDL_Log("free-api StretchBlt (1:1): src=%dx%d[%d,%d] dst=[%d,%d] clipped=%dx%d",
                    wSrc, hSrc, xSrc, ySrc, xDest, yDest, cw, ch);
        }
        return TRUE;
    }

    // Scaled path: precompute per-destination-column source X to avoid division
    // inside the inner loop.  Clip destination to surface bounds first so the
    // lookup table only covers the pixels we will actually write.
    int dxBegin = xDest, dyBegin = yDest;
    int dxEnd   = xDest + wDest, dyEnd = yDest + hDest;
    if (dxBegin < 0) dxBegin = 0;
    if (dyBegin < 0) dyBegin = 0;
    if (dxEnd > dst->surfaceWidth)  dxEnd = dst->surfaceWidth;
    if (dyEnd > dst->surfaceHeight) dyEnd = dst->surfaceHeight;
    if (dxBegin >= dxEnd || dyBegin >= dyEnd) return TRUE;

    // Build srcX lookup table for the clipped destination column range.
    // Use a thread_local cached vector so repeated scaled blits do not heap-
    // allocate on every call (the inner game loop hits this path at frame rate).
    const int clippedW = dxEnd - dxBegin;
    thread_local std::vector<int> srcXTable;
    if (srcXTable.size() < static_cast<size_t>(clippedW)) {
        srcXTable.resize(static_cast<size_t>(clippedW));
    }
    for (int i = 0; i < clippedW; ++i) {
        const int dstCol = dxBegin + i - xDest; // 0-based offset within wDest
        int sx = xSrc + static_cast<int>((static_cast<int64_t>(dstCol) * static_cast<int64_t>(wSrc)) / static_cast<int64_t>(wDest));
        if (sx < 0) sx = 0;
        if (sx >= srcBitmap->width) sx = srcBitmap->width - 1;
        srcXTable[static_cast<size_t>(i)] = sx;
    }

    for (int dstY = dyBegin; dstY < dyEnd; ++dstY) {
        const int dstRow0 = dstY - yDest; // 0-based row within hDest
        const int srcY = ySrc + static_cast<int>((static_cast<int64_t>(dstRow0) * static_cast<int64_t>(hSrc)) / static_cast<int64_t>(hDest));
        if (srcY < 0 || srcY >= srcBitmap->height) continue;

        auto* dstRow = dst->surfacePixels + static_cast<size_t>(dstY) * static_cast<size_t>(dst->surfacePitch);
        const auto* srcRow = srcPixels + static_cast<size_t>(srcY) * static_cast<size_t>(srcBitmap->pitch);

        for (int i = 0; i < clippedW; ++i) {
            const int srcX = srcXTable[static_cast<size_t>(i)];
            const auto* srcPixel = srcRow + static_cast<size_t>(srcX) * 4u;
            auto* dstPixel = dstRow + static_cast<size_t>(dxBegin + i) * 4u;
            dstPixel[0] = srcPixel[0];
            dstPixel[1] = srcPixel[1];
            dstPixel[2] = srcPixel[2];
            dstPixel[3] = 255;
        }
    }

    if (FreeApiGdiDebugEnabled()) {
        SDL_Log("free-api StretchBlt (scaled): src=%dx%d[%d,%d] dst=%dx%d[%d,%d]",
                wSrc, hSrc, xSrc, ySrc, wDest, hDest, xDest, yDest);
    }
    return TRUE;
}

COLORREF WINAPI GetPixel(HDC hdc, int x, int y)
{
    CompatDC* dc = AsCompatDC(reinterpret_cast<void*>(hdc));
    if (!dc) {
        return 0;
    }

    const uint8_t* pixel = nullptr;
    if (dc->kind == CompatDcKind::Surface) {
        if (x < 0 || y < 0 || x >= dc->surfaceWidth || y >= dc->surfaceHeight || !dc->surfacePixels) {
            return 0;
        }
        pixel = dc->surfacePixels + static_cast<size_t>(y) * static_cast<size_t>(dc->surfacePitch) + static_cast<size_t>(x) * 4u;
    } else if (dc->selectedBitmap) {
        if (x < 0 || y < 0 || x >= dc->selectedBitmap->width || y >= dc->selectedBitmap->height) {
            return 0;
        }
        pixel = dc->selectedBitmap->pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(dc->selectedBitmap->pitch) + static_cast<size_t>(x) * 4u;
    }

    if (!pixel) {
        return 0;
    }

    return RGB(pixel[0], pixel[1], pixel[2]);
}

COLORREF WINAPI SetPixel(HDC hdc, int x, int y, COLORREF color)
{
    CompatDC* dc = AsCompatDC(reinterpret_cast<void*>(hdc));
    if (!dc) {
        return color;
    }

    uint8_t* pixel = nullptr;
    if (dc->kind == CompatDcKind::Surface) {
        if (x < 0 || y < 0 || x >= dc->surfaceWidth || y >= dc->surfaceHeight || !dc->surfacePixels) {
            return color;
        }
        pixel = dc->surfacePixels + static_cast<size_t>(y) * static_cast<size_t>(dc->surfacePitch) + static_cast<size_t>(x) * 4u;
    } else if (dc->selectedBitmap) {
        if (x < 0 || y < 0 || x >= dc->selectedBitmap->width || y >= dc->selectedBitmap->height) {
            return color;
        }
        pixel = dc->selectedBitmap->pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(dc->selectedBitmap->pitch) + static_cast<size_t>(x) * 4u;
    }

    if (!pixel) {
        return color;
    }

    // COLORREF layout is 0x00BBGGRR; surface pixel layout is RGBA.
    pixel[0] = static_cast<uint8_t>(color & 0xFFu);          // R
    pixel[1] = static_cast<uint8_t>((color >> 8) & 0xFFu);   // G
    pixel[2] = static_cast<uint8_t>((color >> 16) & 0xFFu);  // B
    pixel[3] = 255;
    return color;
}

} // extern "C"
