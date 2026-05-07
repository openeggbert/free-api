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

extern "C"{
//#205
/** @brief Formatted print using vsnprintf with a 1024-byte fixed buffer. @note Status: PARTIAL */
int WINAPIV wsprintfA(LPSTR lpOut, LPCSTR lpFmt, ...);
}

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

extern "C" {
//#2337
/** @brief Blocks until a message is available or quit is posted. @note Status: PARTIAL */
BOOL WINAPI GetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);

//#2359
/** @brief Performs keyboard translation phase. @note Status: PARTIAL */
BOOL WINAPI TranslateMessage(const MSG* lpMsg);

//#2365
/** @brief Dispatches message to a window procedure. @note Status: PARTIAL */
LRESULT WINAPI DispatchMessageA(const MSG* lpMsg);

//#2388
/** @brief Polls message queue. @note Status: IMPLEMENTED */
BOOL WINAPI PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);

//#2412
/** @brief `PeekMessage` mode constants. @note Status: IMPLEMENTED */
#define PM_NOREMOVE 0x0000
#define PM_REMOVE 0x0001

//#2683
/** @brief Enqueues a message into the global mutex-protected queue. No thread/window validation. @note Status: PARTIAL */
BOOL WINAPI PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

//#2761
/** @brief Waits until the message queue receives work. @note Status: PARTIAL */
BOOL WINAPI WaitMessage(void);

//#2799
/** @brief Default window procedure fallback. @note Status: PARTIAL */
LRESULT WINAPI DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

//#2806
/** @brief Posts quit message to the thread queue. @note Status: IMPLEMENTED */
void WINAPI PostQuitMessage(int nExitCode);

//#2901
/** @brief Registers a window class (ANSI). @note Status: PARTIAL */
ATOM WINAPI RegisterClassA(const WNDCLASSA* lpWndClass);

//#2988
/** @brief Legacy desktop pseudo-window handle. @note Status: IMPLEMENTED */
#define HWND_DESKTOP ((HWND)0)

//#2993
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
//#3028
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

//#3064
/** @brief Destroys a window and releases internal mappings. @note Status: PARTIAL */
BOOL WINAPI DestroyWindow(HWND hWnd);

//#3070
/** @brief Changes window visibility/state. @note Status: PARTIAL */
BOOL WINAPI ShowWindow(HWND hWnd, int nCmdShow);

//#3135
/** @brief Moves/resizes a window via SDL. Ignores repaint semantics. @note Status: PARTIAL */
BOOL WINAPI MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint);

//#4103
/** @brief Sets keyboard focus window. @note Status: PARTIAL */
HWND WINAPI SetFocus(HWND hWnd);

//#4478
/**
 * @brief Stores a polling timer; PeekMessageA generates WM_TIMER messages.
 *
 * lpTimerFunc is ignored. If nIDEvent == 0, a unique ID is generated.
 * Timers only fire while the message loop calls PeekMessageA or GetMessageA.
 * @note Status: PARTIAL
 */
UINT_PTR WINAPI SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, void* lpTimerFunc);

//#4487
/** @brief Removes a stored polling timer. @note Status: PARTIAL */
BOOL WINAPI KillTimer(HWND hWnd, UINT_PTR uIDEvent);

//#4600
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define SM_CYCAPTION 4

//#4703
/** @brief Returns selected system metric value. @note Status: PARTIAL */
int WINAPI GetSystemMetrics(int nIndex);

//#5530
/** @brief Requests immediate window refresh/raise. @note Status: PARTIAL */
BOOL WINAPI UpdateWindow(HWND hWnd);

//#5683
/** @brief Invalidates window client area. @note Status: STUB */
BOOL WINAPI InvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase);

//#5962
/** @brief Sets SDL window title. @note Status: PARTIAL */
BOOL WINAPI SetWindowTextA(HWND hWnd, LPCSTR lpString);

//#6016
/** @brief Returns {0, 0, windowWidth, windowHeight} from the SDL window size. @note Status: PARTIAL */
BOOL WINAPI GetClientRect(HWND hWnd, LPRECT lpRect);

//#6030
/** @brief Adjusts window rectangle for styles. @note Status: STUB */
BOOL WINAPI AdjustWindowRect(LPRECT lpRect, DWORD dwStyle, BOOL bMenu);

//#6070
/** @brief Message box style subset. @note Status: STUB */
#define MB_OK 0x00000000L

//#6136
/** @brief Displays a simple message box replacement. @note Status: STUB */
int WINAPI MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType);

//#6242
/** @brief Shows/hides cursor and returns display counter. @note Status: STUB */
int WINAPI ShowCursor(BOOL bShow);

//#6248
/** @brief Warps the cursor to screen position via SDL_WarpMouseGlobal. @note Status: PARTIAL */
BOOL WINAPI SetCursorPos(int X, int Y);

//#6255
/** @brief Sets active cursor shape. @note Status: STUB */
HCURSOR WINAPI SetCursor(HCURSOR hCursor);

//#6261
/** @brief Retrieves cursor position via SDL_GetGlobalMouseState. @note Status: PARTIAL */
BOOL WINAPI GetCursorPos(LPPOINT lpPoint);

//#6337
/** @brief Converts client to screen coordinates by adding SDL window position. @note Status: PARTIAL */
BOOL WINAPI ClientToScreen(HWND hWnd, LPPOINT lpPoint);

//#6344
/** @brief Converts screen coordinates to client by subtracting SDL window position. @note Status: PARTIAL */
BOOL WINAPI ScreenToClient(HWND hWnd, LPPOINT lpPoint);

//#6497
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

//#6528
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

//#6536
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

//#7107
/** @brief Loads a cursor resource. @note Status: STUB */
HCURSOR WINAPI LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName);

//#7209
/** @brief Loads an icon resource. @note Status: STUB */
HICON WINAPI LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName);

}
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

extern "C"{
//#7320
/**
 * @brief Loads an image from disk (IMAGE_BITMAP + LR_LOADFROMFILE only).
 *
 * Loads a BMP via SDL_LoadBMP, converts to RGBA32, optionally scales.
 * Resource loading (without LR_LOADFROMFILE) is not implemented.
 * @note Status: PARTIAL
 */
HANDLE WINAPI LoadImageA(HINSTANCE hInst, LPCSTR name, UINT type, int cx, int cy, UINT fuLoad);

//#7513
/** @brief Loads string resource text. @note Status: STUB */
int WINAPI LoadStringA(HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, int cchBufferMax);
}

