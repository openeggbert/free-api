#include "internal/FreeApiPath.hpp"

namespace FreeApi::Internal {

std::string NormalizePath(const char* path)
{
    std::string normalized = path ? path : "";
    for (char& ch : normalized) {
        if (ch == '\\') {
            ch = '/';
        }
    }
    return normalized;
}

std::string BuildCommandLine(const int argc, char** argv)
{
    std::string cmdLine;
    for (int i = 1; i < argc; ++i) {
        if (!cmdLine.empty()) {
            cmdLine += ' ';
        }
        if (argv[i]) {
            cmdLine += argv[i];
        }
    }
    return cmdLine;
}

} // namespace FreeApi::Internal
