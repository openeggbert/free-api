/**
 * @file 04_gdi_minimap.cpp
 * @brief Visual demonstration of Planet Blupi's minimap rendering path
 * selection, driven by GetDeviceCaps(SIZEPALETTE) -- see plan.md TASK-0060.
 *
 * Planet Blupi's CPixmap::InitSysPalette() calls GetDeviceCaps(hdc,
 * SIZEPALETTE); a nonzero result means "this is a legacy 256-color palette
 * display" (m_bPalette = TRUE), driving the minimap through CreateBitmap's
 * 8-bit-indexed conversion path (an untested greyscale placeholder -- real
 * palette lookup isn't implemented). A result of exactly 0 means "modern
 * TrueColor host" (m_bPalette = FALSE), taking the 16-bit RGB565 path
 * instead, which converts to real color.
 *
 * This example renders BOTH paths side by side from the *same* logical
 * 2x2-quadrant pattern, so you can see the difference directly:
 *   - Left panel:  CreateBitmap(..., 8, ...)  -- indexed/greyscale path.
 *   - Right panel: CreateBitmap(..., 16, ...) -- RGB565/color path.
 *
 * free-api's GetDeviceCaps(SIZEPALETTE) now returns 0 (fixed this session;
 * it previously returned 256, incorrectly forcing the left-panel/greyscale
 * path in the real game). The console prints which path the real game
 * would take given the current value.
 *
 * Note: free-api's own public API only provides GDI operations against an
 * in-memory pixel buffer (HDC/HBITMAP/StretchBlt/etc.) -- actually
 * presenting that buffer to the screen is `free-direct`'s job in the real
 * games (a DirectDraw primary surface attached to the window). This
 * example stands in for that with a few lines of direct SDL3 surface
 * blitting, clearly marked below, so the demo can show real pixels without
 * depending on free-direct.
 */
#include <windows.h>
#include <windowsx.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <vector>
#include <cstdint>

// Internal, non-public free-api entry points (src/wingdi_dc.cpp) that
// free-direct uses to wrap a real pixel buffer as a GDI-compatible
// Surface-kind HDC. Forward-declared here purely so this example can
// present StretchBlt's output, exactly like tests/test_gdi_regressions.cpp
// does for testing.
extern "C" HDC FreeApiCreateSurfaceDC(void* pixels, int width, int height, int pitch, int bitsPerPixel);
extern "C" BOOL FreeApiDestroySurfaceDC(HDC hdc);

static bool g_running = true;

static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_DESTROY) { g_running = false; return 0; }
    if (msg == WM_KEYDOWN && wParam == VK_ESCAPE) { PostMessageA(hwnd, WM_CLOSE, 0, 0); return 0; }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static HBITMAP Make8BitQuadrantBitmap(int w, int h)
{
    // Four grey shades -- this is what CreateBitmap's 8-bit-indexed path
    // (palette expansion not implemented) actually produces: R=G=B=index.
    std::vector<uint8_t> data(static_cast<size_t>(w) * h);
    const uint8_t shades[4] = {40, 90, 150, 210};
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int quadrant = (x < w / 2 ? 0 : 1) + (y < h / 2 ? 0 : 2);
            data[static_cast<size_t>(y) * w + x] = shades[quadrant];
        }
    }
    return CreateBitmap(w, h, 1, 8, data.data());
}

static HBITMAP Make16BitQuadrantBitmap(int w, int h)
{
    // Same quadrant layout, real RGB565 colors this time: red, green,
    // blue, yellow.
    std::vector<uint16_t> data(static_cast<size_t>(w) * h);
    const uint16_t colors[4] = {
        0xF800, // red
        0x07E0, // green
        0x001F, // blue
        0xFFE0, // yellow
    };
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int quadrant = (x < w / 2 ? 0 : 1) + (y < h / 2 ? 0 : 2);
            data[static_cast<size_t>(y) * w + x] = colors[quadrant];
        }
    }
    return CreateBitmap(w, h, 1, 16, data.data());
}

int main()
{
    printf("=== 04_gdi_minimap ===\n");

    const int sizePalette = GetDeviceCaps(nullptr, SIZEPALETTE);
    printf("GetDeviceCaps(SIZEPALETTE) = %d\n", sizePalette);
    if (sizePalette == 0) {
        printf("-> A real game would treat this as a modern TrueColor host\n");
        printf("   (m_bPalette = FALSE) and take the RIGHT panel's real-color\n");
        printf("   16-bit path for its minimap. This is the current, correct\n");
        printf("   behavior (fixed this session -- see plan.md TASK-0060).\n\n");
    } else {
        printf("-> A real game would treat this as a legacy palette display\n");
        printf("   (m_bPalette = TRUE) and take the LEFT panel's greyscale\n");
        printf("   8-bit path for its minimap -- this was the bug.\n\n");
    }

    WNDCLASSA wc{};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = (HINSTANCE)1;
    wc.hCursor       = LoadCursorA(nullptr, "IDC_ARROW");
    wc.hbrBackground = GetStockBrush(BLACK_BRUSH);
    wc.lpszClassName = "FreeApiExample_GdiMinimap";
    RegisterClassA(&wc);

    const int panelSize = 128;
    const int gap = 16;
    const int clientW = panelSize * 2 + gap * 3;
    const int clientH = panelSize + gap * 2;

    HWND hwnd = CreateWindowExA(0, "FreeApiExample_GdiMinimap",
                                 "Free API Example: GDI Minimap (left=8-bit, right=16-bit)",
                                 WS_POPUPWINDOW | WS_CAPTION | WS_VISIBLE,
                                 100, 100, clientW, clientH,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    if (!hwnd) {
        printf("CreateWindowExA failed.\n");
        return 1;
    }

    // --- free-direct stand-in: an off-screen RGBA32 back buffer wrapped as
    // a GDI Surface DC, purely so this demo can present pixels. ---
    std::vector<uint8_t> backBuffer(static_cast<size_t>(clientW) * clientH * 4, 0);
    HDC backDc = FreeApiCreateSurfaceDC(backBuffer.data(), clientW, clientH, clientW * 4, 32);

    const int srcSize = 4; // small logical source; StretchBlt scales it up
    HBITMAP bmp8  = Make8BitQuadrantBitmap(srcSize, srcSize);
    HBITMAP bmp16 = Make16BitQuadrantBitmap(srcSize, srcSize);

    HDC srcDc8 = CreateCompatibleDC(nullptr);
    SelectObject(srcDc8, bmp8);
    HDC srcDc16 = CreateCompatibleDC(nullptr);
    SelectObject(srcDc16, bmp16);

    StretchBlt(backDc, gap, gap, panelSize, panelSize,
               srcDc8, 0, 0, srcSize, srcSize, SRCCOPY);
    StretchBlt(backDc, gap * 2 + panelSize, gap, panelSize, panelSize,
               srcDc16, 0, 0, srcSize, srcSize, SRCCOPY);

    // --- free-direct stand-in continued: hand the back buffer to SDL for
    // actual on-screen presentation. ---
    SDL_Surface* backSurface = SDL_CreateSurfaceFrom(
        clientW, clientH, SDL_PIXELFORMAT_RGBA32, backBuffer.data(), clientW * 4);

    auto* sdlWindow = reinterpret_cast<SDL_Window*>(hwnd);

    MSG msg{};
    while (g_running) {
        if (PeekMessageA(&msg, nullptr, 0, 0, PM_NOREMOVE)) {
            if (!GetMessageA(&msg, nullptr, 0, 0)) break;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        } else {
            SDL_Surface* windowSurface = SDL_GetWindowSurface(sdlWindow);
            if (windowSurface && backSurface) {
                SDL_BlitSurface(backSurface, nullptr, windowSurface, nullptr);
                SDL_UpdateWindowSurface(sdlWindow);
            }
            SDL_Delay(16);
        }
    }

    SDL_DestroySurface(backSurface);
    DeleteDC(srcDc8);
    DeleteDC(srcDc16);
    DeleteObject(bmp8);
    DeleteObject(bmp16);
    FreeApiDestroySurfaceDC(backDc);

    printf("Exiting cleanly.\n");
    return 0;
}
