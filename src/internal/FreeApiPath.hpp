#pragma once

#include <string>

namespace FreeApi::Internal {

/**
 * @brief Normalizes a Windows-style path for use with a POSIX filesystem call.
 *
 * Strips a leading drive letter (e.g. "C:"), converts backslashes to forward
 * slashes, and strips leading slashes so a Windows-style rooted path like
 * "\User" is treated as relative to the current working directory instead of
 * escaping to the real filesystem root. Shared by every Free API entry point
 * that creates/opens/removes a file or directory by path (CreateDirectoryA,
 * RemoveDirectoryA, DeleteFileA, _lopen, _mkdir, ...) so path handling stays
 * consistent across all of them.
 */
std::string NormalizeFilesystemPath(const char* path);

std::string BuildCommandLine(int argc, char** argv);

} // namespace FreeApi::Internal
