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

HCURSOR WINAPI SetCursor(HCURSOR hCursor)
{
    return hCursor;
}

int WINAPI ShowCursor(BOOL bShow)
{
    (void)bShow;
    return 0;
}

BOOL WINAPI ClientToScreen(HWND hWnd, LPPOINT lpPoint)
{
    if (!hWnd || !lpPoint) {
        return FALSE;
    }

    const LONG inX = lpPoint->x;
    const LONG inY = lpPoint->y;

    // Scale from logical client coordinates to physical client coordinates
    const auto it = g_freeApiWindowStates.find(hWnd);
    if (it != g_freeApiWindowStates.end()) {
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
