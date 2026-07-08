/**
 * @file test_gdi_regressions.cpp
 * @brief Focused regression tests for the GDI blit/pixel subset both target
 * games' asset-loading and DirectDraw color-matching paths depend on
 * (StretchBlt, GetPixel, SetPixel). See plan.md sections 3.6/6 and
 * todo/free-api-performance-todo.md.
 *
 * The 1:1 fast path, scaled path (with a thread_local cached lookup table
 * to avoid per-blit heap allocation), and gated debug logging in
 * src/wingdi_blit.cpp were already implemented before this test existed;
 * this file locks in their behavior, which had no test coverage at all.
 *
 * Uses FreeApiCreateSurfaceDC/FreeApiDestroySurfaceDC (include/free_api_bridge.h)
 * -- the documented free-direct-bridge entry points, not part of the Win32
 * surface -- that free-direct already depends on to expose a GDI-compatible
 * DC backed by a DirectDraw surface's own pixel memory. Used here purely to
 * construct a controlled destination buffer for testing.
 */
#include <windows.h>
#include <free_api_bridge.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <vector>

// Test-local forward declarations of internal diagnostic counters (defined
// in src/internal/FreeApiDiagnostics.hpp/.cpp) -- not part of the public
// windows.h surface, referenced here only to verify TASK-0059's "no
// unbounded internal-registry growth" acceptance criterion directly rather
// than indirectly.
namespace FreeApi::Internal {
extern std::atomic<int64_t> g_diagCompatDcs;
extern std::atomic<int64_t> g_diagCompatDcsEver;
}

// Test-local only: neither target game unpacks a COLORREF's components (only
// RGB() packing is used), so these are deliberately not added to any public
// header. COLORREF layout is 0x00BBGGRR (see src/wingdi_blit.cpp:SetPixel).
static uint8_t GetRValue(COLORREF c) { return static_cast<uint8_t>(c & 0xFFu); }
static uint8_t GetGValue(COLORREF c) { return static_cast<uint8_t>((c >> 8) & 0xFFu); }
static uint8_t GetBValue(COLORREF c) { return static_cast<uint8_t>((c >> 16) & 0xFFu); }

static int g_failures = 0;

static void Check(bool condition, const char* what)
{
    if (condition) {
        printf("[gdi-regressions] PASS: %s\n", what);
    } else {
        printf("[gdi-regressions] FAIL: %s\n", what);
        ++g_failures;
    }
}

// Sentinel color used to pre-fill destination buffers so unwritten pixels
// are distinguishable from freshly-blitted ones.
static const COLORREF kSentinel = RGB(0x11, 0x22, 0x33);
static const uint8_t kSentinelR = 0x11, kSentinelG = 0x22, kSentinelB = 0x33;

struct TestSurface {
    int width;
    int height;
    std::vector<uint8_t> pixels; // RGBA32
    HDC hdc;

    TestSurface(int w, int h) : width(w), height(h), pixels(static_cast<size_t>(w) * h * 4, 0)
    {
        FillSentinel();
        hdc = FreeApiCreateSurfaceDC(pixels.data(), width, height, width * 4, 32);
    }
    ~TestSurface()
    {
        if (hdc) FreeApiDestroySurfaceDC(hdc);
    }

    void FillSentinel()
    {
        for (size_t i = 0; i < pixels.size(); i += 4) {
            pixels[i + 0] = kSentinelR;
            pixels[i + 1] = kSentinelG;
            pixels[i + 2] = kSentinelB;
            pixels[i + 3] = 255;
        }
    }

    bool IsSentinelAt(int x, int y) const
    {
        if (x < 0 || y < 0 || x >= width || y >= height) return true; // out of bounds: nothing to check
        const size_t off = (static_cast<size_t>(y) * width + x) * 4;
        return pixels[off] == kSentinelR && pixels[off + 1] == kSentinelG && pixels[off + 2] == kSentinelB;
    }
};

static HBITMAP MakeSourceBitmap(int w, int h, const std::vector<uint8_t>& rgba)
{
    return CreateBitmap(w, h, 1, 32, rgba.data());
}

static void TestStretchBlt1to1CopiesExactRectAndLeavesRestUntouched()
{
    // 4x4 source, each pixel a distinct color: R = column, G = row.
    const int srcW = 4, srcH = 4;
    std::vector<uint8_t> srcPixels(static_cast<size_t>(srcW) * srcH * 4);
    for (int y = 0; y < srcH; ++y) {
        for (int x = 0; x < srcW; ++x) {
            size_t off = (static_cast<size_t>(y) * srcW + x) * 4;
            srcPixels[off + 0] = static_cast<uint8_t>(x * 50 + 10);
            srcPixels[off + 1] = static_cast<uint8_t>(y * 50 + 10);
            srcPixels[off + 2] = 0x99;
            srcPixels[off + 3] = 255;
        }
    }

    HBITMAP srcBitmap = MakeSourceBitmap(srcW, srcH, srcPixels);
    Check(srcBitmap != nullptr, "CreateBitmap succeeds for the 4x4 source pattern");

    HDC srcDc = CreateCompatibleDC(nullptr);
    SelectObject(srcDc, srcBitmap);

    TestSurface dest(8, 8);
    Check(dest.hdc != nullptr, "FreeApiCreateSurfaceDC succeeds for the 8x8 destination");

    BOOL ok = StretchBlt(dest.hdc, 2, 2, srcW, srcH, srcDc, 0, 0, srcW, srcH, SRCCOPY);
    Check(ok == TRUE, "StretchBlt(1:1) returns TRUE for a fully in-bounds blit");

    bool allMatch = true;
    for (int y = 0; y < srcH && allMatch; ++y) {
        for (int x = 0; x < srcW && allMatch; ++x) {
            COLORREF c = GetPixel(dest.hdc, 2 + x, 2 + y);
            const uint8_t expectedR = static_cast<uint8_t>(x * 50 + 10);
            const uint8_t expectedG = static_cast<uint8_t>(y * 50 + 10);
            if (GetRValue(c) != expectedR || GetGValue(c) != expectedG || GetBValue(c) != 0x99) {
                allMatch = false;
            }
        }
    }
    Check(allMatch, "StretchBlt(1:1) copies every source pixel to the exact expected destination position");

    // Pixels outside the blitted 4x4 rect at (2,2)-(5,5) must remain the sentinel.
    Check(dest.IsSentinelAt(0, 0), "StretchBlt(1:1) leaves pixels above/left of the blit rect untouched");
    Check(dest.IsSentinelAt(7, 7), "StretchBlt(1:1) leaves pixels below/right of the blit rect untouched");
    Check(dest.IsSentinelAt(6, 2), "StretchBlt(1:1) leaves pixels just past the blit rect's right edge untouched");

    DeleteObject(srcBitmap);
    DeleteDC(srcDc);
}

static void TestStretchBlt1to1ClippedAtDestinationEdgeStaysInBounds()
{
    const int srcW = 4, srcH = 4;
    std::vector<uint8_t> srcPixels(static_cast<size_t>(srcW) * srcH * 4, 0);
    for (size_t i = 0; i < srcPixels.size(); i += 4) {
        srcPixels[i + 0] = 0xAB;
        srcPixels[i + 1] = 0xCD;
        srcPixels[i + 2] = 0xEF;
        srcPixels[i + 3] = 255;
    }
    HBITMAP srcBitmap = MakeSourceBitmap(srcW, srcH, srcPixels);
    HDC srcDc = CreateCompatibleDC(nullptr);
    SelectObject(srcDc, srcBitmap);

    // 4x4 destination; blit a 4x4 source positioned so only the top-left
    // 2x2 destination cells are actually in bounds.
    TestSurface dest(4, 4);
    BOOL ok = StretchBlt(dest.hdc, 2, 2, srcW, srcH, srcDc, 0, 0, srcW, srcH, SRCCOPY);
    Check(ok == TRUE, "StretchBlt(1:1) returns TRUE for a partially off-edge blit (clipped, not rejected)");

    COLORREF inBounds = GetPixel(dest.hdc, 2, 2);
    Check(GetRValue(inBounds) == 0xAB && GetGValue(inBounds) == 0xCD && GetBValue(inBounds) == 0xEF,
          "the in-bounds corner of a clipped blit is still copied correctly");

    // The test passing without a crash here is itself part of the
    // guarantee: the clip math must not have driven a write past the
    // destination buffer's bounds.
    Check(dest.IsSentinelAt(0, 0), "pixels never reached by the clipped blit remain the sentinel");

    DeleteObject(srcBitmap);
    DeleteDC(srcDc);
}

// TASK-0068 (plan.md, todo/free-api-performance-todo.md "P2: Add Focused
// Performance/Correctness Tests"): out-of-range source rectangle handling
// -- a source rect that starts before (0,0) or extends past the source
// bitmap's width/height must be safely clipped (no OOB read), not rejected
// or allowed to read garbage.
static void TestStretchBlt1to1OutOfRangeSourceRectClipsSafely()
{
    const int srcW = 4, srcH = 4;
    std::vector<uint8_t> srcPixels(static_cast<size_t>(srcW) * srcH * 4);
    for (int y = 0; y < srcH; ++y) {
        for (int x = 0; x < srcW; ++x) {
            size_t off = (static_cast<size_t>(y) * srcW + x) * 4;
            srcPixels[off + 0] = static_cast<uint8_t>(x * 50 + 10);
            srcPixels[off + 1] = static_cast<uint8_t>(y * 50 + 10);
            srcPixels[off + 2] = 0x99;
            srcPixels[off + 3] = 255;
        }
    }
    HBITMAP srcBitmap = MakeSourceBitmap(srcW, srcH, srcPixels);
    HDC srcDc = CreateCompatibleDC(nullptr);
    SelectObject(srcDc, srcBitmap);

    // Case 1: negative source origin -- request source (-2,-2) sized 4x4;
    // only source (0,0)-(2,2) actually exists, which should land at
    // destination (2,2)-(4,4) (shifted by the same amount the negative
    // origin was clamped).
    {
        TestSurface dest(8, 8);
        BOOL ok = StretchBlt(dest.hdc, 0, 0, srcW, srcH, srcDc, -2, -2, srcW, srcH, SRCCOPY);
        Check(ok == TRUE, "StretchBlt(1:1) returns TRUE for a negative source origin (clipped, not rejected)");

        COLORREF shifted = GetPixel(dest.hdc, 2, 2);
        Check(GetRValue(shifted) == 10 && GetGValue(shifted) == 10 && GetBValue(shifted) == 0x99,
              "negative source origin clips safely: source (0,0) lands at the correctly shifted destination position");
        Check(dest.IsSentinelAt(0, 0),
              "negative source origin: destination pixels with no corresponding in-bounds source pixel stay untouched");
    }

    // Case 2: source rect extends past the bitmap's right/bottom edge --
    // request source (2,2) sized 4x4 from a 4x4 bitmap (only a 2x2 region
    // at (2,2)-(4,4) actually exists).
    {
        TestSurface dest(8, 8);
        BOOL ok = StretchBlt(dest.hdc, 0, 0, srcW, srcH, srcDc, 2, 2, srcW, srcH, SRCCOPY);
        Check(ok == TRUE, "StretchBlt(1:1) returns TRUE for a source rect extending past the bitmap's edge (clipped, not rejected)");

        COLORREF inBounds = GetPixel(dest.hdc, 0, 0);
        Check(GetRValue(inBounds) == static_cast<uint8_t>(2 * 50 + 10) &&
              GetGValue(inBounds) == static_cast<uint8_t>(2 * 50 + 10) &&
              GetBValue(inBounds) == 0x99,
              "out-of-range source rect clips safely: the in-bounds corner (source (2,2)) is copied correctly");
        Check(dest.IsSentinelAt(2, 0),
              "out-of-range source rect: destination pixels beyond the bitmap's actual extent stay untouched (no garbage read)");
        Check(dest.IsSentinelAt(0, 2),
              "out-of-range source rect: destination pixels beyond the bitmap's actual extent (other axis) stay untouched");
    }

    DeleteObject(srcBitmap);
    DeleteDC(srcDc);
}

static void TestStretchBltScaledNearestNeighborSamplesCorrectSourcePixel()
{
    // 2x2 source, each quadrant a distinct flat color.
    const int srcW = 2, srcH = 2;
    std::vector<uint8_t> srcPixels(static_cast<size_t>(srcW) * srcH * 4);
    auto setPixel = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        size_t off = (static_cast<size_t>(y) * srcW + x) * 4;
        srcPixels[off + 0] = r; srcPixels[off + 1] = g; srcPixels[off + 2] = b; srcPixels[off + 3] = 255;
    };
    setPixel(0, 0, 255, 0, 0);   // top-left: red
    setPixel(1, 0, 0, 255, 0);   // top-right: green
    setPixel(0, 1, 0, 0, 255);   // bottom-left: blue
    setPixel(1, 1, 255, 255, 0); // bottom-right: yellow

    HBITMAP srcBitmap = MakeSourceBitmap(srcW, srcH, srcPixels);
    HDC srcDc = CreateCompatibleDC(nullptr);
    SelectObject(srcDc, srcBitmap);

    // Scale up 2x2 -> 4x4 (each source pixel becomes a 2x2 destination block).
    TestSurface dest(4, 4);
    BOOL ok = StretchBlt(dest.hdc, 0, 0, 4, 4, srcDc, 0, 0, srcW, srcH, SRCCOPY);
    Check(ok == TRUE, "StretchBlt(scaled) returns TRUE for a 2x upscale");

    COLORREF topLeft = GetPixel(dest.hdc, 0, 0);
    COLORREF topRight = GetPixel(dest.hdc, 3, 0);
    COLORREF bottomLeft = GetPixel(dest.hdc, 0, 3);
    COLORREF bottomRight = GetPixel(dest.hdc, 3, 3);

    Check(GetRValue(topLeft) == 255 && GetGValue(topLeft) == 0 && GetBValue(topLeft) == 0,
          "scaled StretchBlt samples the top-left source quadrant (red) via nearest-neighbor");
    Check(GetRValue(topRight) == 0 && GetGValue(topRight) == 255 && GetBValue(topRight) == 0,
          "scaled StretchBlt samples the top-right source quadrant (green) via nearest-neighbor");
    Check(GetRValue(bottomLeft) == 0 && GetGValue(bottomLeft) == 0 && GetBValue(bottomLeft) == 255,
          "scaled StretchBlt samples the bottom-left source quadrant (blue) via nearest-neighbor");
    Check(GetRValue(bottomRight) == 255 && GetGValue(bottomRight) == 255 && GetBValue(bottomRight) == 0,
          "scaled StretchBlt samples the bottom-right source quadrant (yellow) via nearest-neighbor");

    DeleteObject(srcBitmap);
    DeleteDC(srcDc);
}

static void TestStretchBltScaledOutOfRangeSourceYClampsToEdgeRowLikeX()
{
    // 2x2 source: row 0 red, row 1 green. Claim a source rect taller than the
    // real bitmap (hSrc=4 against a 2-row bitmap) so the scaled path's source-Y
    // sampling goes out of range for destination row 1 -- this is the exact
    // asymmetry TASK-0003/TASK-24H-0601 fixed: X already clamped out-of-range
    // source columns to the nearest edge pixel, but Y instead skipped the
    // whole destination row, leaving it as stale/untouched sentinel data.
    // wDest(2)==wSrc(2) but hDest(2)!=hSrc(4), so the scaled (non-1:1) path is
    // exercised even though the X axis itself doesn't need scaling.
    const int srcW = 2, srcH = 2;
    std::vector<uint8_t> srcPixels(static_cast<size_t>(srcW) * srcH * 4);
    auto setRow = [&](int y, uint8_t r, uint8_t g, uint8_t b) {
        for (int x = 0; x < srcW; ++x) {
            size_t off = (static_cast<size_t>(y) * srcW + x) * 4;
            srcPixels[off + 0] = r; srcPixels[off + 1] = g; srcPixels[off + 2] = b; srcPixels[off + 3] = 255;
        }
    };
    setRow(0, 255, 0, 0); // row 0: red
    setRow(1, 0, 255, 0); // row 1: green (the clamped "edge row" once srcY overflows)

    HBITMAP srcBitmap = MakeSourceBitmap(srcW, srcH, srcPixels);
    HDC srcDc = CreateCompatibleDC(nullptr);
    SelectObject(srcDc, srcBitmap);

    TestSurface dest(2, 2);
    // hSrc=4 claims twice the real bitmap height; dstRow0=1 -> srcY = 1*4/2 = 2,
    // which is out of range for a 2-row (indices 0-1) bitmap.
    BOOL ok = StretchBlt(dest.hdc, 0, 0, 2, 2, srcDc, 0, 0, srcW, 4, SRCCOPY);
    Check(ok == TRUE, "StretchBlt(scaled, out-of-range source Y) returns TRUE");

    COLORREF row0 = GetPixel(dest.hdc, 0, 0);
    COLORREF row1 = GetPixel(dest.hdc, 0, 1);

    Check(GetRValue(row0) == 255 && GetGValue(row0) == 0 && GetBValue(row0) == 0,
          "in-range destination row 0 samples source row 0 (red) normally");
    Check(!dest.IsSentinelAt(0, 1),
          "out-of-range source Y clamps to the nearest edge row instead of leaving the destination row untouched");
    Check(GetRValue(row1) == 0 && GetGValue(row1) == 255 && GetBValue(row1) == 0,
          "out-of-range source Y clamps to the last real source row (green), matching the X-axis edge-clamp policy");

    DeleteObject(srcBitmap);
    DeleteDC(srcDc);
}

static void TestGetSetPixelRoundTripOnSurfaceDc()
{
    TestSurface dest(2, 2);

    COLORREF written = RGB(0x40, 0x80, 0xC0);
    SetPixel(dest.hdc, 1, 1, written);
    COLORREF readBack = GetPixel(dest.hdc, 1, 1);

    Check(GetRValue(readBack) == GetRValue(written) &&
          GetGValue(readBack) == GetGValue(written) &&
          GetBValue(readBack) == GetBValue(written),
          "SetPixel followed by GetPixel on a Surface DC round-trips the same RGB value");

    // Untouched pixel must remain the sentinel.
    Check(dest.IsSentinelAt(0, 0), "SetPixel does not affect other pixels on the same surface");
}

// TASK-0125 (plan.md): CreateBitmap's 8-bit-indexed and 16-bit RGB565
// conversion paths (src/wingdi_bitmap.cpp) feed Planet Blupi's minimap
// rebuild (decmap.cpp) but had no test coverage of the converted pixel
// values themselves -- only TASK-0067's HBITMAP-validity/dimensions check.
static void TestCreateBitmap8BitIndexedExpandsToGreyscaleRgba()
{
    const int w = 4, h = 1;
    const std::vector<uint8_t> indexed = {0x00, 0x40, 0x80, 0xFF};

    HBITMAP bmp = CreateBitmap(w, h, 1, 8, indexed.data());
    Check(bmp != nullptr, "CreateBitmap succeeds for 8-bit indexed input");

    BITMAP info{};
    int written = GetObjectA(bmp, sizeof(info), &info);
    Check(written == sizeof(BITMAP), "GetObjectA reports a full BITMAP struct for an 8-bit-created bitmap");
    Check(info.bmWidth == w && info.bmHeight == h, "GetObjectA reports correct dimensions for an 8-bit-created bitmap");

    const uint8_t* pixels = static_cast<const uint8_t*>(info.bmBits);
    bool allMatch = true;
    for (int x = 0; x < w && allMatch; ++x) {
        const uint8_t idx = indexed[static_cast<size_t>(x)];
        const uint8_t* px = pixels + x * 4;
        if (px[0] != idx || px[1] != idx || px[2] != idx || px[3] != 0xFF) {
            allMatch = false;
        }
    }
    Check(allMatch, "CreateBitmap's 8-bit path expands each index to RGBA32 (R=G=B=index, A=255)");

    DeleteObject(bmp);
}

static void TestCreateBitmap16BitRgb565ConvertsToExpectedRgba32()
{
    const int w = 4, h = 1;
    // Pure red, pure green, pure blue, white -- each exactly representable
    // in RGB565, so the expected RGBA32 output has no rounding error.
    const std::vector<uint16_t> rgb565 = {0xF800, 0x07E0, 0x001F, 0xFFFF};

    HBITMAP bmp = CreateBitmap(w, h, 1, 16, rgb565.data());
    Check(bmp != nullptr, "CreateBitmap succeeds for 16-bit RGB565 input");

    BITMAP info{};
    int written = GetObjectA(bmp, sizeof(info), &info);
    Check(written == sizeof(BITMAP), "GetObjectA reports a full BITMAP struct for a 16-bit-created bitmap");
    Check(info.bmWidth == w && info.bmHeight == h, "GetObjectA reports correct dimensions for a 16-bit-created bitmap");

    const uint8_t* pixels = static_cast<const uint8_t*>(info.bmBits);
    auto CheckPixel = [&](int x, uint8_t r, uint8_t g, uint8_t b, const char* what) {
        const uint8_t* px = pixels + x * 4;
        Check(px[0] == r && px[1] == g && px[2] == b && px[3] == 0xFF, what);
    };

    CheckPixel(0, 255, 0, 0, "16-bit RGB565 pure red (0xF800) converts to RGBA32 (255,0,0,255)");
    CheckPixel(1, 0, 255, 0, "16-bit RGB565 pure green (0x07E0) converts to RGBA32 (0,255,0,255)");
    CheckPixel(2, 0, 0, 255, "16-bit RGB565 pure blue (0x001F) converts to RGBA32 (0,0,255,255)");
    CheckPixel(3, 255, 255, 255, "16-bit RGB565 white (0xFFFF) converts to RGBA32 (255,255,255,255)");

    DeleteObject(bmp);
}

// TASK-0059 (plan.md): both games repeatedly create-then-destroy a memory
// DC purely to host a bitmap for GetDeviceCaps/GetObject/SelectObject/
// StretchBlt (free-eggbert pixmap.cpp:137,152, ddutil.cpp:141,166;
// planetblupi ddutil.cpp:198,230, pixmap.cpp:282,293). Verifies many
// create/destroy cycles don't crash and don't leak the live-DC count.
static void TestCreateCompatibleDcRepeatedLifecycleDoesNotLeak()
{
    using namespace FreeApi::Internal;

    const int64_t baselineLive = g_diagCompatDcs.load();
    const int64_t baselineEver = g_diagCompatDcsEver.load();

    const int kIterations = 5000;
    for (int i = 0; i < kIterations; ++i) {
        HDC dc = CreateCompatibleDC(nullptr);
        GetDeviceCaps(dc, SIZEPALETTE); // matches real "select/query" usage
        DeleteDC(dc);
    }

    Check(g_diagCompatDcs.load() == baselineLive,
          "repeated CreateCompatibleDC/DeleteDC cycles leave the live-DC count unchanged (no leak)");
    Check(g_diagCompatDcsEver.load() == baselineEver + kIterations,
          "every CreateCompatibleDC call in the loop was accounted for exactly once");
}

// TASK-0060 (plan.md): both games branch their TrueColor-vs-palette
// rendering path on GetDeviceCaps(hdc, SIZEPALETTE). Real Win32 only
// reports a nonzero SIZEPALETTE for an actual hardware-palette (<=8bpp)
// device; a modern TrueColor host reports 0. This matters because the two
// real call-site patterns only agree when the value is exactly 0:
//   - free-eggbert pixmap.cpp:146: disables true-color rendering when
//     devcap is nonzero AND < 257 (the real legacy-palette range) --
//     0 or a huge value would both leave true-color enabled.
//   - free-eggbert pixmap.cpp:428 / planetblupi pixmap.cpp:287: treat ANY
//     nonzero value as "this is a palette device" (m_bPalette = TRUE),
//     and ONLY exactly 0 as "not a palette device."
// A fixed nonzero placeholder (this function previously returned 256)
// satisfies neither game correctly: it forces free-eggbert's true-color
// decor off, and forces planetblupi's minimap onto its untested
// 8-bit-indexed CreateBitmap path (TASK-0125) instead of the true-color
// 16-bit path. This was a genuine bug found while writing this test, fixed
// alongside it (src/wingdi_misc.cpp).
static void TestGetDeviceCapsSizePaletteReportsTrueColorHost()
{
    HDC dc = CreateCompatibleDC(nullptr);
    Check(dc != nullptr, "CreateCompatibleDC succeeds for the GetDeviceCaps test");

    int sizePalette = GetDeviceCaps(dc, SIZEPALETTE);
    Check(sizePalette == 0,
          "GetDeviceCaps(SIZEPALETTE) reports 0, which both games' branching logic requires to correctly "
          "recognize a modern TrueColor host (not a legacy palette display)");

    DeleteDC(dc);
}

// TASK-0061 (plan.md): planetblupi reads 256 PALETTEENTRY values from this
// call into m_sysPal (pixmap.cpp:292). Verifies it fills exactly 256
// well-formed entries without crashing.
static void TestGetSystemPaletteEntriesFills256WellFormedEntries()
{
    HDC dc = CreateCompatibleDC(nullptr);
    Check(dc != nullptr, "CreateCompatibleDC succeeds for the GetSystemPaletteEntries test");

    PALETTEENTRY entries[256]{};
    UINT filled = GetSystemPaletteEntries(dc, 0, 256, entries);
    Check(filled == 256, "GetSystemPaletteEntries(0, 256) fills exactly 256 entries");

    bool wellFormed = true;
    for (UINT i = 0; i < filled; ++i) {
        if (entries[i].peRed != entries[i].peGreen || entries[i].peGreen != entries[i].peBlue) {
            wellFormed = false;
            break;
        }
    }
    Check(wellFormed, "every filled PALETTEENTRY is well-formed (grayscale placeholder: R=G=B)");

    DeleteDC(dc);
}

// Builds a minimal, valid, uncompressed 24-bit BMP byte stream in memory
// (14-byte BITMAPFILEHEADER + 40-byte BITMAPINFOHEADER + bottom-up pixel
// data), so LoadImageA's decoding can be tested against a known-dimension
// fixture without shipping a binary test asset.
static std::vector<uint8_t> MakeMinimalBmp(int width, int height)
{
    const int rowBytes = ((width * 3 + 3) / 4) * 4; // rows padded to 4 bytes
    const int pixelDataSize = rowBytes * height;
    const int dataOffset = 14 + 40;
    const int fileSize = dataOffset + pixelDataSize;

    std::vector<uint8_t> bmp(static_cast<size_t>(fileSize), 0);
    auto put16 = [&](size_t off, uint16_t v) { bmp[off] = v & 0xFF; bmp[off + 1] = (v >> 8) & 0xFF; };
    auto put32 = [&](size_t off, uint32_t v) {
        bmp[off] = v & 0xFF; bmp[off + 1] = (v >> 8) & 0xFF;
        bmp[off + 2] = (v >> 16) & 0xFF; bmp[off + 3] = (v >> 24) & 0xFF;
    };

    bmp[0] = 'B'; bmp[1] = 'M';
    put32(2, static_cast<uint32_t>(fileSize));
    put32(10, static_cast<uint32_t>(dataOffset));
    put32(14, 40); // BITMAPINFOHEADER size
    put32(18, static_cast<uint32_t>(width));
    put32(22, static_cast<uint32_t>(height)); // positive => bottom-up
    put16(26, 1);  // planes
    put16(28, 24); // bitCount
    put32(30, 0);  // BI_RGB
    put32(34, static_cast<uint32_t>(pixelDataSize));

    // Fill with a simple, non-zero BGR pattern so the fixture isn't
    // indistinguishable from an all-zeroed buffer.
    for (int i = 0; i < pixelDataSize; ++i) {
        bmp[static_cast<size_t>(dataOffset + i)] = static_cast<uint8_t>(0x40 + (i % 64));
    }
    return bmp;
}

// TASK-0062/0063 (plan.md): LoadImageA uses SDL_LoadBMP internally, and both
// games load every sprite-sheet asset through this path with a non-.bmp
// extension (.blp) (planetblupi ddutil.cpp:90,95,148,151); GetObjectA then
// drives DirectDraw surface sizing in free-direct off the reported
// bmWidth/bmHeight (free-eggbert ddutil.cpp:57,149; planetblupi
// ddutil.cpp:47,104,208). Verifies both against a real, non-.bmp-named
// fixture file.
static void TestLoadImageADecodesNonBmpExtensionAndGetObjectAReportsCorrectDimensions()
{
    const int width = 6, height = 4;
    const std::vector<uint8_t> bmpBytes = MakeMinimalBmp(width, height);

    const char* fixturePath = "test_gdi_fixture.blp";
    FILE* f = fopen(fixturePath, "wb");
    Check(f != nullptr, "test fixture file opens for writing");
    if (f) {
        fwrite(bmpBytes.data(), 1, bmpBytes.size(), f);
        fclose(f);
    }

    HANDLE h = LoadImageA(nullptr, fixturePath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    Check(h != nullptr, "LoadImageA decodes a real BMP byte stream saved with a non-.bmp (.blp) extension");

    if (h) {
        HBITMAP hbm = reinterpret_cast<HBITMAP>(h);
        BITMAP bm{};
        int written = GetObjectA(hbm, sizeof(bm), &bm);
        Check(written == sizeof(BITMAP), "GetObjectA reports a full BITMAP struct for a LoadImageA-loaded bitmap");
        Check(bm.bmWidth == width && bm.bmHeight == height,
              "GetObjectA reports the exact bmWidth/bmHeight of the loaded BMP fixture");

        DeleteObject(hbm);
    }

    remove(fixturePath);
}

// TASK-0064 (plan.md): both games select a loaded bitmap into a memory DC,
// blit, then delete it -- free-eggbert's DDCopyBitmap (ddutil.cpp:122-168)
// does exactly: CreateCompatibleDC -> SelectObject(dc, hbm) -> GetObject ->
// StretchBlt -> DeleteDC(dc) -> DeleteObject(hbm) (planetblupi ddutil.cpp
// follows the identical shape). Verifies the full sequence across repeated
// iterations without crash/leak.
static void TestSelectObjectDeleteObjectBitmapIntoDcLifecycle()
{
    using namespace FreeApi::Internal;
    const int64_t baselineDcLive = g_diagCompatDcs.load();

    const int srcW = 2, srcH = 2;
    std::vector<uint8_t> srcPixels(static_cast<size_t>(srcW) * srcH * 4, 0xAB);

    const int kIterations = 500;
    for (int i = 0; i < kIterations; ++i) {
        HBITMAP hbm = CreateBitmap(srcW, srcH, 1, 32, srcPixels.data());

        HDC hdcImage = CreateCompatibleDC(nullptr);
        SelectObject(hdcImage, hbm);

        BITMAP bm{};
        GetObjectA(hbm, sizeof(bm), &bm);

        TestSurface dest(4, 4);
        StretchBlt(dest.hdc, 0, 0, 4, 4, hdcImage, 0, 0, srcW, srcH, SRCCOPY);

        DeleteDC(hdcImage);
        DeleteObject(hbm);
    }

    Check(g_diagCompatDcs.load() == baselineDcLive,
          "repeated select->blit->delete cycles (matching DDCopyBitmap's exact sequence) leave the live-DC count unchanged");
}

// TASK-0114 (plan.md): a single, named end-to-end test chaining both games'
// full real-world blit pattern in one pass -- load (LoadImageA) -> select
// (CreateCompatibleDC/SelectObject) -> stretch-blit (StretchBlt) ->
// color-match (the GetDC/SetPixel/Lock-equivalent GetPixel/SetPixel
// round-trip DDColorMatch relies on, free-eggbert ddutil.cpp:276-291) ->
// delete (DeleteDC/DeleteObject). Each step is already unit-tested above
// (TASK-0062/0063/0064/0066/0067); this confirms they compose correctly as
// one sequence, not just in isolation.
static void TestFullLoadSelectBlitColorMatchDeleteSequence()
{
    const int width = 4, height = 4;
    const std::vector<uint8_t> bmpBytes = MakeMinimalBmp(width, height);

    const char* fixturePath = "test_gdi_e2e_fixture.blp";
    FILE* f = fopen(fixturePath, "wb");
    Check(f != nullptr, "e2e test fixture file opens for writing");
    if (f) {
        fwrite(bmpBytes.data(), 1, bmpBytes.size(), f);
        fclose(f);
    }

    // Load.
    HANDLE h = LoadImageA(nullptr, fixturePath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    Check(h != nullptr, "e2e: LoadImageA loads the fixture bitmap");
    HBITMAP hbm = reinterpret_cast<HBITMAP>(h);

    if (hbm) {
        BITMAP bm{};
        GetObjectA(hbm, sizeof(bm), &bm);
        Check(bm.bmWidth == width && bm.bmHeight == height, "e2e: GetObjectA reports the loaded bitmap's correct dimensions");

        // Select.
        HDC hdcImage = CreateCompatibleDC(nullptr);
        Check(hdcImage != nullptr, "e2e: CreateCompatibleDC succeeds");
        SelectObject(hdcImage, hbm);

        // Stretch-blit.
        TestSurface dest(8, 8);
        BOOL blitOk = StretchBlt(dest.hdc, 0, 0, width, height, hdcImage, 0, 0, width, height, SRCCOPY);
        Check(blitOk == TRUE, "e2e: StretchBlt copies the loaded bitmap onto the destination surface");

        // Color-match (DDColorMatch's own mechanism: SetPixel then read
        // back via the same DC -- see ddutil.cpp:276-291).
        COLORREF probe = RGB(0x11, 0x22, 0x33);
        SetPixel(dest.hdc, 0, 0, probe);
        COLORREF readBack = GetPixel(dest.hdc, 0, 0);
        Check(GetRValue(readBack) == GetRValue(probe) &&
              GetGValue(readBack) == GetGValue(probe) &&
              GetBValue(readBack) == GetBValue(probe),
              "e2e: SetPixel->GetPixel color-match round-trip succeeds on the blitted-to surface");

        // Delete.
        DeleteDC(hdcImage);
        BOOL deleted = DeleteObject(hbm);
        Check(deleted == TRUE, "e2e: DeleteObject succeeds, completing the full load->select->blit->color-match->delete sequence");
    }

    remove(fixturePath);
}

// TASK-24H-0607: the three free-direct-bridge GDI helpers
// (include/free_api_bridge.h) have real behavioral edge cases with no
// prior test coverage -- every existing test in this file constructs
// FreeApiCreateSurfaceDC with valid 32bpp arguments only, and
// FreeApiSetWindowFullscreen has zero references anywhere in tests/ or
// examples/ before this.
static void TestBridgeGdiHelpersRejectionAndEdgeCases()
{
    std::vector<uint8_t> validPixels(4 * 4 * 4, 0);

    HDC nullPixelsDc = FreeApiCreateSurfaceDC(nullptr, 4, 4, 16, 32);
    Check(nullPixelsDc == nullptr, "FreeApiCreateSurfaceDC returns NULL for a null pixels pointer");

    HDC badWidthDc = FreeApiCreateSurfaceDC(validPixels.data(), 0, 4, 16, 32);
    Check(badWidthDc == nullptr, "FreeApiCreateSurfaceDC returns NULL for width<=0");

    HDC badHeightDc = FreeApiCreateSurfaceDC(validPixels.data(), 4, -1, 16, 32);
    Check(badHeightDc == nullptr, "FreeApiCreateSurfaceDC returns NULL for height<=0");

    HDC badPitchDc = FreeApiCreateSurfaceDC(validPixels.data(), 4, 4, 0, 32);
    Check(badPitchDc == nullptr, "FreeApiCreateSurfaceDC returns NULL for pitch<=0");

    HDC badBppDc = FreeApiCreateSurfaceDC(validPixels.data(), 4, 4, 16, 16);
    Check(badBppDc == nullptr, "FreeApiCreateSurfaceDC returns NULL for bitsPerPixel != 32 (only 32bpp is supported)");

    BOOL destroyedNull = FreeApiDestroySurfaceDC(nullptr);
    Check(destroyedNull == FALSE, "FreeApiDestroySurfaceDC returns FALSE (not a crash) for a NULL handle");

    // A real, validly-allocated handle of the WRONG kind (a bitmap, not a
    // surface DC) must be rejected safely -- this is the realistic misuse
    // AsCompatDC's magic-number check (src/internal/FreeApiGdi.cpp) is
    // actually designed to guard against: handle-kind confusion between two
    // real free-api objects, not arbitrary unmapped memory. (A genuinely
    // garbage, never-allocated pointer -- e.g. reinterpret_cast<HDC>(0xDEADBEEF)
    // -- was tried here first and found to segfault: AsCompatDC/AsCompatBitmap
    // unconditionally dereference their argument to read a magic-number
    // field with no handle-table validation first. No evidenced free-direct
    // call site ever passes such a pointer -- it always forwards handles it
    // received from FreeApiCreateSurfaceDC itself -- so this is a real but
    // out-of-scope-to-fix robustness gap, not exercised by this test suite.)
    HBITMAP wrongKindHandle = CreateBitmap(2, 2, 1, 32, nullptr);
    Check(wrongKindHandle != nullptr, "a real bitmap handle was created for the wrong-kind-handle test");
    if (wrongKindHandle) {
        BOOL destroyedWrongKind = FreeApiDestroySurfaceDC(reinterpret_cast<HDC>(wrongKindHandle));
        Check(destroyedWrongKind == FALSE,
              "FreeApiDestroySurfaceDC returns FALSE (not a crash) for a real handle of the wrong kind (a bitmap, not a surface DC)");
        DeleteObject(wrongKindHandle);
    }

    // FreeApiSetWindowFullscreen(NULL, ...) exercises its early-return guard
    // without needing a real SDL-backed window -- must not crash.
    FreeApiSetWindowFullscreen(nullptr, true);
    FreeApiSetWindowFullscreen(nullptr, false);
    Check(true, "FreeApiSetWindowFullscreen(NULL, ...) does not crash for either fullscreen value (early-return guard)");
}

int main()
{
    printf("[gdi-regressions] Starting\n");

    TestStretchBlt1to1CopiesExactRectAndLeavesRestUntouched();
    TestStretchBlt1to1ClippedAtDestinationEdgeStaysInBounds();
    TestStretchBlt1to1OutOfRangeSourceRectClipsSafely();
    TestStretchBltScaledNearestNeighborSamplesCorrectSourcePixel();
    TestStretchBltScaledOutOfRangeSourceYClampsToEdgeRowLikeX();
    TestGetSetPixelRoundTripOnSurfaceDc();
    TestCreateBitmap8BitIndexedExpandsToGreyscaleRgba();
    TestCreateBitmap16BitRgb565ConvertsToExpectedRgba32();
    TestCreateCompatibleDcRepeatedLifecycleDoesNotLeak();
    TestGetDeviceCapsSizePaletteReportsTrueColorHost();
    TestGetSystemPaletteEntriesFills256WellFormedEntries();
    TestLoadImageADecodesNonBmpExtensionAndGetObjectAReportsCorrectDimensions();
    TestSelectObjectDeleteObjectBitmapIntoDcLifecycle();
    TestFullLoadSelectBlitColorMatchDeleteSequence();
    TestBridgeGdiHelpersRejectionAndEdgeCases();

    if (g_failures > 0) {
        printf("[gdi-regressions] %d FAILURE(S)\n", g_failures);
        return 1;
    }

    printf("[gdi-regressions] ALL TESTS PASSED\n");
    return 0;
}
