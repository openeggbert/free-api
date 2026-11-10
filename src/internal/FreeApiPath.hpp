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

/**
 * @brief Finds an existing regular file at `relative` under the directory
 * `root`, matching each path component ignoring ASCII case on non-Windows
 * hosts (as NormalizeFilesystemPath does for the working directory).
 *
 * `relative` uses forward slashes, as NormalizeFilesystemPath returns it; a
 * ".." component never leaves `root` (no match). Returns the file's path, or
 * an empty string. Used by LoadImageA's image overlay (docs/image-overlay.md).
 */
std::string FindFileUnderRoot(const std::string& root, const std::string& relative);

std::string BuildCommandLine(int argc, char** argv);

} // namespace FreeApi::Internal
