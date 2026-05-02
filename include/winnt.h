/**
 * @file winnt.h
 * @brief Win32 character/string types, HRESULT, GUID, and basic COM aliases.
 *
 * Provides:
 * - Character types: CHAR, WCHAR, BOOLEAN
 * - String pointer aliases: LPSTR, LPCSTR, LPWSTR, LPCWSTR, TCHAR/LPTSTR/LPCTSTR
 * - Integer types: LONG, ULONG, USHORT, UCHAR
 * - HRESULT type (int32_t)
 * - IUnknown forward declaration (no COM behavior implemented)
 * - GUID/IID/CLSID structures and pointer/reference aliases
 *
 * This project is ANSI-oriented; the UNICODE alias layer exists but most
 * implemented functions are the 'A' variants.
 *
 * @note Status: HEADER_ONLY
 */
#ifndef FREE_API_WINNT_H
#define FREE_API_WINNT_H

#include <stdint.h>

typedef char CHAR;
typedef uint16_t WCHAR;
typedef uint8_t BOOLEAN;

typedef CHAR *PCHAR, *LPCH, *PCH, *NPSTR, *LPSTR, *PSTR;
typedef const CHAR *LPCSTR, *PCSTR;

typedef WCHAR *PWCHAR, *LPWCH, *PWCH, *NWPSTR, *LPWSTR, *PWSTR;
typedef const WCHAR *LPCWSTR, *PCWSTR;

typedef int32_t LONG;
typedef LONG HRESULT;
typedef uint32_t ULONG;
typedef uint32_t *PULONG;
typedef uint16_t USHORT;
typedef uint16_t *PUSHORT;
typedef uint8_t UCHAR;
typedef uint8_t *PUCHAR;
typedef char *PSZ;

typedef void *PVOID;

typedef struct IUnknown IUnknown;

#ifdef UNICODE
typedef WCHAR TCHAR, *PTCHAR;
typedef LPWSTR LPTSTR, PTSTR;
typedef LPCWSTR LPCTSTR;
#else
typedef char TCHAR, *PTCHAR;
typedef LPSTR LPTSTR, PTSTR;
typedef LPCSTR LPCTSTR;
#endif

typedef struct _GUID {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[ 8 ];
} GUID, *LPGUID;

typedef const GUID *LPCGUID;
typedef GUID IID;
typedef IID *LPIID;
typedef const IID *REFIID;
typedef GUID CLSID;
typedef CLSID *LPCLSID;
typedef const CLSID *REFCLSID;

#endif // FREE_API_WINNT_H
