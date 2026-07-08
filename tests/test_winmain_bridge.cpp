/**
 * @file test_winmain_bridge.cpp
 * @brief Regression test for FreeApiRunWinMain (src/winmain_bridge.cpp) --
 * the real process-bootstrap bridge both target games actually launch
 * through via the ../free-direct FREE_API_IMPLEMENT_WINMAIN() macro
 * (include/windows.h). Found by a strict test-coverage audit to have zero
 * automated coverage: tests/test_eggbert_loop.cpp and
 * tests/test_planetblupi_loop.cpp both use a hand-written `int main()` that
 * bypasses this function entirely, so a regression here (e.g. in argv/argc
 * -> lpCmdLine assembly, or exit-code passthrough) would mean a real game
 * fails to launch at all, with zero test signal.
 */
#include <windows.h>
#include <cstdio>
#include <cstring>

// Never actually invoked: src/winmain_bridge.cpp compiles a weak `int
// main(int, char**)` that calls FreeApiRunWinMain(&WinMain, ...) --
// pulling that object file into this test's link (needed for
// FreeApiRunWinMain itself) drags in an unresolved reference to `WinMain`
// regardless of whether the weak main() ultimately loses to this file's
// own (strong) main() below, since static linking resolves undefined
// references at whole-object-file granularity. This dummy definition only
// satisfies the linker; it is provably dead code (the weak main() is never
// selected while a strong main() exists in the same link).
// Matches src/winmain_bridge.cpp's own WinMain declaration exactly: plain
// C++ linkage (declared outside any extern "C" block there), not extern "C".
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return 0;
}

static int g_failures = 0;

static void Check(bool condition, const char* what)
{
    if (condition) {
        printf("[winmain-bridge] PASS: %s\n", what);
    } else {
        printf("[winmain-bridge] FAIL: %s\n", what);
        ++g_failures;
    }
}

static HINSTANCE g_receivedHInstance = (HINSTANCE)0x1; // deliberately non-NULL sentinel
static HINSTANCE g_receivedHPrevInstance = (HINSTANCE)0x1;
// NOTE: lpCmdLine points into a local std::string inside FreeApiRunWinMain
// (src/winmain_bridge.cpp) -- for a short command line, libstdc++'s small-
// string-optimization means that pointer refers to stack memory belonging
// to FreeApiRunWinMain's own frame, valid only for the duration of the
// entryPoint(...) call, exactly matching real Win32 WinMain's lpCmdLine
// lifetime contract. An earlier version of this test saved the raw pointer
// and compared it AFTER FreeApiRunWinMain had already returned (and that
// stack frame was gone) -- a genuine use-after-free that happened to
// "work" in some builds depending on whether anything else clobbered the
// stale stack memory first. Fixed by copying the content into a fixed
// buffer here, inside FakeWinMain, while the pointer is still guaranteed
// valid.
static bool g_receivedLpCmdLineWasNull = false;
static char g_receivedLpCmdLineCopy[256] = {0};
static int g_receivedNCmdShow = -999;
static bool g_fakeWinMainCalled = false;

static int WINAPI FakeWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    g_fakeWinMainCalled = true;
    g_receivedHInstance = hInstance;
    g_receivedHPrevInstance = hPrevInstance;
    g_receivedLpCmdLineWasNull = (lpCmdLine == nullptr);
    if (lpCmdLine) {
        strncpy(g_receivedLpCmdLineCopy, lpCmdLine, sizeof(g_receivedLpCmdLineCopy) - 1);
        g_receivedLpCmdLineCopy[sizeof(g_receivedLpCmdLineCopy) - 1] = '\0';
    } else {
        g_receivedLpCmdLineCopy[0] = '\0';
    }
    g_receivedNCmdShow = nCmdShow;
    return 42; // distinguishable, non-zero, non-(-1) exit code
}

static void ResetCapture()
{
    g_receivedHInstance = (HINSTANCE)0x1;
    g_receivedHPrevInstance = (HINSTANCE)0x1;
    g_receivedLpCmdLineWasNull = false;
    g_receivedLpCmdLineCopy[0] = '\0';
    g_receivedNCmdShow = -999;
    g_fakeWinMainCalled = false;
}

// TASK-24H-1223: NULL entryPoint must be rejected immediately, matching the
// real safety check both games implicitly rely on (a null WinMain pointer
// would otherwise be an immediate crash on call).
static void TestNullEntryPointReturnsMinusOneWithoutCalling()
{
    ResetCapture();
    char* argv[] = {const_cast<char*>("game")};
    int result = FreeApiRunWinMain(nullptr, 1, argv);
    Check(result == -1, "FreeApiRunWinMain(nullptr, ...) returns -1");
    Check(!g_fakeWinMainCalled, "a NULL entryPoint is never invoked");
}

// TASK-24H-1223: proves the full real bootstrap contract both games depend
// on -- hInstance/hPrevInstance are always NULL, nCmdShow is always
// SW_SHOW, argv[1..] is joined into lpCmdLine (argv[0], the program name,
// is excluded -- matching real Win32 WinMain's lpCmdLine semantics), and
// the entry point's return value passes through as FreeApiRunWinMain's own
// return value (a real game's WinMain exit code must survive this bridge
// unchanged).
static void TestArgvToCmdLineAssemblyAndExitCodePassthrough()
{
    ResetCapture();
    char* argv[] = {
        const_cast<char*>("/path/to/game"),
        const_cast<char*>("arg1"),
        const_cast<char*>("arg two"),
    };
    int result = FreeApiRunWinMain(FakeWinMain, 3, argv);

    Check(g_fakeWinMainCalled, "a non-NULL entryPoint is actually invoked");
    Check(g_receivedHInstance == NULL, "hInstance is passed as NULL, matching FreeApiRunWinMain's documented contract");
    Check(g_receivedHPrevInstance == NULL, "hPrevInstance is passed as NULL");
    Check(g_receivedNCmdShow == SW_SHOW, "nCmdShow is passed as SW_SHOW");
    Check(!g_receivedLpCmdLineWasNull && strcmp(g_receivedLpCmdLineCopy, "arg1 arg two") == 0,
          "lpCmdLine joins argv[1..] with spaces, excluding argv[0] (the program name)");
    Check(result == 42, "entryPoint's return value passes through as FreeApiRunWinMain's own return value");
}

// TASK-24H-1223: with no extra arguments beyond the program name,
// lpCmdLine must be NULL (matching real Win32's "no command line" case),
// not an empty non-NULL string -- both games' own argument-parsing code
// may branch on lpCmdLine being NULL.
static void TestNoExtraArgsProducesNullCmdLine()
{
    ResetCapture();
    char* argv[] = {const_cast<char*>("/path/to/game")};
    FreeApiRunWinMain(FakeWinMain, 1, argv);

    Check(g_fakeWinMainCalled, "entryPoint is invoked even with no extra arguments");
    Check(g_receivedLpCmdLineWasNull, "lpCmdLine is NULL (not an empty string) when there are no extra arguments");
}

int main()
{
    printf("[winmain-bridge] Starting\n");

    TestNullEntryPointReturnsMinusOneWithoutCalling();
    TestArgvToCmdLineAssemblyAndExitCodePassthrough();
    TestNoExtraArgsProducesNullCmdLine();

    if (g_failures > 0) {
        printf("[winmain-bridge] %d FAILURE(S)\n", g_failures);
        return 1;
    }

    printf("[winmain-bridge] ALL TESTS PASSED\n");
    return 0;
}
