/**
 * @file windows.h
 * @brief Central public WinAPI compatibility header for Free API.
 *
 * Free API is an SDL3-backed compatibility layer that exposes a Win32/WinAPI-like
 * public surface for old C/C++ games. This is not Wine and not a full WinAPI
 * implementation — only the subset needed by the target game(s) is implemented.
 *
 * Architecture:
 * @code
 * Legacy game source  ->  Free API headers  ->  SDL3 + POSIX
 * @endcode
 *
 * This header includes minwindef.h, windef.h, winnt.h, and declares:
 * - WinBase helpers (Sleep, GetTickCount, ...)
 * - Window class, window creation and lifetime functions
 * - Message constants and message queue/dispatch (PeekMessageA, GetMessageA, ...)
 * - Input: mouse, keyboard, cursor
 * - GDI-like bitmap and DC subset (LoadImageA, StretchBlt, ...)
 * - File/path helpers (CreateDirectoryA, DeleteFileA, _lopen/read/close)
 * - User timers (SetTimer / KillTimer)
 * - WinMain bridge (FREE_API_IMPLEMENT_WINMAIN, FreeApiRunWinMain)
 * - fopen path-normalization wrapper (C++ only)
 *
 * Unsupported areas: real Win32 resources, common dialogs, real GDI drawing,
 * palettes, DIB sections, brushes, icons, cursors, fonts, real Unicode APIs,
 * security descriptors, parent/child window semantics.
 *
 * @note The implementation lives in src/winapi.cpp.
 * @note This header must not expose SDL types.
 */
#ifndef FREE_API_WINDOWS_H
#define FREE_API_WINDOWS_H

#include <winuser.h>
#include <minwindef.h>
#include <windef.h>
#include <winnt.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <synchapi.h>
#include <sysinfoapi.h>
#include <handleapi.h>

#include <debugapi.h>
/** @} */

#include <winbase.h>

/**
 * @name WinUser subset
 * @brief Window, message-loop and input declarations consumed by legacy game code.
 * @note Status: PARTIAL
 */
/** @{ */

/** @brief Program path pointer expected by old CRT startup code. stdlib.h (Visual Studio) does contain_pgmptr, but stdlib.h (GCC) does not contain it.
  * @note Status: PARTIAL
  *
  */
extern char* _pgmptr;

#include <rpcndr.h>

#include <wingdi.h>

#include <winerror.h>

/** @} */

/** @} */

/**
 * @brief WinMain-compatible function pointer type.
 * @note Status: HEADER_ONLY
 */
typedef int(WINAPI* FREE_API_WINMAIN_PROC)(HINSTANCE, HINSTANCE, LPSTR, int);

/**
 * @brief Adapts a standard main() environment to call a WinMain-style function.
 *
 * Sets _pgmptr to argv[0] when available. Builds lpCmdLine from argv[1..] separated
 * by spaces. Calls the entry point with hInstance=NULL, hPrevInstance=NULL, nCmdShow=SW_SHOW.
 * Returns -1 if the entry point pointer is null.
 * @note Status: PARTIAL
 */
int WINAPI FreeApiRunWinMain(FREE_API_WINMAIN_PROC entryPoint, int argc, char** argv);

#ifdef UNICODE
#define SetWindowText SetWindowTextW
#define PostMessage PostMessageW
#define MessageBox MessageBoxW
#define LoadString LoadStringW
#define GetModuleHandle GetModuleHandleW
#define LoadImage LoadImageW
#define GetObject GetObjectW
#define RegisterClass RegisterClassW
#define CreateWindowEx CreateWindowExW
#define CreateWindow CreateWindowW
#define PeekMessage PeekMessageW
#define GetMessage GetMessageW
#define DispatchMessage DispatchMessageW
#define DefWindowProc DefWindowProcW
#define LoadCursor LoadCursorW
#define LoadIcon LoadIconW
#else
#define SetWindowText SetWindowTextA
#define PostMessage PostMessageA
#define MessageBox MessageBoxA
#define LoadString LoadStringA
#define GetModuleHandle GetModuleHandleA
#define LoadImage LoadImageA
#define GetObject GetObjectA
#define RegisterClass RegisterClassA
#define CreateWindowEx CreateWindowExA
#define CreateWindow CreateWindowA
#define PeekMessage PeekMessageA
#define GetMessage GetMessageA
#define DispatchMessage DispatchMessageA
#define DefWindowProc DefWindowProcA
#define LoadCursor LoadCursorA
#define LoadIcon LoadIconA
#define FindResource FindResourceA
#define DeleteFile DeleteFileA
#define wsprintf wsprintfA
#define CreateDirectory CreateDirectoryA
#endif

#ifdef __cplusplus
}

#if !defined(_WIN32)
/**
 * Defines a portable entry point bridge from WinMain style to main.
 *
 * Usage:
 * FREE_API_IMPLEMENT_WINMAIN() {
 *     // WinMain body
 * }
 */
#define FREE_API_IMPLEMENT_WINMAIN() \
    int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow); \
    int main(int argc, char** argv) { return FreeApiRunWinMain(&WinMain, argc, argv); } \
    int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#endif

// ---------------------------------------------------------------------------
// fopen path-normalization wrapper
// ---------------------------------------------------------------------------
// Planet Blupi uses Windows backslash paths like "data\\config.def" which do
// not work on Linux POSIX file systems.  This inline wrapper converts all
// backslashes to forward slashes before calling the real fopen and also tries
// an uppercase-basename fallback for case-sensitive file systems.
// Defined here so every C++ translation unit that includes <windows.h> picks
// it up automatically without changing Planet Blupi source files.
// @note Status: IMPLEMENTED
#ifndef FREE_API_FOPEN_WRAPPER_DEFINED
#define FREE_API_FOPEN_WRAPPER_DEFINED
#include <cstdio>
#include <cctype>
#include <string>
static inline FILE* free_api_fopen(const char* path, const char* mode)
{
    if (!path) return nullptr;
    std::string p(path);
    // Remove Windows drive letter if present (e.g., "C:\path" -> "\path")
    if (p.size() >= 2 && isalpha(static_cast<unsigned char>(p[0])) && p[1] == ':') {
        p.erase(0, 2);
    }
    for (char& c : p) {
        if (c == '\\') c = '/';
    }
    // Remove leading slashes if we want it relative to current dir,
    // but Planet Blupi often uses relative paths like "data\config.def".
    // If it was "c:\Planète Blupi\data\info.blp", after removing "c:" it's "\Planète Blupi\data\info.blp".
    // We should probably make it relative if it starts with a slash after drive removal,
    // because we don't want to look in the root of the Linux filesystem.
    while (!p.empty() && (p[0] == '/' || p[0] == '\\')) {
        p.erase(0, 1);
    }

    if (p.empty()) return nullptr;

    FILE* f = ::fopen(p.c_str(), mode);
    if (!f) {
        // Case-insensitive fallback: uppercase the basename component
        std::string upper = p;
        auto slash = upper.rfind('/');
        size_t base = (slash == std::string::npos) ? 0u : slash + 1u;
        for (size_t i = base; i < upper.size(); ++i)
            upper[i] = static_cast<char>(toupper(static_cast<unsigned char>(upper[i])));
        if (upper != p) f = ::fopen(upper.c_str(), mode);
    }
    return f;
}
#undef fopen
#define fopen free_api_fopen
#endif // FREE_API_FOPEN_WRAPPER_DEFINED

#endif // __cplusplus (outer)

#include <mmsystem.h>

#endif // FREE_API_WINDOWS_H
