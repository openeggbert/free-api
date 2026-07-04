/**
 * @file test_file_regressions.cpp
 * @brief Focused P0 regression tests for file/path behavior both target
 * games (../free-eggbert, ../planetblupi) depend on. See plan.md sections
 * 3.8/6/7.
 *
 * Covers:
 *  - CreateDirectoryA actually creates a directory on disk, with both a
 *    plain forward-slash path and a backslash-containing path (e.g.
 *    free-eggbert/planetblupi's "\User"-style Windows path literals).
 *  - _mkdir actually creates a directory on disk, with both a plain
 *    forward-slash path and a backslash-containing path such as
 *    free-eggbert's own "_mkdir(\"\\User\")" call (event.cpp:4193).
 *  - Backslash-style paths (planetblupi's "image\\init.blp" convention) and
 *    forward-slash paths (its "data/config.def" convention) both resolve
 *    through the fopen wrapper, in the same run.
 */
#include <windows.h>
#include <direct.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>

#if !defined(_WIN32)
#include <unistd.h>
#endif

static int g_failures = 0;

static void Check(bool condition, const char* what)
{
    if (condition) {
        printf("[file-regressions] PASS: %s\n", what);
    } else {
        printf("[file-regressions] FAIL: %s\n", what);
        ++g_failures;
    }
}

static bool DirExists(const std::string& path)
{
    struct stat st{};
    return ::stat(path.c_str(), &st) == 0 && (st.st_mode & S_IFDIR) != 0;
}

static std::string MakeTempRoot(const char* suffix)
{
#if !defined(_WIN32)
    std::string tmpl = std::string("free-api-file-regress-") + suffix + "-XXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    char* dir = mkdtemp(buf.data());
    return dir ? std::string(dir) : std::string();
#else
    (void)suffix;
    return std::string();
#endif
}

static void TestCreateDirectoryACreatesRealDirectory()
{
    std::string root = MakeTempRoot("createdir");
    Check(!root.empty(), "temp root created for CreateDirectoryA test");
    if (root.empty()) return;

    const std::string target = root + "/user_data";
    BOOL created = CreateDirectoryA(target.c_str(), nullptr);
    Check(created == TRUE, "CreateDirectoryA reports success for a fresh directory");
    Check(DirExists(target), "CreateDirectoryA's target directory actually exists on disk");

    // Calling it again on an already-existing directory must still be
    // treated as an acceptable/idempotent outcome (matches ERROR_ALREADY_EXISTS
    // semantics), not crash.
    BOOL createdAgain = CreateDirectoryA(target.c_str(), nullptr);
    (void)createdAgain;
    Check(DirExists(target), "directory still exists after calling CreateDirectoryA on it twice");

    rmdir(target.c_str());
    rmdir(root.c_str());
}

static void TestCreateDirectoryAWithBackslashPath()
{
    std::string root = MakeTempRoot("createdir-bs");
    Check(!root.empty(), "temp root created for CreateDirectoryA backslash test");
    if (root.empty()) return;

    // Windows-style backslash between an already-existing root and a new
    // leaf directory, e.g. the shape of both games' "\User"-style literals.
    const std::string requested = root + "\\user_data";
    const std::string expected  = root + "/user_data";

    BOOL created = CreateDirectoryA(requested.c_str(), nullptr);
    Check(created == TRUE, "CreateDirectoryA reports success for a backslash-containing path");
    Check(DirExists(expected), "CreateDirectoryA normalizes backslashes to the equivalent POSIX directory");

    rmdir(expected.c_str());
    rmdir(root.c_str());
}

static void TestMkdirCreatesRealDirectory()
{
    std::string root = MakeTempRoot("mkdir");
    Check(!root.empty(), "temp root created for _mkdir test");
    if (root.empty()) return;

    const std::string target = root + "/User";
    int rc = _mkdir(target.c_str());
    Check(rc == 0, "_mkdir returns success for a fresh directory");
    Check(DirExists(target), "_mkdir's target directory actually exists on disk");

    rmdir(target.c_str());
    rmdir(root.c_str());
}

static void TestMkdirWithBackslashPath()
{
    std::string root = MakeTempRoot("mkdir-bs");
    Check(!root.empty(), "temp root created for _mkdir backslash test");
    if (root.empty()) return;

    // Matches free-eggbert's own call shape: _mkdir("\User") relative to an
    // existing directory (event.cpp:4193), just rooted at our temp dir
    // instead of the real current working directory.
    const std::string requested = root + "\\User";
    const std::string expected  = root + "/User";

    int rc = _mkdir(requested.c_str());
    Check(rc == 0, "_mkdir returns success for a backslash-containing path");
    Check(DirExists(expected), "_mkdir normalizes backslashes to the equivalent POSIX directory");

    rmdir(expected.c_str());
    rmdir(root.c_str());
}

static void TestBackslashAndForwardSlashPathsBothResolve()
{
    std::string root = MakeTempRoot("paths");
    Check(!root.empty(), "temp root created for path-separator test");
    if (root.empty()) return;

    const std::string subdir = root + "/image";
    Check(::mkdir(subdir.c_str(), 0755) == 0, "subdirectory for path test created");

    const std::string filePath = subdir + "/init.blp";
    FILE* w = fopen(filePath.c_str(), "wb");
    Check(w != nullptr, "fixture file created via plain forward-slash path");
    if (w) {
        fputs("free-api", w);
        fclose(w);
    }

    // Move into the temp root so the game-style relative paths below resolve
    // the same way both games' actual (relative-to-cwd) asset paths do.
#if !defined(_WIN32)
    char oldCwd[4096];
    Check(getcwd(oldCwd, sizeof(oldCwd)) != nullptr, "captured current working directory");
    Check(chdir(root.c_str()) == 0, "chdir into temp root succeeded");
#endif

    // Forward-slash convention, e.g. planetblupi's "data/config.def".
    FILE* forwardSlash = fopen("image/init.blp", "rb");
    Check(forwardSlash != nullptr, "forward-slash path (\"image/init.blp\") opens via fopen");
    if (forwardSlash) fclose(forwardSlash);

    // Backslash convention, e.g. planetblupi's "image\\init.blp" literal.
    FILE* backslash = fopen("image\\init.blp", "rb");
    Check(backslash != nullptr, "backslash path (\"image\\\\init.blp\") opens via fopen wrapper");
    if (backslash) fclose(backslash);

#if !defined(_WIN32)
    chdir(oldCwd);
#endif

    remove(filePath.c_str());
    rmdir(subdir.c_str());
    rmdir(root.c_str());
}

int main()
{
    printf("[file-regressions] Starting\n");

    TestCreateDirectoryACreatesRealDirectory();
    TestCreateDirectoryAWithBackslashPath();
    TestMkdirCreatesRealDirectory();
    TestMkdirWithBackslashPath();
    TestBackslashAndForwardSlashPathsBothResolve();

    if (g_failures > 0) {
        printf("[file-regressions] %d FAILURE(S)\n", g_failures);
        return 1;
    }

    printf("[file-regressions] ALL TESTS PASSED\n");
    return 0;
}
