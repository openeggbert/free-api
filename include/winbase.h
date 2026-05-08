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
/** @brief Calls remove() directly; no backslash normalization. @note Status: PARTIAL */
BOOL WINAPI DeleteFileA(LPCSTR lpFileName);

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
