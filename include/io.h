#ifndef FREE_API_IO_H
#define FREE_API_IO_H

#include <windows.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Minimal io.h compatibility subset - Status: STUB
int WINAPI _lopen(LPCSTR lpPathName, int iReadWrite);
UINT WINAPI _lread(int hFile, LPVOID lpBuffer, UINT uBytes);
int WINAPI _lclose(int hFile);

#ifndef _MAX_FNAME
#define _MAX_FNAME 260
#endif

struct _finddata_t {
    unsigned attrib;
    time_t time_create;
    time_t time_access;
    time_t time_write;
    long size;
    char name[_MAX_FNAME];
};

intptr_t _findfirst(const char* filespec, struct _finddata_t* fileinfo); // Status: STUB
int _findnext(intptr_t handle, struct _finddata_t* fileinfo); // Status: STUB
int _findclose(intptr_t handle); // Status: STUB

#ifdef __cplusplus
}
#endif

#endif // FREE_API_IO_H