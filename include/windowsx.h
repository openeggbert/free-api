/**
 * @file windowsx.h
 * @brief Windows message-cracking macros compatibility placeholder (empty stub).
 *
 * The Windows windowsx.h header provides convenience macros for packing and
 * unpacking WM_* message parameters (e.g., GET_X_LPARAM, HANDLE_WM_PAINT).
 * This header exists to allow old code that includes <windowsx.h> to compile.
 * None of those macros are defined here; add them as needed.
 *
 * Proven unused by both target games beyond one macro: `GetStockBrush` is the
 * only real call either game makes through this header (once each, at
 * startup, for `wc.hbrBackground` -- ../free-eggbert blupi.cpp:725,
 * ../planetblupi blupi.cpp:617). `ddutil.cpp`/`movie.cpp` in both games
 * #include this header but call nothing from it (plan.md §3.1) -- kept only
 * so those files compile.
 *
 * @note Status: STUB
 */
#ifndef FREE_API_WINDOWSX_H
#define FREE_API_WINDOWSX_H

#include <windows.h>

extern "C"{
//#66
/** @brief Retrieves stock brush handle. @note Status: STUB */
HBRUSH WINAPI GetStockBrush(int fnObject);
}

#endif // FREE_API_WINDOWSX_H