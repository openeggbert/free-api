/**
 * @file windef.h
 * @brief Opaque handle typedefs, pointer-sized integer types, and calling-convention macros.
 *
 * Provides:
 * - HANDLE and all Win32 handle aliases (HWND, HINSTANCE, HDC, HBITMAP, etc.)
 *   All are aliases of void*; they are opaque tokens passed to API calls.
 *   Internally: HWND is an SDL_Window* cast to void*; HBITMAP and HDC are
 *   private C++ structs cast to handles (CompatBitmap/CompatDC in winapi.cpp).
 * - Pointer-sized integer types: INT_PTR, UINT_PTR, LONG_PTR, ULONG_PTR, DWORD_PTR
 * - Message parameter types: WPARAM (unsigned pointer-sized), LPARAM (signed pointer-sized)
 * - LRESULT: return type of a window procedure
 * - Calling-convention macros (CALLBACK, WINAPI, etc.) which expand to empty
 *   definitions since calling conventions are irrelevant on non-Windows targets.
 *
 * @note Status: HEADER_ONLY
 */
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
typedef HANDLE HGLOBAL;
typedef HANDLE HRSRC;
typedef HANDLE HDC;
typedef HANDLE HGDIOBJ;
typedef HANDLE HBRUSH;
typedef HANDLE HBITMAP;
typedef HANDLE HPALETTE;
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
