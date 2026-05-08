/**
 * @file direct.h
 * @brief Minimal MSVC direct.h compatibility: _chdir, _getcwd, _mkdir.
 *
 * Provides inline POSIX wrappers so old Windows code using these CRT
 * directory-manipulation functions compiles and works on Linux/macOS.
 *
 * - _chdir: delegates to POSIX chdir()
 * - _getcwd: delegates to POSIX getcwd()
 * - _mkdir: delegates to POSIX mkdir(path, 0777)
 *
 * @note Status: HEADER_ONLY
 */
#ifndef FREE_API_DIRECT_H
#define FREE_API_DIRECT_H

#if defined(_WIN32)
/*
 * On Windows / MinGW the underlying CRT (msvcrt) already exports
 * _chdir / _getcwd / _mkdir natively. Because our compatibility include
 * directory shadows MinGW's <direct.h>, we must redeclare them here
 * ourselves. They will be resolved at link time against msvcrt.
 */
#  ifdef __cplusplus
extern "C" {
#  endif
int   _chdir(const char* path);
char* _getcwd(char* buffer, int maxlen);
int   _mkdir(const char* path);
#  ifdef __cplusplus
}
#  endif
#else
/* On POSIX systems forward Windows-style names to the POSIX equivalents. */
#  include <unistd.h>
#  include <sys/stat.h>

/** @brief Changes current directory. @note Status: PARTIAL */
static inline int _chdir(const char* path) { return chdir(path); }
/** @brief Gets current working directory. @note Status: PARTIAL */
static inline char* _getcwd(char* buffer, int maxlen) { return getcwd(buffer, maxlen); }
/** @brief Creates a directory with mode 0777. @note Status: PARTIAL */
static inline int _mkdir(const char* path) { return mkdir(path, 0777); }
#endif

#endif // FREE_API_DIRECT_H