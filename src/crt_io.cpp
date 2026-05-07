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

} // extern "C"
