/**
 * @file test_file_paths.cpp
 * @brief Consolidated file/path regression tests using real, evidence-derived
 * game asset path shapes, per plan.md TASK-0082/0088/0115.
 *
 * Each path string below is copied verbatim (or with the sprintf format
 * applied) from the real games' own source, not invented:
 *  - "data/config.def" via `fopen` (free-eggbert blupi.cpp:102).
 *  - "image\\init.blp" via `LoadImageA` (planetblupi/free-eggbert asset
 *    loading convention; also covered for plain fopen in
 *    test_file_regressions.cpp's TestBackslashAndForwardSlashPathsBothResolve).
 *  - "sound\\sound%.3d.blp" via `fopen` (free-eggbert sound.cpp:440).
 *  - "\\User\\*.xch" via `_findfirst` (free-eggbert event.cpp:4741).
 *
 * TASK-0082: the case-insensitive fopen fallback (include/windows.h's
 * free_api_fopen wrapper) is already tested for one MCI/MIDI scenario
 * (tests/basic_test.cpp); this file extends that coverage to a plain,
 * non-MIDI asset file.
 *
 * TASK-0086: `_findfirst`/`_findnext`/`_findclose` (src/crt_io.cpp) are now
 * real, `std::filesystem`-backed implementations, scoped to the one
 * wildcard shape free-eggbert's design-file picker actually uses (a
 * directory plus a simple "*.ext" pattern) -- tested below both for a
 * missing directory (returns -1, no crash) and for real matching files in
 * a real directory.
 */
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>

#if !defined(_WIN32)
#include <unistd.h>
#endif

// TASK-24H-0703: forward-declares the exact same internal symbol
// (src/internal/FreeApiPath.hpp) already used the same way by
// tests/test_gdi_regressions.cpp's g_diagCompatDcs/g_diagCompatBitmaps
// pattern -- not part of any public header, referenced here only to
// characterize NormalizeFilesystemPath's exact current behavior directly
// (see TestNormalizeFilesystemPathCharacterization below), independent of
// whichever public entry point (LoadImageA, _mkdir, DeleteFileA, ...)
// happens to call it.
namespace FreeApi::Internal {
std::string NormalizeFilesystemPath(const char* path);
}

static int g_failures = 0;

static void Check(bool condition, const char* what)
{
    if (condition) {
        printf("[file-paths] PASS: %s\n", what);
    } else {
        printf("[file-paths] FAIL: %s\n", what);
        ++g_failures;
    }
}

static std::string MakeTempRoot(const char* suffix)
{
#if !defined(_WIN32)
    std::string tmpl = std::string("free-api-file-paths-") + suffix + "-XXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    char* dir = mkdtemp(buf.data());
    return dir ? std::string(dir) : std::string();
#else
    (void)suffix;
    return std::string();
#endif
}

// TASK-0082: fopen's case-insensitive basename fallback, for a plain
// (non-MIDI) asset file -- basic_test.cpp already covers this for the MCI
// MIDI-open path; this covers the generic fopen wrapper directly.
static void TestFopenCaseInsensitiveFallbackForPlainAssetFile()
{
    std::string root = MakeTempRoot("config");
    Check(!root.empty(), "temp root created for the fopen case-fallback test");
    if (root.empty()) return;

    const std::string subdir = root + "/data";
    Check(::mkdir(subdir.c_str(), 0755) == 0, "data/ subdirectory created");

    // File actually exists with an uppercase basename on disk...
    const std::string actualPath = subdir + "/CONFIG.DEF";
    FILE* w = fopen(actualPath.c_str(), "wb");
    Check(w != nullptr, "CONFIG.DEF fixture file created");
    if (w) {
        fputs("free-api", w);
        fclose(w);
    }

#if !defined(_WIN32)
    char oldCwd[4096];
    Check(getcwd(oldCwd, sizeof(oldCwd)) != nullptr, "captured current working directory");
    Check(chdir(root.c_str()) == 0, "chdir into temp root succeeded");
#endif

    // ...but both games request it in lowercase, matching free-eggbert's
    // real literal `fopen("data/config.def", "rb")` (blupi.cpp:102).
    FILE* f = fopen("data/config.def", "rb");
    Check(f != nullptr, "fopen(\"data/config.def\") succeeds via the case-insensitive basename fallback");
    if (f) fclose(f);

#if !defined(_WIN32)
    chdir(oldCwd);
#endif

    remove(actualPath.c_str());
    rmdir(subdir.c_str());
    rmdir(root.c_str());
}

// LoadImageA with the real "image\\init.blp" backslash path shape.
static std::vector<uint8_t> MakeMinimalBmp(int width, int height)
{
    const int rowBytes = ((width * 3 + 3) / 4) * 4;
    const int pixelDataSize = rowBytes * height;
    const int dataOffset = 14 + 40;
    const int fileSize = dataOffset + pixelDataSize;

    std::vector<uint8_t> bmp(static_cast<size_t>(fileSize), 0);
    auto put16 = [&](size_t off, uint16_t v) { bmp[off] = v & 0xFF; bmp[off + 1] = (v >> 8) & 0xFF; };
    auto put32 = [&](size_t off, uint32_t v) {
        bmp[off] = v & 0xFF; bmp[off + 1] = (v >> 8) & 0xFF;
        bmp[off + 2] = (v >> 16) & 0xFF; bmp[off + 3] = (v >> 24) & 0xFF;
    };
    bmp[0] = 'B'; bmp[1] = 'M';
    put32(2, static_cast<uint32_t>(fileSize));
    put32(10, static_cast<uint32_t>(dataOffset));
    put32(14, 40);
    put32(18, static_cast<uint32_t>(width));
    put32(22, static_cast<uint32_t>(height));
    put16(26, 1);
    put16(28, 24);
    put32(30, 0);
    put32(34, static_cast<uint32_t>(pixelDataSize));
    for (int i = 0; i < pixelDataSize; ++i) {
        bmp[static_cast<size_t>(dataOffset + i)] = static_cast<uint8_t>(0x30 + (i % 64));
    }
    return bmp;
}

static void TestLoadImageAWithBackslashInitBlpPathShape()
{
    std::string root = MakeTempRoot("image");
    Check(!root.empty(), "temp root created for the LoadImageA path-shape test");
    if (root.empty()) return;

    const std::string subdir = root + "/image";
    Check(::mkdir(subdir.c_str(), 0755) == 0, "image/ subdirectory created");

    const std::string actualPath = subdir + "/init.blp";
    std::vector<uint8_t> bmp = MakeMinimalBmp(4, 4);
    FILE* f = fopen(actualPath.c_str(), "wb");
    Check(f != nullptr, "init.blp fixture file created");
    if (f) {
        fwrite(bmp.data(), 1, bmp.size(), f);
        fclose(f);
    }

#if !defined(_WIN32)
    char oldCwd[4096];
    getcwd(oldCwd, sizeof(oldCwd));
    Check(chdir(root.c_str()) == 0, "chdir into temp root succeeded");
#endif

    // Real literal path shape, backslash-separated (planetblupi/free-eggbert
    // asset convention).
    HANDLE h = LoadImageA(nullptr, "image\\init.blp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    Check(h != nullptr, "LoadImageA(\"image\\\\init.blp\") succeeds via backslash-path normalization");
    if (h) DeleteObject(reinterpret_cast<HBITMAP>(h));

#if !defined(_WIN32)
    chdir(oldCwd);
#endif

    remove(actualPath.c_str());
    rmdir(subdir.c_str());
    rmdir(root.c_str());
}

// fopen with the real sprintf-formatted "sound\\sound%.3d.blp" path shape
// (free-eggbert sound.cpp:440).
static void TestFopenWithSprintfFormattedSoundPathShape()
{
    std::string root = MakeTempRoot("sound");
    Check(!root.empty(), "temp root created for the sound-path-shape test");
    if (root.empty()) return;

    const std::string subdir = root + "/sound";
    Check(::mkdir(subdir.c_str(), 0755) == 0, "sound/ subdirectory created");

    char name[64];
    snprintf(name, sizeof(name), "sound%.3d.blp", 1);
    const std::string actualPath = subdir + "/" + name;
    FILE* w = fopen(actualPath.c_str(), "wb");
    Check(w != nullptr, "sound001.blp fixture file created");
    if (w) {
        fputs("free-api", w);
        fclose(w);
    }

#if !defined(_WIN32)
    char oldCwd[4096];
    getcwd(oldCwd, sizeof(oldCwd));
    Check(chdir(root.c_str()) == 0, "chdir into temp root succeeded");
#endif

    char requestedPath[64];
    snprintf(requestedPath, sizeof(requestedPath), "sound\\sound%.3d.blp", 1);
    FILE* f = fopen(requestedPath, "rb");
    Check(f != nullptr, "fopen of the exact sprintf-formatted \"sound\\\\sound%.3d.blp\" shape succeeds");
    if (f) fclose(f);

#if !defined(_WIN32)
    chdir(oldCwd);
#endif

    remove(actualPath.c_str());
    rmdir(subdir.c_str());
    rmdir(root.c_str());
}

// _findfirst with the real "\User\*.xch" path shape (free-eggbert
// event.cpp:4741). _findfirst is a permanent stub (TASK-0086) that always
// returns -1; this locks in that documented, current behavior rather than
// assuming success.
static void TestFindFirstWithUserXchPathShapeReturnsMinusOneForMissingDirectory()
{
    // "\User" does not exist in this test's working directory -- real
    // std::filesystem-backed enumeration (TASK-0086) correctly reports no
    // matches for a nonexistent directory, matching the same -1 return the
    // old permanent stub gave, but for the real reason now.
    struct _finddata_t fileinfo{};
    intptr_t handle = _findfirst("\\User\\*.xch", &fileinfo);
    Check(handle == -1, "_findfirst(\"\\\\User\\\\*.xch\") returns -1 when the directory doesn't exist, not a crash");
}

// TASK-0086: _findfirst/_findnext/_findclose now do a real directory
// listing, scoped to the one wildcard shape free-eggbert's design-file
// picker actually uses: a directory plus a simple "*.ext" pattern
// (event.cpp:4741, "\User\*.xch").
static void TestFindFirstFindNextEnumerateRealMatchingFiles()
{
    std::string root = MakeTempRoot("findfirst");
    Check(!root.empty(), "temp root created for the _findfirst test");
    if (root.empty()) return;

    const std::string subdir = root + "/User";
    Check(::mkdir(subdir.c_str(), 0755) == 0, "User/ subdirectory created");

    const char* names[] = {"save1.xch", "save2.xch", "other.txt"};
    for (const char* name : names) {
        FILE* f = fopen((subdir + "/" + name).c_str(), "wb");
        Check(f != nullptr, "fixture file created");
        if (f) { fputs("x", f); fclose(f); }
    }

#if !defined(_WIN32)
    char oldCwd[4096];
    getcwd(oldCwd, sizeof(oldCwd));
    Check(chdir(root.c_str()) == 0, "chdir into temp root succeeded");
#endif

    struct _finddata_t fileinfo{};
    intptr_t handle = _findfirst("\\User\\*.xch", &fileinfo);
    Check(handle != -1, "_findfirst(\"\\\\User\\\\*.xch\") finds matching files in a real directory");

    int count = 0;
    bool sawSave1 = false, sawSave2 = false, sawOther = false;
    if (handle != -1) {
        do {
            ++count;
            std::string name(fileinfo.name);
            if (name == "save1.xch") sawSave1 = true;
            if (name == "save2.xch") sawSave2 = true;
            if (name == "other.txt") sawOther = true;
        } while (_findnext(handle, &fileinfo) == 0);
        _findclose(handle);
    }

    Check(count == 2, "_findfirst/_findnext enumerate exactly the 2 files matching \"*.xch\" (not the 3rd, non-matching file)");
    Check(sawSave1 && sawSave2 && !sawOther,
          "_findfirst/_findnext return the correct matching filenames and exclude the non-matching one");

#if !defined(_WIN32)
    chdir(oldCwd);
#endif

    for (const char* name : names) {
        remove((subdir + "/" + name).c_str());
    }
    rmdir(subdir.c_str());
    rmdir(root.c_str());
}

// TASK-0009/TASK-24H-0701/0702: free-eggbert's design-mission file picker
// (event.cpp:4741-4747) drains via _findnext in a do-while loop exactly like
// TestFindFirstFindNextEnumerateRealMatchingFiles above, but unlike that
// test, the real game never calls _findclose afterward -- previously this
// leaked one FindSession entry (in g_findSessions, src/crt_io.cpp) per
// screen visit, permanently, since only _findclose ever erased an entry.
// _findnext's exhaustion branch now auto-erases the session itself, so a
// caller that never calls _findclose no longer leaks anything. The session
// table is file-local (anonymous namespace in crt_io.cpp), so this is
// verified indirectly through the public handle contract: once a session is
// auto-erased on exhaustion, a subsequent _findclose on that same handle
// must report "nothing to close" (-1) rather than successfully erasing an
// entry that was already gone -- exactly matching real Win32's contract for
// an already-closed/unknown handle. Repeated across several drain cycles to
// confirm this isn't a one-off coincidence.
static void TestFindFirstFindNextRepeatedDrainWithoutCloseDoesNotLeakSession()
{
    std::string root = MakeTempRoot("findfirst_leak");
    Check(!root.empty(), "temp root created for the _findfirst leak-regression test");
    if (root.empty()) return;

    const std::string subdir = root + "/User";
    Check(::mkdir(subdir.c_str(), 0755) == 0, "User/ subdirectory created for leak-regression test");

    const char* names[] = {"save1.xch", "save2.xch"};
    for (const char* name : names) {
        FILE* f = fopen((subdir + "/" + name).c_str(), "wb");
        Check(f != nullptr, "leak-regression fixture file created");
        if (f) { fputs("x", f); fclose(f); }
    }

#if !defined(_WIN32)
    char oldCwd[4096];
    getcwd(oldCwd, sizeof(oldCwd));
    Check(chdir(root.c_str()) == 0, "chdir into temp root succeeded for leak-regression test");
#endif

    bool allAutoErased = true;
    const int kDrainCycles = 5;
    for (int cycle = 0; cycle < kDrainCycles; ++cycle) {
        struct _finddata_t fileinfo{};
        intptr_t handle = _findfirst("\\User\\*.xch", &fileinfo);
        if (handle == -1) { allAutoErased = false; continue; }

        // Drain fully via _findnext without ever calling _findclose --
        // matching free-eggbert's real, leak-inducing call shape exactly.
        while (_findnext(handle, &fileinfo) == 0) { /* keep draining */ }

        // The session should already be auto-erased at this point (the last
        // _findnext call hit exhaustion). A caller-side _findclose on this
        // now-stale handle must find nothing left to close.
        if (_findclose(handle) != -1) {
            allAutoErased = false;
        }
    }

    Check(allAutoErased,
          "repeated _findfirst/_findnext drain-without-_findclose cycles auto-erase their session on exhaustion "
          "(a post-drain _findclose finds nothing left to close) instead of leaking one entry per cycle");

#if !defined(_WIN32)
    chdir(oldCwd);
#endif

    for (const char* name : names) {
        remove((subdir + "/" + name).c_str());
    }
    rmdir(subdir.c_str());
    rmdir(root.c_str());
}

// TASK-24H-1235: audit.md Finding C3 -- the directory_iterator(dir, ec)
// constructor only makes the initial directory *open* non-throwing; the
// per-entry advance/status queries used to be the throwing overloads, so a
// filesystem error mid-scan (not just "file not found", which
// is_regular_file() already treats as a non-error) would throw
// std::filesystem::filesystem_error uncaught -- with no exception handling
// anywhere in this codebase, that reaches std::terminate() and crashes the
// whole process. A self-referential symlink (name -> itself) reproduces
// this deterministically and without root: resolving it to determine
// is_regular_file() hits ELOOP (too many levels of symbolic links), a real
// stat() error distinct from "not found". Exercises the exact call path
// free-eggbert's design-mission file picker (event.cpp:4741-4747) uses.
#if !defined(_WIN32)
static void TestFindFirstSkipsUnresolvableSymlinkInsteadOfCrashing()
{
    std::string root = MakeTempRoot("findfirst_eloop");
    Check(!root.empty(), "temp root created for the _findfirst ELOOP-regression test");
    if (root.empty()) return;

    const std::string subdir = root + "/User";
    Check(::mkdir(subdir.c_str(), 0755) == 0, "User/ subdirectory created for ELOOP-regression test");

    FILE* f = fopen((subdir + "/save1.xch").c_str(), "wb");
    Check(f != nullptr, "real fixture file created alongside the broken symlink");
    if (f) { fputs("x", f); fclose(f); }

    // A symlink that points to itself: resolving its target (as
    // is_regular_file() must, to know what it points to) hits ELOOP.
    Check(symlink("loop.xch", (subdir + "/loop.xch").c_str()) == 0,
          "self-referential loop.xch symlink created");

    char oldCwd[4096];
    getcwd(oldCwd, sizeof(oldCwd));
    Check(chdir(root.c_str()) == 0, "chdir into temp root succeeded for ELOOP-regression test");

    struct _finddata_t fileinfo{};
    intptr_t handle = _findfirst("\\User\\*.xch", &fileinfo);
    Check(handle != -1,
          "_findfirst does not crash and still finds the real match when the directory also "
          "contains an unresolvable (self-referential) symlink");

    int count = 0;
    bool sawSave1 = false, sawLoop = false;
    if (handle != -1) {
        do {
            ++count;
            std::string name(fileinfo.name);
            if (name == "save1.xch") sawSave1 = true;
            if (name == "loop.xch") sawLoop = true;
        } while (_findnext(handle, &fileinfo) == 0);
        _findclose(handle);
    }

    Check(count == 1 && sawSave1 && !sawLoop,
          "the unresolvable symlink is silently skipped (not a match, not a crash); "
          "only the real regular file is reported");

    chdir(oldCwd);

    remove((subdir + "/loop.xch").c_str());
    remove((subdir + "/save1.xch").c_str());
    rmdir(subdir.c_str());
    rmdir(root.c_str());
}
#endif

// TASK-24H-0703: parity/characterization test pinning down
// NormalizeFilesystemPath's exact current input/output behavior directly,
// as a before/after equivalence check for any future consolidation
// refactor (TASK-24H-0704/0705/0706). Covers every shape its three
// transformation steps (drive-letter strip, backslash-to-forward-slash,
// leading-slash strip) can independently produce.
//
// Of the three path-normalization implementations this task originally
// named, only two are independently direct-testable: NormalizeFilesystemPath
// (this test) and free_api_fopen (the next test below, since it's a public,
// callable-by-name header function). The fourth original implementation,
// NormalizePath, was removed entirely as dead code (TASK-24H-0615) once
// LoadImageA (its only caller) migrated to NormalizeFilesystemPath -- so
// only three implementations remain in total, not four. The third,
// NormalizeMidiPath (src/MidiMusic.cpp), has file-local `static` linkage
// and cannot be forward-declared/called directly without exposing a new
// internal symbol beyond its own translation unit (which this task's own
// "no unrelated API added" rule and this project's scope discipline both
// argue against for a test-only need) -- its progressive-suffix-uppercase
// fallback behavior already has real, if indirect, characterization
// coverage via tests/test_mci_sequences.cpp's
// TestMidiOpenFindsUppercaseFixtureViaLowercaseName.
static void TestNormalizeFilesystemPathCharacterization()
{
    using namespace FreeApi::Internal;

    struct Case {
        const char* input;
        const char* expected;
        const char* what;
    };
    const Case cases[] = {
        {"data/config.def", "data/config.def", "plain relative path passes through unchanged"},
        {"data\\config.def", "data/config.def", "backslashes convert to forward slashes"},
        {"\\User\\save1.xch", "User/save1.xch", "a leading backslash is stripped, staying relative to CWD"},
        {"/User/save1.xch", "User/save1.xch", "a leading forward slash is stripped, staying relative to CWD"},
        {"C:\\Planete Blupi\\data\\info.blp", "Planete Blupi/data/info.blp",
         "a drive letter is stripped, then the remaining backslashes convert and the leading slash strips"},
        {"c:data\\config.def", "data/config.def", "a lowercase drive letter is also stripped"},
        {"", "", "an empty string passes through unchanged"},
    };

    for (const auto& c : cases) {
        const std::string actual = NormalizeFilesystemPath(c.input);
        char msg[256];
        snprintf(msg, sizeof(msg), "NormalizeFilesystemPath(\"%s\") == \"%s\": %s", c.input, c.expected, c.what);
        Check(actual == c.expected, msg);
    }

    Check(NormalizeFilesystemPath(nullptr).empty(), "NormalizeFilesystemPath(nullptr) returns an empty string, not a crash");
}

// TASK-24H-0703: companion characterization test for free_api_fopen
// (include/windows.h) -- proves its prefix-normalization step currently
// produces the exact same relative path NormalizeFilesystemPath does for
// the same input shapes (both strip a drive letter, convert backslashes,
// and strip leading slashes identically), which is exactly the invariant
// a future TASK-24H-0705 consolidation would need to preserve. Also
// exercises free_api_fopen's own unique case-insensitive-basename
// fallback, which NormalizeFilesystemPath does not have and must not gain.
static void TestFreeApiFopenPrefixNormalizationCharacterization()
{
    std::string root = MakeTempRoot("fopen-prefix");
    Check(!root.empty(), "temp root created for the free_api_fopen prefix-normalization characterization test");
    if (root.empty()) return;

    const std::string subdir = root + "/data";
    Check(::mkdir(subdir.c_str(), 0755) == 0, "data/ subdirectory created");

    const std::string fixturePath = subdir + "/info.blp";
    FILE* w = fopen(fixturePath.c_str(), "wb");
    Check(w != nullptr, "info.blp fixture file created");
    if (w) {
        fputs("free-api", w);
        fclose(w);
    }

#if !defined(_WIN32)
    char oldCwd[4096];
    Check(getcwd(oldCwd, sizeof(oldCwd)) != nullptr, "captured current working directory");
    Check(chdir(root.c_str()) == 0, "chdir into temp root succeeded for the fopen prefix test");
#endif

    // Same drive-letter-prefixed, backslash-mixed shape as
    // TestNormalizeFilesystemPathCharacterization's matching case above --
    // free_api_fopen must resolve it to the identical relative path.
    FILE* f = free_api_fopen("C:\\data\\info.blp", "rb");
    Check(f != nullptr,
          "free_api_fopen resolves a drive-letter-prefixed, backslash path the same way "
          "NormalizeFilesystemPath's equivalent case does");
    if (f) fclose(f);

#if !defined(_WIN32)
    Check(chdir(oldCwd) == 0, "chdir restored after the fopen prefix test");
#endif

    remove(fixturePath.c_str());
    rmdir(subdir.c_str());
    rmdir(root.c_str());
}

int main()
{
    printf("[file-paths] Starting\n");

    TestNormalizeFilesystemPathCharacterization();
    TestFreeApiFopenPrefixNormalizationCharacterization();
    TestFopenCaseInsensitiveFallbackForPlainAssetFile();
    TestLoadImageAWithBackslashInitBlpPathShape();
    TestFopenWithSprintfFormattedSoundPathShape();
    TestFindFirstWithUserXchPathShapeReturnsMinusOneForMissingDirectory();
    TestFindFirstFindNextEnumerateRealMatchingFiles();
    TestFindFirstFindNextRepeatedDrainWithoutCloseDoesNotLeakSession();
#if !defined(_WIN32)
    TestFindFirstSkipsUnresolvableSymlinkInsteadOfCrashing();
#endif

    if (g_failures > 0) {
        printf("[file-paths] %d FAILURE(S)\n", g_failures);
        return 1;
    }

    printf("[file-paths] ALL TESTS PASSED\n");
    return 0;
}
