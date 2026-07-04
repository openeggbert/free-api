/**
 * @file test_loadstring_regressions.cpp
 * @brief Regression tests for LoadStringA's generated real-text lookup.
 *
 * Both target games source all on-screen UI text through LoadStringA.
 * cmake/ExtractStringTable.cmake extracts STRINGTABLE entries from whichever
 * sibling game's own resource/*.rc file is found (checked in order:
 * ../free-eggbert, then ../planetblupi) and compiles them into free-api, so
 * LoadStringA can return real text for that game's IDs instead of the
 * "RES_<id>" placeholder.
 *
 * Because only one sibling's table is ever compiled into a given build (or
 * none, in a standalone build with neither sibling checked out), this test
 * cannot assume which -- if either -- table is present. It is also NOT valid
 * to assume an ID from the "wrong" game falls back to the placeholder: both
 * games assign their own IDs independently, so the same numeric ID may
 * legitimately be a *different*, real STRINGTABLE entry in whichever game's
 * table actually got compiled in (e.g. planetblupi's ID 512 is "N", but
 * free-eggbert's own ID 512 is a real, unrelated string too) -- that is a
 * correct result, not a bug, and must not be flagged as a failure.
 *
 * So this test only hard-asserts two things that are true regardless of
 * environment:
 *   1. An ID far outside either game's real ID range always falls back to
 *      the "RES_<id>" placeholder.
 *   2. At least one of the two known (id -> text) pairs below resolves to
 *      its exact expected real text -- true whenever this repository is
 *      checked out alongside at least one of its two target games (the
 *      normal development setup), which is what the extraction mechanism
 *      exists to prove.
 * Each individual known-ID lookup is otherwise logged, not hard-asserted,
 * since a "different real string from the other game" is an acceptable
 * outcome for it.
 */
#include <windows.h>
#include <cstdio>
#include <cstring>

static int g_failures = 0;

static void Check(bool condition, const char* what)
{
    if (condition) {
        printf("[loadstring-regressions] PASS: %s\n", what);
    } else {
        printf("[loadstring-regressions] FAIL: %s\n", what);
        ++g_failures;
    }
}

// planetblupi's TX_DIRECT_N (include/resource.h) = 512, text "N"
// (resource/blupi-e.rc).
static const unsigned int kPlanetblupiId = 512;
static const char* kPlanetblupiText = "N";

// free-eggbert's TX_BUTTON_QUITTER (resource/resource.h) = 106, text
// "Quit BLUPI" (resource/Eggbert2.rc).
static const unsigned int kEggbertId = 106;
static const char* kEggbertText = "Quit BLUPI";

static bool IsPlaceholderFor(const char* buffer, unsigned int id)
{
    char expected[64];
    snprintf(expected, sizeof(expected), "RES_%u", id);
    return strcmp(buffer, expected) == 0;
}

// Looks up one known (id -> real text) pair and logs what came back. Does
// NOT hard-assert real-vs-placeholder here: if a *different* game's table
// is the one compiled in, this ID may legitimately resolve to that other
// game's own real string instead (both games assign IDs independently, so
// numeric collisions across games are expected and not an error). Returns
// true only when the exact expected text for THIS id was returned.
static bool LooksUpKnownId(unsigned int id, const char* expectedText, const char* label)
{
    char buffer[256] = {};
    int len = LoadStringA(nullptr, id, buffer, sizeof(buffer));
    Check(len >= 0, "LoadStringA does not fail outright for a known ID");

    const bool isReal = (strcmp(buffer, expectedText) == 0);
    if (isReal) {
        printf("[loadstring-regressions] INFO: %s (id=%u) resolved to its real STRINGTABLE text \"%s\"\n",
               label, id, buffer);
    } else {
        printf("[loadstring-regressions] INFO: %s (id=%u) did not resolve to \"%s\" in this build, got \"%s\" "
               "(expected if a different sibling's table -- or none -- is compiled in)\n",
               label, id, expectedText, buffer);
    }
    return isReal;
}

static void TestAtLeastOneKnownGameResolvesToRealText()
{
    const bool planetblupiReal = LooksUpKnownId(kPlanetblupiId, kPlanetblupiText, "planetblupi TX_DIRECT_N");
    const bool eggbertReal = LooksUpKnownId(kEggbertId, kEggbertText, "free-eggbert TX_BUTTON_QUITTER");

    // This is the one meaningful hard guarantee across both known IDs: in
    // the normal development setup (free-api checked out alongside at
    // least one of its two target games), real STRINGTABLE extraction must
    // actually be wired up end-to-end for that game.
    Check(planetblupiReal || eggbertReal,
          "at least one target game's known ID resolves to its real STRINGTABLE text");
}

static void TestUnknownIdAlwaysFallsBackToPlaceholder()
{
    // An ID far outside either game's real STRINGTABLE range must always
    // fall back to the placeholder, regardless of which (if any) sibling's
    // table is compiled in.
    const unsigned int unknownId = 999999u;
    char buffer[64] = {};
    int len = LoadStringA(nullptr, unknownId, buffer, sizeof(buffer));

    Check(len > 0, "LoadStringA returns a positive length for an unknown ID");
    Check(IsPlaceholderFor(buffer, unknownId), "LoadStringA falls back to \"RES_<id>\" for an ID with no STRINGTABLE entry");
}

static void TestBufferTruncation()
{
    // A real Win32 LoadString truncates to fit and returns the truncated
    // length (not counting the terminator) when the buffer is too small.
    char tinyBuffer[4] = {};
    int len = LoadStringA(nullptr, kEggbertId, tinyBuffer, sizeof(tinyBuffer));

    Check(len == static_cast<int>(sizeof(tinyBuffer)) - 1,
          "LoadStringA truncates to fit a too-small buffer and returns the truncated length");
    Check(tinyBuffer[sizeof(tinyBuffer) - 1] == '\0',
          "LoadStringA's truncated output is still null-terminated");
}

int main()
{
    printf("[loadstring-regressions] Starting\n");

    TestAtLeastOneKnownGameResolvesToRealText();
    TestUnknownIdAlwaysFallsBackToPlaceholder();
    TestBufferTruncation();

    if (g_failures > 0) {
        printf("[loadstring-regressions] %d FAILURE(S)\n", g_failures);
        return 1;
    }

    printf("[loadstring-regressions] ALL TESTS PASSED\n");
    return 0;
}
