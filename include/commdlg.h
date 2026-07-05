/**
 * @file commdlg.h
 * @brief Common dialogs compatibility placeholder (empty stub).
 *
 * The Windows Common Dialog API (GetOpenFileName, GetSaveFileName, etc.) is not
 * implemented in this layer. This header exists solely to allow legacy code that
 * includes <commdlg.h> to compile without errors. No runtime behavior is provided.
 *
 * Proven unused by both target games at runtime: neither ../free-eggbert's
 * movie.cpp:9 nor ../planetblupi's movie.cpp:6 (the only two #include sites)
 * calls any real Common Dialog API -- the #include exists purely so those
 * files compile (plan.md §3.1).
 *
 * @note Status: STUB
 */
#ifndef FREE_API_COMMDLG_H
#define FREE_API_COMMDLG_H

#include <windows.h>

#endif // FREE_API_COMMDLG_H