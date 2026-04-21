#ifndef FREE_API_WINDOWS_H
#define FREE_API_WINDOWS_H

#include "windef.h"
#include "winnt.h"

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

#ifdef __cplusplus
}
#endif

#endif // FREE_API_WINDOWS_H
