/**
 * @file wtypes.h
 * @brief Minimal OLE/WTypes compatibility subset.
 *
 * Provides a handful of OLE/Automation types required by the game source:
 * - VARTYPE (variant type tag for VARIANT/VARIANTARG)
 * - SCODE (COM status code, equivalent to HRESULT)
 * - DATE (OLE date/time as double)
 * - CLIPFORMAT (clipboard format identifier)
 * - byte alias macro (expected by some Win32 CRT headers)
 *
 * These types are declared to compile but are not functionally used at runtime;
 * OLE/COM is not implemented in this compatibility layer.
 *
 * Proven unused by both target games at runtime: only ../free-eggbert
 * includes this header (misc.hpp:5, blupi.cpp:14; ../planetblupi does not
 * include it at all) and none of these types are ever exercised as real
 * OLE/COM behavior -- they exist purely so free-eggbert's source compiles
 * (plan.md §3.1).
 *
 * @note Status: STUB
 */
#ifndef FREE_API_WTYPES_H
#define FREE_API_WTYPES_H

#include <windows.h>

/** @brief OLE variant type tag. @note Status: STUB */
typedef unsigned short VARTYPE;
/** @brief COM status code; synonym for HRESULT. @note Status: STUB */
typedef long SCODE;
/** @brief OLE date/time (days since 30 Dec 1899 as double). @note Status: STUB */
typedef double DATE;
/** @brief Clipboard format identifier. @note Status: STUB */
typedef WORD CLIPFORMAT;

/** @brief Legacy Win32 `byte` alias expected by some old game source files. @note Status: STUB */
#ifndef byte
#define byte BYTE
#endif

#endif // FREE_API_WTYPES_H