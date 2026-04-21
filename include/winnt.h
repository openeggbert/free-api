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
typedef uint32_t ULONG;
typedef uint32_t *PULONG;
typedef uint16_t USHORT;
typedef uint16_t *PUSHORT;
typedef uint8_t UCHAR;
typedef uint8_t *PUCHAR;
typedef char *PSZ;

typedef void *PVOID;

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
} GUID;

#endif // FREE_API_WINNT_H
