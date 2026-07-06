/**
 * @file test_loadstring_regressions.cpp
 * @brief Regression tests for LoadStringA's generated real-text lookup.
 *
 * Both target games source all on-screen UI text through LoadStringA.
 * cmake/ExtractStringTable.cmake extracts STRINGTABLE entries from whichever
 * game is actually driving the build (CMAKE_PROJECT_NAME) and compiles them
 * into free-api, so LoadStringA can return real text for that game's IDs
 * instead of the "RES_<id>" placeholder. See docs/out-of-scope.md's
 * "RES_<id>" note for why that placeholder is a debug-only marker, never
 * acceptable in shipped game UI.
 *
 * CMakeLists.txt passes this test a compile-time marker for which build
 * mode it's running in (FREE_API_TARGET_GAME_FREE_EGGBERT /
 * FREE_API_TARGET_GAME_PLANETBLUPI / neither == standalone), mirroring the
 * same CMAKE_PROJECT_NAME detection cmake/ExtractStringTable.cmake itself
 * uses -- so the right contract is hard-asserted for the actual build mode
 * instead of guessed at:
 *   - Standalone (neither macro defined): no target game is driving the
 *     build, so CMake never enforces REQUIRE_STRINGS/VERIFY_ID (see
 *     cmake/ExtractStringTable.cmake). A known game ID may legitimately
 *     return either real text (if the developer-convenience sibling lookup
 *     found a game checked out next to free-api) or the placeholder (if
 *     not) -- both are logged, neither is a hard failure.
 *   - Target-game mode (either macro defined): CMake configure already
 *     failed loudly if the .rc were missing/empty/wrong, so a known game ID
 *     MUST resolve to its real text and must NEVER be the "RES_<id>"
 *     placeholder -- that is hard-asserted here, for both the single id-106
 *     sentinel (also used by ExtractStringTable.cmake's own VERIFY_ID/
 *     VERIFY_TEXT) and a broader sample of IDs spanning several distinct
 *     used-ID categories (direct symbols, computed offsets, the button-
 *     tooltip table) drawn from cmake/used-string-ids/*.txt -- see
 *     docs/used-string-ids.md for the full evidence-based manifest and how
 *     CMake configure-time verification of every used ID (not just this
 *     sample) works.
 * An ID far outside either game's real STRINGTABLE range always falls back
 * to the placeholder in every mode -- that is also hard-asserted.
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

static bool IsPlaceholderFor(const char* buffer, unsigned int id)
{
    char expected[64];
    snprintf(expected, sizeof(expected), "RES_%u", id);
    return strcmp(buffer, expected) == 0;
}

// TX_BUTTON_QUITTER: id 106, real text "Quit BLUPI" in both Eggbert2.rc and
// blupi-e.rc -- the same known-ID pair cmake/ExtractStringTable.cmake's
// VERIFY_ID/VERIFY_TEXT checks use at configure time for both target games.
static const unsigned int kKnownId = 106;
static const char* kKnownText = "Quit BLUPI";

// Far outside either game's real STRINGTABLE ID range in every build mode.
static const unsigned int kUnknownId = 999999u;

struct KnownIdCase {
    unsigned int id;
    const char* text;
};

// A handful of IDs proven used by the actual game (see
// cmake/used-string-ids/*.txt and docs/used-string-ids.md for the full,
// evidence-based manifest this sample is drawn from), spanning several
// distinct categories -- not just the single VERIFY_ID/VERIFY_TEXT sentinel
// (id 106) checked at CMake configure time. Text taken directly from a live
// extraction of each game's own .rc.
#if defined(FREE_API_TARGET_GAME_FREE_EGGBERT)
static const KnownIdCase kKnownIdSamples[] = {
    {106u, "Quit BLUPI"},                                              // direct TX_BUTTON_QUITTER
    {125u, "<<<<    P A U S E    >>>>"},                                // direct TX_PAUSE
    {149u, "Choose music"},                                             // direct TX_MUSIC
    {288u, "Game saved ..."},                                           // direct TX_GAMESAVED
    {400u, "Hi, I'm Blupi. Press the Right arrow to make me move ..."}, // computed: table_tutorial[] direct
    {500u, "Hi, I'm Blupi. Press the Right arrow to make me move ..."}, // computed: table_tutorial[] +100 (joystick variant)
    {600u, "Left or Right"},                                            // computed: joystick/key setup label range
    {3000u, "Well done!"},                                              // computed: TX_WIN1 + GetWorld()%5
    {3100u, "You have failed, try again..."},                           // computed: TX_LOST1 + GetWorld()%5
    {107u, "Previous mission"},                                         // button-tooltip table
};
#elif defined(FREE_API_TARGET_GAME_PLANETBLUPI)
static const KnownIdCase kKnownIdSamples[] = {
    {106u, "Quit BLUPI"},                            // direct TX_BUTTON_QUITTER
    {150u, "Training number"},                        // direct TX_SCHOOL
    {178u, "Scenery choice"},                          // direct TX_REGION, via DrawTextCenter
    {1u, "Go"},                                        // computed: TX_ACTION_GO + rank (GetText helper)
    {1000u, "Impossible"},                             // computed: TX_ERROR_MISC + rank (GetErr helper)
    {500u, "1: Grow tomatoes\n2: Eat"},                // computed: TX_REPEAT_* (ListSearch helper)
    {2000u, "Sick Blupi"},                             // computed: TX_OBJ_* range (GetResHili)
    {3000u, "Well done !"},                            // computed: TX_WIN1 + GetWorld()%5
    {3100u, "You have failed, try again..."},          // computed: TX_LOST1 + GetWorld()%5
    {126u, "Blupi's energy"},                          // computed: TX_JAUGE1 + i
};
#endif

static void TestUnknownIdAlwaysFallsBackToPlaceholder()
{
    char buffer[64] = {};
    int len = LoadStringA(nullptr, kUnknownId, buffer, sizeof(buffer));

    Check(len > 0, "LoadStringA returns a positive length for an unknown ID");
    Check(IsPlaceholderFor(buffer, kUnknownId),
          "LoadStringA falls back to \"RES_<id>\" for an ID with no STRINGTABLE entry, in every build mode");
}

#if defined(FREE_API_TARGET_GAME_FREE_EGGBERT) || defined(FREE_API_TARGET_GAME_PLANETBLUPI)

static void TestKnownGameIdReturnsRealTextNeverPlaceholder()
{
    char buffer[256] = {};
    int len = LoadStringA(nullptr, kKnownId, buffer, sizeof(buffer));

    Check(len > 0, "LoadStringA returns a positive length for a known game string ID (target-game build)");
    Check(strcmp(buffer, kKnownText) == 0,
          "LoadStringA returns the exact real STRINGTABLE text for a known game string ID (target-game build)");
    Check(!IsPlaceholderFor(buffer, kKnownId),
          "LoadStringA never falls back to \"RES_<id>\" for a known game string ID in a target-game build");
}

// Exercises several distinct used-ID categories from cmake/used-string-ids/
// (direct symbols, computed offsets, and the button-tooltip table), not
// just the single id-106 sentinel above -- a broken STRINGTABLE parse or a
// used ID silently missing from the generated table could otherwise still
// slip past a check that only ever looks at id 106.
static void TestSeveralKnownGameIdsReturnRealTextNeverPlaceholder()
{
    char nameBuf[64];
    for (const KnownIdCase& sample : kKnownIdSamples) {
        char buffer[256] = {};
        int len = LoadStringA(nullptr, sample.id, buffer, sizeof(buffer));

        snprintf(nameBuf, sizeof(nameBuf), "id %u returns a positive length", sample.id);
        Check(len > 0, nameBuf);

        snprintf(nameBuf, sizeof(nameBuf), "id %u returns its exact real STRINGTABLE text", sample.id);
        Check(strcmp(buffer, sample.text) == 0, nameBuf);

        snprintf(nameBuf, sizeof(nameBuf), "id %u never falls back to \"RES_<id>\"", sample.id);
        Check(!IsPlaceholderFor(buffer, sample.id), nameBuf);
    }
}

#else

static void TestStandaloneKnownIdMayReturnEitherRealTextOrPlaceholder()
{
    // Standalone free-api builds never set REQUIRE_STRINGS/VERIFY_ID (see
    // cmake/ExtractStringTable.cmake), so this is genuinely either outcome
    // depending on whether the developer-convenience sibling lookup found a
    // target game checked out next to free-api -- log which, assert neither.
    char buffer[256] = {};
    int len = LoadStringA(nullptr, kKnownId, buffer, sizeof(buffer));
    Check(len > 0, "LoadStringA returns a positive length in standalone mode");

    if (IsPlaceholderFor(buffer, kKnownId)) {
        printf("[loadstring-regressions] INFO: standalone build with no sibling table -- id %u returned the placeholder (acceptable)\n",
               kKnownId);
    } else {
        printf("[loadstring-regressions] INFO: standalone build found a sibling table -- id %u resolved to \"%s\"\n",
               kKnownId, buffer);
    }
}

#endif

static void TestBufferTruncation()
{
    // A real Win32 LoadString truncates to fit and returns the truncated
    // length (not counting the terminator) when the buffer is too small.
    // Whatever kKnownId resolves to in this build (real text or the
    // placeholder), it is always longer than 3 characters, so this holds
    // regardless of build mode.
    char tinyBuffer[4] = {};
    int len = LoadStringA(nullptr, kKnownId, tinyBuffer, sizeof(tinyBuffer));

    Check(len == static_cast<int>(sizeof(tinyBuffer)) - 1,
          "LoadStringA truncates to fit a too-small buffer and returns the truncated length");
    Check(tinyBuffer[sizeof(tinyBuffer) - 1] == '\0',
          "LoadStringA's truncated output is still null-terminated");
}

static void TestNullBufferReturnsZero()
{
    int len = LoadStringA(nullptr, kKnownId, nullptr, 256);
    Check(len == 0, "LoadStringA returns 0 for a NULL output buffer");
}

static void TestZeroBufferMaxReturnsZero()
{
    char buffer[16];
    buffer[0] = 'X';
    int len = LoadStringA(nullptr, kKnownId, buffer, 0);
    Check(len == 0, "LoadStringA returns 0 when cchBufferMax == 0");
}

int main()
{
    printf("[loadstring-regressions] Starting\n");

    TestUnknownIdAlwaysFallsBackToPlaceholder();
#if defined(FREE_API_TARGET_GAME_FREE_EGGBERT) || defined(FREE_API_TARGET_GAME_PLANETBLUPI)
    TestKnownGameIdReturnsRealTextNeverPlaceholder();
    TestSeveralKnownGameIdsReturnRealTextNeverPlaceholder();
#else
    TestStandaloneKnownIdMayReturnEitherRealTextOrPlaceholder();
#endif
    TestBufferTruncation();
    TestNullBufferReturnsZero();
    TestZeroBufferMaxReturnsZero();

    if (g_failures > 0) {
        printf("[loadstring-regressions] %d FAILURE(S)\n", g_failures);
        return 1;
    }

    printf("[loadstring-regressions] ALL TESTS PASSED\n");
    return 0;
}
