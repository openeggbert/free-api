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
#include <string>

namespace {

// Encodes a single decoded Unicode code point as UTF-8 into `out`.
void AppendUtf8CodePoint(std::string& out, uint32_t codepoint)
{
    if (codepoint <= 0x7Fu) {
        out += static_cast<char>(codepoint);
    } else if (codepoint <= 0x7FFu) {
        out += static_cast<char>(0xC0u | (codepoint >> 6));
        out += static_cast<char>(0x80u | (codepoint & 0x3Fu));
    } else if (codepoint <= 0xFFFFu) {
        out += static_cast<char>(0xE0u | (codepoint >> 12));
        out += static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu));
        out += static_cast<char>(0x80u | (codepoint & 0x3Fu));
    } else {
        out += static_cast<char>(0xF0u | (codepoint >> 18));
        out += static_cast<char>(0x80u | ((codepoint >> 12) & 0x3Fu));
        out += static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu));
        out += static_cast<char>(0x80u | (codepoint & 0x3Fu));
    }
}

// Decodes a null-terminated UTF-16 string (WCHAR = uint16_t code units,
// matching real Win32 semantics) into UTF-8, resolving surrogate pairs. An
// unpaired surrogate is emitted as the Unicode replacement character rather
// than silently corrupting the byte stream.
std::string Utf16ToUtf8(const WCHAR* text)
{
    std::string out;
    while (*text) {
        uint32_t codepoint = *text++;
        if (codepoint >= 0xD800u && codepoint <= 0xDBFFu) {
            if (*text >= 0xDC00u && *text <= 0xDFFFu) {
                const uint32_t low = *text++;
                codepoint = 0x10000u + ((codepoint - 0xD800u) << 10) + (low - 0xDC00u);
            } else {
                codepoint = 0xFFFDu; // unpaired high surrogate
            }
        } else if (codepoint >= 0xDC00u && codepoint <= 0xDFFFu) {
            codepoint = 0xFFFDu; // unpaired low surrogate
        }
        AppendUtf8CodePoint(out, codepoint);
    }
    return out;
}

} // namespace

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
        const std::string utf8 = Utf16ToUtf8(lpOutputString);
        printf("%s", utf8.c_str());
    }
}

HMODULE WINAPI GetModuleHandleA(LPCSTR lpModuleName) {
    (void)lpModuleName;
    return reinterpret_cast<HMODULE>(static_cast<uintptr_t>(1));
}

} // extern "C"

#endif // !_WIN32
