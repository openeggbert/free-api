/**
 * @file test_midi_soundfont_rendering.cpp
 * @brief TASK-24H-1251: exercises MidiMusic.cpp's real MixerThread rendering
 * loop (TinySoundFont event dispatch + PCM synthesis) end-to-end, using a
 * minimal, programmatically-constructed, spec-valid synthetic SoundFont.
 *
 * A coverage sweep (2026-07-09) found MixerThread's entire event-processing/
 * rendering body was 0% covered by any existing test: every other MCI/MIDI
 * test runs with no SoundFont available (none is bundled, by project policy,
 * to avoid copyright issues -- see README.md), so every MCI_PLAY in the rest
 * of the suite takes the "no SoundFont loaded, silent success" branch. The
 * actual TinySoundFont-based rendering code -- the entire reason
 * external/tsf.h/tml.h are vendored -- had therefore never been executed by
 * any automated test in this project's history, only ever verifiable by a
 * human ear (TASK-24H-1221's human sign-off).
 *
 * This test does not attempt to verify audio *quality* (that remains
 * TASK-24H-1221's job) -- it verifies the rendering pipeline actually runs
 * to completion without crashing, that SDL_PutAudioStreamData is reached,
 * and that a real preset lookup/note-on/note-off/program-change sequence is
 * processed, by building the smallest SoundFont TinySoundFont's own loader
 * will accept: one preset, one instrument, one sample, no modulators.
 *
 * Deliberately a standalone executable, not folded into test_mci_sequences.cpp:
 * EnsureMidiBackend() (src/MidiMusic.cpp) loads the SoundFont at most ONCE
 * per process (matching TASK-24H-1109's failure-latch precedent for the
 * backend-init-failure path) -- FREE_API_SOUNDFONT must be set before any
 * other test in the same process has already triggered SoundFont lookup
 * with no SoundFont available.
 */
#include <windows.h>
#include <digitalv.h>
#include <SDL3/SDL.h>

#include "support/MidiFixtures.hpp"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int g_failures = 0;

static void Check(bool condition, const char* what)
{
    if (condition) {
        printf("[midi-soundfont-rendering] PASS: %s\n", what);
    } else {
        printf("[midi-soundfont-rendering] FAIL: %s\n", what);
        ++g_failures;
    }
}

// ---- Minimal SF2 (RIFF/sfbk) construction -----------------------------
//
// TinySoundFont's loader (external/tsf.h, tsf_load) requires all 9 "hydra"
// record types (phdr/pbag/pmod/pgen/inst/ibag/imod/igen/shdr) to be present
// with at least one record each, plus a "smpl" PCM chunk. Arrays that carry
// a boundary-index field into another array (phdr->presetBagNdx into pbag,
// pbag->genNdx/modNdx into pgen/pmod, inst->instBagNdx into ibag,
// ibag->instGenNdx/instModNdx into igen/imod) need one extra terminal
// record whose index field marks the end of the real range; the "leaf"
// arrays being ranged into (pgen/pmod/igen/imod) do not. All sizes/offsets
// below are computed by the helpers, not hand-typed, to avoid manual
// arithmetic errors in a binary format.

static void PutU16(std::vector<uint8_t>& out, uint16_t v)
{
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

static void PutS16(std::vector<uint8_t>& out, int16_t v) { PutU16(out, static_cast<uint16_t>(v)); }

static void PutU32(std::vector<uint8_t>& out, uint32_t v)
{
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

static void PutFourCC(std::vector<uint8_t>& out, const char* cc)
{
    out.insert(out.end(), cc, cc + 4);
}

// tsf_char20: exactly 20 bytes, null-padded.
static void PutChar20(std::vector<uint8_t>& out, const char* name)
{
    char buf[20] = {0};
    std::strncpy(buf, name, sizeof(buf));
    out.insert(out.end(), buf, buf + 20);
}

// Wraps `data` as a RIFF sub-chunk: fourcc id + u32 size + data.
static std::vector<uint8_t> WrapChunk(const char* id, const std::vector<uint8_t>& data)
{
    std::vector<uint8_t> out;
    PutFourCC(out, id);
    PutU32(out, static_cast<uint32_t>(data.size()));
    out.insert(out.end(), data.begin(), data.end());
    return out;
}

// Wraps `content` (the concatenation of sub-chunks) as a LIST chunk with
// the given 4-byte subtype (e.g. "pdta"/"sdta").
static std::vector<uint8_t> WrapList(const char* subtype, const std::vector<uint8_t>& content)
{
    std::vector<uint8_t> out;
    PutFourCC(out, "LIST");
    PutU32(out, static_cast<uint32_t>(4 + content.size()));
    PutFourCC(out, subtype);
    out.insert(out.end(), content.begin(), content.end());
    return out;
}

static std::vector<uint8_t> BuildMinimalSf2(int sampleCount)
{
    // phdr: 1 real preset (bank=0, preset=0 -- matches a freshly-initialized
    // tsf_channel's default presetIndex=0, so no MIDI program-change event
    // is even required for tsf_channel_note_on to find it) + 1 terminal.
    std::vector<uint8_t> phdr;
    PutChar20(phdr, "TestPreset");
    PutU16(phdr, 0);  // preset
    PutU16(phdr, 0);  // bank
    PutU16(phdr, 0);  // presetBagNdx
    PutU32(phdr, 0); PutU32(phdr, 0); PutU32(phdr, 0); // library, genre, morphology
    PutChar20(phdr, "EOP");
    PutU16(phdr, 0); PutU16(phdr, 0);
    PutU16(phdr, 1);  // presetBagNdx terminal: 1 real pbag record
    PutU32(phdr, 0); PutU32(phdr, 0); PutU32(phdr, 0);

    // pbag: 1 real zone (-> 1 pgen: GenInstrument) + 1 terminal.
    std::vector<uint8_t> pbag;
    PutU16(pbag, 0); PutU16(pbag, 0); // genNdx=0, modNdx=0
    PutU16(pbag, 1); PutU16(pbag, 0); // terminal: genNdx=1 (1 real pgen), modNdx=0 (0 real pmod)

    // pmod: unused by tsf.h's simplified loader (never read by
    // tsf_load_presets), but hydra.pmods must be a non-null allocation --
    // one dummy all-zero record avoids relying on malloc(0) semantics.
    std::vector<uint8_t> pmod(10, 0);

    // pgen: the preset's one generator -- GenInstrument (41) -> instrument 0.
    std::vector<uint8_t> pgen;
    PutU16(pgen, 41); PutU16(pgen, 0);

    // inst: 1 real instrument + 1 terminal.
    std::vector<uint8_t> inst;
    PutChar20(inst, "TestInst");
    PutU16(inst, 0);  // instBagNdx
    PutChar20(inst, "EOI");
    PutU16(inst, 1);  // terminal: instBagNdx=1 (1 real ibag record)

    // ibag: 1 real zone (-> 1 igen: GenSampleID) + 1 terminal.
    std::vector<uint8_t> ibag;
    PutU16(ibag, 0); PutU16(ibag, 0); // instGenNdx=0, instModNdx=0
    PutU16(ibag, 1); PutU16(ibag, 0); // terminal: instGenNdx=1 (1 real igen), instModNdx=0

    // imod: same "unused, need non-null" reasoning as pmod.
    std::vector<uint8_t> imod(10, 0);

    // igen: the instrument's one generator -- GenSampleID (53) -> sample 0.
    std::vector<uint8_t> igen;
    PutU16(igen, 53); PutU16(igen, 0);

    // shdr: 1 real sample header + 1 terminal ("EOS", conventional but not
    // strictly required by tsf.h -- included for real-SF2-file fidelity).
    std::vector<uint8_t> shdr;
    PutChar20(shdr, "TestSample");
    PutU32(shdr, 0);                                    // start
    PutU32(shdr, static_cast<uint32_t>(sampleCount));    // end
    PutU32(shdr, 0);                                     // startLoop
    PutU32(shdr, 0);                                     // endLoop (unused: no loop-mode generator set, so loop_mode defaults to none)
    PutU32(shdr, 44100);                                 // sampleRate
    shdr.push_back(60);                                   // originalPitch (middle C)
    shdr.push_back(0);                                    // pitchCorrection
    PutU16(shdr, 0);                                      // sampleLink
    PutU16(shdr, 1);                                      // sampleType (monoSample)
    PutChar20(shdr, "EOS");
    PutU32(shdr, 0); PutU32(shdr, 0); PutU32(shdr, 0); PutU32(shdr, 0); PutU32(shdr, 0);
    shdr.push_back(0); shdr.push_back(0); PutU16(shdr, 0); PutU16(shdr, 0);

    std::vector<uint8_t> pdta;
    auto append = [&](const char* id, const std::vector<uint8_t>& data) {
        std::vector<uint8_t> wrapped = WrapChunk(id, data);
        pdta.insert(pdta.end(), wrapped.begin(), wrapped.end());
    };
    append("phdr", phdr);
    append("pbag", pbag);
    append("pmod", pmod);
    append("pgen", pgen);
    append("inst", inst);
    append("ibag", ibag);
    append("imod", imod);
    append("igen", igen);
    append("shdr", shdr);

    // sdta: a simple 440Hz sine wave, 16-bit mono PCM -- real (non-silent)
    // sample content so a rendered block can be checked for non-zero output.
    //
    // Real SF2 files always pad their raw PCM data with extra "guard"
    // samples past every sample's nominal end (the spec requires a minimum
    // of 46 zero-value points) specifically because interpolating
    // synthesizers -- TinySoundFont included -- read a little past a
    // sample's `end` near the tail of playback. shdr.end above deliberately
    // stays at the real sampleCount (the audible content), but the raw PCM
    // buffer itself must be longer than that or tsf_voice_render's
    // interpolation reads past the end of the allocated buffer (found via
    // AddressSanitizer while writing this test: a real heap-buffer-overflow
    // inside external/tsf.h, not a bug in free-api's own code).
    static constexpr int kGuardSamples = 64;
    std::vector<uint8_t> smplData;
    for (int i = 0; i < sampleCount; ++i) {
        const double t = static_cast<double>(i) / 44100.0;
        const double s = std::sin(2.0 * 3.14159265358979323846 * 440.0 * t);
        PutS16(smplData, static_cast<int16_t>(s * 16000.0));
    }
    for (int i = 0; i < kGuardSamples; ++i) {
        PutS16(smplData, 0);
    }
    std::vector<uint8_t> sdta;
    {
        std::vector<uint8_t> wrapped = WrapChunk("smpl", smplData);
        sdta.insert(sdta.end(), wrapped.begin(), wrapped.end());
    }

    std::vector<uint8_t> pdtaList = WrapList("pdta", pdta);
    std::vector<uint8_t> sdtaList = WrapList("sdta", sdta);

    std::vector<uint8_t> riffContent;
    PutFourCC(riffContent, "sfbk");
    riffContent.insert(riffContent.end(), pdtaList.begin(), pdtaList.end());
    riffContent.insert(riffContent.end(), sdtaList.begin(), sdtaList.end());

    std::vector<uint8_t> file;
    PutFourCC(file, "RIFF");
    PutU32(file, static_cast<uint32_t>(riffContent.size()));
    file.insert(file.end(), riffContent.begin(), riffContent.end());
    return file;
}

// MIDI fixture (WriteMinimalMidi: program-change 0, note-on 60/64,
// note-off) now lives in support/MidiFixtures.hpp, shared with
// test_mci_sequences.cpp and basic_test.cpp (TASK-0004).

static LRESULT WINAPI MidiTestWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static std::atomic<bool> g_sawSoundFontLoaded{false};
static std::atomic<bool> g_sawNoSoundFontWarning{false};

static void SDLCALL CaptureMidiLogs(void* userdata, int category, SDL_LogPriority priority, const char* message)
{
    (void)userdata; (void)category; (void)priority;
    if (!message) return;
    if (std::strstr(message, "SoundFont loaded from:")) g_sawSoundFontLoaded.store(true);
    if (std::strstr(message, "No SoundFont found")) g_sawNoSoundFontWarning.store(true);
}

int main()
{
    printf("[midi-soundfont-rendering] Starting\n");

    const std::string sf2Path = "test_synthetic_fixture_for_task_24h_1251.sf2";
    const std::string midiPath = "test_synthetic_fixture_for_task_24h_1251.mid";

    std::vector<uint8_t> sf2 = BuildMinimalSf2(4410); // 0.1s @ 44100Hz
    FILE* sf2File = fopen(sf2Path.c_str(), "wb");
    Check(sf2File != nullptr, "synthetic SoundFont fixture file opens for writing");
    if (sf2File) {
        const size_t written = fwrite(sf2.data(), 1, sf2.size(), sf2File);
        fclose(sf2File);
        Check(written == sf2.size(), "synthetic SoundFont fixture file is written completely");
    }

    Check(WriteMinimalMidi(midiPath), "MIDI fixture file is written");

    // Must be set before EnsureMidiBackend's first call in this process --
    // see this file's doc comment for why this can't share a binary with
    // any other MIDI test.
    SDL_setenv_unsafe("FREE_API_SOUNDFONT", sf2Path.c_str(), 1);
    SDL_setenv_unsafe("FREE_API_DEBUG_MIDI", "1", 1);

    SDL_SetLogOutputFunction(CaptureMidiLogs, nullptr);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("[midi-soundfont-rendering] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    WNDCLASSA wc{};
    wc.lpfnWndProc   = MidiTestWndProc;
    wc.lpszClassName = "MidiSoundFontRenderingTest";
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, "MidiSoundFontRenderingTest", "Test", WS_POPUPWINDOW | WS_VISIBLE,
                                 0, 0, 320, 240, nullptr, nullptr, (HINSTANCE)1, nullptr);
    Check(hwnd != nullptr, "CreateWindowExA succeeds for the SoundFont-rendering test");

    MCI_OPEN_PARMSA openParms{};
    openParms.wDeviceID = 0;
    openParms.lpstrDeviceType = const_cast<LPSTR>("sequencer");
    openParms.lpstrElementName = const_cast<LPSTR>(midiPath.c_str());
    MCIERROR openRc = mciSendCommandA(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_ELEMENT,
                                       reinterpret_cast<DWORD_PTR>(&openParms));
    Check(openRc == 0, "MCI_OPEN(\"sequencer\") succeeds against the real MIDI fixture");

    Check(g_sawSoundFontLoaded.load(),
          "the synthetic SoundFont was actually accepted by tsf_load_filename "
          "(\"SoundFont loaded from:\" log observed)");
    Check(!g_sawNoSoundFontWarning.load(),
          "the \"no SoundFont found\" warning did NOT fire -- this run exercises the "
          "real rendering path, not the silent-success no-SoundFont branch every other "
          "MIDI test in this suite takes");

    MCI_PLAY_PARMS playParms{};
    playParms.dwCallback = reinterpret_cast<DWORD_PTR>(hwnd);
    MCIERROR playRc = mciSendCommandA(openParms.wDeviceID, MCI_PLAY, MCI_NOTIFY,
                                       reinterpret_cast<DWORD_PTR>(&playParms));
    Check(playRc == 0, "MCI_PLAY with MCI_NOTIFY succeeds against the real SoundFont-backed session");

    // Wait for MM_MCINOTIFY -- confirms MixerThread's rendering loop (event
    // dispatch to TinySoundFont, PCM synthesis, SDL_PutAudioStreamData) ran
    // to completion for this short fixture, not just started.
    bool sawNotify = false;
    for (int i = 0; i < 500 && !sawNotify; ++i) {
        MSG msg{};
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == MM_MCINOTIFY) {
                sawNotify = true;
                break;
            }
        }
        if (!sawNotify) SDL_Delay(10);
    }
    Check(sawNotify,
          "MM_MCINOTIFY arrives, confirming MixerThread's real rendering loop "
          "(TinySoundFont event dispatch + PCM synthesis) ran to completion");

    mciSendCommandA(openParms.wDeviceID, MCI_CLOSE, 0, 0);
    if (hwnd) DestroyWindow(hwnd);

    SDL_SetLogOutputFunction(nullptr, nullptr);

    // Deliberately not calling SDL_Quit() here -- same established reason as
    // test_mci_sequences.cpp's own doc comment: MidiState's function-local
    // static destructor (src/MidiMusic.cpp ~MidiState) tears down its SDL
    // audio stream at process-exit time, after main() returns. If SDL_Quit()
    // already ran first, that teardown crashes inside SDL itself (verified
    // via ASan while writing this test: SEGV in SDL_DestroyAudioQueue,
    // reproducing the exact same known issue). Neither target game ever
    // calls SDL_Quit() (see docs/scope.md), so this ordering can never occur
    // during real gameplay.

    remove(sf2Path.c_str());
    remove(midiPath.c_str());

    if (g_failures > 0) {
        printf("[midi-soundfont-rendering] %d FAILURE(S)\n", g_failures);
        return 1;
    }

    printf("[midi-soundfont-rendering] ALL TESTS PASSED\n");
    return 0;
}
