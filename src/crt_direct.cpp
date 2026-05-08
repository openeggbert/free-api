/**
 * @file crt_direct.cpp
 * @brief Non-Windows implementations of _chdir, _getcwd, _mkdir.
 *
 * On Windows these symbols are provided by the CRT (msvcrt) and resolved
 * at link time, so this file is compiled out.
 */

#if !defined(_WIN32)

#include <unistd.h>
#include <sys/stat.h>

extern "C" {

int _chdir(const char* path)
{
    return chdir(path);
}

char* _getcwd(char* buffer, int maxlen)
{
    return getcwd(buffer, static_cast<size_t>(maxlen));
}

int _mkdir(const char* path)
{
    return mkdir(path, 0777);
}

} // extern "C"

#endif // !_WIN32
