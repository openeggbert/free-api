#include "windows.h"
#include "internal/FreeApiPath.hpp"

#include <string>

using namespace FreeApi::Internal;

extern "C" {

#if defined(_WIN32)
/*
 * On Windows we MUST NOT define our own `_pgmptr` symbol: MinGW's <stdlib.h>
 * (transitively included via "windows.h") declares it as a dllimport from
 * msvcrt of type `char**` aliased through `__imp__pgmptr`. Defining a local
 * `char* _pgmptr` would clash with that declaration.
 *
 * msvcrt does populate _pgmptr on its own when CRT startup runs through main,
 * but legacy game code (blupi.cpp) defines its own WinMain and we additionally
 * supply a weak `main` (below). Depending on the link path, _pgmptr can end up
 * empty/NULL by the time misc.cpp's GetCurrentDir runs:
 *     strncpy(pName, _pgmptr, lg-1);
 * which then dereferences NULL and crashes with 0xC0000005.
 *
 * Repair it eagerly via _set_pgmptr() with the absolute module path obtained
 * from GetModuleFileNameA(). Done in a high-priority constructor so it runs
 * before any user code (main/WinMain) that may consume _pgmptr. We only set
 * it if msvcrt left it empty.
 */
DWORD WINAPI GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize);
errno_t _set_pgmptr(const char* _Pgm);
errno_t _get_pgmptr(char** _Value);

static char s_pgmptr_buffer[MAX_PATH] = {0};

__attribute__((constructor(101)))
static void free_api_init_pgmptr(void)
{
    char* current = NULL;
    if (_get_pgmptr(&current) == 0 && current && current[0] != '\0') {
        return; /* msvcrt already populated it */
    }
    DWORD n = GetModuleFileNameA(NULL, s_pgmptr_buffer, (DWORD)sizeof(s_pgmptr_buffer));
    if (n == 0 || n >= sizeof(s_pgmptr_buffer)) {
        return;
    }
    s_pgmptr_buffer[n] = '\0';
    (void)_set_pgmptr(s_pgmptr_buffer);
}
#else
char* _pgmptr = nullptr;
#endif

int WINAPI FreeApiRunWinMain(FREE_API_WINMAIN_PROC entryPoint, int argc, char** argv)
{
    if (!entryPoint) {
        return -1;
    }

    if (argc > 0 && argv && argv[0]) {
#if defined(_WIN32)
        /* Prefer the full module path obtained from the OS; fall back to argv[0]
         * only if the GetModuleFileNameA constructor failed. */
        if (s_pgmptr_buffer[0] == '\0') {
            _pgmptr = argv[0];
        }
#else
        _pgmptr = argv[0];
#endif
    }

    std::string commandLine = BuildCommandLine(argc, argv);
    return entryPoint(NULL, NULL, commandLine.empty() ? NULL : commandLine.data(), SW_SHOW);
}

} // extern "C"

// WinMain entry bridge for legacy projects without explicit main()
// Wrapped in a weak symbol to allow targets to override it if they define their own main()
#ifndef FREE_API_NO_MAIN
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);

__attribute__((weak))
int main(int argc, char** argv)
{
    return FreeApiRunWinMain(&WinMain, argc, argv);
}
#endif
