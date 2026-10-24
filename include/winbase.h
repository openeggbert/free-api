//
// Created by robertvokac on 5/3/26.
//

#ifndef FREE_API_WINDOWS_WINBASE_H
#define FREE_API_WINDOWS_WINBASE_H

#include <minwindef.h>

//#97
#ifndef ZeroMemory
#define ZeroMemory(Destination, Length) memset((Destination), 0, (Length))
#endif

#ifndef FillMemory
#define FillMemory(Destination, Length, Fill) memset((Destination), (Fill), (Length))
#endif

#ifndef CopyMemory
#define CopyMemory(Destination, Source, Length) memmove((Destination), (Source), (Length))
#endif

//#223
/** @brief Security attributes for directory creation. @note Status: PARTIAL */
typedef struct _SECURITY_ATTRIBUTES {
    DWORD nLength;
    LPVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

/**
 * @brief Legacy memory status structure filled by `GlobalMemoryStatus`.
 * @note Status: PARTIAL
 */
typedef struct _MEMORYSTATUS {
    DWORD dwLength;
    DWORD dwMemoryLoad;
    DWORD dwTotalPhys;
    DWORD dwAvailPhys;
    DWORD dwTotalPageFile;
    DWORD dwAvailPageFile;
    DWORD dwTotalVirtual;
    DWORD dwAvailVirtual;
} MEMORYSTATUS, *LPMEMORYSTATUS;

/**
 * @brief Reports process-visible memory statistics.
 *
 * Values are compatibility approximations and should not be treated as exact
 * physical memory telemetry.
 * @note Status: PARTIAL
 */
void WINAPI GlobalMemoryStatus(LPMEMORYSTATUS lpBuffer);

//#922
#define OF_READ 0x0000

extern "C" {
//#1052
/** @brief Releases resource handle. @note Status: STUB */
BOOL WINAPI FreeResource(HGLOBAL hResData);

//#1059
/** @brief Locks resource memory. @note Status: STUB */
LPVOID WINAPI LockResource(HGLOBAL hResData);

//#1063
/** @brief Unlocks resource memory. @note Status: STUB */
BOOL WINAPI UnlockResource(HGLOBAL hResData);

//#2261
/** @brief Loads resource block handle. @note Status: STUB */
HGLOBAL WINAPI LoadResource(HMODULE hModule, HRSRC hResInfo);

//#2270
/** @brief Returns resource size in bytes. @note Status: STUB */
DWORD WINAPI SizeofResource(HMODULE hModule, HRSRC hResInfo);

//#3485
/** @brief Calls POSIX open(O_RDONLY); ignores iReadWrite mode. @note Status: PARTIAL */
int WINAPI _lopen(LPCSTR lpPathName, int iReadWrite);

//#3501
/** @brief Calls POSIX read(); returns byte count. @note Status: PARTIAL */
UINT WINAPI _lread(int hFile, LPVOID lpBuffer, UINT uBytes);

//#3537
/** @brief Calls POSIX close(). @note Status: PARTIAL */
int WINAPI _lclose(int hFile);

//#4227
/** @brief Returns current module handle. @note Status: STUB */
HMODULE WINAPI GetModuleHandleA(LPCSTR lpModuleName);

//#4442
/** @brief Finds an embedded resource. @note Status: STUB */
HRSRC WINAPI FindResourceA(HMODULE hModule, LPCSTR lpName, LPCSTR lpType);
}

//#5258
/**
 * @brief Creates a directory; security descriptor is ignored.
 *
 * Normalizes Windows-style paths (removes drive letter, converts backslashes,
 * strips leading slashes). Calls mkdir(path, 0755). Treats EEXIST as success.
 * Does not recursively create missing parent directories.
 * @note Status: PARTIAL
 */
BOOL WINAPI CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);

//#5441
/** @brief Removes a directory. @note Status: PARTIAL */
BOOL WINAPI RemoveDirectoryA(LPCSTR lpPathName);

//#5509
/** @brief Removes a file; the path is normalized like every other Free API path. @note Status: PARTIAL */
BOOL WINAPI DeleteFileA(LPCSTR lpFileName);

/**
 * @name Drive types (real Win32 values)
 * @note Status: IMPLEMENTED
 */
/** @{ */
#define DRIVE_UNKNOWN     0
#define DRIVE_NO_ROOT_DIR 1
#define DRIVE_REMOVABLE   2
#define DRIVE_FIXED       3
#define DRIVE_REMOTE      4
#define DRIVE_CDROM       5
#define DRIVE_RAMDISK     6
/** @} */

/**
 * @brief Reports every drive as `DRIVE_FIXED`.
 *
 * Free API maps every drive letter onto the working directory (paths lose
 * their drive, see FreeApiPath.hpp), so there is no CD-ROM drive. A game
 * that insists on one behaves as the original did without its CD: Speedy
 * Blupi 1.0 (blupi_recon src/blupi.cpp:145, ReadConfig) then starts only
 * with its own `-nocd` switch.
 * @note Status: PARTIAL
 */
UINT WINAPI GetDriveTypeA(LPCSTR lpRootPathName);

/**
 * @brief Returns "C:\" (3 characters).
 *
 * The working directory is the root of every drive here, so that is its
 * name; a path built from it ("C:\..\data\x.blp") normalizes back to one
 * relative to the working directory. Returns the required size, terminator
 * included, when `nBufferLength` is too small. Call site: blupi_recon
 * src/blupi.cpp:110 (ReadConfig, Speedy Eggbert builds).
 * @note Status: PARTIAL
 */
DWORD WINAPI GetCurrentDirectoryA(DWORD nBufferLength, LPSTR lpBuffer);

//#6101
/** @brief Sets an environment variable. @note Status: PARTIAL */
BOOL WINAPI SetEnvironmentVariableA(LPCSTR lpName, LPCSTR lpValue);

//#6234
/** @brief Returns the last error code for the current thread. @note Status: PARTIAL */
DWORD WINAPI GetLastError(void);

//#6245
/** @brief Sets the last error code for the current thread. @note Status: PARTIAL */
void WINAPI SetLastError(DWORD dwErrCode);

#endif //FREE_API_WINDOWS_WINBASE_H
