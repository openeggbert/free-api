/**
* @file minwindef.h
 * @brief Minimal Win32-compatible basic definitions used by Free API.
 *
 * This header intentionally declares only the subset currently used by Free API.
 * It is not a full copy of the Windows SDK minwindef.h.
 *
 * See docs/minwindef.md for detailed documentation.
 */

#pragma once

#include <stdint.h>

#include <basestd.h>

/* Miscellaneous constants */

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void *)0)
#endif
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#define CALLBACK __stdcall
#define WINAPI __stdcall
#define WINAPIV __cdecl
#define APIENTRY WINAPI
#define APIPRIVATE __stdcall
#define PASCAL __stdcall

#ifndef __stdcall
#define __stdcall
#endif

#ifndef __cdecl
#define __cdecl
#endif

/* Basic scalar types */

typedef uint32_t       DWORD;
typedef int            BOOL;
typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef float          FLOAT;
typedef FLOAT          *PFLOAT;
typedef BOOL           *PBOOL;
typedef BOOL           *LPBOOL;
typedef BYTE           *PBYTE;
typedef BYTE           *LPBYTE;
typedef int            *PINT;
typedef int            *LPINT;
typedef WORD           *PWORD;
typedef WORD           *LPWORD;
typedef DWORD          *LPDWORD;
typedef void           *LPVOID;
typedef const void     *LPCVOID;

typedef int            INT;
typedef unsigned int   UINT;
typedef unsigned int   *PUINT;

typedef LONG_PTR LRESULT;
typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;

typedef WORD ATOM;

/* Handle types */

typedef void* HANDLE;

typedef HANDLE HGLOBAL;
typedef HANDLE HINSTANCE;
typedef HANDLE HMODULE;
typedef HANDLE HRSRC;

/* Free API compatibility handles currently kept here for existing code. */

typedef HANDLE HWND;
typedef HANDLE HDC;
typedef HANDLE HGDIOBJ;
typedef HANDLE HBRUSH;
typedef HANDLE HBITMAP;
/**
 * @brief Opaque palette handle type.
 *
 * No palette-creation/manipulation API exists anywhere in Free API (no
 * function takes or returns HPALETTE) -- kept only as an opaque
 * type-compatibility placeholder for real Win32 headers. See
 * docs/out-of-scope.md's "Compile-only stubs" table (TASK-24H-0111).
 * @note Status: VESTIGIAL (kept, unused)
 */
typedef HANDLE HPALETTE;
typedef HANDLE HICON;
typedef HANDLE HCURSOR;
typedef HANDLE HMENU;
/**
 * @brief Opaque font handle type.
 *
 * No font-creation/manipulation API exists anywhere in Free API (no
 * function takes or returns HFONT) -- kept only as an opaque
 * type-compatibility placeholder for real Win32 headers. See
 * docs/out-of-scope.md's "Compile-only stubs" table (TASK-24H-0111).
 * @note Status: VESTIGIAL (kept, unused)
 */
typedef HANDLE HFONT;
