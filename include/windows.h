#ifndef FREE_API_WINDOWS_H
#define FREE_API_WINDOWS_H

#include <windef.h>
#include <winnt.h>
#include <minwindef.h>
#include <windef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// WinBase.h (Subset)
void WINAPI Sleep(DWORD dwMilliseconds);
DWORD WINAPI GetTickCount(void);
BOOL WINAPI CloseHandle(HANDLE hObject);
void WINAPI OutputDebugStringA(LPCSTR lpOutputString);
void WINAPI OutputDebugStringW(LPCWSTR lpOutputString);

#ifdef UNICODE
#define OutputDebugString OutputDebugStringW
#else
#define OutputDebugString OutputDebugStringA
#endif

// WinUser.h (Subset)
typedef WORD ATOM;

typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *PRECT, *LPRECT;

typedef struct tagPOINT {
    LONG x;
    LONG y;
} POINT, *PPOINT, *LPPOINT;

typedef struct tagMSG {
    HWND   hwnd;
    UINT   message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD  time;
    POINT  pt;
} MSG, *PMSG, *LPMSG;

typedef LRESULT(CALLBACK* WNDPROC)(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

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

#define WM_NULL 0x0000
#define WM_CLOSE 0x0010
#define WM_DESTROY 0x0002
#define WM_QUIT 0x0012

#define PM_NOREMOVE 0x0000
#define PM_REMOVE 0x0001

#define SW_HIDE 0
#define SW_SHOW 5

#define WS_OVERLAPPEDWINDOW 0x00CF0000L
#define WS_VISIBLE 0x10000000L

ATOM WINAPI RegisterClassA(const WNDCLASSA* lpWndClass);
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
BOOL WINAPI DestroyWindow(HWND hWnd);
BOOL WINAPI ShowWindow(HWND hWnd, int nCmdShow);
BOOL WINAPI UpdateWindow(HWND hWnd);
BOOL WINAPI PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);
BOOL WINAPI TranslateMessage(const MSG* lpMsg);
LRESULT WINAPI DispatchMessageA(const MSG* lpMsg);
LRESULT WINAPI DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
void WINAPI PostQuitMessage(int nExitCode);

typedef int(WINAPI* FREE_API_WINMAIN_PROC)(HINSTANCE, HINSTANCE, LPSTR, int);
int WINAPI FreeApiRunWinMain(FREE_API_WINMAIN_PROC entryPoint, int argc, char** argv);

#define RegisterClass RegisterClassA
#define CreateWindowEx CreateWindowExA
#define PeekMessage PeekMessageA
#define DispatchMessage DispatchMessageA
#define DefWindowProc DefWindowProcA

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
#endif

#endif // FREE_API_WINDOWS_H
