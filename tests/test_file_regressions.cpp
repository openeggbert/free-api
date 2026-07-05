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

// TASK-0073/0084 (plan.md): both games' DDLoadPalette (free-eggbert
// ddutil.cpp:204,232-240; planetblupi ddutil.cpp:268,297,303-306) first
// tries FindResourceA(NULL, szBitmap, RT_BITMAP), and -- since that always
// misses (no embedded resource section) -- falls through to
// _lopen/_lread/_lclose against a real .bmp asset file, reading
// BITMAPFILEHEADER + BITMAPINFOHEADER + a PALETTEENTRY array directly at
// fixed byte offsets. This is the actual, live palette-read path exercised
// by both games. The fixture below is built byte-for-byte at the real,
// standard on-disk BMP offsets (not by writing the BITMAPFILEHEADER/
// BITMAPINFOHEADER structs directly), so this test also proves those
// structs are correctly packed to the real 14-byte/40-byte on-disk sizes
// (include/wingdi.h's BITMAPFILEHEADER previously lacked #pragma pack,
// making it 16 bytes here vs. the real format's 14 -- a genuine bug found
// while writing this test, fixed alongside it: every _lread of a real .bmp
// file's header this way was misaligned by 2 bytes).
static void TestFindResourceAMissesThenLopenLreadLcloseDecodesRealBmpHeader()
{
    HRSRC found = FindResourceA(nullptr, MAKEINTRESOURCEA(1), RT_BITMAP);
    Check(found == nullptr, "FindResourceA misses (matches the safe-stub behavior both games' fallback logic expects)");

    // Hand-build a real 14-byte BITMAPFILEHEADER + 40-byte BITMAPINFOHEADER
    // + a small palette, at the exact standard on-disk offsets.
    const uint16_t width = 4;
    const uint16_t colorsUsed = 4;
    std::vector<uint8_t> fixture(14 + 40 + colorsUsed * 4, 0);
    auto put16 = [&](size_t off, uint16_t v) { fixture[off] = v & 0xFF; fixture[off + 1] = (v >> 8) & 0xFF; };
    auto put32 = [&](size_t off, uint32_t v) {
        fixture[off] = v & 0xFF; fixture[off + 1] = (v >> 8) & 0xFF;
        fixture[off + 2] = (v >> 16) & 0xFF; fixture[off + 3] = (v >> 24) & 0xFF;
    };
    // BITMAPFILEHEADER (offsets 0-13)
    fixture[0] = 'B'; fixture[1] = 'M';
    put32(2, static_cast<uint32_t>(fixture.size())); // bfSize
    put16(6, 0); put16(8, 0);                        // bfReserved1/2
    put32(10, 14 + 40 + colorsUsed * 4);              // bfOffBits (past the palette, no pixel data needed)
    // BITMAPINFOHEADER (offsets 14-53)
    put32(14, 40);          // biSize
    put32(18, width);       // biWidth
    put32(22, 1);           // biHeight
    put16(26, 1);           // biPlanes
    put16(28, 8);           // biBitCount (paletted)
    put32(30, 0);           // biCompression (BI_RGB)
    put32(34, 0);           // biSizeImage
    put32(38, 0);           // biXPelsPerMeter
    put32(42, 0);           // biYPelsPerMeter
    put32(46, colorsUsed);  // biClrUsed
    put32(50, 0);           // biClrImportant
    // Palette (offsets 54..), BGR + reserved, distinct per entry.
    for (uint16_t i = 0; i < colorsUsed; ++i) {
        size_t off = 54 + i * 4;
        fixture[off + 0] = static_cast<uint8_t>(i * 10);       // blue
        fixture[off + 1] = static_cast<uint8_t>(i * 20);       // green
        fixture[off + 2] = static_cast<uint8_t>(i * 30);       // red
        fixture[off + 3] = 0;
    }

    const char* fixturePath = "test_palette_fixture.bmp";
    FILE* f = fopen(fixturePath, "wb");
    Check(f != nullptr, "palette fixture file opens for writing");
    if (f) {
        fwrite(fixture.data(), 1, fixture.size(), f);
        fclose(f);
    }

    int fh = _lopen(fixturePath, OF_READ);
    Check(fh != -1, "_lopen succeeds against the real BMP-shaped fixture (the live fallback path)");

    if (fh != -1) {
        BITMAPFILEHEADER bf{};
        BITMAPINFOHEADER bi{};
        UINT bfRead = _lread(fh, &bf, sizeof(bf));
        UINT biRead = _lread(fh, &bi, sizeof(bi));

        std::vector<PALETTEENTRY> ape(colorsUsed);
        UINT apeRead = _lread(fh, ape.data(), static_cast<UINT>(sizeof(PALETTEENTRY) * colorsUsed));
        _lclose(fh);

        Check(bfRead == sizeof(BITMAPFILEHEADER), "_lread reads exactly sizeof(BITMAPFILEHEADER) (14) bytes");
        Check(bi.biSize == sizeof(BITMAPINFOHEADER),
              "the BITMAPINFOHEADER read immediately after is correctly aligned (biSize == 40, matching both games' own validity check)");
        Check(bi.biBitCount == 8 && bi.biClrUsed == colorsUsed,
              "BITMAPINFOHEADER fields decode correctly (biBitCount, biClrUsed) from the real on-disk layout");
        Check(apeRead == sizeof(PALETTEENTRY) * colorsUsed, "the palette array reads the expected byte count");

        bool paletteCorrect = true;
        for (uint16_t i = 0; i < colorsUsed; ++i) {
            // Real DIB color tables store BGR; both games flip B/R when
            // copying into their own PALETTEENTRY (peRed/peGreen/peBlue),
            // but this test reads the raw RGBQUAD-shaped bytes directly via
            // PALETTEENTRY's matching byte layout, so the raw stored order
            // (blue, green, red, reserved) is what's checked here.
            if (ape[i].peRed != static_cast<uint8_t>(i * 10) ||
                ape[i].peGreen != static_cast<uint8_t>(i * 20) ||
                ape[i].peBlue != static_cast<uint8_t>(i * 30)) {
                paletteCorrect = false;
                break;
            }
        }
        Check(paletteCorrect, "palette entries decode byte-for-byte correctly from the real on-disk fixture");
    }

    remove(fixturePath);
}

int main()
{
    printf("[file-regressions] Starting\n");

    TestCreateDirectoryACreatesRealDirectory();
    TestCreateDirectoryAWithBackslashPath();
    TestMkdirCreatesRealDirectory();
    TestMkdirWithBackslashPath();
    TestBackslashAndForwardSlashPathsBothResolve();
    TestFindResourceAMissesThenLopenLreadLcloseDecodesRealBmpHeader();

    if (g_failures > 0) {
        printf("[file-regressions] %d FAILURE(S)\n", g_failures);
        return 1;
    }

    printf("[file-regressions] ALL TESTS PASSED\n");
    return 0;
}
