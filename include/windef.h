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

#include <winnt.h>
#include <minwindef.h>

typedef DWORD COLORREF;

/** @brief Win32 rectangle structure. @note Status: IMPLEMENTED */
typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *PRECT, *LPRECT;

/** @brief Win32 integer point structure. @note Status: IMPLEMENTED */
typedef struct tagPOINT {
    LONG x;
    LONG y;
} POINT, *PPOINT, *LPPOINT;

#endif // FREE_API_WINDEF_H
