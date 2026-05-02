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

/* Miscellaneous constants */

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
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
typedef HANDLE HPALETTE;
typedef HANDLE HICON;
typedef HANDLE HCURSOR;
typedef HANDLE HMENU;
typedef HANDLE HFONT;
