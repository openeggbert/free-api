//
// Created by robertvokac on 5/3/26.
//

#ifndef FREE_API_WINDOWS_WINBASE_H
#define FREE_API_WINDOWS_WINBASE_H

#include <minwindef.h>
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

#endif //FREE_API_WINDOWS_WINBASE_H
