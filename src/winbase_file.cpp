#include "windows.h"
#include "io.h"
#include "internal/FreeApiPath.hpp"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>
#include <cctype>
#include <string>
#include <unordered_map>

extern "C" {

static int g_nextFileHandle = 3;
static std::unordered_map<int, FILE*> g_openFiles;

static std::string NormalizePathA(LPCSTR path)
{
    if (!path) {
        return {};
    }

    std::string result(path);

    if (result.size() >= 2 &&
        std::isalpha(static_cast<unsigned char>(result[0])) &&
        result[1] == ':') {
        result.erase(0, 2);
    }

    for (char& c : result) {
        if (c == '\\') {
            c = '/';
        }
    }

    while (!result.empty() && result.front() == '/') {
        result.erase(result.begin());
    }

    return result;
}

int WINAPI _lopen(LPCSTR lpPathName, int iReadWrite)
{
    (void)iReadWrite;

    if (!lpPathName) {
        return -1;
    }

    std::string path = NormalizePathA(lpPathName);
    FILE* file = fopen(path.c_str(), "rb");

    if (!file) {
        SDL_Log("free-api _lopen: failed to open '%s' (orig: '%s')",
                path.c_str(), lpPathName);
        return -1;
    }

    int handle = g_nextFileHandle++;
    g_openFiles[handle] = file;
    return handle;
}

UINT WINAPI _lread(int hFile, LPVOID lpBuffer, UINT uBytes)
{
    if (!lpBuffer || uBytes == 0) {
        return 0;
    }

    auto it = g_openFiles.find(hFile);
    if (it == g_openFiles.end() || !it->second) {
        return 0;
    }

    size_t bytesRead = std::fread(lpBuffer, 1, static_cast<size_t>(uBytes), it->second);
    return static_cast<UINT>(bytesRead);
}

int WINAPI _lclose(int hFile)
{
    auto it = g_openFiles.find(hFile);
    if (it == g_openFiles.end() || !it->second) {
        return -1;
    }

    int result = std::fclose(it->second);
    g_openFiles.erase(it);

    return result == 0 ? 0 : -1;
}

BOOL WINAPI DeleteFileA(LPCSTR lpFileName)
{
    if (!lpFileName) {
        return FALSE;
    }

    std::string path = NormalizePathA(lpFileName);
    return std::remove(path.c_str()) == 0 ? TRUE : FALSE;
}

BOOL WINAPI CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    (void)lpSecurityAttributes;

    if (!lpPathName) {
        return FALSE;
    }

    std::string path = NormalizePathA(lpPathName);

    if (path.empty()) {
        return TRUE;
    }

    if (SDL_CreateDirectory(path.c_str())) {
        return TRUE;
    }

    SDL_Log("free-api CreateDirectoryA: failed to create '%s' (orig: '%s'): %s",
            path.c_str(), lpPathName, SDL_GetError());

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