#include "windows.h"
#include "internal/FreeApiGdi.hpp"
#include "internal/FreeApiDiagnostics.hpp"
#include "internal/FreeApiPath.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <cstdarg>
#include <cstring>
#include <mutex>
#include <set>
#include <string>

using namespace FreeApi::Internal;

namespace {

// Image overlay (docs/image-overlay.md): FREE_API_IMAGE_OVERLAY names a
// folder laid out like the game's own (e.g. IMAGE08/OBJECT.BLP). An image
// found there replaces the one the game asked for when it is exactly
// FREE_API_IMAGE_SCALE (default 2) times as wide and as high as the
// original. The bitmap then carries that scale: GetObjectA reports the
// original's size and GDI coordinates stay the game's, while Free Direct's
// enlarged surfaces (FREE_DIRECT_BACKING_SCALE) keep the extra detail.
// Read at every call, so the variables may change at run time (tests do).
constexpr int kMaxOverlayScale = 8;

bool ReadOverlayConfig(std::string& root, int& scale)
{
    const char* rootEnv = SDL_getenv("FREE_API_IMAGE_OVERLAY");
    if (!rootEnv || !*rootEnv) return false;
    root = rootEnv;
    scale = 2;
    const char* scaleEnv = SDL_getenv("FREE_API_IMAGE_SCALE");
    if (scaleEnv && *scaleEnv) {
        const long value = SDL_strtol(scaleEnv, nullptr, 10);
        if (value < 1 || value > kMaxOverlayScale) return false;
        scale = static_cast<int>(value);
    }
    return true;
}

// Logs a problem with one overlay file once, not on every reload of it
// (the games reload their images on palette changes).
void WarnOverlayOnce(const std::string& path, const char* format, ...)
{
    static std::mutex mutex;
    static std::set<std::string> warned;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!warned.insert(path).second) return;
    }
    char message[1024];
    va_list args;
    va_start(args, format);
    SDL_vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    SDL_Log("free-api image overlay: %s", message);  // sdl-log-gating: intentional (overlay file rejected)
}

// A BMP's width and height from its header, without decoding the pixels.
bool ReadBmpSize(const std::string& path, int& width, int& height)
{
    SDL_IOStream* io = SDL_IOFromFile(path.c_str(), "rb");
    if (!io) return false;
    uint8_t header[26] = {};
    const size_t got = SDL_ReadIO(io, header, sizeof(header));
    SDL_CloseIO(io);
    if (got < sizeof(header) || header[0] != 'B' || header[1] != 'M') return false;
    const auto le32 = [&header](int offset) {
        return static_cast<uint32_t>(header[offset]) | (static_cast<uint32_t>(header[offset + 1]) << 8) |
               (static_cast<uint32_t>(header[offset + 2]) << 16) | (static_cast<uint32_t>(header[offset + 3]) << 24);
    };
    if (le32(14) == 12) {  // BITMAPCOREHEADER: 16-bit dimensions
        width = header[18] | (header[19] << 8);
        height = header[20] | (header[21] << 8);
    } else {
        width = static_cast<int32_t>(le32(18));
        height = static_cast<int32_t>(le32(22));
        if (height < 0) height = -height;  // top-down DIB
    }
    return width > 0 && height > 0;
}

// The overlay replacement for normalizedPath (the original, relative to the
// working directory), or null to load the original.
CompatBitmap* LoadOverlayBitmap(const std::string& normalizedPath)
{
    std::string root;
    int scale = 1;
    if (!ReadOverlayConfig(root, scale)) return nullptr;

    // The original's own name first; then the same name as .png or .bmp.
    std::string path = FindFileUnderRoot(root, normalizedPath);
    const size_t slash = normalizedPath.find_last_of('/');
    const size_t dot = normalizedPath.find_last_of('.');
    const std::string stem = (dot != std::string::npos && (slash == std::string::npos || dot > slash))
                                 ? normalizedPath.substr(0, dot) : normalizedPath;
    for (const char* extension : {".png", ".bmp"}) {
        if (!path.empty()) break;
        path = FindFileUnderRoot(root, stem + extension);
    }
    if (path.empty()) return nullptr;

    int originalWidth = 0, originalHeight = 0;
    if (!ReadBmpSize(normalizedPath, originalWidth, originalHeight)) {
        WarnOverlayOnce(path, "'%s' ignored: the original '%s' is not a readable BMP", path.c_str(), normalizedPath.c_str());
        return nullptr;
    }
    // SDL_image decodes by content (BMP, and PNG where it has the codec).
    SDL_Surface* loaded = IMG_Load(path.c_str());
    if (!loaded) loaded = SDL_LoadBMP(path.c_str());
    if (!loaded) {
        WarnOverlayOnce(path, "'%s' ignored: cannot decode it: %s", path.c_str(), SDL_GetError());
        return nullptr;
    }
    if (loaded->w != originalWidth * scale || loaded->h != originalHeight * scale) {
        WarnOverlayOnce(path, "'%s' ignored: it is %dx%d, expected %dx%d (%dx '%s', %dx%d)", path.c_str(),
                        loaded->w, loaded->h, originalWidth * scale, originalHeight * scale, scale,
                        normalizedPath.c_str(), originalWidth, originalHeight);
        SDL_DestroySurface(loaded);
        return nullptr;
    }
    CompatBitmap* bitmap = CreateCompatBitmapFromSurface(loaded);
    SDL_DestroySurface(loaded);
    if (!bitmap) return nullptr;
    // An alpha channel (PNG, 32-bit BMP) that is not fully opaque is kept:
    // StretchBlt carries it into the surface and Free Direct blends with it
    // (docs/image-overlay.md). Images without one keep the colour key alone.
    for (size_t i = 3; i < bitmap->pixels.size(); i += 4) {
        if (bitmap->pixels[i] != 255) {
            bitmap->hasAlpha = true;
            break;
        }
    }
    bitmap->scale = scale;
    if (FreeApiGdiDebugEnabled()) {
        SDL_Log("free-api LoadImageA: overlay '%s' (%dx%d, scale %d%s) replaces '%s'", path.c_str(), bitmap->width, bitmap->height, scale, bitmap->hasAlpha ? ", alpha" : "", normalizedPath.c_str());
    }
    return bitmap;
}

} // namespace

extern "C" {

HANDLE WINAPI LoadImageA(HINSTANCE hInst, LPCSTR name, UINT type, int cx, int cy, UINT fuLoad)
{
    (void)hInst;

    if (type != IMAGE_BITMAP || !name) {
        return NULL;
    }

    if ((fuLoad & LR_LOADFROMFILE) == 0) {
        SDL_Log("free-api LoadImageA: resource bitmap loading is not implemented for '%s'", name);  // sdl-log-gating: intentional (unsupported-input)
        return NULL;
    }

    // TASK-24H-0605: NormalizeFilesystemPath, not the weaker NormalizePath --
    // every other file-opening entry point in the codebase (_lopen,
    // CreateDirectoryA, _mkdir, _findfirst, ...) already uses the stronger
    // function, which additionally strips a leading drive letter/leading
    // slashes so a Windows-style rooted path (e.g. "\User\foo.bmp") is
    // treated as relative rather than escaping to the real filesystem root.
    std::string normalizedPath = NormalizeFilesystemPath(name);
    if (cx <= 0 && cy <= 0) {
        if (CompatBitmap* overlay = LoadOverlayBitmap(normalizedPath)) {
            return reinterpret_cast<HANDLE>(overlay);
        }
    }
    SDL_Surface* loaded = SDL_LoadBMP(normalizedPath.c_str());
    if (!loaded) {
        SDL_Log("free-api LoadImageA: SDL_LoadBMP failed for '%s': %s", normalizedPath.c_str(), SDL_GetError());  // sdl-log-gating: intentional (failure)
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
    // Logical size: an overlay image (scale > 1) reports the size of the
    // original it replaces, which is what blupi_recon's DDLoadBitmap
    // creates its DirectDraw surface from (src/ddutil.cpp:104-118).
    info.bmWidth      = bitmap->width / bitmap->scale;
    info.bmHeight     = bitmap->height / bitmap->scale;
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

    // TASK-0004: gated behind diagnostics-enabled, matching the rest of the
    // diagnostics system's convention (FreeApiDiagnosticsFastEnabled() is
    // cached once per process, so an inc/dec pair is always consistently
    // gated together -- never observes a mid-process toggle).
    if (FreeApiDiagnosticsFastEnabled()) {
        AdjustDiagLiveBytes(g_diagCompatBitmapPixelCapacityBytes,
                            g_diagCompatBitmapPixelCapacityHighWaterBytes,
                            -static_cast<int64_t>(bitmap->pixels.capacity()));
    }
    // TASK-24H-1229: clear the magic tag before delete so a double-delete
    // of the same (now-freed) handle fails AsCompatBitmap's validation
    // instead of risking a double-free against stale-but-still-tagged memory.
    bitmap->magic = 0;
    delete bitmap;
    // TASK-0004: g_diagCompatBitmaps/g_diagCompatBitmapsEver are
    // intentionally NOT gated behind FreeApiDiagnosticsFastEnabled(), unlike
    // the other diagnostic-only counters in this file --
    // tests/test_gdi_regressions.cpp reads both directly as an always-on
    // leak-detection primitive (independent of whether verbose diagnostics
    // logging is enabled), so gating them would silently turn those tests
    // into no-ops rather than real checks. Verified this the hard way:
    // gating them broke 2 real test assertions before this was reverted to
    // just these two counters; g_diagCompatBitmapsDestroyed (never read by
    // any test) stays gated.
    g_diagCompatBitmaps.fetch_sub(1, std::memory_order_relaxed);
    if (FreeApiDiagnosticsFastEnabled()) {
        g_diagCompatBitmapsDestroyed.fetch_add(1, std::memory_order_relaxed);
    }
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
    // TASK-0004: see DeleteObject's matching comment above -- these two
    // stay ungated (test-critical leak-detection primitive); only the
    // pixel-capacity-bytes tracking below (diagnostics-snapshot-only) gates.
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
    if (FreeApiDiagnosticsFastEnabled()) {
        AdjustDiagLiveBytes(g_diagCompatBitmapPixelCapacityBytes,
                            g_diagCompatBitmapPixelCapacityHighWaterBytes,
                            static_cast<int64_t>(bitmap->pixels.capacity()));
    }

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
            // TASK-24H-1231-sibling: widen through size_t, matching the
            // pitch computation above -- a plain `y * nWidth + x` int
            // multiplication is the same signed-overflow UB class fixed
            // there, just at the per-pixel-index site instead of the pitch
            // site. Not reachable by either game's real, small, fixed
            // bitmap dimensions (planetblupi's minimap is always
            // DIMMAPX=DIMMAPY=128); purely defensive.
            for (int y = 0; y < nHeight; ++y) {
                const size_t rowBase = static_cast<size_t>(y) * static_cast<size_t>(nWidth);
                for (int x = 0; x < nWidth; ++x) {
                    const size_t idxPos = rowBase + static_cast<size_t>(x);
                    uint8_t idx = src[idxPos];
                    // Expand to RGBA: treat index as greyscale placeholder
                    dst[idxPos * 4 + 0] = idx;
                    dst[idxPos * 4 + 1] = idx;
                    dst[idxPos * 4 + 2] = idx;
                    dst[idxPos * 4 + 3] = 0xFF;
                }
            }
        } else if (nBitCount == 16) {
            // 16-bit RGB565: convert to RGBA32
            const uint16_t* src = static_cast<const uint16_t*>(lpBits);
            uint8_t* dst = bitmap->pixels.data();
            for (int y = 0; y < nHeight; ++y) {
                const size_t rowBase = static_cast<size_t>(y) * static_cast<size_t>(nWidth);
                for (int x = 0; x < nWidth; ++x) {
                    const size_t idxPos = rowBase + static_cast<size_t>(x);
                    uint16_t px = src[idxPos];
                    uint8_t r = static_cast<uint8_t>(((px >> 11) & 0x1F) * 255 / 31);
                    uint8_t g = static_cast<uint8_t>(((px >> 5)  & 0x3F) * 255 / 63);
                    uint8_t b = static_cast<uint8_t>(((px >> 0)  & 0x1F) * 255 / 31);
                    dst[idxPos * 4 + 0] = r;
                    dst[idxPos * 4 + 1] = g;
                    dst[idxPos * 4 + 2] = b;
                    dst[idxPos * 4 + 3] = 0xFF;
                }
            }
        } else if (nBitCount == 32) {
            const size_t byteCount = static_cast<size_t>(nWidth) * static_cast<size_t>(nHeight) * 4u;
            memcpy(bitmap->pixels.data(), lpBits, byteCount);
        } else {
            SDL_Log("free-api CreateBitmap: unsupported bpp=%u, pixels zeroed", nBitCount);  // sdl-log-gating: intentional (fallback warning)
        }
    }

    return reinterpret_cast<HBITMAP>(bitmap);
}

} // extern "C"
