#pragma once
// free-api compatibility header: sys/timeb.h
// Provides struct timeb and ftime() for platforms where sys/timeb.h is missing or incomplete.

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

#endif // FREE_API_SYS_TIMEB_H
