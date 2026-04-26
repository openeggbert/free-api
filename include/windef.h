#ifndef FREE_API_WINDEF_H
#define FREE_API_WINDEF_H

#include <stdint.h>

#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void *)0)
#endif
#endif

typedef void* HANDLE;
typedef HANDLE HWND;
typedef HANDLE HINSTANCE;
typedef HANDLE HMODULE;
typedef HANDLE HDC;
typedef HANDLE HBRUSH;
typedef HANDLE HICON;
typedef HANDLE HCURSOR;
typedef HANDLE HMENU;
typedef HANDLE HFONT;

typedef intptr_t INT_PTR, *PINT_PTR;
typedef uintptr_t UINT_PTR, *PUINT_PTR;
typedef intptr_t LONG_PTR, *PLONG_PTR;
typedef uintptr_t ULONG_PTR, *PULONG_PTR;
typedef uintptr_t DWORD_PTR, *PDWORD_PTR;

typedef LONG_PTR LRESULT;
typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;

#define CALLBACK __stdcall
#define WINAPI __stdcall
#define WINAPIV __cdecl
#define APIENTRY WINAPI
#define APIPRIVATE __stdcall
#define PASCAL __stdcall

#undef __stdcall
#define __stdcall
#undef __cdecl
#define __cdecl

#endif // FREE_API_WINDEF_H
