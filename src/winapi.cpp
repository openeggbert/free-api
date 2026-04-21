#include "windows.h"
#include <thread>
#include <chrono>
#include <cstdio>

extern "C" {

void WINAPI Sleep(DWORD dwMilliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(dwMilliseconds));
}

DWORD WINAPI GetTickCount(void) {
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return static_cast<DWORD>(ms.count());
}

BOOL WINAPI CloseHandle(HANDLE hObject) {
    // placeholder for now
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

}
