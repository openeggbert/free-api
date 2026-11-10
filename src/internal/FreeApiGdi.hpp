#pragma once

#include <cstdint>
#include <vector>

struct SDL_Surface;

namespace FreeApi::Internal {

constexpr uint32_t kCompatBitmapMagic = 0x504d5442u; // 'BTMP'
constexpr uint32_t kCompatDcMagic     = 0x30434446u; // 'FDC0'

enum class CompatDcKind {
    Memory,
    Surface
};

struct CompatBitmap {
    uint32_t magic        = kCompatBitmapMagic;
    int      width        = 0;   // pixels in `pixels` (physical)
    int      height       = 0;
    int      pitch        = 0;
    int      bitsPerPixel = 32;
    // Physical pixels per logical pixel on each axis. 1 except for a higher-resolution
    // image LoadImageA took from the image overlay: GetObjectA then reports width/scale x
    // height/scale and GDI coordinates on it are logical (docs/image-overlay.md).
    int      scale        = 1;
    std::vector<uint8_t> pixels;
};

struct CompatDC {
    uint32_t    magic              = kCompatDcMagic;
    CompatDcKind kind              = CompatDcKind::Memory;
    CompatBitmap* selectedBitmap  = nullptr;
    uint8_t*    surfacePixels      = nullptr;
    int         surfaceWidth       = 0;
    int         surfaceHeight      = 0;
    int         surfacePitch       = 0;
    int         surfaceBitsPerPixel = 32;
    // Surface DC from FreeApiCreateScaledSurfaceDC: surfaceWidth/Height are physical, and
    // coordinates passed to GDI on this DC are logical (multiplied by surfaceScale).
    int         surfaceScale       = 1;
};

// Physical pixels per logical pixel of what a DC draws into or reads from: the surface's
// scale, or the selected bitmap's for a memory DC.
inline int CompatDcScale(const CompatDC& dc)
{
    if (dc.kind == CompatDcKind::Surface) return dc.surfaceScale;
    return dc.selectedBitmap ? dc.selectedBitmap->scale : 1;
}

CompatBitmap* AsCompatBitmap(void* object);
CompatDC*     AsCompatDC(void* dc);

CompatBitmap* CreateCompatBitmapFromSurface(SDL_Surface* surface);
void          ScaleCompatBitmap(CompatBitmap& bitmap, int targetWidth, int targetHeight);

} // namespace FreeApi::Internal
