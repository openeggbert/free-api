#include "windows.h"
#include "internal/FreeApiPath.hpp"

#include <string>
#if defined(__ANDROID__)
#include <SDL3/SDL.h>
#endif

using namespace FreeApi::Internal;

extern "C" {

#if defined(_WIN32)
/*
 * On Windows we MUST NOT define our own `_pgmptr` symbol: MinGW's <stdlib.h>
 * (transitively included via "windows.h") declares it as a dllimport from
 * msvcrt of type `char**` aliased through `__imp__pgmptr`. Defining a local
 * `char* _pgmptr` would clash with that declaration.
 *
 * msvcrt does populate _pgmptr on its own when CRT startup runs through main,
 * but legacy game code (blupi.cpp) defines its own WinMain and we additionally
 * supply a weak `main` (below). Depending on the link path, _pgmptr can end up
 * empty/NULL by the time misc.cpp's GetCurrentDir runs:
 *     strncpy(pName, _pgmptr, lg-1);
 * which then dereferences NULL and crashes with 0xC0000005.
 *
 * On modern MinGW/msvcrt we could use _get_pgmptr/_set_pgmptr, but to ensure
 * compatibility with all versions of the runtime and avoid linker issues,
 * we can directly manipulate the exported `_pgmptr` if needed.
 */
DWORD WINAPI GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize);

static char s_pgmptr_buffer[MAX_PATH] = {0};

__attribute__((constructor(101)))
static void free_api_init_pgmptr(void)
{
    /* Use GetModuleFileNameA to find our real path regardless of how we were started. */
    DWORD n = GetModuleFileNameA(NULL, s_pgmptr_buffer, (DWORD)sizeof(s_pgmptr_buffer));
    if (n > 0 && n < sizeof(s_pgmptr_buffer)) {
        s_pgmptr_buffer[n] = '\0';
        /* We can't easily call _set_pgmptr if it's not in the import lib.
         * But we can at least ensure s_pgmptr_buffer is ready. */
    }
}
#else
char* _pgmptr = nullptr;
#endif

int WINAPI FreeApiRunWinMain(FREE_API_WINMAIN_PROC entryPoint, int argc, char** argv)
{
#if defined(__ANDROID__)
    SDL_Log("FREEAPI_ANDROID: FreeApiRunWinMain entered entryPoint=%p argc=%d",
            (void*)(uintptr_t)entryPoint, argc);
#endif
    if (!entryPoint) {
#if defined(__ANDROID__)
        SDL_Log("FREEAPI_ANDROID: FreeApiRunWinMain entryPoint is NULL, returning -1");
#endif
        return -1;
    }

#if defined(_WIN32)
    /* If msvcrt's _pgmptr is empty, try to use our buffer. */
    if (_pgmptr == NULL || _pgmptr[0] == '\0') {
        if (s_pgmptr_buffer[0] != '\0') {
            /* This might still fail if _pgmptr is a macro expanding to (*__p__pgmptr())
             * which is how it's often implemented in MinGW. */
             _pgmptr = s_pgmptr_buffer;
        } else if (argc > 0 && argv && argv[0]) {
            _pgmptr = argv[0];
        }
    }
#else
    if (argc > 0 && argv && argv[0]) {
        _pgmptr = argv[0];
    }
#endif

    std::string commandLine = BuildCommandLine(argc, argv);
#if defined(__ANDROID__)
    SDL_Log("FREEAPI_ANDROID: FreeApiRunWinMain calling WinMain entryPoint");
#endif
    int result = entryPoint(NULL, NULL, commandLine.empty() ? NULL : commandLine.data(), SW_SHOW);
#if defined(__ANDROID__)
    SDL_Log("FREEAPI_ANDROID: FreeApiRunWinMain WinMain returned %d", result);
#endif
    return result;
}

} // extern "C"

// WinMain entry bridge for legacy projects without explicit main()
// Wrapped in a weak symbol to allow targets to override it if they define their own main()
#ifndef FREE_API_NO_MAIN
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);

#if defined(__ANDROID__)
// ============================================================
// Android asset extraction
// ============================================================
// On Android all game assets live inside the APK and are only accessible
// via the Android Asset Manager (SDL_IOFromFile).  Regular fopen() with a
// relative path looks in the process working directory, which is NOT the
// APK asset tree.  We therefore extract all listed asset directories to the
// app's internal-storage directory on first launch, then chdir() there so
// that every subsequent fopen(relative) call works transparently.
//
// The list of top-level directories to extract is read from the manifest
// file "freeapi_android_assets.txt" (one directory name per line) that
// every FreeApi game must ship in its APK assets root.
// ============================================================

#include <SDL3/SDL.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <string>
#include <ftw.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

namespace {

// Create `path` and every missing ancestor directory.
// Returns true if the final directory exists after the call.
bool AndroidMkdirAll(const char* path)
{
    char tmp[1024];
    SDL_strlcpy(tmp, path, sizeof(tmp));
    const size_t len = SDL_strlen(tmp);
    for (size_t i = 1; i <= len; ++i) {
        if (tmp[i] == '/' || tmp[i] == '\0') {
            const char saved = tmp[i];
            tmp[i] = '\0';
            int rc = mkdir(tmp, 0755);
            if (rc != 0 && errno != EEXIST) {
                SDL_Log("free-api: mkdir(\"%s\") FAILED errno=%d", tmp, errno);
                tmp[i] = saved;
                return false;
            }
            tmp[i] = saved;
        }
    }
    return true;
}

// nftw callback for recursive directory removal.
static int AndroidNftwRemove(const char* fpath, const struct stat* /*sb*/,
                             int /*typeflag*/, struct FTW* /*ftwbuf*/)
{
    return remove(fpath);
}

// Recursively remove a directory tree.  No-op if it doesn't exist.
static void AndroidRemoveTree(const char* path)
{
    struct stat st{};
    if (stat(path, &st) != 0) return; // doesn't exist, nothing to do
    // nftw with FTW_DEPTH ensures children are visited before parents.
    nftw(path, AndroidNftwRemove, 64, FTW_DEPTH | FTW_PHYS);
}

// Strip trailing '/' characters from a string.
static std::string StripTrailingSlash(const std::string& s)
{
    size_t end = s.size();
    while (end > 0 && s[end - 1] == '/') {
        --end;
    }
    return s.substr(0, end);
}

struct AndroidExtractCtx {
    const char* destRoot; // absolute path to internal storage (not owned)
    int fileCount;
    int dirCount;
    int failCount;
};

// SDL_EnumerateDirectoryCallback that recursively extracts APK assets.
SDL_EnumerationResult AndroidExtractEntry(void* ud,
                                          const char* dirpath,
                                          const char* fname)
{
    auto* ctx = static_cast<AndroidExtractCtx*>(ud);

    // SDL_EnumerateDirectory on Android may pass dirpath with a trailing '/'.
    // Strip it to avoid double-slash paths like "data//config.def" which the
    // Android Asset Manager cannot resolve.
    const std::string cleanDir = StripTrailingSlash(std::string(dirpath));
    const std::string srcPath = cleanDir.empty()
                                    ? std::string(fname)
                                    : cleanDir + "/" + fname;
    const std::string dstPath = std::string(ctx->destRoot) + "/" + srcPath;

    SDL_Log("free-api: trying asset \"%s\" -> \"%s\"", srcPath.c_str(), dstPath.c_str());

    // Try to open the entry as a readable file from APK assets.
    // SDL_IOFromFile on Android first tries internalStorage/srcPath (which
    // does not exist yet on first run), then falls back to the APK Asset
    // Manager.  For a directory the fallback also fails, returning NULL.
    SDL_IOStream* src = SDL_IOFromFile(srcPath.c_str(), "rb");
    if (src) {
        // ---- Regular file: extract to internal storage. ----
        const std::string dstDir = dstPath.substr(0, dstPath.rfind('/'));
        if (!AndroidMkdirAll(dstDir.c_str())) {
            SDL_Log("free-api: cannot create dir \"%s\" for file \"%s\"",
                    dstDir.c_str(), srcPath.c_str());
            SDL_CloseIO(src);
            ctx->failCount++;
            return SDL_ENUM_CONTINUE;
        }

        FILE* out = fopen(dstPath.c_str(), "wb");
        if (out) {
            char buf[8192];
            size_t n;
            size_t totalBytes = 0;
            while ((n = SDL_ReadIO(src, buf, sizeof(buf))) > 0) {
                fwrite(buf, 1, n, out);
                totalBytes += n;
            }
            fclose(out);
            SDL_Log("free-api: extracted %s (%zu bytes)", srcPath.c_str(), totalBytes);
            ctx->fileCount++;
        } else {
            SDL_Log("free-api: cannot write \"%s\" (errno=%d)", dstPath.c_str(), errno);
            ctx->failCount++;
        }
        SDL_CloseIO(src);
    } else {
        // ---- Assume it is a subdirectory: recurse. ----
        SDL_Log("free-api: descending into directory \"%s\"", srcPath.c_str());
        ctx->dirCount++;
        SDL_ClearError();
        if (!SDL_EnumerateDirectory(srcPath.c_str(), AndroidExtractEntry, ud)) {
            SDL_Log("free-api: SDL_EnumerateDirectory(\"%s\") failed: %s",
                    srcPath.c_str(), SDL_GetError());
            ctx->failCount++;
        }
    }
    return SDL_ENUM_CONTINUE;
}

// Return the APK's last-update time (seconds since epoch) or 0 on failure.
// /proc/self/exe on Android is a symlink to the APK path.
long long AndroidGetApkTimestamp()
{
    // Try the standard /data/app path via /proc/self/exe
    struct stat st{};
    if (stat("/proc/self/exe", &st) == 0) {
        return static_cast<long long>(st.st_mtime);
    }
    return 0;
}

void AndroidExtractAssets(const char* internalPath)
{
    // Sentinel contains the APK timestamp from the previous extraction.
    // If the APK has been updated (re-installed), we re-extract.
    const std::string sentinel = std::string(internalPath) + "/.freeapi_extracted";
    const long long apkTs = AndroidGetApkTimestamp();
    SDL_Log("free-api: APK timestamp = %lld", apkTs);

    bool needExtract = true;
    struct stat st{};
    if (stat(sentinel.c_str(), &st) == 0) {
        // Read stored timestamp from sentinel file
        FILE* sf = fopen(sentinel.c_str(), "r");
        if (sf) {
            long long storedTs = 0;
            if (fscanf(sf, "%lld", &storedTs) == 1 && storedTs == apkTs && apkTs != 0) {
                SDL_Log("free-api: sentinel timestamp %lld matches APK, skipping extraction", storedTs);
                needExtract = false;
            } else {
                SDL_Log("free-api: sentinel timestamp mismatch (stored=%lld, apk=%lld), re-extracting", storedTs, apkTs);
            }
            fclose(sf);
        }
    }
    if (!needExtract) {
        // Even when skipping extraction, verify critical file exists
        const std::string configPath = std::string(internalPath) + "/data/config.def";
        struct stat cfgSt{};
        if (stat(configPath.c_str(), &cfgSt) != 0) {
            SDL_Log("free-api: sentinel present but data/config.def missing, forcing re-extraction");
            needExtract = true;
        }
    }
    if (!needExtract) return;

    SDL_Log("free-api: extracting APK assets to %s ...", internalPath);

    // Read the manifest listing which top-level asset directories to extract.
    SDL_IOStream* mf = SDL_IOFromFile("freeapi_android_assets.txt", "rb");
    if (!mf) {
        SDL_Log("free-api: freeapi_android_assets.txt not found in APK assets "
                "(%s) – cannot extract game data", SDL_GetError());
        return;
    }

    const Sint64 size = SDL_GetIOSize(mf);
    if (size <= 0 || size >= 4096) {
        SDL_Log("free-api: manifest size %" SDL_PRIs64 " is out of range", size);
        SDL_CloseIO(mf);
        return;
    }
    char buf[4096];
    buf[SDL_ReadIO(mf, buf, static_cast<size_t>(size))] = '\0';
    SDL_CloseIO(mf);

    // ---- AAssetManager diagnostics ----
    // Get the asset manager to run direct-open tests independent of SDL.
    AAssetManager* amgr = NULL;
    {
        JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();
        jobject activity = (jobject)SDL_GetAndroidActivity();
        if (env && activity) {
            jclass cls = env->GetObjectClass(activity);
            jmethodID mid = env->GetMethodID(cls, "getAssets", "()Landroid/content/res/AssetManager;");
            if (mid) {
                jobject jassets = env->CallObjectMethod(activity, mid);
                if (jassets) {
                    amgr = AAssetManager_fromJava(env, jassets);
                    env->DeleteLocalRef(jassets);
                }
            }
            env->DeleteLocalRef(cls);
            env->DeleteLocalRef(activity);
        }
    }
    SDL_Log("FREEAPI_ANDROID: AAssetManager pointer = %p", (void*)amgr);

    if (amgr) {
        // Direct open tests
        AAsset* a1 = AAssetManager_open(amgr, "data/config.def", AASSET_MODE_STREAMING);
        SDL_Log("FREEAPI_ANDROID: direct open \"data/config.def\" = %s",
                a1 ? "OK" : "FAILED");
        if (a1) AAsset_close(a1);

        AAsset* a2 = AAssetManager_open(amgr, "assets/data/config.def", AASSET_MODE_STREAMING);
        SDL_Log("FREEAPI_ANDROID: direct open \"assets/data/config.def\" = %s",
                a2 ? "OK" : "FAILED");
        if (a2) AAsset_close(a2);

        // Directory listing tests
        const char* testDirs[] = { "", "data", "assets", "assets/data" };
        for (int di = 0; di < 4; ++di) {
            AAssetDir* ad = AAssetManager_openDir(amgr, testDirs[di]);
            SDL_Log("FREEAPI_ANDROID: AAssetManager_openDir(\"%s\") = %p",
                    testDirs[di], (void*)ad);
            if (ad) {
                int count = 0;
                const char* name;
                while ((name = AAssetDir_getNextFileName(ad)) != NULL) {
                    if (count < 20) {
                        SDL_Log("FREEAPI_ANDROID:   entry[%d]: \"%s\"", count, name);
                    }
                    count++;
                }
                SDL_Log("FREEAPI_ANDROID:   total entries: %d", count);
                AAssetDir_close(ad);
            }
        }
    }

    AndroidExtractCtx ctx{ internalPath, 0, 0, 0 };

    // Before extraction, remove previously extracted directories so that
    // SDL_EnumerateDirectory falls through to the APK Asset Manager rather
    // than enumerating the (possibly stale/partial) filesystem copy.
    {
        char* q = buf;
        while (*q) {
            while (*q == '\n' || *q == '\r') { ++q; }
            char* ls = q;
            while (*q && *q != '\n' && *q != '\r') { ++q; }
            if (q > ls) {
                const char sv = *q; *q = '\0';
                const std::string rmPath = std::string(internalPath) + "/" + ls;
                SDL_Log("free-api: removing old extracted dir \"%s\" before re-extraction", rmPath.c_str());
                AndroidRemoveTree(rmPath.c_str());
                *q = sv;
            }
        }
    }

    // Parse line by line, skipping blank lines and CR/LF.
    char* p = buf;
    while (*p) {
        while (*p == '\n' || *p == '\r') { ++p; }
        char* lineStart = p;
        while (*p && *p != '\n' && *p != '\r') { ++p; }
        if (p > lineStart) {
            const char saved = *p;
            *p = '\0';
            const std::string dir(lineStart);
            SDL_Log("free-api: extracting asset dir \"%s\"", dir.c_str());
            // NOTE: Do NOT pre-create the destination directory here!
            // SDL_SYS_EnumerateDirectory on Android resolves relative paths
            // to SDL_GetAndroidInternalStoragePath()/dir.  If that directory
            // exists on the filesystem, opendir() succeeds and SDL enumerates
            // the (empty) filesystem directory instead of the APK assets.
            // The callback creates parent directories on demand when extracting
            // each file, so pre-creation is unnecessary and harmful.
            if (!SDL_EnumerateDirectory(dir.c_str(), AndroidExtractEntry, &ctx)) {
                SDL_Log("free-api: SDL_EnumerateDirectory(\"%s\") failed: %s",
                        dir.c_str(), SDL_GetError());
                ctx.failCount++;
            }
            *p = saved;
        }
    }

    SDL_Log("free-api: extraction totals: %d files, %d dirs, %d failures",
            ctx.fileCount, ctx.dirCount, ctx.failCount);

    // Verify critical files exist before writing sentinel.
    struct stat verifySt{};
    const std::string verifyPath = std::string(internalPath) + "/data/config.def";
    if (stat(verifyPath.c_str(), &verifySt) == 0 && verifySt.st_size > 0) {
        // Write sentinel with APK timestamp so we re-extract when APK changes.
        FILE* sf = fopen(sentinel.c_str(), "w");
        if (sf) {
            fprintf(sf, "%lld\n", apkTs);
            fclose(sf);
        }
        SDL_Log("free-api: asset extraction complete, sentinel written");
    } else {
        SDL_Log("free-api: EXTRACTION FAILED – data/config.def missing or empty after extraction!");
    }
}

void FreeApiAndroidSetup()
{
    SDL_Log("free-api: FreeApiAndroidSetup begin");

    const char* internalPath = SDL_GetAndroidInternalStoragePath();
    if (!internalPath || internalPath[0] == '\0') {
        SDL_Log("free-api: SDL_GetAndroidInternalStoragePath() returned null/empty");
        return;
    }
    SDL_Log("free-api: internal storage path = %s", internalPath);

    // Extract all APK assets listed in the manifest to internal storage.
    AndroidExtractAssets(internalPath);

    // Change CWD so that fopen(relative) resolves inside internal storage.
    if (chdir(internalPath) != 0) {
        SDL_Log("free-api: chdir(\"%s\") failed (errno=%d)", internalPath, errno);
    } else {
        SDL_Log("free-api: CWD set to %s", internalPath);
    }

    // ---- Post-extraction smoke test ----
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
        SDL_Log("FREEAPI_ANDROID: smoke-test CWD = %s", cwd);
    }
    struct stat dirSt{};
    if (stat("data", &dirSt) == 0 && S_ISDIR(dirSt.st_mode)) {
        SDL_Log("FREEAPI_ANDROID: smoke-test data/ directory EXISTS");
    } else {
        SDL_Log("FREEAPI_ANDROID: smoke-test data/ directory MISSING (errno=%d)", errno);
    }
    struct stat cfgSt{};
    if (stat("data/config.def", &cfgSt) == 0) {
        SDL_Log("FREEAPI_ANDROID: smoke-test data/config.def EXISTS size=%lld",
                (long long)cfgSt.st_size);
    } else {
        SDL_Log("FREEAPI_ANDROID: smoke-test data/config.def MISSING (errno=%d)", errno);
    }
    FILE* testF = fopen("data/config.def", "rb");
    if (testF) {
        fseek(testF, 0, SEEK_END);
        long sz = ftell(testF);
        fclose(testF);
        SDL_Log("FREEAPI_ANDROID: smoke-test fopen(data/config.def) OK size=%ld", sz);
    } else {
        SDL_Log("FREEAPI_ANDROID: smoke-test fopen(data/config.def) FAILED errno=%d", errno);
    }
}

} // anonymous namespace

// On Android, SDLActivity loads libmain.so and looks for the exported symbol
// SDL_main (not main).  Define it here so that any game using FreeApi +
// SDLActivity automatically gets the correct Android/SDL entrypoint without
// requiring game-specific bridge code.
extern "C" int SDL_main(int argc, char** argv)
{
    SDL_Log("free-api: SDL_main starting (argc=%d)", argc);
    FreeApiAndroidSetup();
    const int ret = FreeApiRunWinMain(&WinMain, argc, argv);
    SDL_Log("free-api: SDL_main exiting ret=%d SDL_GetError='%s'",
            ret, SDL_GetError());
    return ret;
}

#else
__attribute__((weak))
int main(int argc, char** argv)
{
    return FreeApiRunWinMain(&WinMain, argc, argv);
}
#endif // __ANDROID__
#endif // FREE_API_NO_MAIN
