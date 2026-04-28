#include "windows.h"
#include "io.h"
#include "mmsystem.h"
#include "digitalv.h"
#include <SDL3/SDL.h>

#include <chrono>
#include <cstdio>
#include <queue>
#include <string>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdarg>
#include <fcntl.h>
#include <unistd.h>

namespace {
    constexpr uint32_t kCompatBitmapMagic = 0x504d5442u; // 'BTMP'
    constexpr uint32_t kCompatDcMagic = 0x30434446u;     // 'FDC0'

    enum class CompatDcKind {
        Memory,
        Surface
    };

    struct CompatBitmap {
        uint32_t magic = kCompatBitmapMagic;
        int width = 0;
        int height = 0;
        int pitch = 0;
        int bitsPerPixel = 32;
        std::vector<uint8_t> pixels;
    };

    struct CompatDC {
        uint32_t magic = kCompatDcMagic;
        CompatDcKind kind = CompatDcKind::Memory;
        CompatBitmap* selectedBitmap = nullptr;
        uint8_t* surfacePixels = nullptr;
        int surfaceWidth = 0;
        int surfaceHeight = 0;
        int surfacePitch = 0;
        int surfaceBitsPerPixel = 32;
    };

    std::unordered_map<std::string, WNDPROC> g_registeredClasses;
    std::unordered_map<HWND, WNDPROC> g_windowProcedures;
    std::queue<MSG> g_messageQueue;
    std::unordered_set<UINT> g_activeTimerIds;
    bool g_videoInitialized = false;
    std::atomic<UINT> g_nextTimerId{1};
    HWND g_focusWindow = NULL;

    CompatBitmap* AsCompatBitmap(HGDIOBJ object)
    {
        auto* bitmap = reinterpret_cast<CompatBitmap*>(object);
        if (!bitmap || bitmap->magic != kCompatBitmapMagic) {
            return nullptr;
        }
        return bitmap;
    }

    CompatDC* AsCompatDC(HDC dc)
    {
        auto* compatDc = reinterpret_cast<CompatDC*>(dc);
        if (!compatDc || compatDc->magic != kCompatDcMagic) {
            return nullptr;
        }
        return compatDc;
    }

    std::string NormalizePath(const char* path)
    {
        std::string normalized = path ? path : "";
        for (char& ch : normalized) {
            if (ch == '\\') {
                ch = '/';
            }
        }
        return normalized;
    }

    CompatBitmap* CreateCompatBitmapFromSurface(SDL_Surface* surface)
    {
        if (!surface) {
            return nullptr;
        }

        SDL_Surface* rgbaSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        if (!rgbaSurface) {
            SDL_Log("free-api LoadImageA: SDL_ConvertSurface failed: %s", SDL_GetError());
            return nullptr;
        }

        auto* bitmap = new CompatBitmap{};
        bitmap->width = rgbaSurface->w;
        bitmap->height = rgbaSurface->h;
        bitmap->bitsPerPixel = 32;
        bitmap->pitch = bitmap->width * 4;
        bitmap->pixels.resize(static_cast<size_t>(bitmap->pitch) * static_cast<size_t>(bitmap->height));

        for (int y = 0; y < bitmap->height; ++y) {
            const auto* srcRow = static_cast<const uint8_t*>(rgbaSurface->pixels) + static_cast<size_t>(y) * static_cast<size_t>(rgbaSurface->pitch);
            auto* dstRow = bitmap->pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(bitmap->pitch);
            memcpy(dstRow, srcRow, static_cast<size_t>(bitmap->pitch));
        }

        SDL_DestroySurface(rgbaSurface);
        return bitmap;
    }

    void ScaleCompatBitmap(CompatBitmap& bitmap, const int targetWidth, const int targetHeight)
    {
        if (targetWidth <= 0 || targetHeight <= 0 || (targetWidth == bitmap.width && targetHeight == bitmap.height)) {
            return;
        }

        std::vector<uint8_t> scaled(static_cast<size_t>(targetWidth) * static_cast<size_t>(targetHeight) * 4u, 0);
        const int srcWidth = bitmap.width;
        const int srcHeight = bitmap.height;
        const int srcPitch = bitmap.pitch;
        const uint8_t* srcData = bitmap.pixels.data();

        for (int y = 0; y < targetHeight; ++y) {
            const int srcY = (y * srcHeight) / targetHeight;
            auto* dstRow = scaled.data() + static_cast<size_t>(y) * static_cast<size_t>(targetWidth) * 4u;
            for (int x = 0; x < targetWidth; ++x) {
                const int srcX = (x * srcWidth) / targetWidth;
                const auto* srcPixel = srcData + static_cast<size_t>(srcY) * static_cast<size_t>(srcPitch) + static_cast<size_t>(srcX) * 4u;
                auto* dstPixel = dstRow + static_cast<size_t>(x) * 4u;
                dstPixel[0] = srcPixel[0];
                dstPixel[1] = srcPixel[1];
                dstPixel[2] = srcPixel[2];
                dstPixel[3] = srcPixel[3];
            }
        }

        bitmap.width = targetWidth;
        bitmap.height = targetHeight;
        bitmap.pitch = targetWidth * 4;
        bitmap.pixels.swap(scaled);
    }

    bool EnsureVideoSubsystem()
    {
        if (g_videoInitialized) {
            return true;
        }

        if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
            SDL_Log("free-api EnsureVideoSubsystem: SDL_INIT_VIDEO failed: %s", SDL_GetError());
            return false;
        }

        g_videoInitialized = true;
        SDL_Log("free-api EnsureVideoSubsystem: SDL video initialized");
        return true;
    }

    HWND FindWindowById(const SDL_WindowID windowId)
    {
        for (const auto& [hwnd, proc] : g_windowProcedures) {
            auto* sdlWindow = reinterpret_cast<SDL_Window*>(hwnd);
            if (sdlWindow && SDL_GetWindowID(sdlWindow) == windowId) {
                return hwnd;
            }
        }
        return NULL;
    }

    void PushMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        MSG msg{};
        msg.hwnd = hwnd;
        msg.message = message;
        msg.wParam = wParam;
        msg.lParam = lParam;
        msg.time = static_cast<DWORD>(SDL_GetTicks());
        msg.pt = {0, 0};
        g_messageQueue.push(msg);
    }

    void PumpSdlEvents()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    PushMessage(NULL, WM_QUIT, 0, 0);
                    break;
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    PushMessage(FindWindowById(event.window.windowID), WM_CLOSE, 0, 0);
                    break;
                default:
                    break;
            }
        }
    }

    std::string BuildCommandLine(const int argc, char** argv)
    {
        std::string cmdLine;
        for (int i = 1; i < argc; ++i) {
            if (!cmdLine.empty()) {
                cmdLine += ' ';
            }
            if (argv[i]) {
                cmdLine += argv[i];
            }
        }
        return cmdLine;
    }
}

extern "C" {

char* _pgmptr = nullptr;

void WINAPI Sleep(DWORD dwMilliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(dwMilliseconds));
}

DWORD WINAPI GetTickCount(void) {
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return static_cast<DWORD>(ms.count());
}

void WINAPI GlobalMemoryStatus(LPMEMORYSTATUS lpBuffer)
{
    if (!lpBuffer) {
        return;
    }

    lpBuffer->dwLength = sizeof(MEMORYSTATUS);
    lpBuffer->dwMemoryLoad = 25;
    lpBuffer->dwTotalPhys = 512u * 1024u * 1024u;
    lpBuffer->dwAvailPhys = 256u * 1024u * 1024u;
    lpBuffer->dwTotalPageFile = 1024u * 1024u * 1024u;
    lpBuffer->dwAvailPageFile = 512u * 1024u * 1024u;
    lpBuffer->dwTotalVirtual = 1024u * 1024u * 1024u;
    lpBuffer->dwAvailVirtual = 512u * 1024u * 1024u;
}

BOOL WINAPI CloseHandle(HANDLE hObject) {
    // placeholder for now
    return TRUE;
}

void WINAPI OutputDebugStringA(LPCSTR lpOutputString) {
    if (lpOutputString) {
        printf("%s", lpOutputString);
    }
}

void WINAPI OutputDebugStringW(LPCWSTR lpOutputString) {

    if (lpOutputString) {
        while (*lpOutputString) {
            printf("%c", (char)*lpOutputString++);
        }
    }
}

ATOM WINAPI RegisterClassA(const WNDCLASSA* lpWndClass)
{
    if (!lpWndClass || !lpWndClass->lpszClassName || !lpWndClass->lpfnWndProc) {
        return 0;
    }

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

    SDL_Log("free-api CreateWindowExA: class=%s title=%s style=0x%08lx exStyle=0x%08lx pos=(%d,%d) size=%dx%d", 
            lpClassName ? lpClassName : "<null>",
            lpWindowName ? lpWindowName : "<null>",
            static_cast<unsigned long>(dwStyle),
            static_cast<unsigned long>(dwExStyle),
            X,
            Y,
            nWidth,
            nHeight);

    if (!lpClassName) {
        SDL_Log("free-api CreateWindowExA: missing class name");
        return NULL;
    }

    const auto classIt = g_registeredClasses.find(lpClassName);
    if (classIt == g_registeredClasses.end()) {
        SDL_Log("free-api CreateWindowExA: class not registered: %s", lpClassName);
        return NULL;
    }

    if (!EnsureVideoSubsystem()) {
        return NULL;
    }

    const int width = nWidth > 0 ? nWidth : 640;
    const int height = nHeight > 0 ? nHeight : 480;
    Uint32 flags = SDL_WINDOW_RESIZABLE;

    if ((dwStyle & WS_VISIBLE) == 0) {
        flags |= SDL_WINDOW_HIDDEN;
    }

    if ((dwStyle & WS_POPUP) != 0 && (dwStyle & WS_CAPTION) == 0) {
        flags |= SDL_WINDOW_BORDERLESS;
    }

    SDL_Log("free-api SDL_CreateWindow: title=%s width=%d height=%d flags=0x%08x", 
            lpWindowName ? lpWindowName : lpClassName,
            width,
            height,
            static_cast<unsigned>(flags));
    auto* sdlWindow = SDL_CreateWindow(lpWindowName ? lpWindowName : lpClassName, width, height, flags);
    if (!sdlWindow) {
        SDL_Log("free-api SDL_CreateWindow failed: %s", SDL_GetError());
        return NULL;
    }

    SDL_Log("free-api SDL_CreateWindow result: window=%p id=%u", static_cast<void*>(sdlWindow), static_cast<unsigned>(SDL_GetWindowID(sdlWindow)));

    const int posX = (X < 0) ? SDL_WINDOWPOS_CENTERED : X;
    const int posY = (Y < 0) ? SDL_WINDOWPOS_CENTERED : Y;
    SDL_SetWindowPosition(sdlWindow, posX, posY);
    SDL_Log("free-api SDL_SetWindowPosition: window=%p x=%d y=%d", static_cast<void*>(sdlWindow), posX, posY);

    HWND hwnd = reinterpret_cast<HWND>(sdlWindow);
    g_windowProcedures[hwnd] = classIt->second;
    g_focusWindow = hwnd;

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

    SDL_Log("free-api CreateWindowExA result: hwnd=%p visible=%s popup=%s caption=%s", 
            hwnd,
            ((dwStyle & WS_VISIBLE) != 0) ? "yes" : "no",
            ((dwStyle & WS_POPUP) != 0) ? "yes" : "no",
            ((dwStyle & WS_CAPTION) != 0) ? "yes" : "no");

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

    g_windowProcedures.erase(hWnd);
    SDL_DestroyWindow(reinterpret_cast<SDL_Window*>(hWnd));

    if (g_windowProcedures.empty() && g_videoInitialized) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        g_videoInitialized = false;
    }

    return TRUE;
}

BOOL WINAPI ShowWindow(HWND hWnd, int nCmdShow)
{
    if (!hWnd) {
        return FALSE;
    }

    auto* sdlWindow = reinterpret_cast<SDL_Window*>(hWnd);
    SDL_Log("free-api ShowWindow: hwnd=%p cmd=%d window=%p", hWnd, nCmdShow, static_cast<void*>(sdlWindow));
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
    SDL_Log("free-api ShowWindow applied: window=%p size=%dx%d", static_cast<void*>(sdlWindow), windowWidth, windowHeight);

    return TRUE;
}

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

BOOL WINAPI UpdateWindow(HWND hWnd)
{
    if (!hWnd) {
        return FALSE;
    }

    auto* sdlWindow = reinterpret_cast<SDL_Window*>(hWnd);
    SDL_RaiseWindow(sdlWindow);
    SDL_Log("free-api UpdateWindow: hwnd=%p raised window=%p", hWnd, static_cast<void*>(sdlWindow));
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

BOOL WINAPI PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    PushMessage(hWnd, Msg, wParam, lParam);
    return TRUE;
}

int WINAPI MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
    (void)hWnd;
    (void)uType;

    const char* caption = lpCaption ? lpCaption : "Message";
    const char* text = lpText ? lpText : "";
    fprintf(stderr, "[MessageBoxA] %s: %s\n", caption, text);
    return 1;
}

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
    return TRUE;
}

BOOL WINAPI ScreenToClient(HWND hWnd, LPPOINT lpPoint)
{
    if (!hWnd || !lpPoint) {
        return FALSE;
    }

    int x = 0;
    int y = 0;
    if (!SDL_GetWindowPosition(reinterpret_cast<SDL_Window*>(hWnd), &x, &y)) {
        return FALSE;
    }

    lpPoint->x -= x;
    lpPoint->y -= y;
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

    int x = 0;
    int y = 0;
    if (!SDL_GetWindowPosition(reinterpret_cast<SDL_Window*>(hWnd), &x, &y)) {
        return FALSE;
    }

    lpPoint->x += x;
    lpPoint->y += y;
    return TRUE;
}

BOOL WINAPI SetCursorPos(int X, int Y)
{
    return SDL_WarpMouseGlobal(static_cast<float>(X), static_cast<float>(Y));
}

int WINAPI LoadStringA(HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, int cchBufferMax)
{
    (void)hInstance;
    if (!lpBuffer || cchBufferMax <= 0) {
        return 0;
    }

    int written = snprintf(lpBuffer, static_cast<size_t>(cchBufferMax), "RES_%u", uID);
    if (written < 0) {
        lpBuffer[0] = '\0';
        return 0;
    }
    if (written >= cchBufferMax) {
        return cchBufferMax - 1;
    }
    return written;
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

HBRUSH WINAPI GetStockBrush(int fnObject)
{
    return reinterpret_cast<HBRUSH>(static_cast<uintptr_t>(fnObject + 1));
}

HMODULE WINAPI GetModuleHandleA(LPCSTR lpModuleName)
{
    (void)lpModuleName;
    return reinterpret_cast<HMODULE>(static_cast<uintptr_t>(1));
}

HDC FreeApiCreateSurfaceDC(void* pixels, int width, int height, int pitch, int bitsPerPixel)
{
    if (!pixels || width <= 0 || height <= 0 || pitch <= 0 || bitsPerPixel != 32) {
        return NULL;
    }

    auto* dc = new CompatDC{};
    dc->kind = CompatDcKind::Surface;
    dc->surfacePixels = static_cast<uint8_t*>(pixels);
    dc->surfaceWidth = width;
    dc->surfaceHeight = height;
    dc->surfacePitch = pitch;
    dc->surfaceBitsPerPixel = bitsPerPixel;
    return reinterpret_cast<HDC>(dc);
}

BOOL FreeApiDestroySurfaceDC(HDC hdc)
{
    auto* dc = AsCompatDC(hdc);
    if (!dc) {
        return FALSE;
    }

    delete dc;
    return TRUE;
}

HANDLE WINAPI LoadImageA(HINSTANCE hInst, LPCSTR name, UINT type, int cx, int cy, UINT fuLoad)
{
    (void)hInst;

    if (type != IMAGE_BITMAP || !name) {
        return NULL;
    }

    if ((fuLoad & LR_LOADFROMFILE) == 0) {
        SDL_Log("free-api LoadImageA: resource bitmap loading is not implemented for '%s'", name);
        return NULL;
    }

    std::string normalizedPath = NormalizePath(name);
    SDL_Surface* loaded = SDL_LoadBMP(normalizedPath.c_str());
    if (!loaded) {
        SDL_Log("free-api LoadImageA: SDL_LoadBMP failed for '%s': %s", normalizedPath.c_str(), SDL_GetError());
        return NULL;
    }

    CompatBitmap* bitmap = CreateCompatBitmapFromSurface(loaded);
    SDL_DestroySurface(loaded);
    if (!bitmap) {
        return NULL;
    }

    if (cx > 0 && cy > 0) {
        ScaleCompatBitmap(*bitmap, cx, cy);
    }

    SDL_Log("free-api LoadImageA: loaded bitmap '%s' -> %dx%d", normalizedPath.c_str(), bitmap->width, bitmap->height);
    return reinterpret_cast<HANDLE>(bitmap);
}

int WINAPI GetObjectA(HANDLE h, int c, LPVOID pv)
{
    if (!h || !pv || c <= 0) {
        return 0;
    }

    CompatBitmap* bitmap = AsCompatBitmap(reinterpret_cast<HGDIOBJ>(h));
    if (!bitmap) {
        return 0;
    }

    BITMAP info{};
    info.bmType = 0;
    info.bmWidth = bitmap->width;
    info.bmHeight = bitmap->height;
    info.bmWidthBytes = bitmap->pitch;
    info.bmPlanes = 1;
    info.bmBitsPixel = static_cast<WORD>(bitmap->bitsPerPixel);
    info.bmBits = bitmap->pixels.data();

    const int copySize = c < static_cast<int>(sizeof(BITMAP)) ? c : static_cast<int>(sizeof(BITMAP));
    memcpy(pv, &info, static_cast<size_t>(copySize));
    return static_cast<int>(sizeof(BITMAP));
}

BOOL WINAPI DeleteObject(HGDIOBJ ho)
{
    auto* bitmap = AsCompatBitmap(ho);
    if (!bitmap) {
        return FALSE;
    }

    delete bitmap;
    return TRUE;
}

HDC WINAPI CreateCompatibleDC(HDC hdc)
{
    (void)hdc;
    auto* dc = new CompatDC{};
    dc->kind = CompatDcKind::Memory;
    return reinterpret_cast<HDC>(dc);
}

HGDIOBJ WINAPI SelectObject(HDC hdc, HGDIOBJ h)
{
    CompatDC* dc = AsCompatDC(hdc);
    if (!dc) {
        return NULL;
    }

    if (CompatBitmap* bitmap = AsCompatBitmap(h)) {
        HGDIOBJ previous = reinterpret_cast<HGDIOBJ>(dc->selectedBitmap);
        dc->selectedBitmap = bitmap;
        return previous;
    }

    return NULL;
}

BOOL WINAPI DeleteDC(HDC hdc)
{
    CompatDC* dc = AsCompatDC(hdc);
    if (!dc) {
        return FALSE;
    }

    delete dc;
    return TRUE;
}

BOOL WINAPI StretchBlt(HDC hdcDest,
                       int xDest,
                       int yDest,
                       int wDest,
                       int hDest,
                       HDC hdcSrc,
                       int xSrc,
                       int ySrc,
                       int wSrc,
                       int hSrc,
                       DWORD rop)
{
    if (rop != SRCCOPY) {
        SDL_Log("free-api StretchBlt: unsupported ROP=0x%08lx", static_cast<unsigned long>(rop));
        return FALSE;
    }

    CompatDC* dst = AsCompatDC(hdcDest);
    CompatDC* src = AsCompatDC(hdcSrc);
    if (!dst || !src || dst->kind != CompatDcKind::Surface || dst->surfaceBitsPerPixel != 32 || !dst->surfacePixels || !src->selectedBitmap) {
        SDL_Log("free-api StretchBlt: unsupported DC pair dst=%p src=%p", reinterpret_cast<void*>(hdcDest), reinterpret_cast<void*>(hdcSrc));
        return FALSE;
    }

    if (wDest <= 0 || hDest <= 0 || wSrc <= 0 || hSrc <= 0) {
        return FALSE;
    }

    CompatBitmap* srcBitmap = src->selectedBitmap;
    const uint8_t* srcPixels = srcBitmap->pixels.data();

    for (int y = 0; y < hDest; ++y) {
        const int dstY = yDest + y;
        if (dstY < 0 || dstY >= dst->surfaceHeight) {
            continue;
        }

        const int srcY = ySrc + static_cast<int>((static_cast<int64_t>(y) * static_cast<int64_t>(hSrc)) / static_cast<int64_t>(hDest));
        if (srcY < 0 || srcY >= srcBitmap->height) {
            continue;
        }

        auto* dstRow = dst->surfacePixels + static_cast<size_t>(dstY) * static_cast<size_t>(dst->surfacePitch);
        const auto* srcRow = srcPixels + static_cast<size_t>(srcY) * static_cast<size_t>(srcBitmap->pitch);

        for (int x = 0; x < wDest; ++x) {
            const int dstX = xDest + x;
            if (dstX < 0 || dstX >= dst->surfaceWidth) {
                continue;
            }

            const int srcX = xSrc + static_cast<int>((static_cast<int64_t>(x) * static_cast<int64_t>(wSrc)) / static_cast<int64_t>(wDest));
            if (srcX < 0 || srcX >= srcBitmap->width) {
                continue;
            }

            const auto* srcPixel = srcRow + static_cast<size_t>(srcX) * 4u;
            auto* dstPixel = dstRow + static_cast<size_t>(dstX) * 4u;
            dstPixel[0] = srcPixel[0];
            dstPixel[1] = srcPixel[1];
            dstPixel[2] = srcPixel[2];
            dstPixel[3] = 255;
        }
    }

    int dstSampleX = xDest;
    int dstSampleY = yDest;
    if (dstSampleX < 0) dstSampleX = 0;
    if (dstSampleY < 0) dstSampleY = 0;
    if (dstSampleX >= dst->surfaceWidth) dstSampleX = dst->surfaceWidth - 1;
    if (dstSampleY >= dst->surfaceHeight) dstSampleY = dst->surfaceHeight - 1;

    uint8_t dstR = 0;
    uint8_t dstG = 0;
    uint8_t dstB = 0;
    if (dst->surfaceWidth > 0 && dst->surfaceHeight > 0) {
        const auto* dstSample = dst->surfacePixels + static_cast<size_t>(dstSampleY) * static_cast<size_t>(dst->surfacePitch) + static_cast<size_t>(dstSampleX) * 4u;
        dstR = dstSample[0];
        dstG = dstSample[1];
        dstB = dstSample[2];
    }

    int srcSampleX = xSrc;
    int srcSampleY = ySrc;
    if (srcSampleX < 0) srcSampleX = 0;
    if (srcSampleY < 0) srcSampleY = 0;
    if (srcSampleX >= srcBitmap->width) srcSampleX = srcBitmap->width - 1;
    if (srcSampleY >= srcBitmap->height) srcSampleY = srcBitmap->height - 1;

    uint8_t srcR = 0;
    uint8_t srcG = 0;
    uint8_t srcB = 0;
    if (srcBitmap->width > 0 && srcBitmap->height > 0) {
        const auto* srcSample = srcPixels + static_cast<size_t>(srcSampleY) * static_cast<size_t>(srcBitmap->pitch) + static_cast<size_t>(srcSampleX) * 4u;
        srcR = srcSample[0];
        srcG = srcSample[1];
        srcB = srcSample[2];
    }

    SDL_Log("free-api StretchBlt: copied src=%dx%d[%d,%d] rgb=(%u,%u,%u) to dst=%dx%d[%d,%d] rgb=(%u,%u,%u)",
            wSrc,
            hSrc,
            xSrc,
            ySrc,
            static_cast<unsigned>(srcR),
            static_cast<unsigned>(srcG),
            static_cast<unsigned>(srcB),
            wDest,
            hDest,
            xDest,
            yDest,
            static_cast<unsigned>(dstR),
            static_cast<unsigned>(dstG),
            static_cast<unsigned>(dstB));
    return TRUE;
}

COLORREF WINAPI GetPixel(HDC hdc, int x, int y)
{
    CompatDC* dc = AsCompatDC(hdc);
    if (!dc) {
        return 0;
    }

    const uint8_t* pixel = nullptr;
    if (dc->kind == CompatDcKind::Surface) {
        if (x < 0 || y < 0 || x >= dc->surfaceWidth || y >= dc->surfaceHeight || !dc->surfacePixels) {
            return 0;
        }
        pixel = dc->surfacePixels + static_cast<size_t>(y) * static_cast<size_t>(dc->surfacePitch) + static_cast<size_t>(x) * 4u;
    } else if (dc->selectedBitmap) {
        if (x < 0 || y < 0 || x >= dc->selectedBitmap->width || y >= dc->selectedBitmap->height) {
            return 0;
        }
        pixel = dc->selectedBitmap->pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(dc->selectedBitmap->pitch) + static_cast<size_t>(x) * 4u;
    }

    if (!pixel) {
        return 0;
    }

    return RGB(pixel[0], pixel[1], pixel[2]);
}

COLORREF WINAPI SetPixel(HDC hdc, int x, int y, COLORREF color)
{
    CompatDC* dc = AsCompatDC(hdc);
    if (!dc) {
        return color;
    }

    uint8_t* pixel = nullptr;
    if (dc->kind == CompatDcKind::Surface) {
        if (x < 0 || y < 0 || x >= dc->surfaceWidth || y >= dc->surfaceHeight || !dc->surfacePixels) {
            return color;
        }
        pixel = dc->surfacePixels + static_cast<size_t>(y) * static_cast<size_t>(dc->surfacePitch) + static_cast<size_t>(x) * 4u;
    } else if (dc->selectedBitmap) {
        if (x < 0 || y < 0 || x >= dc->selectedBitmap->width || y >= dc->selectedBitmap->height) {
            return color;
        }
        pixel = dc->selectedBitmap->pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(dc->selectedBitmap->pitch) + static_cast<size_t>(x) * 4u;
    }

    if (!pixel) {
        return color;
    }

    // COLORREF layout is 0x00BBGGRR; surface pixel layout is RGBA.
    pixel[0] = static_cast<uint8_t>(color & 0xFFu);          // R
    pixel[1] = static_cast<uint8_t>((color >> 8) & 0xFFu);   // G
    pixel[2] = static_cast<uint8_t>((color >> 16) & 0xFFu);  // B
    pixel[3] = 255;
    return color;
}

int WINAPI GetDeviceCaps(HDC hdc, int index)
{
    (void)hdc;
    if (index == SIZEPALETTE) {
        return 256;
    }
    return 0;
}

UINT WINAPI GetSystemPaletteEntries(HDC hdc, UINT iStartIndex, UINT nEntries, LPVOID lppe)
{
    (void)hdc;
    if (!lppe) {
        return 0;
    }

    PALETTEENTRY* entries = reinterpret_cast<PALETTEENTRY*>(lppe);
    for (UINT i = 0; i < nEntries; ++i) {
        UINT value = (iStartIndex + i) & 0xFFu;
        entries[i].peRed = static_cast<BYTE>(value);
        entries[i].peGreen = static_cast<BYTE>(value);
        entries[i].peBlue = static_cast<BYTE>(value);
        entries[i].peFlags = 0;
    }
    return nEntries;
}

BOOL WINAPI GetClientRect(HWND hWnd, LPRECT lpRect)
{
    if (!hWnd || !lpRect) {
        return FALSE;
    }

    int w = 0;
    int h = 0;
    if (!SDL_GetWindowSize(reinterpret_cast<SDL_Window*>(hWnd), &w, &h)) {
        return FALSE;
    }

    lpRect->left = 0;
    lpRect->top = 0;
    lpRect->right = w;
    lpRect->bottom = h;
    return TRUE;
}

HRSRC WINAPI FindResourceA(HMODULE hModule, LPCSTR lpName, LPCSTR lpType)
{
    (void)hModule;
    (void)lpName;
    (void)lpType;
    return NULL;
}

HGLOBAL WINAPI LoadResource(HMODULE hModule, HRSRC hResInfo)
{
    (void)hModule;
    (void)hResInfo;
    return NULL;
}

DWORD WINAPI SizeofResource(HMODULE hModule, HRSRC hResInfo)
{
    (void)hModule;
    (void)hResInfo;
    return 0;
}

LPVOID WINAPI LockResource(HGLOBAL hResData)
{
    return hResData;
}

int WINAPI _lopen(LPCSTR lpPathName, int iReadWrite)
{
    int flags = O_RDONLY;
    (void)iReadWrite;
    return open(lpPathName, flags);
}

UINT WINAPI _lread(int hFile, LPVOID lpBuffer, UINT uBytes)
{
    if (!lpBuffer) {
        return 0;
    }

    ssize_t bytesRead = read(hFile, lpBuffer, uBytes);
    return bytesRead > 0 ? static_cast<UINT>(bytesRead) : 0;
}

int WINAPI _lclose(int hFile)
{
    return close(hFile);
}

intptr_t _findfirst(const char* filespec, struct _finddata_t* fileinfo)
{
    (void)filespec;
    if (fileinfo) {
        memset(fileinfo, 0, sizeof(*fileinfo));
    }
    return -1;
}

int _findnext(intptr_t handle, struct _finddata_t* fileinfo)
{
    (void)handle;
    (void)fileinfo;
    return -1;
}

int _findclose(intptr_t handle)
{
    (void)handle;
    return 0;
}

BOOL WINAPI DeleteFileA(LPCSTR lpFileName)
{
    if (!lpFileName) {
        return FALSE;
    }
    return remove(lpFileName) == 0 ? TRUE : FALSE;
}

BOOL WINAPI UnlockResource(HGLOBAL hResData)
{
    (void)hResData;
    return FALSE;
}

BOOL WINAPI FreeResource(HGLOBAL hResData)
{
    (void)hResData;
    return FALSE;
}

int WINAPIV wsprintfA(LPSTR lpOut, LPCSTR lpFmt, ...)
{
    if (!lpOut || !lpFmt) {
        return 0;
    }

    va_list args;
    va_start(args, lpFmt);
    int written = vsnprintf(lpOut, 1024, lpFmt, args);
    va_end(args);
    return written;
}

int WINAPI GetSystemMetrics(int nIndex)
{
    if (nIndex == SM_CXSCREEN) {
        return 1024;
    }

    if (nIndex == SM_CYSCREEN) {
        return 768;
    }

    if (nIndex == SM_CYCAPTION) {
        return 24;
    }

    return 0;
}

BOOL WINAPI AdjustWindowRect(LPRECT lpRect, DWORD dwStyle, BOOL bMenu)
{
    (void)dwStyle;
    (void)bMenu;
    return lpRect ? TRUE : FALSE;
}

HWND WINAPI SetFocus(HWND hWnd)
{
    HWND oldFocus = g_focusWindow;
    g_focusWindow = hWnd;
    if (hWnd) {
        auto* sdlWindow = reinterpret_cast<SDL_Window*>(hWnd);
        SDL_RaiseWindow(sdlWindow);
        SDL_Log("free-api SetFocus: old=%p new=%p window=%p", oldFocus, hWnd, static_cast<void*>(sdlWindow));
    } else {
        SDL_Log("free-api SetFocus: old=%p new=<null>", oldFocus);
    }
    return oldFocus;
}

UINT_PTR WINAPI SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, void* lpTimerFunc)
{
    (void)hWnd;
    (void)uElapse;
    (void)lpTimerFunc;

    if (nIDEvent != 0) {
        return nIDEvent;
    }

    return g_nextTimerId.fetch_add(1);
}

BOOL WINAPI KillTimer(HWND hWnd, UINT_PTR uIDEvent)
{
    (void)hWnd;
    (void)uIDEvent;
    return TRUE;
}

BOOL WINAPI PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
{
    (void)hWnd;
    (void)wMsgFilterMin;
    (void)wMsgFilterMax;

    if (!lpMsg) {
        return FALSE;
    }

    if (g_messageQueue.empty()) {
        PumpSdlEvents();
    }

    if (g_messageQueue.empty()) {
        return FALSE;
    }

    *lpMsg = g_messageQueue.front();
    if ((wRemoveMsg & PM_REMOVE) != 0) {
        g_messageQueue.pop();
    }

    return TRUE;
}

BOOL WINAPI GetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
{
    if (!lpMsg) {
        return FALSE;
    }

    while (true) {
        if (PeekMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, PM_REMOVE)) {
            if (lpMsg->message == WM_QUIT) {
                return FALSE;
            }
            return TRUE;
        }

        SDL_Delay(1);
    }
}

BOOL WINAPI TranslateMessage(const MSG* lpMsg)
{
    return lpMsg ? TRUE : FALSE;
}

LRESULT WINAPI DispatchMessageA(const MSG* lpMsg)
{
    if (!lpMsg) {
        return 0;
    }

    if (lpMsg->message == WM_QUIT) {
        return 0;
    }

    const auto it = g_windowProcedures.find(lpMsg->hwnd);
    if (it != g_windowProcedures.end() && it->second) {
        return it->second(lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam);
    }

    return DefWindowProcA(lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam);
}

LRESULT WINAPI DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    (void)wParam;
    (void)lParam;

    if (Msg == WM_CLOSE) {
        DestroyWindow(hWnd);
        PostQuitMessage(0);
        return 0;
    }

    if (Msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }

    return 0;
}

void WINAPI PostQuitMessage(int nExitCode)
{
    PushMessage(NULL, WM_QUIT, static_cast<WPARAM>(nExitCode), 0);
}

BOOL WINAPI WaitMessage(void)
{
    if (!g_messageQueue.empty()) {
        return TRUE;
    }

    PumpSdlEvents();
    if (!g_messageQueue.empty()) {
        return TRUE;
    }

    SDL_Delay(1);
    PumpSdlEvents();
    return TRUE;
}

MMRESULT WINAPI timeSetEvent(UINT uDelay,
                             UINT uResolution,
                             LPTIMECALLBACK lpTimeProc,
                             DWORD_PTR dwUser,
                             UINT fuEvent)
{
    (void)uDelay;
    (void)uResolution;
    (void)lpTimeProc;
    (void)dwUser;
    (void)fuEvent;

    if (uDelay == 0) {
        SDL_Log("free-api timeSetEvent: zero delay is unsupported in compatibility mode");
        return 0;
    }

    const UINT timerId = g_nextTimerId.fetch_add(1);
    g_activeTimerIds.insert(timerId);
    return timerId;
}

MMRESULT WINAPI timeKillEvent(UINT uTimerID)
{
    if (uTimerID == 0 || g_activeTimerIds.erase(uTimerID) == 0) {
        SDL_Log("free-api timeKillEvent: unknown timer id %u", uTimerID);
        return 1;
    }

    return MMSYSERR_NOERROR;
}

MMRESULT WINAPI joyGetPosEx(UINT uJoyID, LPJOYINFOEX pji)
{
    (void)uJoyID;
    if (!pji) {
        return 1;
    }

    if (pji->dwSize >= sizeof(JOYINFOEX)) {
        memset(pji, 0, sizeof(JOYINFOEX));
        pji->dwSize = sizeof(JOYINFOEX);
    }

    return 1;
}

UINT WINAPI joyGetNumDevs(void)
{
    return 0;
}

UINT WINAPI midiOutGetNumDevs(void)
{
    return 0;
}

MMRESULT WINAPI midiOutOpen(LPHMIDIOUT phmo, UINT uDeviceID, DWORD_PTR dwCallback, DWORD_PTR dwInstance, DWORD fdwOpen)
{
    (void)uDeviceID;
    (void)dwCallback;
    (void)dwInstance;
    (void)fdwOpen;
    if (phmo) {
        *phmo = reinterpret_cast<HMIDIOUT>(static_cast<uintptr_t>(1));
    }
    return MMSYSERR_NOERROR;
}

MMRESULT WINAPI midiOutSetVolume(HMIDIOUT hmo, DWORD dwVolume)
{
    (void)hmo;
    (void)dwVolume;
    return MMSYSERR_NOERROR;
}

MMRESULT WINAPI midiOutClose(HMIDIOUT hmo)
{
    (void)hmo;
    return MMSYSERR_NOERROR;
}

MCIERROR WINAPI mciSendCommandA(MCIDEVICEID mciId, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam)
{
    (void)mciId;
    (void)uMsg;
    (void)fdwCommand;
    (void)dwParam;
    return 0;
}

MCIDEVICEID WINAPI mciGetDeviceIDA(LPCSTR lpszDevice)
{
    (void)lpszDevice;
    return 1;
}

BOOL WINAPI mciGetErrorStringA(MCIERROR mcierr, LPSTR pszText, UINT cchText)
{
    (void)mcierr;
    if (!pszText || cchText == 0) {
        return FALSE;
    }

    snprintf(pszText, cchText, "MCI error");
    return TRUE;
}

int WINAPI FreeApiRunWinMain(FREE_API_WINMAIN_PROC entryPoint, int argc, char** argv)
{
    if (!entryPoint) {
        return -1;
    }

    if (argc > 0 && argv && argv[0]) {
        _pgmptr = argv[0];
    }

    std::string commandLine = BuildCommandLine(argc, argv);
    return entryPoint(NULL, NULL, commandLine.empty() ? NULL : commandLine.data(), SW_SHOW);
}

}
