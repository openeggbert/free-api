#include "windows.h"
#include <SDL3/SDL.h>

#include <chrono>
#include <cstdio>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>

namespace {
    std::unordered_map<std::string, WNDPROC> g_registeredClasses;
    std::unordered_map<HWND, WNDPROC> g_windowProcedures;
    std::queue<MSG> g_messageQueue;
    bool g_videoInitialized = false;

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

void WINAPI Sleep(DWORD dwMilliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(dwMilliseconds));
}

DWORD WINAPI GetTickCount(void) {
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return static_cast<DWORD>(ms.count());
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
    (void)dwStyle;
    (void)X;
    (void)Y;
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
    const Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

    auto* sdlWindow = SDL_CreateWindow(lpWindowName ? lpWindowName : lpClassName, width, height, flags);
    if (!sdlWindow) {
        return NULL;
    }

    HWND hwnd = reinterpret_cast<HWND>(sdlWindow);
    g_windowProcedures[hwnd] = classIt->second;
    return hwnd;
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
    (void)nCmdShow;
    return hWnd ? TRUE : FALSE;
}

BOOL WINAPI UpdateWindow(HWND hWnd)
{
    return hWnd ? TRUE : FALSE;
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

int WINAPI FreeApiRunWinMain(FREE_API_WINMAIN_PROC entryPoint, int argc, char** argv)
{
    if (!entryPoint) {
        return -1;
    }

    std::string commandLine = BuildCommandLine(argc, argv);
    return entryPoint(NULL, NULL, commandLine.empty() ? NULL : commandLine.data(), SW_SHOW);
}

}
