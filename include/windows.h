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

/** @brief Window creation payload for `WM_CREATE` processing. @note Status: PARTIAL */
typedef struct tagCREATESTRUCTA {
    LPVOID lpCreateParams;
    HINSTANCE hInstance;
    HMENU hMenu;
    HWND hwndParent;
    int cy;
    int cx;
    int y;
    int x;
    LONG style;
    LPCSTR lpszName;
    LPCSTR lpszClass;
    DWORD dwExStyle;
} CREATESTRUCTA, *LPCREATESTRUCTA;

typedef CREATESTRUCTA CREATESTRUCT;
typedef LPCREATESTRUCTA LPCREATESTRUCT;

typedef LRESULT(CALLBACK* WNDPROC)(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

/** @brief Class registration descriptor for `RegisterClassA`. @note Status: PARTIAL */
typedef struct tagWNDCLASSA {
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    LPCSTR lpszMenuName;
    LPCSTR lpszClassName;
} WNDCLASSA, *PWNDCLASSA, *LPWNDCLASSA;

/** @brief ANSI WNDCLASS alias (non-unicode default). @note Status: IMPLEMENTED */
typedef WNDCLASSA WNDCLASS;
typedef PWNDCLASSA PWNDCLASS;
typedef LPWNDCLASSA LPWNDCLASS;

/**
 * @name Win32 message constants
 * @brief Minimal message subset used by the current game runtime.
 * @note Status: PARTIAL
 */
/** @{ */
#define WM_NULL 0x0000
#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_CLOSE 0x0010
#define WM_QUIT 0x0012
#define WM_SYSCOLORCHANGE 0x0015
#define WM_ACTIVATEAPP 0x001C
#define WM_SETCURSOR 0x0020
#define WM_NCMOUSEMOVE 0x00A0
#define WM_DISPLAYCHANGE 0x007E
#define WM_KEYDOWN 0x0100
#define WM_KEYUP 0x0101
#define WM_SYSKEYDOWN 0x0104
#define WM_SYSKEYUP 0x0105
#define WM_TIMER 0x0113
#define WM_QUERYNEWPALETTE 0x030F
#define WM_PALETTECHANGED 0x0311
#define WM_MOUSEMOVE 0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_RBUTTONDOWN 0x0204
#define WM_RBUTTONUP 0x0205
#define WM_MBUTTONDOWN 0x0207
#define WM_MBUTTONUP 0x0208
#define WM_MOUSEWHEEL 0x020A
#define WM_CHAR 0x0102
#define WM_USER 0x0400
/** @} */

/**
 * @name Mouse button/modifier key flags for WM_MOUSEMOVE and related messages.
 * @brief Minimal MK_* subset used by the current game runtime.
 * @note Status: IMPLEMENTED
 */
/** @{ */
#define MK_LBUTTON  0x0001
#define MK_RBUTTON  0x0002
#define MK_SHIFT    0x0004
#define MK_CONTROL  0x0008
#define MK_MBUTTON  0x0010
/** @} */

/** @brief `PeekMessage` mode constants. @note Status: IMPLEMENTED */
#define PM_NOREMOVE 0x0000
#define PM_REMOVE 0x0001

#define SW_HIDE 0
#define SW_SHOW 5

/** @brief Message box style subset. @note Status: STUB */
#define MB_OK 0x00000000L

#ifndef E_FAIL
#define E_FAIL ((HRESULT)0x80004005L)
#endif

/**
 * @name Virtual-key subset
 * @brief Keyboard VK constants consumed by the game.
 * @note Status: PARTIAL
 */
/** @{ */
#define VK_F5 0x74
#define VK_F6 0x75
#define VK_F7 0x76
#define VK_F8 0x77
#define VK_F10 0x79
#define VK_F1 0x70
#define VK_F2 0x71
#define VK_F3 0x72
#define VK_F4 0x73
#define VK_PAUSE 0x13
#define VK_END 0x23
#define VK_ESCAPE 0x1B
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_HOME 0x24
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT  0x22
#define VK_TAB   0x09
#define VK_BACK  0x08
#define VK_DELETE 0x2E
#define VK_INSERT 0x2D
#define VK_MENU  0x12
#define VK_F9  0x78
#define VK_F11 0x7A
#define VK_F12 0x7B
/** @brief Letters A-Z map directly to ASCII 0x41-0x5A. */
/** @brief Digits 0-9 map directly to ASCII 0x30-0x39. */
/** @} */

/**
 * @name Window class/style constants
 * @brief Legacy style values interpreted by the compatibility layer.
 * @note Status: PARTIAL
 */
/** @{ */
#define CS_VREDRAW 0x0001
#define CS_HREDRAW 0x0002

#define WS_VISIBLE 0x10000000L
#define WS_POPUP 0x80000000L
#define WS_CHILD 0x40000000L
#define WS_CAPTION 0x00C00000L
#define WS_POPUPWINDOW 0x80880000L
#define WS_OVERLAPPEDWINDOW 0x00CF0000L

#define WS_EX_TOPMOST 0x00000008L

#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define SM_CYCAPTION 4

#define BLACK_BRUSH 4

#define IMAGE_BITMAP 0

#define LR_LOADFROMFILE 0x00000010
#define LR_CREATEDIBSECTION 0x00002000
/** @} */

#define SRCCOPY 0x00CC0020

#define SIZEPALETTE 104

#define OF_READ 0x0000

#define CLR_INVALID 0xFFFFFFFF

#define RT_BITMAP ((LPCSTR)2)

#ifndef MAKEINTRESOURCEA
#define MAKEINTRESOURCEA(i) ((LPSTR)((ULONG_PTR)((WORD)(i))))
#endif
#ifndef MAKEINTRESOURCE
#define MAKEINTRESOURCE MAKEINTRESOURCEA
#endif

#ifndef ZeroMemory
#define ZeroMemory(Destination, Length) memset((Destination), 0, (Length))
#endif

#ifndef FillMemory
#define FillMemory(Destination, Length, Fill) memset((Destination), (Fill), (Length))
#endif

#ifndef CopyMemory
#define CopyMemory(Destination, Source, Length) memmove((Destination), (Source), (Length))
#endif

/** @brief Legacy desktop pseudo-window handle. @note Status: IMPLEMENTED */
#define HWND_DESKTOP ((HWND)0)

/** @brief Packs RGB bytes into COLORREF. @note Status: IMPLEMENTED */
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))

#ifndef MAKELONG
#define MAKELONG(a, b) ((LONG)(((WORD)((DWORD_PTR)(a) & 0xFFFF)) | ((DWORD)((WORD)((DWORD_PTR)(b) & 0xFFFF))) << 16))
#endif

#ifndef LOWORD
#define LOWORD(l) ((WORD)((DWORD_PTR)(l) & 0xFFFF))
#endif

#ifndef HIWORD
#define HIWORD(l) ((WORD)((DWORD_PTR)(l) >> 16))
#endif

/** @brief Registers a window class (ANSI). @note Status: PARTIAL */
ATOM WINAPI RegisterClassA(const WNDCLASSA* lpWndClass);
/** @brief Creates a window with extended style (ANSI). @note Status: PARTIAL */
HWND WINAPI CreateWindowExA(DWORD dwExStyle,
                            LPCSTR lpClassName,
                            LPCSTR lpWindowName,
                            DWORD dwStyle,
                            int X,
                            int Y,
                            int nWidth,
                            int nHeight,
                            HWND hWndParent,
                            HMENU hMenu,
                            HINSTANCE hInstance,
                            LPVOID lpParam);
/** @brief Creates a window (ANSI). @note Status: PARTIAL */
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
                          LPVOID lpParam);
/** @brief Destroys a window and releases internal mappings. @note Status: PARTIAL */
BOOL WINAPI DestroyWindow(HWND hWnd);
/** @brief Changes window visibility/state. @note Status: PARTIAL */
BOOL WINAPI ShowWindow(HWND hWnd, int nCmdShow);
/** @brief Requests immediate window refresh/raise. @note Status: PARTIAL */
BOOL WINAPI UpdateWindow(HWND hWnd);
/** @brief Moves/resizes a window via SDL. Ignores repaint semantics. @note Status: PARTIAL */
BOOL WINAPI MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint);
/** @brief Invalidates window client area. @note Status: STUB */
BOOL WINAPI InvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase);
/** @brief Polls message queue. @note Status: IMPLEMENTED */
BOOL WINAPI PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);
/** @brief Blocks until a message is available or quit is posted. @note Status: PARTIAL */
BOOL WINAPI GetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);
/** @brief Performs keyboard translation phase. @note Status: PARTIAL */
BOOL WINAPI TranslateMessage(const MSG* lpMsg);
/** @brief Dispatches message to a window procedure. @note Status: PARTIAL */
LRESULT WINAPI DispatchMessageA(const MSG* lpMsg);
/** @brief Default window procedure fallback. @note Status: PARTIAL */
LRESULT WINAPI DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
/** @brief Posts quit message to the thread queue. @note Status: IMPLEMENTED */
void WINAPI PostQuitMessage(int nExitCode);
/** @brief Waits until the message queue receives work. @note Status: PARTIAL */
BOOL WINAPI WaitMessage(void);

/** @brief Sets SDL window title. @note Status: PARTIAL */
BOOL WINAPI SetWindowTextA(HWND hWnd, LPCSTR lpString);
/** @brief Enqueues a message into the global mutex-protected queue. No thread/window validation. @note Status: PARTIAL */
BOOL WINAPI PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
/** @brief Displays a simple message box replacement. @note Status: STUB */
int WINAPI MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType);
/** @brief Retrieves cursor position via SDL_GetGlobalMouseState. @note Status: PARTIAL */
BOOL WINAPI GetCursorPos(LPPOINT lpPoint);
/** @brief Converts screen coordinates to client by subtracting SDL window position. @note Status: PARTIAL */
BOOL WINAPI ScreenToClient(HWND hWnd, LPPOINT lpPoint);
/** @brief Sets active cursor shape. @note Status: STUB */
HCURSOR WINAPI SetCursor(HCURSOR hCursor);
/** @brief Shows/hides cursor and returns display counter. @note Status: STUB */
int WINAPI ShowCursor(BOOL bShow);
/** @brief Converts client to screen coordinates by adding SDL window position. @note Status: PARTIAL */
BOOL WINAPI ClientToScreen(HWND hWnd, LPPOINT lpPoint);
/** @brief Warps the cursor to screen position via SDL_WarpMouseGlobal. @note Status: PARTIAL */
BOOL WINAPI SetCursorPos(int X, int Y);
/** @brief Loads string resource text. @note Status: STUB */
int WINAPI LoadStringA(HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, int cchBufferMax);
/** @brief Returns current module handle. @note Status: STUB */
HMODULE WINAPI GetModuleHandleA(LPCSTR lpModuleName);
/**
 * @brief Loads an image from disk (IMAGE_BITMAP + LR_LOADFROMFILE only).
 *
 * Loads a BMP via SDL_LoadBMP, converts to RGBA32, optionally scales.
 * Resource loading (without LR_LOADFROMFILE) is not implemented.
 * @note Status: PARTIAL
 */
HANDLE WINAPI LoadImageA(HINSTANCE hInst, LPCSTR name, UINT type, int cx, int cy, UINT fuLoad);
/** @brief Fills a BITMAP structure for an internal compatible bitmap. @note Status: PARTIAL */
int WINAPI GetObjectA(HANDLE h, int c, LPVOID pv);
/** @brief Deletes an internal compatible bitmap. @note Status: PARTIAL */
BOOL WINAPI DeleteObject(HGDIOBJ ho);
/** @brief Creates a GDI bitmap from raw pixel data. @note Status: PARTIAL */
HBITMAP WINAPI CreateBitmap(int nWidth, int nHeight, UINT nPlanes, UINT nBitCount, const void* lpBits);
/** @brief Allocates an internal memory DC. @note Status: PARTIAL */
HDC WINAPI CreateCompatibleDC(HDC hdc);
/** @brief Selects an internal bitmap into a memory DC; returns previously selected object. @note Status: PARTIAL */
HGDIOBJ WINAPI SelectObject(HDC hdc, HGDIOBJ h);
/** @brief Deletes an internal compatible DC. @note Status: PARTIAL */
BOOL WINAPI DeleteDC(HDC hdc);
/**
 * @brief Copies/scales a region from a source DC to a destination DC.
 *
 * Only SRCCOPY from a memory DC with selected bitmap to an internal surface DC
 * is implemented. Uses nearest-neighbor scaling.
 * @note Status: PARTIAL
 */
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
                       DWORD rop);
/** @brief Reads an RGB color from a surface DC or selected bitmap. @note Status: PARTIAL */
COLORREF WINAPI GetPixel(HDC hdc, int x, int y);
/** @brief Writes an RGB color into a surface DC or selected bitmap. @note Status: PARTIAL */
COLORREF WINAPI SetPixel(HDC hdc, int x, int y, COLORREF color);
/** @brief Returns 256 for SIZEPALETTE; otherwise 0. @note Status: PARTIAL */
int WINAPI GetDeviceCaps(HDC hdc, int index);
/** @brief Fills palette entries as grayscale values. @note Status: PARTIAL */
UINT WINAPI GetSystemPaletteEntries(HDC hdc, UINT iStartIndex, UINT nEntries, LPVOID lppe);
/** @brief Finds an embedded resource. @note Status: STUB */
HRSRC WINAPI FindResourceA(HMODULE hModule, LPCSTR lpName, LPCSTR lpType);
/** @brief Loads resource block handle. @note Status: STUB */
HGLOBAL WINAPI LoadResource(HMODULE hModule, HRSRC hResInfo);
/** @brief Returns resource size in bytes. @note Status: STUB */
DWORD WINAPI SizeofResource(HMODULE hModule, HRSRC hResInfo);
/** @brief Locks resource memory. @note Status: STUB */
LPVOID WINAPI LockResource(HGLOBAL hResData);
/** @brief Unlocks resource memory. @note Status: STUB */
BOOL WINAPI UnlockResource(HGLOBAL hResData);
/** @brief Releases resource handle. @note Status: STUB */
BOOL WINAPI FreeResource(HGLOBAL hResData);
/** @brief Calls POSIX open(O_RDONLY); ignores iReadWrite mode. @note Status: PARTIAL */
int WINAPI _lopen(LPCSTR lpPathName, int iReadWrite);
/** @brief Calls POSIX read(); returns byte count. @note Status: PARTIAL */
UINT WINAPI _lread(int hFile, LPVOID lpBuffer, UINT uBytes);
/** @brief Calls POSIX close(). @note Status: PARTIAL */
int WINAPI _lclose(int hFile);
/** @brief Calls remove() directly; no backslash normalization. @note Status: PARTIAL */
BOOL WINAPI DeleteFileA(LPCSTR lpFileName);
/** @brief Formatted print using vsnprintf with a 1024-byte fixed buffer. @note Status: PARTIAL */
int WINAPIV wsprintfA(LPSTR lpOut, LPCSTR lpFmt, ...);

/** @brief Security attributes for directory creation. @note Status: PARTIAL */
typedef struct _SECURITY_ATTRIBUTES {
    DWORD nLength;
    LPVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

/**
 * @brief Creates a directory; security descriptor is ignored.
 *
 * Normalizes Windows-style paths (removes drive letter, converts backslashes,
 * strips leading slashes). Calls mkdir(path, 0755). Treats EEXIST as success.
 * Does not recursively create missing parent directories.
 * @note Status: PARTIAL
 */
BOOL WINAPI CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
/** @brief Loads a cursor resource. @note Status: STUB */
HCURSOR WINAPI LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName);
/** @brief Loads an icon resource. @note Status: STUB */
HICON WINAPI LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName);
/** @brief Retrieves stock brush handle. @note Status: STUB */
HBRUSH WINAPI GetStockBrush(int fnObject);
/** @brief Returns selected system metric value. @note Status: PARTIAL */
int WINAPI GetSystemMetrics(int nIndex);
/** @brief Adjusts window rectangle for styles. @note Status: STUB */
BOOL WINAPI AdjustWindowRect(LPRECT lpRect, DWORD dwStyle, BOOL bMenu);
/** @brief Sets keyboard focus window. @note Status: PARTIAL */
HWND WINAPI SetFocus(HWND hWnd);
/**
 * @brief Stores a polling timer; PeekMessageA generates WM_TIMER messages.
 *
 * lpTimerFunc is ignored. If nIDEvent == 0, a unique ID is generated.
 * Timers only fire while the message loop calls PeekMessageA or GetMessageA.
 * @note Status: PARTIAL
 */
UINT_PTR WINAPI SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, void* lpTimerFunc);
/** @brief Removes a stored polling timer. @note Status: PARTIAL */
BOOL WINAPI KillTimer(HWND hWnd, UINT_PTR uIDEvent);

static inline BOOL SetRect(LPRECT lprc, int xLeft, int yTop, int xRight, int yBottom)
{
    if (!lprc) {
        return FALSE;
    }
    lprc->left = xLeft;
    lprc->top = yTop;
    lprc->right = xRight;
    lprc->bottom = yBottom;
    return TRUE;
}

static inline BOOL IntersectRect(LPRECT lprcDst, const RECT* lprcSrc1, const RECT* lprcSrc2)
{
    if (!lprcDst || !lprcSrc1 || !lprcSrc2) {
        return FALSE;
    }

    lprcDst->left = lprcSrc1->left > lprcSrc2->left ? lprcSrc1->left : lprcSrc2->left;
    lprcDst->top = lprcSrc1->top > lprcSrc2->top ? lprcSrc1->top : lprcSrc2->top;
    lprcDst->right = lprcSrc1->right < lprcSrc2->right ? lprcSrc1->right : lprcSrc2->right;
    lprcDst->bottom = lprcSrc1->bottom < lprcSrc2->bottom ? lprcSrc1->bottom : lprcSrc2->bottom;

    return (lprcDst->right > lprcDst->left && lprcDst->bottom > lprcDst->top) ? TRUE : FALSE;
}

static inline BOOL UnionRect(LPRECT lprcDst, const RECT* lprcSrc1, const RECT* lprcSrc2)
{
    if (!lprcDst || !lprcSrc1 || !lprcSrc2) {
        return FALSE;
    }

    lprcDst->left = lprcSrc1->left < lprcSrc2->left ? lprcSrc1->left : lprcSrc2->left;
    lprcDst->top = lprcSrc1->top < lprcSrc2->top ? lprcSrc1->top : lprcSrc2->top;
    lprcDst->right = lprcSrc1->right > lprcSrc2->right ? lprcSrc1->right : lprcSrc2->right;
    lprcDst->bottom = lprcSrc1->bottom > lprcSrc2->bottom ? lprcSrc1->bottom : lprcSrc2->bottom;

    return TRUE;
}

/** @brief Returns {0, 0, windowWidth, windowHeight} from the SDL window size. @note Status: PARTIAL */
BOOL WINAPI GetClientRect(HWND hWnd, LPRECT lpRect);
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
