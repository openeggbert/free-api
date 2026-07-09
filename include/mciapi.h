//
// Created by robertvokac on 5/3/26.
//

/**
 * @file mciapi.h
 * @brief MCIDEVICEID typedef, kept standalone-compilable for real Win32
 * header compatibility. Not included by either target game -- mmsystem.h
 * (actually included by both) is the canonical definition site; see the
 * guard comment below.
 *
 * @note Status: HEADER_ONLY
 */

#ifndef FREE_API_WINDOWS_MCIAPI_H
#define FREE_API_WINDOWS_MCIAPI_H
#include <minwindef.h>

// TASK-24H-0104/0905: not currently included by either target game (only a
// commented-out reference exists in ../free-eggbert/src/movie.cpp) or by
// any other free-api header -- mmsystem.h (actually used by both games) is
// the canonical definition site for MCIDEVICEID. Pulling it in here keeps
// this header self-contained/compilable standalone while still sharing one
// definition via the guard, rather than redefining the typedef.
#include <mmsystem.h>

#ifndef FREE_API_MCIDEVICEID_DEFINED
#define FREE_API_MCIDEVICEID_DEFINED
typedef UINT MCIDEVICEID;
#endif

#endif //FREE_API_WINDOWS_MCIAPI_H
