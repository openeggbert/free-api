#include "io.h"

#include <cstring>

extern "C" {

intptr_t _findfirst(const char* filespec, struct _finddata_t* fileinfo)
{
    (void)filespec;
    if (fileinfo) {
        memset(fileinfo, 0, sizeof(*fileinfo));
    }
    return -1;
}

int _findnext(intptr_t handle, struct _finddata_t* fileinfo)
{
    (void)handle;
    (void)fileinfo;
    return -1;
}

int _findclose(intptr_t handle)
{
    (void)handle;
    return 0;
}

#if defined(_WIN32)
/* POSIX access() — thin wrapper over MinGW/CRT _access().                    */
/* Provided here because our compatibility <io.h> shadows MinGW's <io.h>      */
/* (which would normally expose this via inline). _access itself is exported  */
/* by msvcrt and resolved at link time.                                       */
int access(const char* path, int mode)
{
    return _access(path, mode);
}
#endif

} // extern "C"
