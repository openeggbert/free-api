#include "windows.h"
#include "internal/FreeApiPath.hpp"

#include <string>

using namespace FreeApi::Internal;

extern "C" {

char* _pgmptr = nullptr;

int WINAPI FreeApiRunWinMain(FREE_API_WINMAIN_PROC entryPoint, int argc, char** argv)
{
    if (!entryPoint) {
        return -1;
    }

    if (argc > 0 && argv && argv[0]) {
        _pgmptr = argv[0];
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
