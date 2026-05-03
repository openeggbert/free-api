//
// Created by robertvokac on 5/3/26.
//

#ifndef FREE_API_WINDOWS_DEBUGAPI_H
#define FREE_API_WINDOWS_DEBUGAPI_H

#include <minwindef.h>
#include <winnt.h>

/** @brief Writes debug text to the compatibility logger (ANSI). @note Status: IMPLEMENTED */
void WINAPI OutputDebugStringA(LPCSTR lpOutputString);
/** @brief Writes debug text to the compatibility logger (wide). @note Status: IMPLEMENTED */
void WINAPI OutputDebugStringW(LPCWSTR lpOutputString);

#ifdef UNICODE
#define OutputDebugString OutputDebugStringW
#else
#define OutputDebugString OutputDebugStringA
#endif

#endif //FREE_API_WINDOWS_DEBUGAPI_H
