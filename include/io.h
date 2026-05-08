/**
 * @file io.h
 * @brief Minimal MSVC io.h compatibility: legacy file I/O and file-search stubs.
 *
 * Provides the subset of Windows io.h API used by old C/C++ games:
 * - Legacy low-level file I/O: _lopen, _lread, _lclose (POSIX wrappers, see windows.h)
 * - MSVC file-search API: _findfirst, _findnext, _findclose
 *   These are NOT implemented; they return -1/failure immediately (stub).
 * - _finddata_t structure for use with the above
 * - _MAX_FNAME constant
 *
 * @note Status: PARTIAL
 */
#ifndef FREE_API_IO_H
#define FREE_API_IO_H

#include <windows.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Provide F_OK/R_OK/W_OK/X_OK and access()/_access() on all platforms.
 *
 * On Windows our compatibility <io.h> shadows MinGW's <io.h>, so we must
 * redeclare the CRT symbols ourselves. On non-Windows, the system headers
 * normally provide them, but since our header shadows the system <io.h> on
 * those platforms too (via the compatibility include directory), we provide
 * the constants unconditionally and forward-declare access() for Windows.
 */
#ifndef F_OK
#  define F_OK 0
#endif
#ifndef X_OK
#  define X_OK 1
#endif
#ifndef W_OK
#  define W_OK 2
#endif
#ifndef R_OK
#  define R_OK 4
#endif

#if defined(_WIN32)
/** @brief CRT _access (resolved against msvcrt at link time). */
int _access(const char* path, int mode);
/** @brief POSIX access() wrapper (implemented in crt_io.cpp on Windows). */
int access(const char* path, int mode);
#endif /* _WIN32 */

/** @brief Low-level file open; declared in windows.h. @note Status: PARTIAL */
int WINAPI _lopen(LPCSTR lpPathName, int iReadWrite);
/** @brief Low-level file read; declared in windows.h. @note Status: PARTIAL */
UINT WINAPI _lread(int hFile, LPVOID lpBuffer, UINT uBytes);
/** @brief Low-level file close; declared in windows.h. @note Status: PARTIAL */
int WINAPI _lclose(int hFile);

#ifndef _MAX_FNAME
#define _MAX_FNAME 260
#endif

/**
 * @brief Win32 file search result structure (MSVC CRT compatible).
 * @note Status: HEADER_ONLY
 */
struct _finddata_t {
    unsigned attrib;
    time_t time_create;
    time_t time_access;
    time_t time_write;
    long size;
    char name[_MAX_FNAME];
};

/** @brief Begins a file search by wildcard pattern. @note Status: STUB */
intptr_t _findfirst(const char* filespec, struct _finddata_t* fileinfo);
/** @brief Advances to the next file matching pattern. @note Status: STUB */
int _findnext(intptr_t handle, struct _finddata_t* fileinfo);
/** @brief Closes a file search handle. @note Status: STUB */
int _findclose(intptr_t handle);

#ifdef __cplusplus
}
#endif

#endif // FREE_API_IO_H