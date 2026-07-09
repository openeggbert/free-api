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
 * FreeApiGdiDebugEnabled(), midiDebugEnabled(), g_debugInput, or the local
 * `diag` variable (src/wingdi_dc.cpp).
 *
 * A short, explicit, file:line allowlist below covers the confirmed-
 * intentional unconditional failure/startup/warning-path logs (the same
 * sites TASK-24H-1101/1102/1106-1111/1227 individually reviewed and
 * documented as compliant, rare, or deliberately always-visible). Anything
 * else found unconditional is a real regression this test is meant to
 * catch.
 */
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
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

// file:line pairs (relative to src/) confirmed intentional/unconditional:
// failure paths, rare startup warnings, or an unsupported-input fallback
// warning. Each is a real call site reviewed this session (or by the
// TASK-24H-1101/1102/1106-1111/1227 gating sweep) and deliberately left
// unconditional -- not silenced behind a diagnostics flag -- because it
// signals a real problem or fires at most once per process.
static const std::set<std::pair<std::string, int>> kAllowlist = {
    {"winmm.cpp", 138},           // timeSetEvent: invalid args (failure)
    {"winmm.cpp", 154},           // timeSetEvent: SDL_INIT_EVENTS failed (failure)
    {"winmm.cpp", 173},           // timeSetEvent: SDL_AddTimer failed (failure)
    {"winmm.cpp", 214},           // timeKillEvent: unknown timer id (warning, not fatal)
    {"winmm.cpp", 350},           // mciSendCommandA: avivideo decline (documented, TASK-24H-1111)
    {"winbase_file.cpp", 55},     // _lopen: failed to open (failure)
    {"winbase_file.cpp", 142},    // CreateDirectoryA: failed to create (failure)
    {"MidiMusic.cpp", 110},       // No SoundFont found (always-visible startup warning)
    {"MidiMusic.cpp", 428},       // SDL_InitSubSystem(AUDIO) failed (once-per-process via backendInitFailed latch, TASK-24H-1109)
    {"MidiMusic.cpp", 438},       // SDL_OpenAudioDeviceStream failed (same latch)
    {"MidiMusic.cpp", 585},       // MCI_OPEN: failed to load MIDI file (failure)
    {"internal/FreeApiSdlVideo.cpp", 21}, // EnsureVideoSubsystem: SDL_INIT_VIDEO failed (failure)
    {"wingdi_bitmap.cpp", 22},    // LoadImageA: resource bitmap loading not implemented (unsupported-input)
    {"wingdi_bitmap.cpp", 35},    // LoadImageA: SDL_LoadBMP failed (failure)
    {"wingdi_bitmap.cpp", 180},   // CreateBitmap: unsupported bpp, pixels zeroed (fallback warning)
    {"internal/FreeApiGdi.cpp", 40}, // CreateCompatBitmapFromSurface: SDL_ConvertSurface failed (failure)
};

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
static const std::string kGatePattern =
    "(FreeApiDiagnosticsEnabled\\(\\)|FreeApiDiagnosticsFastEnabled\\(\\)|"
    "FreeApiGdiDebugEnabled\\(\\)|midiDebugEnabled\\(\\)|g_debugInput|\\bdiag\\b)";

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

        const int lineNo = static_cast<int>(i) + 1;
        if (kAllowlist.count({relPath, lineNo})) continue;

        if (!IsGatedLine(lines, i)) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "ungated SDL_Log at %s:%d is neither behind a recognized "
                     "gate nor allowlisted: %s",
                     relPath.c_str(), lineNo, Trim(raw).c_str());
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
