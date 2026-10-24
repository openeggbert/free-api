#include "internal/FreeApiPath.hpp"

#include <cctype>
#include <filesystem>
#include <system_error>
#include <vector>

namespace FreeApi::Internal {

namespace {

// Win32 resolves "." and ".." textually (GetFullPathName), so "E:\US\..\x"
// names "E:\x" even when "US" does not exist; POSIX needs every directory on
// the way. A rooted path's ".." at the root stays at the root, as on Windows
// (and so cannot climb out of the working directory either). Call site:
// blupi_recon src/misc.cpp:163 (AddCDPath, "<CD path>..\image16\...").
std::string CollapseDotSegments(const std::string& path, bool rooted)
{
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= path.size()) {
        size_t end = path.find('/', start);
        if (end == std::string::npos) end = path.size();
        std::string part = path.substr(start, end - start);
        if (part == "..") {
            if (!parts.empty() && parts.back() != "..") {
                parts.pop_back();
            } else if (!rooted) {
                parts.push_back(part);
            }
        } else if (!part.empty() && part != ".") {
            parts.push_back(part);
        }
        start = end + 1;
    }
    std::string result;
    for (const std::string& part : parts) {
        if (!result.empty()) result += '/';
        result += part;
    }
    if (!path.empty() && path.back() == '/' && !result.empty()) result += '/';
    return result;
}

#if !defined(_WIN32)
bool EqualsIgnoringAsciiCase(const std::string& a, const std::string& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

// Win32 file systems ignore case; the games spell their folders and files
// in either case ("data\world001.blp" for DATA/WORLD001.BLP). Each component
// that does not exist as spelled is replaced by the directory entry that
// matches it ignoring ASCII case; from the first component that matches
// nothing, the rest stays as given (a file about to be created). Call site:
// blupi_recon src/decor.cpp:288 (CDecor::GetMissionPath, "data\world%.3d.blp").
std::string ResolveCaseInsensitive(const std::string& path)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (path.empty() || fs::exists(fs::path(path), ec)) return path;

    std::string resolved;
    size_t start = 0;
    while (start < path.size()) {
        size_t end = path.find('/', start);
        if (end == std::string::npos) end = path.size();
        std::string part = path.substr(start, end - start);
        std::string candidate = resolved.empty() ? part : resolved + "/" + part;
        if (!part.empty() && part != "." && part != ".." && !fs::exists(fs::path(candidate), ec)) {
            bool found = false;
            const fs::path dir = resolved.empty() ? fs::path(".") : fs::path(resolved);
            for (fs::directory_iterator it(dir, ec), last; !ec && it != last; it.increment(ec)) {
                const std::string name = it->path().filename().string();
                if (EqualsIgnoringAsciiCase(name, part)) {
                    candidate = resolved.empty() ? name : resolved + "/" + name;
                    found = true;
                    break;
                }
            }
            if (!found) {
                return candidate + path.substr(end);
            }
        }
        resolved = candidate;
        start = end + 1;
    }
    if (path.back() == '/') resolved += '/';
    return resolved;
}
#endif

} // namespace

std::string NormalizeFilesystemPath(const char* path)
{
    if (!path) {
        return {};
    }

    std::string result(path);
    bool rooted = false;

    if (result.size() >= 2 &&
        std::isalpha(static_cast<unsigned char>(result[0])) &&
        result[1] == ':') {
        result.erase(0, 2);
        rooted = true;
    }

    for (char& c : result) {
        if (c == '\\') {
            c = '/';
        }
    }

    while (!result.empty() && result.front() == '/') {
        result.erase(result.begin());
        rooted = true;
    }

    if (result.find("..") != std::string::npos || result.find("/./") != std::string::npos ||
        result.rfind("./", 0) == 0) {
        result = CollapseDotSegments(result, rooted);
    }

#if !defined(_WIN32)
    result = ResolveCaseInsensitive(result);
#endif

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
