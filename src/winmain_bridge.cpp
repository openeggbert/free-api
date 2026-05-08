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
 * On modern MinGW/msvcrt we could use _get_pgmptr/_set_pgmptr, but to ensure
 * compatibility with all versions of the runtime and avoid linker issues,
 * we can directly manipulate the exported `_pgmptr` if needed.
 */
DWORD WINAPI GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize);

static char s_pgmptr_buffer[MAX_PATH] = {0};

__attribute__((constructor(101)))
static void free_api_init_pgmptr(void)
{
    /* Use GetModuleFileNameA to find our real path regardless of how we were started. */
    DWORD n = GetModuleFileNameA(NULL, s_pgmptr_buffer, (DWORD)sizeof(s_pgmptr_buffer));
    if (n > 0 && n < sizeof(s_pgmptr_buffer)) {
        s_pgmptr_buffer[n] = '\0';
        /* We can't easily call _set_pgmptr if it's not in the import lib.
         * But we can at least ensure s_pgmptr_buffer is ready. */
    }
}
#else
char* _pgmptr = nullptr;
#endif

int WINAPI FreeApiRunWinMain(FREE_API_WINMAIN_PROC entryPoint, int argc, char** argv)
{
    if (!entryPoint) {
        return -1;
    }

#if defined(_WIN32)
    /* If msvcrt's _pgmptr is empty, try to use our buffer. */
    if (_pgmptr == NULL || _pgmptr[0] == '\0') {
        if (s_pgmptr_buffer[0] != '\0') {
            /* This might still fail if _pgmptr is a macro expanding to (*__p__pgmptr())
             * which is how it's often implemented in MinGW. */
             _pgmptr = s_pgmptr_buffer;
        } else if (argc > 0 && argv && argv[0]) {
            _pgmptr = argv[0];
        }
    }
#else
    if (argc > 0 && argv && argv[0]) {
        _pgmptr = argv[0];
    }
#endif

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
