#include "windows.h"
#include "internal/FreeApiWindowRegistry.hpp"
#include "internal/FreeApiMessageQueue.hpp"

#include <SDL3/SDL.h>
#include <cmath>

using namespace FreeApi::Internal;

namespace {
// Mouse events already use SDL's logical render coordinates. Coordinate
// conversions must use the same scale, viewport and letterbox offsets.
SDL_Renderer* LogicalRenderer(SDL_Window* window)
{
    SDL_Renderer* renderer = SDL_GetRenderer(window);
    int width = 0, height = 0;
    SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_DISABLED;
    if (!renderer || !SDL_GetRenderLogicalPresentation(renderer, &width, &height, &mode) ||
        mode == SDL_LOGICAL_PRESENTATION_DISABLED) return nullptr;
    return renderer;
}

// A window the game made full screen (Free Direct's DDSCL_FULLSCREEN and
// SetDisplayMode call FreeApiSetWindowFullscreen). Under real DirectDraw the
// screen then is the display mode the game set, e.g. 640x480: screen and
// client coordinates are the same. The games rely on it: blupi_recon's
// CPixmap::Display (games/sb2/src/pixmap.cpp:1184-1196) blits to the primary
// surface, which has no clipper in full screen, at the rectangle
// ClientToScreen returns. So in full screen ClientToScreen and ScreenToClient
// leave points unchanged, and GetCursorPos/SetCursorPos use the same
// (logical) coordinates, whatever the real display's size.
bool IsGameFullscreen(HWND hwnd)
{
    const auto it = g_freeApiWindowStates.find(hwnd);
    return it != g_freeApiWindowStates.end() && it->second.isFullscreen;
}

// Window position -> the game's logical coordinates, and back.
void WindowToLogical(HWND hwnd, float& x, float& y)
{
    auto* window = reinterpret_cast<SDL_Window*>(hwnd);
    if (SDL_Renderer* renderer = LogicalRenderer(window)) {
        SDL_RenderCoordinatesFromWindow(renderer, x, y, &x, &y);
        return;
    }
    const auto it = g_freeApiWindowStates.find(hwnd);
    int pw = 0, ph = 0;
    if (it != g_freeApiWindowStates.end() && SDL_GetWindowSize(window, &pw, &ph) && pw > 0 && ph > 0) {
        x = x * static_cast<float>(it->second.width) / static_cast<float>(pw);
        y = y * static_cast<float>(it->second.height) / static_cast<float>(ph);
    }
}

void LogicalToWindow(HWND hwnd, float& x, float& y)
{
    auto* window = reinterpret_cast<SDL_Window*>(hwnd);
    if (SDL_Renderer* renderer = LogicalRenderer(window)) {
        SDL_RenderCoordinatesToWindow(renderer, x, y, &x, &y);
        return;
    }
    const auto it = g_freeApiWindowStates.find(hwnd);
    int pw = 0, ph = 0;
    if (it != g_freeApiWindowStates.end() && it->second.width > 0 && it->second.height > 0 &&
        SDL_GetWindowSize(window, &pw, &ph)) {
        x = x * static_cast<float>(pw) / static_cast<float>(it->second.width);
        y = y * static_cast<float>(ph) / static_cast<float>(it->second.height);
    }
}
}

extern "C" {

BOOL WINAPI GetCursorPos(LPPOINT lpPoint)
{
    if (!lpPoint) {
        return FALSE;
    }

    float x = 0.0f;
    float y = 0.0f;
    if (g_focusWindow && IsGameFullscreen(g_focusWindow)) {
        // Full screen: the screen is the game's display mode (see IsGameFullscreen).
        SDL_GetMouseState(&x, &y);
        WindowToLogical(g_focusWindow, x, y);
        lpPoint->x = static_cast<LONG>(std::lround(x));
        lpPoint->y = static_cast<LONG>(std::lround(y));
        InputLog("GetCursorPos (full screen) -> (%d,%d)", (int)lpPoint->x, (int)lpPoint->y);
        return TRUE;
    }
    SDL_GetGlobalMouseState(&x, &y);
    lpPoint->x = static_cast<LONG>(x);
    lpPoint->y = static_cast<LONG>(y);
    InputLog("GetCursorPos -> screen=(%d,%d)", (int)lpPoint->x, (int)lpPoint->y);
    return TRUE;
}

BOOL WINAPI ScreenToClient(HWND hWnd, LPPOINT lpPoint)
{
    if (!hWnd || !lpPoint) {
        return FALSE;
    }
    if (IsGameFullscreen(hWnd)) {
        return TRUE;  // screen == client in the game's full screen (IsGameFullscreen)
    }

    int winX = 0;
    int winY = 0;
    if (!SDL_GetWindowPosition(reinterpret_cast<SDL_Window*>(hWnd), &winX, &winY)) {
        return FALSE;
    }

    const LONG inX = lpPoint->x;
    const LONG inY = lpPoint->y;

    lpPoint->x -= winX;
    lpPoint->y -= winY;

    if (SDL_Renderer* renderer = LogicalRenderer(reinterpret_cast<SDL_Window*>(hWnd))) {
        float x = 0.0f, y = 0.0f;
        if (!SDL_RenderCoordinatesFromWindow(renderer, static_cast<float>(lpPoint->x),
                                             static_cast<float>(lpPoint->y), &x, &y)) return FALSE;
        lpPoint->x = static_cast<LONG>(std::lround(x));
        lpPoint->y = static_cast<LONG>(std::lround(y));
        return TRUE;
    }

    // Before logical presentation exists, use the tracked client size.
    const auto it = g_freeApiWindowStates.find(hWnd);
    if (it != g_freeApiWindowStates.end()) {
        int pw, ph;
        if (SDL_GetWindowSize(reinterpret_cast<SDL_Window*>(hWnd), &pw, &ph) && pw > 0 && ph > 0) {
            lpPoint->x = static_cast<LONG>(std::lround(static_cast<double>(lpPoint->x) * it->second.width / pw));
            lpPoint->y = static_cast<LONG>(std::lround(static_cast<double>(lpPoint->y) * it->second.height / ph));
        }
    }

    InputLog("ScreenToClient hwnd=%p win=(%d,%d) screen=(%d,%d) -> client=(%d,%d)",
        static_cast<void*>(hWnd), winX, winY, static_cast<int>(inX), static_cast<int>(inY), static_cast<int>(lpPoint->x), static_cast<int>(lpPoint->y));
    return TRUE;
}

// Last cursor handle passed to SetCursor. Free API does not decode real
// cursor shapes (LoadCursorA is a safe non-null-handle stub), so this is
// only tracked for source-level compatibility with code that inspects the
// previously-active handle -- it has no effect on what is actually shown.
static HCURSOR g_currentCursor = nullptr;

HCURSOR WINAPI SetCursor(HCURSOR hCursor)
{
    const HCURSOR previous = g_currentCursor;
    g_currentCursor = hCursor;
    return previous;
}

// WinAPI-style signed display counter: each ShowCursor(TRUE) increments it,
// each ShowCursor(FALSE) decrements it, and the real OS cursor is visible
// whenever the counter is >= 0, matching real Win32 ShowCursor semantics.
static int g_cursorShowCount = 0;

int WINAPI ShowCursor(BOOL bShow)
{
    g_cursorShowCount += bShow ? 1 : -1;

    if (g_cursorShowCount >= 0) {
        SDL_ShowCursor();
    } else {
        SDL_HideCursor();
    }

    return g_cursorShowCount;
}

BOOL WINAPI ClientToScreen(HWND hWnd, LPPOINT lpPoint)
{
    if (!hWnd || !lpPoint) {
        return FALSE;
    }
    if (IsGameFullscreen(hWnd)) {
        return TRUE;  // screen == client in the game's full screen (IsGameFullscreen)
    }

    const LONG inX = lpPoint->x;
    const LONG inY = lpPoint->y;

    if (SDL_Renderer* renderer = LogicalRenderer(reinterpret_cast<SDL_Window*>(hWnd))) {
        float x = 0.0f, y = 0.0f;
        if (!SDL_RenderCoordinatesToWindow(renderer, static_cast<float>(lpPoint->x),
                                           static_cast<float>(lpPoint->y), &x, &y)) return FALSE;
        lpPoint->x = static_cast<LONG>(std::lround(x));
        lpPoint->y = static_cast<LONG>(std::lround(y));
    } else {
        // The renderer is not initialized yet (e.g. initial cursor placement).
        const auto it = g_freeApiWindowStates.find(hWnd);
        if (it != g_freeApiWindowStates.end() && it->second.width > 0 && it->second.height > 0) {
            int pw = 0, ph = 0;
            if (SDL_GetWindowSize(reinterpret_cast<SDL_Window*>(hWnd), &pw, &ph) && pw > 0 && ph > 0) {
                lpPoint->x = static_cast<LONG>(std::lround(static_cast<double>(lpPoint->x) * pw / it->second.width));
                lpPoint->y = static_cast<LONG>(std::lround(static_cast<double>(lpPoint->y) * ph / it->second.height));
            }
        }
    }

    int winX = 0;
    int winY = 0;
    if (!SDL_GetWindowPosition(reinterpret_cast<SDL_Window*>(hWnd), &winX, &winY)) {
        return FALSE;
    }

    lpPoint->x += winX;
    lpPoint->y += winY;
    InputLog("ClientToScreen hwnd=%p win=(%d,%d) client=(%d,%d) -> screen=(%d,%d)",
        static_cast<void*>(hWnd), winX, winY, static_cast<int>(inX), static_cast<int>(inY), static_cast<int>(lpPoint->x), static_cast<int>(lpPoint->y));
    return TRUE;
}

BOOL WINAPI SetCursorPos(int X, int Y)
{
    if (g_focusWindow && IsGameFullscreen(g_focusWindow)) {
        float x = static_cast<float>(X), y = static_cast<float>(Y);
        LogicalToWindow(g_focusWindow, x, y);
        SDL_WarpMouseInWindow(reinterpret_cast<SDL_Window*>(g_focusWindow), x, y);
        return TRUE;
    }
    return SDL_WarpMouseGlobal(static_cast<float>(X), static_cast<float>(Y));
}

HCURSOR WINAPI LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName)
{
    (void)hInstance;
    (void)lpCursorName;
    return reinterpret_cast<HCURSOR>(static_cast<uintptr_t>(1));
}

HICON WINAPI LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName)
{
    (void)hInstance;
    (void)lpIconName;
    return reinterpret_cast<HICON>(static_cast<uintptr_t>(1));
}

} // extern "C"
