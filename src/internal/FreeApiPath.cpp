#include "internal/FreeApiPath.hpp"

#include <cctype>

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

std::string NormalizeFilesystemPath(const char* path)
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
