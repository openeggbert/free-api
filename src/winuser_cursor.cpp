#include "windows.h"
#include "internal/FreeApiWindowRegistry.hpp"
#include "internal/FreeApiMessageQueue.hpp"

#include <SDL3/SDL.h>

using namespace FreeApi::Internal;

extern "C" {

BOOL WINAPI GetCursorPos(LPPOINT lpPoint)
{
    if (!lpPoint) {
        return FALSE;
    }

    float x = 0.0f;
    float y = 0.0f;
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

    int winX = 0;
    int winY = 0;
    if (!SDL_GetWindowPosition(reinterpret_cast<SDL_Window*>(hWnd), &winX, &winY)) {
        return FALSE;
    }

    const LONG inX = lpPoint->x;
    const LONG inY = lpPoint->y;

    lpPoint->x -= winX;
    lpPoint->y -= winY;

    // Scale from physical client coordinates to logical client coordinates
    const auto it = g_freeApiWindowStates.find(hWnd);
    if (it != g_freeApiWindowStates.end()) {
        int pw, ph;
        if (SDL_GetWindowSize(reinterpret_cast<SDL_Window*>(hWnd), &pw, &ph) && pw > 0 && ph > 0) {
            lpPoint->x = lpPoint->x * it->second.width / pw;
            lpPoint->y = lpPoint->y * it->second.height / ph;
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

    const LONG inX = lpPoint->x;
    const LONG inY = lpPoint->y;

    // Scale from logical client coordinates to physical client coordinates.
    // TASK-24H-1240: this division's actual denominator is the LOGICAL
    // width/height (it->second.width/height), unlike ScreenToClient's
    // mirror-image division above, whose denominator is the physical
    // pw/ph -- so the pw > 0 && ph > 0 guard alone (kept for the
    // SDL_GetWindowSize() success check) does not protect THIS division
    // against a zero logical width/height. Add that guard too, matching
    // ScreenToClient's fallback: skip the scaling step (leave the point
    // unscaled) rather than divide by zero, still returning TRUE below.
    // Currently unreachable -- CreateWindowExA always populates a positive
    // logical width/height -- but a latent SIGFPE risk if that invariant
    // is ever broken by a future change.
    const auto it = g_freeApiWindowStates.find(hWnd);
    if (it != g_freeApiWindowStates.end() && it->second.width > 0 && it->second.height > 0) {
        int pw, ph;
        if (SDL_GetWindowSize(reinterpret_cast<SDL_Window*>(hWnd), &pw, &ph) && pw > 0 && ph > 0) {
            lpPoint->x = lpPoint->x * pw / it->second.width;
            lpPoint->y = lpPoint->y * ph / it->second.height;
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
