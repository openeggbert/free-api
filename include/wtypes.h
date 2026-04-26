#ifndef FREE_API_WTYPES_H
#define FREE_API_WTYPES_H

#include <windows.h>

// OLE / WTypes compatibility subset - Status: STUB
typedef unsigned short VARTYPE;
typedef long SCODE;
typedef double DATE;
typedef WORD CLIPFORMAT;

// Legacy Win32 alias expected by the original game source - Status: STUB
#ifndef byte
#define byte BYTE
#endif

#endif // FREE_API_WTYPES_H