/**
 * @file test_sdl_log_gating.cpp
 * @brief TASK-24H-1113: a lightweight source-scan guard against future
 * ungated SDL_Log additions in src/**.cpp.
 *
 * This is deliberately a simple text-based heuristic, not full static
 * analysis (see plan.md TASK-24H-1113's "Out of scope" note): it looks for
 * one of a handful of known gating shapes actually used throughout this
 * codebase --
 *   - same-line:      if (GATE) SDL_Log(...)
 *   - block:           if (GATE) {\n    SDL_Log(...)\n}
 *   - early-return:    if (!GATE) return;\n  ... SDL_Log(...) later in the
 *                       same function
 *   - compile-time:    #if defined(__ANDROID__) ... SDL_Log(...) ... #endif
 * "GATE" is one of: FreeApiDiagnosticsEnabled(), FreeApiDiagnosticsFastEnabled(),
 * FreeApiGdiDebugEnabled(), midiDebugEnabled(), g_debugInput (either its
 * implicit-bool-conversion form or an explicit g_debugInput.load(...) call,
 * TASK-24H-1243), or the local `diag` variable (src/wingdi_dc.cpp).
 *
 * TASK-0004 (maintainability sweep, 2026-07-09): the allowlist below used to
 * be a `{file, line-number}` set. That broke every time an unrelated edit
 * anywhere above one of the 16 allowlisted call sites shifted its line
 * number -- confirmed to have happened at least 6-7 separate times across
 * this project's sessions, each requiring a manual re-grep-and-fix cycle.
 * It's now an inline marker comment on each intentionally-unconditional
 * SDL_Log call's own line (`// sdl-log-gating: intentional (reason)`),
 * matching the same shape as clang-tidy's `// NOLINT` convention -- the
 * marker travels with the call site through any edit, so this allowlist can
 * no longer drift out of sync with line numbers. See git history for the
 * prior line-number-keyed version if ever needed.
 *
 * The marker covers the confirmed-intentional unconditional failure/
 * startup/warning-path logs (the same sites TASK-24H-1101/1102/1106-1111/
 * 1227 individually reviewed and documented as compliant, rare, or
 * deliberately always-visible). Anything else found unconditional is a real
 * regression this test is meant to catch.
 */
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int g_failures = 0;

static void Check(bool condition, const char* what)
{
    if (condition) {
        printf("[sdl-log-gating] PASS: %s\n", what);
    } else {
        printf("[sdl-log-gating] FAIL: %s\n", what);
        ++g_failures;
    }
}

// The inline marker an intentionally-unconditional SDL_Log call carries on
// its own line -- see this file's doc comment above for why this replaced
// a line-number-keyed allowlist.
static const char* const kIntentionalMarker = "sdl-log-gating: intentional";

static std::string Trim(const std::string& s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static size_t LeadingWhitespace(const std::string& s)
{
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return i;
}

// One of the recognized gate tokens/calls, as a regex alternation.
//
// TASK-24H-1243: g_debugInput's reads became explicit
// `.load(std::memory_order_relaxed)` calls (relaxed, not the
// implicit-bool-conversion default of seq_cst) -- the optional
// "(\.load\(...\))?" suffix below tolerates that shape alongside the
// plain `g_debugInput` implicit-conversion form still used elsewhere.
static const std::string kGatePattern =
    "(FreeApiDiagnosticsEnabled\\(\\)|FreeApiDiagnosticsFastEnabled\\(\\)|"
    "FreeApiGdiDebugEnabled\\(\\)|midiDebugEnabled\\(\\)|"
    "g_debugInput(\\.load\\([^)]*\\))?|\\bdiag\\b)";

static bool IsGatedLine(const std::vector<std::string>& lines, size_t targetIdx)
{
    static const std::regex sameLineGateLog(
        "if\\s*\\(\\s*" + kGatePattern + "[^{]*\\)\\s*\\{?\\s*SDL_Log\\(");
    static const std::regex earlyReturnGate(
        "if\\s*\\(\\s*!\\s*" + kGatePattern + "\\s*\\)\\s*return");
    static const std::regex blockOpenGate(
        "if\\s*\\(\\s*" + kGatePattern + "[^{]*\\)\\s*\\{\\s*$");

    if (std::regex_search(lines[targetIdx], sameLineGateLog)) {
        return true;
    }

    // Find the nearest preceding column-0 "}" (this codebase's convention
    // for a top-level function's closing brace) -- everything after it, up
    // to and including targetIdx, is "this function's body" for our
    // purposes.
    size_t functionStart = 0;
    for (size_t i = targetIdx; i-- > 0;) {
        if (lines[i] == "}") {
            functionStart = i + 1;
            break;
        }
    }

    bool earlyReturnGated = false;
    std::vector<size_t> gateBlockIndents;

    for (size_t i = functionStart; i <= targetIdx; ++i) {
        const std::string& raw = lines[i];
        std::string trimmed = Trim(raw);
        size_t indent = LeadingWhitespace(raw);

        while (!gateBlockIndents.empty() && trimmed == "}" && indent <= gateBlockIndents.back()) {
            gateBlockIndents.pop_back();
        }

        if (std::regex_search(raw, earlyReturnGate)) {
            earlyReturnGated = true;
        } else if (std::regex_search(raw, blockOpenGate)) {
            gateBlockIndents.push_back(indent);
        }
    }

    return earlyReturnGated || !gateBlockIndents.empty();
}

static void ScanFile(const fs::path& srcRoot, const fs::path& file)
{
    std::ifstream in(file);
    if (!in) {
        Check(false, ("could not open file for scanning: " + file.string()).c_str());
        return;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }

    const std::string relPath = fs::relative(file, srcRoot).generic_string();

    // Track #if defined(__ANDROID__) / #else / #endif nesting: 1 = active
    // android-only branch, 2 = the #else branch of an android-only #if
    // (i.e. NOT android-only), 0 = an unrelated #if/#ifdef/#ifndef.
    static const std::regex androidIf("^\\s*#\\s*if\\s+defined\\(__ANDROID__\\)\\s*$");
    static const std::regex anyIf("^\\s*#\\s*(if|ifdef|ifndef)\\b");
    static const std::regex anyElse("^\\s*#\\s*else\\b");
    static const std::regex anyEndif("^\\s*#\\s*endif\\b");

    std::vector<int> ppStack;

    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& raw = lines[i];

        if (std::regex_search(raw, androidIf)) {
            ppStack.push_back(1);
            continue;
        }
        if (std::regex_search(raw, anyIf)) {
            ppStack.push_back(0);
            continue;
        }
        if (std::regex_search(raw, anyElse)) {
            if (!ppStack.empty() && ppStack.back() == 1) ppStack.back() = 2;
            continue;
        }
        if (std::regex_search(raw, anyEndif)) {
            if (!ppStack.empty()) ppStack.pop_back();
            continue;
        }

        if (raw.find("SDL_Log(") == std::string::npos) continue;

        const bool androidGated = !ppStack.empty() && ppStack.back() == 1;
        if (androidGated) continue;

        // TASK-0004: the marker travels with the call site, so this check
        // is immune to unrelated line-shift edits (see this file's doc
        // comment for why this replaced a {file, line-number} allowlist).
        if (raw.find(kIntentionalMarker) != std::string::npos) continue;

        const int lineNo = static_cast<int>(i) + 1;
        if (!IsGatedLine(lines, i)) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "ungated SDL_Log at %s:%d is neither behind a recognized "
                     "gate nor marked \"%s\": %s",
                     relPath.c_str(), lineNo, kIntentionalMarker, Trim(raw).c_str());
            Check(false, msg);
        }
    }
}

int main()
{
    printf("[sdl-log-gating] Starting\n");

#ifndef FREE_API_SRC_DIR
#error "FREE_API_SRC_DIR must be defined by CMakeLists.txt"
#endif

    const fs::path srcRoot = FREE_API_SRC_DIR;
    Check(fs::exists(srcRoot) && fs::is_directory(srcRoot),
          "the configured free-api src/ directory exists and is scannable");

    int filesScanned = 0;
    if (fs::exists(srcRoot)) {
        for (const auto& entry : fs::recursive_directory_iterator(srcRoot)) {
            if (entry.is_regular_file() && entry.path().extension() == ".cpp") {
                ScanFile(srcRoot, entry.path());
                ++filesScanned;
            }
        }
    }

    char scannedMsg[128];
    snprintf(scannedMsg, sizeof(scannedMsg), "scanned %d .cpp files under src/", filesScanned);
    Check(filesScanned > 10, scannedMsg);

    if (g_failures > 0) {
        printf("[sdl-log-gating] %d FAILURE(S)\n", g_failures);
        return 1;
    }

    printf("[sdl-log-gating] ALL TESTS PASSED\n");
    return 0;
}
