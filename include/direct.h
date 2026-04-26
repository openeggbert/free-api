#ifndef FREE_API_DIRECT_H
#define FREE_API_DIRECT_H

#include <unistd.h>
#include <sys/stat.h>

// Minimal direct.h compatibility subset - Status: STUB
static inline int _chdir(const char* path) { return chdir(path); }
static inline char* _getcwd(char* buffer, int maxlen) { return getcwd(buffer, maxlen); }
static inline int _mkdir(const char* path) { return mkdir(path, 0777); }

#endif // FREE_API_DIRECT_H