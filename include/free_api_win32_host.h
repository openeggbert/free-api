/**
 * @file free_api_win32_host.h
 * @brief Names the Windows SDK's own headers expect from <windows.h>, on a Windows host only.
 *
 * Free API's <windows.h> stands in for the SDK's also when Free API is built for Windows
 * (MinGW-w64). Free Direct's networking includes the system's Winsock headers there, through
 * ENet and IXWebSocket: <winsock2.h> includes <windows.h>, which resolves to Free API's, and then
 * uses the names below. Free API implements none of them; they only match the SDK's
 * definitions, so the system's declarations compile next to Free API's types.
 *
 * Included by windows.h when _WIN32 is defined; Linux, Android and the web never see it.
 * Only 64-bit Windows is supported (Free API's calling-convention macros may be empty).
 *
 * @note Status: HEADER_ONLY
 */
#ifndef FREE_API_WIN32_HOST_H
#define FREE_API_WIN32_HOST_H

#if defined(_WIN32)

#include <minwindef.h>
#include <winnt.h>

/* basetsd.h */
typedef signed char INT8, *PINT8;
typedef unsigned char UINT8, *PUINT8;
typedef signed short INT16, *PINT16;
typedef unsigned short UINT16, *PUINT16;
typedef signed int INT32, *PINT32;
typedef unsigned int UINT32, *PUINT32;
typedef signed long long INT64, *PINT64, LONG64, *PLONG64;
typedef unsigned long long UINT64, *PUINT64, ULONG64, *PULONG64, DWORD64, *PDWORD64;

/* minwindef.h; WINBOOL is MinGW-w64's name for the SDK's BOOL. */
typedef int WINBOOL;
typedef HANDLE *PHANDLE, *LPHANDLE;
typedef INT_PTR (WINAPI *FARPROC)();

/* minwinbase.h */
typedef struct _OVERLAPPED {
    ULONG_PTR Internal;
    ULONG_PTR InternalHigh;
    union {
        struct {
            DWORD Offset;
            DWORD OffsetHigh;
        };
        PVOID Pointer;
    };
    HANDLE hEvent;
} OVERLAPPED, *LPOVERLAPPED;

/* winnt.h */
#ifndef DECLSPEC_IMPORT
#define DECLSPEC_IMPORT __declspec(dllimport)
#endif
#ifndef NTSYSAPI
#define NTSYSAPI DECLSPEC_IMPORT
#endif
#ifndef NTAPI
#define NTAPI __stdcall
#endif
#ifndef C_ASSERT
#define C_ASSERT(e) typedef char __C_ASSERT__[(e) ? 1 : -1]
#endif

/* guiddef.h: GUID, IID, CLSID and REFIID come from Free API's winnt.h. Keep the SDK's
 * guiddef.h (which <winsock2.h> includes) from defining them a second time; its REFIID
 * would be a C++ reference where Free API's is a pointer. */
#define GUID_DEFINED
#define _GUIDDEF_H_

#endif /* _WIN32 */

#endif /* FREE_API_WIN32_HOST_H */
