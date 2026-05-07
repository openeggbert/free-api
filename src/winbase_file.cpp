#include "windows.h"
#include "io.h"
#include "internal/FreeApiPath.hpp"

#include <SDL3/SDL.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <string>

extern "C" {

int WINAPI _lopen(LPCSTR lpPathName, int iReadWrite)
{
    int flags = O_RDONLY;
    (void)iReadWrite;
    return open(lpPathName, flags);
}

UINT WINAPI _lread(int hFile, LPVOID lpBuffer, UINT uBytes)
{
    if (!lpBuffer) {
        return 0;
    }

    ssize_t bytesRead = read(hFile, lpBuffer, uBytes);
    return bytesRead > 0 ? static_cast<UINT>(bytesRead) : 0;
}

int WINAPI _lclose(int hFile)
{
    return close(hFile);
}

BOOL WINAPI DeleteFileA(LPCSTR lpFileName)
{
    if (!lpFileName) {
        return FALSE;
    }
    return remove(lpFileName) == 0 ? TRUE : FALSE;
}

BOOL WINAPI CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    (void)lpSecurityAttributes; // security descriptors not supported on Linux
    if (!lpPathName) {
        return FALSE;
    }
    // Convert backslashes to forward slashes and remove drive letter
    std::string path(lpPathName);
    if (path.size() >= 2 && isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':') {
        path.erase(0, 2);
    }
    for (char& c : path) {
        if (c == '\\') c = '/';
    }
    while (!path.empty() && (path[0] == '/' || path[0] == '\\')) {
        path.erase(0, 1);
    }

    if (path.empty()) return TRUE; // Already exists (root)

    // mkdir returns 0 on success, -1 on error (EEXIST is treated as success)
    int rc = mkdir(path.c_str(), 0755);
    if (rc == 0 || errno == EEXIST) {
        return TRUE;
    }
    SDL_Log("free-api CreateDirectoryA: failed to create '%s' (orig: '%s'): %s", path.c_str(), lpPathName, strerror(errno));
    return FALSE;
}

HRSRC WINAPI FindResourceA(HMODULE hModule, LPCSTR lpName, LPCSTR lpType)
{
    (void)hModule;
    (void)lpName;
    (void)lpType;
    return NULL;
}

HGLOBAL WINAPI LoadResource(HMODULE hModule, HRSRC hResInfo)
{
    (void)hModule;
    (void)hResInfo;
    return NULL;
}

DWORD WINAPI SizeofResource(HMODULE hModule, HRSRC hResInfo)
{
    (void)hModule;
    (void)hResInfo;
    return 0;
}

LPVOID WINAPI LockResource(HGLOBAL hResData)
{
    return hResData;
}

BOOL WINAPI UnlockResource(HGLOBAL hResData)
{
    (void)hResData;
    return FALSE;
}

BOOL WINAPI FreeResource(HGLOBAL hResData)
{
    (void)hResData;
    return FALSE;
}

} // extern "C"
