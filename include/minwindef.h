/**
 * @file minwindef.h
 * @brief Basic Win32 scalar typedefs and fundamental constants.
 *
 * Defines the minimal set of scalar types and constants expected by old C/C++
 * Win32 code: MAX_PATH, TRUE/FALSE, DWORD, BOOL, BYTE, WORD, FLOAT, INT, UINT,
 * and common pointer-to-scalar typedefs (LPVOID, LPDWORD, etc.).
 *
 * All types are implemented as standard C99/C++11 integer type aliases.
 * Note: old WinAPI code uses LP-prefixed types heavily; "LP" historically meant
 * "long pointer" but in modern flat-memory models it simply means a pointer.
 *
 * @note Status: HEADER_ONLY
 */
#pragma once

#include <stdint.h>

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

typedef uint32_t      DWORD;
typedef int           BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef float         FLOAT;
typedef FLOAT         *PFLOAT;
typedef BOOL          *PBOOL;
typedef BOOL          *LPBOOL;
typedef BYTE          *PBYTE;
typedef BYTE          *LPBYTE;
typedef int           *PINT;
typedef int           *LPINT;
typedef WORD          *PWORD;
typedef WORD          *LPWORD;
typedef uint32_t      *LPDWORD;
typedef void          *LPVOID;
typedef const void    *LPCVOID;

typedef int           INT;
typedef unsigned int  UINT;
typedef unsigned int  *PUINT;