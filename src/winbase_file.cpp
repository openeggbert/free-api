#include "windows.h"
#include "io.h"
#include "internal/FreeApiPath.hpp"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <filesystem>

extern "C" {

#if !defined(_WIN32)
static thread_local DWORD g_lastError = 0;

DWORD WINAPI GetLastError(void)
{
    return g_lastError;
}

void WINAPI SetLastError(DWORD dwErrCode)
{
    g_lastError = dwErrCode;
}
#endif

// TASK-24H-0707: no mutex protects these, unlike the equivalent _findfirst
// session table (g_findSessionsMutex, src/crt_io.cpp). This is a latent
// data race if _lopen/_lread/_lclose were ever called concurrently from
// multiple threads -- intentional/deferred, not an oversight: neither
// target game's file-I/O call sites (both single-threaded ddutil.cpp
// chains) use these functions from more than one thread.
static int g_nextFileHandle = 3;
static std::unordered_map<int, FILE*> g_openFiles;

static std::string NormalizePathA(LPCSTR path)
{
    return FreeApi::Internal::NormalizeFilesystemPath(path);
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
        // TASK-24H-1110: intentionally unconditional (failure path only).
        // Confirmed this session: each game has exactly one _lopen call
        // site (../free-eggbert/src/ddutil.cpp, ../planetblupi/src/ddutil.cpp
        // -- both a single, non-looping bitmap-open call), not a
        // multi-candidate probe loop, so this cannot fire more than once
        // per bitmap-load attempt.
        SDL_Log("free-api _lopen: failed to open '%s' (orig: '%s')",  // sdl-log-gating: intentional (failure)
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
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    std::string path = NormalizePathA(lpPathName);

    if (path.empty()) {
        return TRUE;
    }

    if (SDL_CreateDirectory(path.c_str())) {
        return TRUE;
    }

    /* WinAPI expects ERROR_ALREADY_EXISTS if the directory exists. */
    {
        std::error_code ec;
        if (std::filesystem::exists(path, ec)) {
            SetLastError(ERROR_ALREADY_EXISTS);
            return FALSE;
        }
    }

    // TASK-24H-1110: intentionally unconditional (failure path only).
    // Both games' only CreateDirectoryA call sites are inside their own
    // AddUserPath() (misc.cpp) -- confirmed this session that this is NOT
    // uniformly dead code: free-eggbert's AddUserPath body is gated behind
    // "#if _CD || _LEGACY" (neither macro is ever defined in its build, so
    // it's dead there), but planetblupi's AddUserPath has no such guard --
    // it's live, called from decio.cpp's save/load paths. So this log can
    // genuinely fire for planetblupi if directory creation ever fails for a
    // real reason (permissions, disk full, etc.); it does not fire "never"
    // as free-eggbert's does, but a real filesystem failure is rare by
    // nature, so this stays an acceptable unconditional failure log either way.
    SDL_Log("free-api CreateDirectoryA: failed to create '%s' (orig: '%s'): %s",  // sdl-log-gating: intentional (failure)
            path.c_str(), lpPathName, SDL_GetError());

    return FALSE;
}

BOOL WINAPI RemoveDirectoryA(LPCSTR lpPathName)
{
    if (!lpPathName) {
        return FALSE;
    }

    std::string path = NormalizePathA(lpPathName);
    return SDL_RemovePath(path.c_str()) ? TRUE : FALSE;
}

BOOL WINAPI SetEnvironmentVariableA(LPCSTR lpName, LPCSTR lpValue)
{
    if (!lpName) {
        return FALSE;
    }
    // Real Win32 deletes the variable when lpValue is NULL rather than
    // setting it to an empty string -- zero call sites in either target
    // game (test-infrastructure-only), found by this session's
    // correctness audit.
    if (!lpValue) {
        return SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), lpName) ? TRUE : FALSE;
    }
    return SDL_SetEnvironmentVariable(SDL_GetEnvironment(), lpName, lpValue, true) ? TRUE : FALSE;
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