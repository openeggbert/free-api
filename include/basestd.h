//
// Created by robertvokac on 5/2/26.
//

/**
 * @file basestd.h
 * @brief Pointer-sized integer type aliases (INT_PTR/UINT_PTR/LONG_PTR/
 * ULONG_PTR/DWORD_PTR and their pointer forms), matching real Win32
 * semantics on both 32- and 64-bit targets.
 *
 * @note Status: HEADER_ONLY
 */

#ifndef FREE_API_WINDOWS_BASESTD_H
#define FREE_API_WINDOWS_BASESTD_H

#include <stdint.h>

typedef intptr_t INT_PTR, *PINT_PTR;
typedef uintptr_t UINT_PTR, *PUINT_PTR;
typedef intptr_t LONG_PTR, *PLONG_PTR;
typedef uintptr_t ULONG_PTR, *PULONG_PTR;
typedef uintptr_t DWORD_PTR, *PDWORD_PTR;

#endif //FREE_API_WINDOWS_BASESTD_H
