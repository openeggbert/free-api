#include "windows.h"
#include "internal/FreeApiWindowRegistry.hpp"
#include "internal/FreeApiMessageQueue.hpp"
#include "internal/FreeApiSdlVideo.hpp"
#include "internal/FreeApiDiagnostics.hpp"

#include <SDL3/SDL.h>

using namespace FreeApi::Internal;

extern "C" {

ATOM WINAPI RegisterClassA(const WNDCLASSA* lpWndClass)
{
    if (!lpWndClass || !lpWndClass->lpszClassName || !lpWndClass->lpfnWndProc) {
        return 0;
    }

    // Only lpfnWndProc is retained. hIcon/hCursor/hbrBackground/style/
    // cbClsExtra/cbWndExtra/lpszMenuName are intentionally dropped -- no
    // evidenced call site in either target game reads any of them back
    // after registration. See tests/test_winuser_regressions.cpp's
    // TestRegisterClassADiscardsNonWndprocFields (TASK-24H-0301).
    g_registeredClasses[lpWndClass->lpszClassName] = lpWndClass->lpfnWndProc;
    return 1;
}

HWND WINAPI CreateWindowExA(const DWORD dwExStyle,
                            LPCSTR lpClassName,
                            LPCSTR lpWindowName,
                            const DWORD dwStyle,
                            const int X,
                            const int Y,
                            const int nWidth,
                            const int nHeight,
                            HWND hWndParent,
                            HMENU hMenu,
                            HINSTANCE hInstance,
                            LPVOID lpParam)
{
    (void)dwExStyle;
    (void)hWndParent;
    (void)hMenu;
    (void)hInstance;
    (void)lpParam;

    if (FreeApiDiagnosticsEnabled()) {
        SDL_Log("free-api CreateWindowExA: class=%s title=%s style=0x%08lx exStyle=0x%08lx pos=(%d,%d) size=%dx%d",
                lpClassName ? lpClassName : "<null>",
                lpWindowName ? lpWindowName : "<null>",
                static_cast<unsigned long>(dwStyle),
                static_cast<unsigned long>(dwExStyle),
                X,
                Y,
                nWidth,
                nHeight);
    }

    if (!lpClassName) {
        if (FreeApiDiagnosticsEnabled()) {
            SDL_Log("free-api CreateWindowExA: missing class name");
        }
        return NULL;
    }

    const auto classIt = g_registeredClasses.find(lpClassName);
    if (classIt == g_registeredClasses.end()) {
        if (FreeApiDiagnosticsEnabled()) {
            SDL_Log("free-api CreateWindowExA: class not registered: %s", lpClassName);
        }
        return NULL;
    }

    if (!EnsureVideoSubsystem()) {
        return NULL;
    }

    const int width = nWidth > 0 ? nWidth : 640;
    const int height = nHeight > 0 ? nHeight : 480;
    // Do NOT use SDL_WINDOW_RESIZABLE by default: on some Wayland/X11 compositors
    // a resizable popup window immediately receives a WM_CLOSE from the compositor.
    // Planet Blupi uses WS_POPUPWINDOW|WS_CAPTION (popup with title bar, fixed size).
    Uint32 flags = 0;

    if ((dwStyle & WS_VISIBLE) == 0) {
        flags |= SDL_WINDOW_HIDDEN;
    }

    if ((dwStyle & WS_POPUP) != 0 && (dwStyle & WS_CAPTION) == 0) {
        flags |= SDL_WINDOW_BORDERLESS;
    }

    if (FreeApiDiagnosticsEnabled()) {
        SDL_Log("free-api SDL_CreateWindow: title=%s width=%d height=%d flags=0x%08x",
                lpWindowName ? lpWindowName : lpClassName,
                width,
                height,
                static_cast<unsigned>(flags));
    }
    auto* sdlWindow = SDL_CreateWindow(lpWindowName ? lpWindowName : lpClassName, width, height, flags);
    if (!sdlWindow) {
        if (FreeApiDiagnosticsEnabled()) {
            SDL_Log("free-api SDL_CreateWindow failed: %s", SDL_GetError());
        }
        return NULL;
    }

    if (FreeApiDiagnosticsEnabled()) {
        SDL_Log("free-api SDL_CreateWindow result: window=%p id=%u", static_cast<void*>(sdlWindow), static_cast<unsigned>(SDL_GetWindowID(sdlWindow)));
    }

    const int posX = (X < 0) ? SDL_WINDOWPOS_CENTERED : X;
    const int posY = (Y < 0) ? SDL_WINDOWPOS_CENTERED : Y;
    SDL_SetWindowPosition(sdlWindow, posX, posY);
    if (FreeApiDiagnosticsEnabled()) {
        SDL_Log("free-api SDL_SetWindowPosition: window=%p x=%d y=%d", static_cast<void*>(sdlWindow), posX, posY);
    }

    HWND hwnd = reinterpret_cast<HWND>(sdlWindow);
    g_windowProcedures[hwnd] = classIt->second;
    g_windowsById[SDL_GetWindowID(sdlWindow)] = hwnd;
    g_focusWindow = hwnd;

    FreeApiWindowState state;
    state.width = width;
    state.height = height;
    state.isFullscreen = false;
    g_freeApiWindowStates[hwnd] = state;

    CREATESTRUCTA createStruct{};
    createStruct.lpCreateParams = lpParam;
    createStruct.hInstance = hInstance;
    createStruct.hMenu = hMenu;
    createStruct.hwndParent = hWndParent;
    createStruct.cy = height;
    createStruct.cx = width;
    createStruct.y = Y;
    createStruct.x = X;
    createStruct.style = static_cast<LONG>(dwStyle);
    createStruct.lpszName = lpWindowName;
    createStruct.lpszClass = lpClassName;
    createStruct.dwExStyle = dwExStyle;
    classIt->second(hwnd, WM_CREATE, 0, reinterpret_cast<LPARAM>(&createStruct));

    if (FreeApiDiagnosticsEnabled()) {
        SDL_Log("free-api CreateWindowExA result: hwnd=%p visible=%s popup=%s caption=%s",
                hwnd,
                ((dwStyle & WS_VISIBLE) != 0) ? "yes" : "no",
                ((dwStyle & WS_POPUP) != 0) ? "yes" : "no",
                ((dwStyle & WS_CAPTION) != 0) ? "yes" : "no");
    }

    return hwnd;
}

HWND WINAPI CreateWindowA(LPCSTR lpClassName,
                          LPCSTR lpWindowName,
                          DWORD dwStyle,
                          int X,
                          int Y,
                          int nWidth,
                          int nHeight,
                          HWND hWndParent,
                          HMENU hMenu,
                          HINSTANCE hInstance,
                          LPVOID lpParam)
{
    return CreateWindowExA(0,
                           lpClassName,
                           lpWindowName,
                           dwStyle,
                           X,
                           Y,
                           nWidth,
                           nHeight,
                           hWndParent,
                           hMenu,
                           hInstance,
                           lpParam);
}

BOOL WINAPI DestroyWindow(HWND hWnd)
{
    if (!hWnd) {
        return FALSE;
    }

    // Guard against double-destroy: every other teardown function in this
    // codebase (DeleteObject/DeleteDC/FreeApiDestroySurfaceDC/KillTimer/
    // timeKillEvent/_lclose/...) safely no-ops on an already-torn-down
    // handle instead of touching it again. This one didn't, and a real,
    // ordinary play sequence can post WM_CLOSE twice for the same hwnd
    // (e.g. planetblupi's WM_PHASE_BYE quit-confirmation screen posts it
    // from more than one place without changing phase first), so a stale
    // non-null hWnd here isn't just a theoretical input -- it's the second
    // dispatch of a real double-post. g_windowProcedures is populated for
    // every window CreateWindowExA successfully creates and erased right
    // here on destroy, so "not present" reliably means "already destroyed
    // (or never a real window)".
    const auto procIt = g_windowProcedures.find(hWnd);
    if (procIt == g_windowProcedures.end()) {
        return FALSE;
    }

    // Real Win32 semantics: DestroyWindow synchronously sends WM_DESTROY to
    // the window's own procedure before the window is actually torn down.
    // Both target games rely on this to run their own WM_DESTROY handler
    // (killing their frame-pump timer via KillTimer/timeKillEvent, releasing
    // game objects) before quitting -- without this dispatch, that cleanup
    // never ran and the timer could keep firing after WinMain's message
    // loop had already exited.
    if (procIt->second) {
        procIt->second(hWnd, WM_DESTROY, 0, 0);
    }

    g_windowProcedures.erase(hWnd);
    g_freeApiWindowStates.erase(hWnd);
    if (g_focusWindow == hWnd) {
        g_focusWindow = NULL;
    }
    auto* sdlWin = reinterpret_cast<SDL_Window*>(hWnd);
    g_windowsById.erase(SDL_GetWindowID(sdlWin));
    SDL_DestroyWindow(sdlWin);

    ShutdownVideoSubsystemIfLastWindow();

    return TRUE;
}

BOOL WINAPI ShowWindow(HWND hWnd, int nCmdShow)
{
    if (!hWnd) {
        return FALSE;
    }

    auto* sdlWindow = reinterpret_cast<SDL_Window*>(hWnd);
    if (FreeApiDiagnosticsEnabled()) {
        SDL_Log("free-api ShowWindow: hwnd=%p cmd=%d window=%p", hWnd, nCmdShow, static_cast<void*>(sdlWindow));
    }
    // TASK-24H-0306: cases 2/6/3/9 below use raw numeric literals (real
    // Win32 SW_SHOWMINIMIZED/SW_MINIMIZE/SW_SHOWMAXIMIZED/SW_RESTORE
    // values) rather than named constants, because only SW_HIDE/SW_SHOW are
    // declared in include/winuser.h -- and these four branches are
    // currently structurally unreachable anyway: FreeApiRunWinMain
    // (src/winmain_bridge.cpp) always calls the game's entry point with
    // nCmdShow=SW_SHOW, and both games forward that same value straight
    // into their own ShowWindow(g_hWnd, nCmdShow) call. Kept for source-
    // completeness, not because they're exercised in practice.
    switch (nCmdShow) {
        case SW_HIDE:
            SDL_HideWindow(sdlWindow);
            break;
        case 2: // SW_SHOWMINIMIZED
        case 6: // SW_MINIMIZE
            SDL_MinimizeWindow(sdlWindow);
            break;
        case 3: // SW_SHOWMAXIMIZED
            SDL_MaximizeWindow(sdlWindow);
            SDL_ShowWindow(sdlWindow);
            SDL_RaiseWindow(sdlWindow);
            break;
        case 9: // SW_RESTORE
            SDL_RestoreWindow(sdlWindow);
            SDL_ShowWindow(sdlWindow);
            SDL_RaiseWindow(sdlWindow);
            break;
        default:
            SDL_ShowWindow(sdlWindow);
            SDL_RaiseWindow(sdlWindow);
            break;
    }

    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(sdlWindow, &windowWidth, &windowHeight);
    if (FreeApiDiagnosticsEnabled()) {
        SDL_Log("free-api ShowWindow applied: window=%p size=%dx%d", static_cast<void*>(sdlWindow), windowWidth, windowHeight);
    }

    return TRUE;
}

// TASK-24H-0309: does NOT update g_freeApiWindowStates's logical
// width/height, so GetClientRect/ClientToScreen/ScreenToClient's tracked
// size would go stale after a real resize. Confirmed harmless, not a
// TODO: the only call sites in either target game are inside movie.cpp,
// itself dead-reach since the AVI probe always fails (see
// docs/out-of-scope.md's MCI digital-video section) -- neither game's
// live code path ever actually calls this. Do not add logical-state
// tracking here without new evidence a real, reachable call site needs it.
BOOL WINAPI MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint)
{
    (void)bRepaint;
    if (!hWnd) {
        return FALSE;
    }

    SDL_Window* sdlWindow = reinterpret_cast<SDL_Window*>(hWnd);
    SDL_SetWindowPosition(sdlWindow, X, Y);
    SDL_SetWindowSize(sdlWindow, nWidth, nHeight);
    return TRUE;
}

BOOL WINAPI InvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase)
{
    (void)hWnd;
    (void)lpRect;
    (void)bErase;
    return TRUE;
}

// TASK-24H-0312: implemented as SDL_RaiseWindow, not real Win32's "force an
// immediate WM_PAINT if the update region is non-empty" semantics -- safe
// because neither target game ever uses WM_PAINT/BeginPaint/EndPaint/
// PAINTSTRUCT at all (confirmed zero references in either game's source).
// Do not implement a real WM_PAINT dispatch/paint cycle without an
// evidenced call site.
BOOL WINAPI UpdateWindow(HWND hWnd)
{
    if (!hWnd) {
        return FALSE;
    }

    auto* sdlWindow = reinterpret_cast<SDL_Window*>(hWnd);
    SDL_RaiseWindow(sdlWindow);
    if (FreeApiDiagnosticsEnabled()) {
        SDL_Log("free-api UpdateWindow: hwnd=%p raised window=%p", hWnd, static_cast<void*>(sdlWindow));
    }
    return TRUE;
}

BOOL WINAPI SetWindowTextA(HWND hWnd, LPCSTR lpString)
{
    if (!hWnd) {
        return FALSE;
    }

    SDL_Window* sdlWindow = reinterpret_cast<SDL_Window*>(hWnd);
    SDL_SetWindowTitle(sdlWindow, lpString ? lpString : "");
    return TRUE;
}

BOOL WINAPI GetClientRect(HWND hWnd, LPRECT lpRect)
{
    if (!hWnd || !lpRect) {
        return FALSE;
    }

    // Return the logical client size the game expects (e.g., 640x480).
    const auto it = g_freeApiWindowStates.find(hWnd);
    if (it != g_freeApiWindowStates.end()) {
        lpRect->left   = 0;
        lpRect->top    = 0;
        lpRect->right  = it->second.width;
        lpRect->bottom = it->second.height;
        return TRUE;
    }

    // TASK-24H-0314: defensive-only fallback, not a real, evidenced call
    // path. CreateWindowExA always inserts a g_freeApiWindowStates entry for
    // every window it creates (the sole insertion point), so this branch is
    // only reachable for an hWnd that was never created via
    // CreateWindowExA/CreateWindowA -- effectively unreachable from either
    // game's real code. Kept for robustness against a hypothetically
    // foreign HWND, not because it's exercised in practice.
    int w = 0;
    int h = 0;
    if (!SDL_GetWindowSize(reinterpret_cast<SDL_Window*>(hWnd), &w, &h)) {
        return FALSE;
    }

    lpRect->left   = 0;
    lpRect->top    = 0;
    lpRect->right  = w;
    lpRect->bottom = h;
    return TRUE;
}

HWND WINAPI SetFocus(HWND hWnd)
{
    HWND oldFocus = g_focusWindow;
    g_focusWindow = hWnd;
    if (hWnd) {
        auto* sdlWindow = reinterpret_cast<SDL_Window*>(hWnd);
        SDL_RaiseWindow(sdlWindow);
        if (FreeApiDiagnosticsEnabled()) {
            SDL_Log("free-api SetFocus: old=%p new=%p window=%p", oldFocus, hWnd, static_cast<void*>(sdlWindow));
        }
    } else {
        if (FreeApiDiagnosticsEnabled()) {
            SDL_Log("free-api SetFocus: old=%p new=<null>", oldFocus);
        }
    }
    return oldFocus;
}

} // extern "C"
