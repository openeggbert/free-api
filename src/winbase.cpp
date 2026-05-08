#include "windows.h"

/*
 * On Windows / MinGW, every symbol defined below (Sleep, GetTickCount,
 * GlobalMemoryStatus, CloseHandle, OutputDebugStringA/W, GetModuleHandleA)
 * is already exported by kernel32.dll. Defining them again in this static
 * library produces:
 *
 *     multiple definition of `Sleep'
 *
 * at link time as soon as libkernel32.a is pulled in for any other import.
 *
 * The purpose of this translation unit is to *emulate* the WinAPI on
 * non-Windows hosts; on real Windows we must defer to kernel32. Therefore
 * the entire body is compiled out on _WIN32, mirroring the same pattern
 * already used in direct.h, io.h and crt_io.cpp.
 */
#if !defined(_WIN32)

#include <chrono>
#include <thread>
#include <cstdio>

extern "C" {

void WINAPI Sleep(DWORD dwMilliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(dwMilliseconds));
}

DWORD WINAPI GetTickCount(void) {
    using namespace std::chrono;

    static const auto start = steady_clock::now();
    const auto now = steady_clock::now();

    const auto ms = duration_cast<milliseconds>(now - start).count();

    return static_cast<DWORD>(
        static_cast<uint64_t>(ms) & 0xFFFFFFFFu
    );
}

void WINAPI GlobalMemoryStatus(LPMEMORYSTATUS lpBuffer)
{
    if (!lpBuffer) {
        return;
    }

    lpBuffer->dwLength        = sizeof(MEMORYSTATUS);
    lpBuffer->dwMemoryLoad    = 25;
    lpBuffer->dwTotalPhys     = 512u * 1024u * 1024u;
    lpBuffer->dwAvailPhys     = 256u * 1024u * 1024u;
    lpBuffer->dwTotalPageFile = 1024u * 1024u * 1024u;
    lpBuffer->dwAvailPageFile = 512u * 1024u * 1024u;
    lpBuffer->dwTotalVirtual  = 1024u * 1024u * 1024u;
    lpBuffer->dwAvailVirtual  = 512u * 1024u * 1024u;
}

BOOL WINAPI CloseHandle(HANDLE hObject) {
    (void)hObject;
    return TRUE;
}

void WINAPI OutputDebugStringA(LPCSTR lpOutputString) {
    if (lpOutputString) {
        printf("%s", lpOutputString);
    }
}

void WINAPI OutputDebugStringW(LPCWSTR lpOutputString) {
    if (lpOutputString) {
        while (*lpOutputString) {
            printf("%c", (char)*lpOutputString++);
        }
    }
}

HMODULE WINAPI GetModuleHandleA(LPCSTR lpModuleName) {
    (void)lpModuleName;
    return reinterpret_cast<HMODULE>(static_cast<uintptr_t>(1));
}

} // extern "C"

#endif // !_WIN32
