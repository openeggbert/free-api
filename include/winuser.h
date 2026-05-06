#pragma once

#include <minwindef.h>
#include <windef.h>

//#145
#ifndef MAKEINTRESOURCEA
#define MAKEINTRESOURCEA(i) ((LPSTR)((ULONG_PTR)((WORD)(i))))
#endif
#ifndef MAKEINTRESOURCE
#define MAKEINTRESOURCE MAKEINTRESOURCEA
#endif

//#159
#define RT_BITMAP ((LPCSTR)2)

//#251
#define SW_HIDE 0
#define SW_SHOW 5

//#373
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
 * @brief Win32-style message entry used by the Free API dispatch loop.
 *
 * `MSG` is the public container passed through the emulated message queue.
 * It is filled by functions such as `GetMessage()` and `PeekMessage()`,
 * optionally processed by `TranslateMessage()`, and delivered to a window
 * procedure by `DispatchMessage()`.
 *
 * In Free API, messages may come from translated SDL events, timer callbacks,
 * internal window events, or explicit calls such as `PostMessage()` and
 * `PostQuitMessage()`.
 *
 * @note Status: PARTIAL
 *       The layout matches the supported Win32 subset, but Free API does not
 *       fully emulate all native queue, thread, timing, and coordinate
 *       semantics.
 * @note REVIEWED
 */
typedef struct tagMSG
{
    /** @brief Target window handle, or `NULL` for thread/global messages. */
    HWND hwnd;

    /** @brief Message identifier, for example, `WM_PAINT`, `WM_KEYDOWN`, or `WM_QUIT`. */
    UINT message;

    /** @brief First message-specific parameter; meaning depends on @ref message. */
    WPARAM wParam;

    /** @brief Second message-specific parameter; meaning depends on @ref message. */
    LPARAM lParam;

    /**
     * @brief Message timestamp in milliseconds.
     *
     * Present for Win32 `MSG` compatibility. Free API may currently provide this
     * only for selected message sources; otherwise it may be zero.
     */
    DWORD time;

    /** @brief Cursor position associated with the message, where available. */
    POINT pt;
} MSG, *PMSG, *LPMSG;


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

//#1857
#define WS_VISIBLE 0x10000000L
#define WS_POPUP 0x80000000L
#define WS_CHILD 0x40000000L
#define WS_CAPTION 0x00C00000L
#define WS_POPUPWINDOW 0x80880000L
#define WS_OVERLAPPEDWINDOW 0x00CF0000L

#define WS_EX_TOPMOST 0x00000008L

//#1934
/**
 * @name Window class/style constants
 * @brief Legacy style values interpreted by the compatibility layer.
 * @note Status: PARTIAL
 */
/** @{ */
#define CS_VREDRAW 0x0001
#define CS_HREDRAW 0x0002

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

//#2412
/** @brief `PeekMessage` mode constants. @note Status: IMPLEMENTED */
#define PM_NOREMOVE 0x0000
#define PM_REMOVE 0x0001

//#2988
/** @brief Legacy desktop pseudo-window handle. @note Status: IMPLEMENTED */
#define HWND_DESKTOP ((HWND)0)

//#4600
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define SM_CYCAPTION 4

//#6070
/** @brief Message box style subset. @note Status: STUB */
#define MB_OK 0x00000000L

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

//#7297
#define IMAGE_BITMAP 0

//#7308
#define LR_LOADFROMFILE 0x00000010
#define LR_CREATEDIBSECTION 0x00002000
