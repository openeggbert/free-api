/**
 * @file crt_direct.cpp
 * @brief Non-Windows implementations of _chdir, _getcwd, _mkdir.
 *
 * On Windows these symbols are provided by the CRT (msvcrt) and resolved
 * at link time, so this file is compiled out.
 */

#if !defined(_WIN32)

#include "internal/FreeApiPath.hpp"

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
    // Normalize Windows-style paths (e.g. free-eggbert's _mkdir("\User"))
    // the same way CreateDirectoryA/_lopen do, so a leading backslash is
    // treated as relative to the current working directory rather than
    // escaping to the real filesystem root.
    const std::string normalized = FreeApi::Internal::NormalizeFilesystemPath(path);
    if (normalized.empty()) {
        return -1;
    }
    return mkdir(normalized.c_str(), 0777);
}

} // extern "C"

#endif // !_WIN32
