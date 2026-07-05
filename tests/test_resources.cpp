/**
 * @file test_resources.cpp
 * @brief Consolidated resource-subsystem regression test, per plan.md
 * TASK-0078: the resource subsystem has two genuinely different behaviors
 * that both need durable coverage in one place --
 *  - `FindResourceA` is a permanent safe stub that always misses (both
 *    games' real fallback path then reads the resource data directly from
 *    a file instead), see TASK-0073/0071.
 *  - `LoadStringA` has a real-text backing store extracted from whichever
 *    game's own `.rc` is driving the build, see TASK-0074/0075.
 *
 * This file is a consolidating smoke test confirming both contracts hold;
 * the exhaustive fixture-based coverage for each lives in its own focused
 * file (tests/test_file_regressions.cpp's
 * TestFindResourceAMissesThenLopenLreadLcloseDecodesRealBmpHeader for the
 * former; tests/test_loadstring_regressions.cpp for the latter) and is not
 * duplicated here.
 */
#include <windows.h>
#include <cstdio>
#include <cstring>

static int g_failures = 0;

static void Check(bool condition, const char* what)
{
    if (condition) {
        printf("[resources] PASS: %s\n", what);
    } else {
        printf("[resources] FAIL: %s\n", what);
        ++g_failures;
    }
}

static void TestFindResourceAAlwaysMisses()
{
    HRSRC h = FindResourceA(nullptr, MAKEINTRESOURCEA(1), RT_BITMAP);
    Check(h == nullptr, "FindResourceA always misses (permanent safe stub; see TASK-0071/0073)");
}

static void TestLoadStringAUnknownIdFallsBackToPlaceholder()
{
    // An ID far outside either target game's real STRINGTABLE ID range
    // always falls back to the "RES_<id>" placeholder, regardless of which
    // (if either) game's string table is compiled into this build -- see
    // tests/test_loadstring_regressions.cpp for the full real-text contract.
    const UINT unknownId = 999999;
    char buffer[128] = {};
    int len = LoadStringA(nullptr, unknownId, buffer, sizeof(buffer));
    Check(len > 0, "LoadStringA returns a nonzero length for an unknown ID (placeholder text)");

    char expected[64];
    snprintf(expected, sizeof(expected), "RES_%u", unknownId);
    Check(strcmp(buffer, expected) == 0, "LoadStringA falls back to the \"RES_<id>\" placeholder for an unknown ID");
}

int main()
{
    printf("[resources] Starting\n");

    TestFindResourceAAlwaysMisses();
    TestLoadStringAUnknownIdFallsBackToPlaceholder();

    if (g_failures > 0) {
        printf("[resources] %d FAILURE(S)\n", g_failures);
        return 1;
    }

    printf("[resources] ALL TESTS PASSED\n");
    return 0;
}
