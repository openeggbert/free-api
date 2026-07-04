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
 * Uses FreeApiCreateSurfaceDC/FreeApiDestroySurfaceDC -- internal, non-WINAPI
 * C entry points (declared in src/wingdi_dc.cpp) that free-direct already
 * depends on to expose a GDI-compatible DC backed by a DirectDraw surface's
 * own pixel memory. Forward-declared here (not part of the public windows.h
 * surface) purely to construct a controlled destination buffer for testing.
 */
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

extern "C" HDC FreeApiCreateSurfaceDC(void* pixels, int width, int height, int pitch, int bitsPerPixel);
extern "C" BOOL FreeApiDestroySurfaceDC(HDC hdc);

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

int main()
{
    printf("[gdi-regressions] Starting\n");

    TestStretchBlt1to1CopiesExactRectAndLeavesRestUntouched();
    TestStretchBlt1to1ClippedAtDestinationEdgeStaysInBounds();
    TestStretchBltScaledNearestNeighborSamplesCorrectSourcePixel();
    TestGetSetPixelRoundTripOnSurfaceDc();

    if (g_failures > 0) {
        printf("[gdi-regressions] %d FAILURE(S)\n", g_failures);
        return 1;
    }

    printf("[gdi-regressions] ALL TESTS PASSED\n");
    return 0;
}
