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
// Moved to individual executable targets to avoid multiple definition errors.
