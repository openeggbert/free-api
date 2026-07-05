//
// Created by robertvokac on 5/3/26.
//

#ifndef FREE_API_WINDOWS_RPCNDR_H
#define FREE_API_WINDOWS_RPCNDR_H

/**
 * @brief Legacy `byte` alias used by older codebases.
 *
 * A deliberate, narrowly-scoped exception to "don't pollute global macros" --
 * justified by legacy CRT compatibility for code that expects `byte` to
 * exist as a bare identifier (as some old Win32 CRT headers provided), not a
 * general policy of macro-heavy compatibility.
 * @note Status: STUB
 */
#ifndef byte
#define byte BYTE
#endif

#endif //FREE_API_WINDOWS_RPCNDR_H
