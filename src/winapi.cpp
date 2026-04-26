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
#include <cstdarg>
#include <fcntl.h>
#include <unistd.h>

namespace {
    std::unordered_map<std::string, WNDPROC> g_registeredClasses;
    std::unordered_map<HWND, WNDPROC> g_windowProcedures;
    std::queue<MSG> g_messageQueue;
    bool g_videoInitialized = false;
    std::atomic<UINT> g_nextTimerId{1};
    HWND g_focusWindow = NULL;

    bool EnsureVideoSubsystem()
    {
        if (g_videoInitialized) {
            return true;
        }

        if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
            return false;
        }

        g_videoInitialized = true;
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

    if (!lpClassName) {
        return NULL;
    }

    const auto classIt = g_registeredClasses.find(lpClassName);
    if (classIt == g_registeredClasses.end()) {
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

    auto* sdlWindow = SDL_CreateWindow(lpWindowName ? lpWindowName : lpClassName, width, height, flags);
    if (!sdlWindow) {
        return NULL;
    }

    const int posX = (X < 0) ? SDL_WINDOWPOS_CENTERED : X;
    const int posY = (Y < 0) ? SDL_WINDOWPOS_CENTERED : Y;
    SDL_SetWindowPosition(sdlWindow, posX, posY);

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

    SDL_RaiseWindow(reinterpret_cast<SDL_Window*>(hWnd));
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

HANDLE WINAPI LoadImageA(HINSTANCE hInst, LPCSTR name, UINT type, int cx, int cy, UINT fuLoad)
{
    (void)hInst;
    (void)name;
    (void)type;
    (void)cx;
    (void)cy;
    (void)fuLoad;

    BITMAP* bitmap = new BITMAP{};
    bitmap->bmWidth = cx > 0 ? cx : 320;
    bitmap->bmHeight = cy > 0 ? cy : 200;
    bitmap->bmBitsPixel = 8;
    return reinterpret_cast<HANDLE>(bitmap);
}

int WINAPI GetObjectA(HANDLE h, int c, LPVOID pv)
{
    if (!h || !pv || c <= 0) {
        return 0;
    }

    BITMAP* src = reinterpret_cast<BITMAP*>(h);
    int copySize = c < static_cast<int>(sizeof(BITMAP)) ? c : static_cast<int>(sizeof(BITMAP));
    memcpy(pv, src, static_cast<size_t>(copySize));
    return copySize;
}

BOOL WINAPI DeleteObject(HGDIOBJ ho)
{
    if (!ho) {
        return FALSE;
    }

    delete reinterpret_cast<BITMAP*>(ho);
    return TRUE;
}

HDC WINAPI CreateCompatibleDC(HDC hdc)
{
    (void)hdc;
    return reinterpret_cast<HDC>(static_cast<uintptr_t>(g_nextTimerId.fetch_add(1) + 1024));
}

HGDIOBJ WINAPI SelectObject(HDC hdc, HGDIOBJ h)
{
    (void)hdc;
    return h;
}

BOOL WINAPI DeleteDC(HDC hdc)
{
    return hdc ? TRUE : FALSE;
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
    (void)hdcDest;
    (void)xDest;
    (void)yDest;
    (void)wDest;
    (void)hDest;
    (void)hdcSrc;
    (void)xSrc;
    (void)ySrc;
    (void)wSrc;
    (void)hSrc;
    (void)rop;
    return TRUE;
}

COLORREF WINAPI GetPixel(HDC hdc, int x, int y)
{
    (void)hdc;
    (void)x;
    (void)y;
    return 0;
}

COLORREF WINAPI SetPixel(HDC hdc, int x, int y, COLORREF color)
{
    (void)hdc;
    (void)x;
    (void)y;
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
        SDL_RaiseWindow(reinterpret_cast<SDL_Window*>(hWnd));
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
    PumpSdlEvents();
    SDL_Delay(1);
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
    return g_nextTimerId.fetch_add(1);
}

MMRESULT WINAPI timeKillEvent(UINT uTimerID)
{
    (void)uTimerID;
    return 0;
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
