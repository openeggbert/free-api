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
    int      width        = 0;
    int      height       = 0;
    int      pitch        = 0;
    int      bitsPerPixel = 32;
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
};

CompatBitmap* AsCompatBitmap(void* object);
CompatDC*     AsCompatDC(void* dc);

CompatBitmap* CreateCompatBitmapFromSurface(SDL_Surface* surface);
void          ScaleCompatBitmap(CompatBitmap& bitmap, int targetWidth, int targetHeight);

} // namespace FreeApi::Internal
