//
// Created by robertvokac on 5/6/26.
//

/**
 * @file winerror.h
 * @brief HRESULT/Win32 error-code constants: E_FAIL, ERROR_ALREADY_EXISTS,
 * ERROR_INVALID_PARAMETER.
 *
 * @note Status: HEADER_ONLY
 */

#ifndef FREE_API_WINDOWS_WINERROR_H
#define FREE_API_WINDOWS_WINERROR_H

//#8114
#ifndef E_FAIL
#define E_FAIL ((HRESULT)0x80004005L)
#endif

#ifndef ERROR_ALREADY_EXISTS
#define ERROR_ALREADY_EXISTS 183L
#endif

#ifndef ERROR_INVALID_PARAMETER
#define ERROR_INVALID_PARAMETER 87L
#endif

#endif //FREE_API_WINDOWS_WINERROR_H
