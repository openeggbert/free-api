#include "platform/PlatformProcessInfo.hpp"

#include <cstdio>
#include <cstring>

namespace FreeApi::Platform {

long ReadRssKB()
{
#if defined(__linux__)
    FILE* f = std::fopen("/proc/self/status", "r");
    if (!f) return 0;
    char line[256];
    long rss = 0;
    while (std::fgets(line, sizeof(line), f)) {
        if (std::strncmp(line, "VmRSS:", 6) == 0) {
            std::sscanf(line + 6, "%ld", &rss);
            break;
        }
    }
    std::fclose(f);
    return rss;
#else
    return 0;
#endif
}

} // namespace FreeApi::Platform
