#pragma once
// free-api compatibility header: sys/timeb.h
// Provides struct timeb and ftime() for platforms where sys/timeb.h is missing or incomplete.
// Real behavior (not a pure forwarder like Windows.h/WinUser.h in this same
// directory) -- used by free-eggbert (blupi.cpp, pixmap.cpp) and blupi_recon
// (src/blupi.cpp) for timing.
//
// @note Status: IMPLEMENTED

#ifndef FREE_API_SYS_TIMEB_H
#define FREE_API_SYS_TIMEB_H

#include <ctime>
#include <chrono>

struct timeb {
    time_t         time;
    unsigned short millitm;
    short          timezone;
    short          dstflag;
};

inline int ftime(struct timeb* tb) {
    if (!tb) return -1;
    auto now = std::chrono::system_clock::now();
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                   now.time_since_epoch());
    tb->time    = static_cast<time_t>(ms.count() / 1000);
    tb->millitm = static_cast<unsigned short>(ms.count() % 1000);
    tb->timezone = 0;
    tb->dstflag  = 0;
    return 0;
}

// The Microsoft C runtime's spellings of the same, as blupi_recon's
// unchanged historical source uses them (src/blupi.cpp:593-608, Benchmark).
struct _timeb {
    time_t         time;
    unsigned short millitm;
    short          timezone;
    short          dstflag;
};

inline void _ftime(struct _timeb* tb) {
    if (!tb) return;
    struct timeb now;
    ftime(&now);
    tb->time     = now.time;
    tb->millitm  = now.millitm;
    tb->timezone = now.timezone;
    tb->dstflag  = now.dstflag;
}

#endif // FREE_API_SYS_TIMEB_H
