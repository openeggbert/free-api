/**
 * @file direct.h
 * @brief Minimal MSVC direct.h compatibility: _chdir, _getcwd, _mkdir.
 *
 * Provides platform-neutral wrappers so old Windows code using these CRT
 * directory-manipulation functions compiles and works on Linux/macOS.
 *
 * On Windows the real CRT symbols are resolved at link time.
 * On non-Windows, the implementations live in free-api/src/crt_direct.cpp
 * to avoid exposing platform-specific headers here.
 *
 * @note Status: PARTIAL
 */
#ifndef FREE_API_DIRECT_H
#define FREE_API_DIRECT_H

#ifdef __cplusplus
extern "C" {
#endif

int   _chdir(const char* path);
char* _getcwd(char* buffer, int maxlen);
int   _mkdir(const char* path);

#ifdef __cplusplus
}
#endif

#endif // FREE_API_DIRECT_H