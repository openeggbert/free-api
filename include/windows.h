#ifndef FREE_API_WINDOWS_H
#define FREE_API_WINDOWS_H

#include <minwindef.h>
#include <windef.h>
#include <winnt.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// WinBase.h (Subset) - Status: PARTIAL
void WINAPI Sleep(DWORD dwMilliseconds);
DWORD WINAPI GetTickCount(void);
BOOL WINAPI CloseHandle(HANDLE hObject);
void WINAPI OutputDebugStringA(LPCSTR lpOutputString);
void WINAPI OutputDebugStringW(LPCWSTR lpOutputString);

typedef struct _MEMORYSTATUS {
    DWORD dwLength;
    DWORD dwMemoryLoad;
    DWORD dwTotalPhys;
    DWORD dwAvailPhys;
    DWORD dwTotalPageFile;
    DWORD dwAvailPageFile;
    DWORD dwTotalVirtual;
    DWORD dwAvailVirtual;
} MEMORYSTATUS, *LPMEMORYSTATUS;

void WINAPI GlobalMemoryStatus(LPMEMORYSTATUS lpBuffer); // Status: STUB

#ifdef UNICODE
#define OutputDebugString OutputDebugStringW
#else
#define OutputDebugString OutputDebugStringA
#endif

// WinUser.h (Subset) - Status: PARTIAL
typedef WORD ATOM;
typedef DWORD COLORREF;
typedef UINT MCIDEVICEID;

extern char* _pgmptr; // Status: STUB

// Legacy alias used by old Win32 codebases - Status: STUB
#ifndef byte
#define byte BYTE
#endif

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

typedef struct tagBITMAP {
    LONG bmType;
    LONG bmWidth;
    LONG bmHeight;
    LONG bmWidthBytes;
    WORD bmPlanes;
    WORD bmBitsPixel;
    LPVOID bmBits;
} BITMAP, *PBITMAP, *LPBITMAP;

typedef struct tagRGBQUAD {
    BYTE rgbBlue;
    BYTE rgbGreen;
    BYTE rgbRed;
    BYTE rgbReserved;
} RGBQUAD, *LPRGBQUAD;

#ifndef FREE_API_PALETTEENTRY_DEFINED
#define FREE_API_PALETTEENTRY_DEFINED
typedef struct tagPALETTEENTRY {
    BYTE peRed;
    BYTE peGreen;
    BYTE peBlue;
    BYTE peFlags;
} PALETTEENTRY, *LPPALETTEENTRY;
#endif

typedef struct tagBITMAPFILEHEADER {
    WORD bfType;
    DWORD bfSize;
    WORD bfReserved1;
    WORD bfReserved2;
    DWORD bfOffBits;
} BITMAPFILEHEADER, *LPBITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER {
    DWORD biSize;
    LONG biWidth;
    LONG biHeight;
    WORD biPlanes;
    WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG biXPelsPerMeter;
    LONG biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BITMAPINFOHEADER, *LPBITMAPINFOHEADER;

typedef struct tagMSG {
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    POINT pt;
} MSG, *PMSG, *LPMSG;

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
#define WM_USER 0x0400

#define PM_NOREMOVE 0x0000
#define PM_REMOVE 0x0001

#define SW_HIDE 0
#define SW_SHOW 5

#define MB_OK 0x00000000L

#ifndef E_FAIL
#define E_FAIL ((HRESULT)0x80004005L)
#endif

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

#define SRCCOPY 0x00CC0020

#define SIZEPALETTE 104

#define OF_READ 0x0000

#define CLR_INVALID 0xFFFFFFFF

#define RT_BITMAP ((LPCSTR)2)

#ifndef MAKEINTRESOURCEA
#define MAKEINTRESOURCEA(i) ((LPSTR)((ULONG_PTR)((WORD)(i))))
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

#define HWND_DESKTOP ((HWND)0)

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
BOOL WINAPI DestroyWindow(HWND hWnd);
BOOL WINAPI ShowWindow(HWND hWnd, int nCmdShow);
BOOL WINAPI UpdateWindow(HWND hWnd);
BOOL WINAPI MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint); // Status: STUB
BOOL WINAPI InvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase); // Status: STUB
BOOL WINAPI PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);
BOOL WINAPI GetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax); // Status: STUB
BOOL WINAPI TranslateMessage(const MSG* lpMsg);
LRESULT WINAPI DispatchMessageA(const MSG* lpMsg);
LRESULT WINAPI DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
void WINAPI PostQuitMessage(int nExitCode);
BOOL WINAPI WaitMessage(void); // Status: STUB

BOOL WINAPI SetWindowTextA(HWND hWnd, LPCSTR lpString); // Status: STUB
BOOL WINAPI PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam); // Status: STUB
int WINAPI MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType); // Status: STUB
BOOL WINAPI GetCursorPos(LPPOINT lpPoint); // Status: STUB
BOOL WINAPI ScreenToClient(HWND hWnd, LPPOINT lpPoint); // Status: STUB
HCURSOR WINAPI SetCursor(HCURSOR hCursor); // Status: STUB
int WINAPI ShowCursor(BOOL bShow); // Status: STUB
BOOL WINAPI ClientToScreen(HWND hWnd, LPPOINT lpPoint); // Status: STUB
BOOL WINAPI SetCursorPos(int X, int Y); // Status: STUB
int WINAPI LoadStringA(HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, int cchBufferMax); // Status: STUB
HMODULE WINAPI GetModuleHandleA(LPCSTR lpModuleName); // Status: STUB
HANDLE WINAPI LoadImageA(HINSTANCE hInst, LPCSTR name, UINT type, int cx, int cy, UINT fuLoad); // Status: STUB
int WINAPI GetObjectA(HANDLE h, int c, LPVOID pv); // Status: STUB
BOOL WINAPI DeleteObject(HGDIOBJ ho); // Status: STUB
HDC WINAPI CreateCompatibleDC(HDC hdc); // Status: STUB
HGDIOBJ WINAPI SelectObject(HDC hdc, HGDIOBJ h); // Status: STUB
BOOL WINAPI DeleteDC(HDC hdc); // Status: STUB
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
                       DWORD rop); // Status: STUB
COLORREF WINAPI GetPixel(HDC hdc, int x, int y); // Status: STUB
COLORREF WINAPI SetPixel(HDC hdc, int x, int y, COLORREF color); // Status: STUB
int WINAPI GetDeviceCaps(HDC hdc, int index); // Status: STUB
UINT WINAPI GetSystemPaletteEntries(HDC hdc, UINT iStartIndex, UINT nEntries, LPVOID lppe); // Status: STUB
HRSRC WINAPI FindResourceA(HMODULE hModule, LPCSTR lpName, LPCSTR lpType); // Status: STUB
HGLOBAL WINAPI LoadResource(HMODULE hModule, HRSRC hResInfo); // Status: STUB
DWORD WINAPI SizeofResource(HMODULE hModule, HRSRC hResInfo); // Status: STUB
LPVOID WINAPI LockResource(HGLOBAL hResData); // Status: STUB
BOOL WINAPI UnlockResource(HGLOBAL hResData); // Status: STUB
BOOL WINAPI FreeResource(HGLOBAL hResData); // Status: STUB
int WINAPI _lopen(LPCSTR lpPathName, int iReadWrite); // Status: STUB
UINT WINAPI _lread(int hFile, LPVOID lpBuffer, UINT uBytes); // Status: STUB
int WINAPI _lclose(int hFile); // Status: STUB
BOOL WINAPI DeleteFileA(LPCSTR lpFileName); // Status: STUB
int WINAPIV wsprintfA(LPSTR lpOut, LPCSTR lpFmt, ...); // Status: STUB
HCURSOR WINAPI LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName); // Status: STUB
HICON WINAPI LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName); // Status: STUB
HBRUSH WINAPI GetStockBrush(int fnObject); // Status: STUB
int WINAPI GetSystemMetrics(int nIndex); // Status: STUB
BOOL WINAPI AdjustWindowRect(LPRECT lpRect, DWORD dwStyle, BOOL bMenu); // Status: STUB
HWND WINAPI SetFocus(HWND hWnd); // Status: STUB
UINT_PTR WINAPI SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, void* lpTimerFunc); // Status: STUB
BOOL WINAPI KillTimer(HWND hWnd, UINT_PTR uIDEvent); // Status: STUB

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

BOOL WINAPI GetClientRect(HWND hWnd, LPRECT lpRect); // Status: STUB

typedef int(WINAPI* FREE_API_WINMAIN_PROC)(HINSTANCE, HINSTANCE, LPSTR, int);
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
#endif

#include <mmsystem.h>

#endif // FREE_API_WINDOWS_H
