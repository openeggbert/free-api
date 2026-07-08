#include "io.h"
#include "internal/FreeApiPath.hpp"

#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cctype>
#include <algorithm>

namespace {

// TASK-0086: real _findfirst/_findnext/_findclose, scoped only to the
// wildcard shape either target game actually uses -- a directory plus a
// simple "*.ext"-style pattern (free-eggbert event.cpp:4741:
// "\User\*.xch"). Not a general Windows wildcard implementation (no "?",
// no multiple "*").
bool MatchesSimpleWildcard(const std::string& name, const std::string& pattern)
{
    auto toLower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    };
    const std::string lname = toLower(name);
    const std::string lpattern = toLower(pattern);

    const size_t star = lpattern.find('*');
    if (star == std::string::npos) {
        return lname == lpattern; // no wildcard: exact (case-insensitive) match
    }

    const std::string prefix = lpattern.substr(0, star);
    const std::string suffix = lpattern.substr(star + 1);
    if (lname.size() < prefix.size() + suffix.size()) {
        return false;
    }
    return lname.compare(0, prefix.size(), prefix) == 0 &&
           lname.compare(lname.size() - suffix.size(), suffix.size(), suffix) == 0;
}

struct FindSession {
    std::vector<std::string> matches;
    size_t index = 0;
};

std::mutex g_findSessionsMutex;
std::unordered_map<intptr_t, FindSession> g_findSessions;
intptr_t g_nextFindHandle = 1;

void FillFindData(struct _finddata_t* fileinfo, const std::string& name)
{
    memset(fileinfo, 0, sizeof(*fileinfo));
    std::strncpy(fileinfo->name, name.c_str(), sizeof(fileinfo->name) - 1);
}

} // namespace

extern "C" {

intptr_t _findfirst(const char* filespec, struct _finddata_t* fileinfo)
{
    if (fileinfo) {
        memset(fileinfo, 0, sizeof(*fileinfo));
    }
    if (!filespec) {
        return -1;
    }

    const std::string normalized = FreeApi::Internal::NormalizeFilesystemPath(filespec);
    const size_t slash = normalized.rfind('/');
    const std::string dir = (slash == std::string::npos) ? std::string(".") : normalized.substr(0, slash);
    const std::string pattern = (slash == std::string::npos) ? normalized : normalized.substr(slash + 1);

    FindSession session;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        const std::string name = entry.path().filename().string();
        if (MatchesSimpleWildcard(name, pattern)) {
            session.matches.push_back(name);
        }
    }

    if (session.matches.empty()) {
        return -1;
    }

    if (fileinfo) {
        FillFindData(fileinfo, session.matches[0]);
    }
    session.index = 1;

    std::lock_guard<std::mutex> lock(g_findSessionsMutex);
    const intptr_t handle = g_nextFindHandle++;
    g_findSessions[handle] = std::move(session);
    return handle;
}

int _findnext(intptr_t handle, struct _finddata_t* fileinfo)
{
    std::lock_guard<std::mutex> lock(g_findSessionsMutex);
    auto it = g_findSessions.find(handle);
    if (it == g_findSessions.end()) {
        return -1;
    }
    FindSession& session = it->second;
    if (session.index >= session.matches.size()) {
        // TASK-0009/TASK-24H-0701: a real caller has no further use for a
        // handle that can never return another match. free-eggbert's design-
        // mission file picker (event.cpp:4741-4747) drains via this loop and
        // never calls _findclose, which used to leak one session entry per
        // screen visit. Auto-erase on exhaustion instead: a subsequent
        // _findnext/_findclose on this now-erased handle behaves exactly like
        // an unknown handle (returns -1), matching real Win32's contract for
        // an invalid/already-closed handle -- harmless, not a crash.
        g_findSessions.erase(it);
        return -1;
    }
    if (fileinfo) {
        FillFindData(fileinfo, session.matches[session.index]);
    }
    ++session.index;
    return 0;
}

int _findclose(intptr_t handle)
{
    std::lock_guard<std::mutex> lock(g_findSessionsMutex);
    return g_findSessions.erase(handle) > 0 ? 0 : -1;
}

#if defined(_WIN32)
/* POSIX access() — thin wrapper over MinGW/CRT _access().                    */
/* Provided here because our compatibility <io.h> shadows MinGW's <io.h>      */
/* (which would normally expose this via inline). _access itself is exported  */
/* by msvcrt and resolved at link time.                                       */
int access(const char* path, int mode)
{
    return _access(path, mode);
}
#endif

} // extern "C"
