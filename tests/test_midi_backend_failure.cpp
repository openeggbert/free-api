/**
 * @file test_midi_backend_failure.cpp
 * @brief Regression test for TASK-24H-1109: EnsureMidiBackend() (src/MidiMusic.cpp)
 * must log a persistent audio-backend-init failure at most ONCE per process,
 * not once per MCI_OPEN call.
 *
 * Deliberately a standalone executable, not folded into test_mci_sequences.cpp:
 * this test forces a REAL SDL audio init failure by setting SDL_AUDIODRIVER to
 * an invalid driver name before SDL is touched at all. EnsureMidiBackend's
 * failure latch (g_midi.backendInitFailed) is permanent for the life of the
 * process once tripped -- running this in the same binary as other MCI tests
 * would permanently break every MCI_OPEN("sequencer") call for any test that
 * happened to run afterward. Isolating it in its own process avoids that.
 */
#include <windows.h>
#include <digitalv.h>
#include <SDL3/SDL.h>
#include <atomic>
#include <cstdio>
#include <cstring>

static int g_failures = 0;

static void Check(bool condition, const char* what)
{
    if (condition) {
        printf("[midi-backend-failure] PASS: %s\n", what);
    } else {
        printf("[midi-backend-failure] FAIL: %s\n", what);
        ++g_failures;
    }
}

static std::atomic<int> g_backendFailureLogCount{0};

static void SDLCALL CountBackendFailureLogs(void* userdata, int category, SDL_LogPriority priority, const char* message)
{
    (void)category;
    (void)priority;
    (void)userdata;
    // Matches EnsureMidiBackend's exact failure message prefixes
    // (src/MidiMusic.cpp: "[midi] SDL_InitSubSystem(AUDIO) failed: ..." or
    // "[midi] SDL_OpenAudioDeviceStream failed: ..."). Not gated behind
    // FREE_API_DEBUG_MIDI -- these are always-visible error logs by design
    // (TASK-24H-1109's "do not silence the failure entirely" rule), so no
    // env var setup is needed to observe them here.
    if (message && strstr(message, "[midi]") && strstr(message, "failed")) {
        g_backendFailureLogCount.fetch_add(1);
    }
}

static MCIERROR OpenSequencerBogusElement()
{
    // MCI_OPEN_ELEMENT must be set: mciSendCommandA (src/winmm.cpp:322-326)
    // intercepts MCI_OPEN_TYPE-only opens (no MCI_OPEN_ELEMENT) as an
    // "avivideo device-type probe" and short-circuits with
    // MCIERR_UNSUPPORTED_FUNCTION before MidiMusicSendCommand/
    // EnsureMidiBackend are ever reached -- both games' real movie-probing
    // code uses exactly that shape, so it must not be confused with a
    // sequencer open here. The element name doesn't need to point to a real
    // file: EnsureMidiBackend() is called and fails (src/MidiMusic.cpp:579)
    // before the MIDI file is ever loaded (:583-589), so this reaches and
    // exercises exactly the failure path under test either way.
    MCI_OPEN_PARMSA openParms{};
    openParms.wDeviceID = 0;
    openParms.lpstrDeviceType = const_cast<LPSTR>("sequencer");
    openParms.lpstrElementName = const_cast<LPSTR>("nonexistent_fixture_for_task_24h_1109.mid");
    return mciSendCommandA(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_ELEMENT,
                            reinterpret_cast<DWORD_PTR>(&openParms));
}

int main()
{
    printf("[midi-backend-failure] Starting\n");

    // Force a real, persistent SDL_InitSubSystem(SDL_INIT_AUDIO) failure --
    // must be set before SDL touches the audio subsystem at all. Video is
    // unaffected (independent subsystem), matching every other test's
    // SDL_Init(SDL_INIT_VIDEO) usage.
    SDL_setenv_unsafe("SDL_AUDIODRIVER", "totally_bogus_driver_for_task_24h_1109_test", 1);

    SDL_SetLogOutputFunction(CountBackendFailureLogs, nullptr);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("[midi-backend-failure] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    const int kAttempts = 5;
    int failureCount = 0;
    for (int i = 0; i < kAttempts; ++i) {
        MCIERROR rc = OpenSequencerBogusElement();
        if (rc != 0) ++failureCount;
    }

    Check(failureCount == kAttempts,
          "MCI_OPEN(\"sequencer\") fails on every call when the audio backend is persistently unavailable");
    Check(g_backendFailureLogCount.load() == 1,
          "EnsureMidiBackend's failure log fires exactly once across 5 MCI_OPEN calls, not once per call");

    SDL_SetLogOutputFunction(nullptr, nullptr);
    SDL_Quit();

    if (g_failures > 0) {
        printf("[midi-backend-failure] %d FAILURE(S)\n", g_failures);
        return 1;
    }

    printf("[midi-backend-failure] ALL TESTS PASSED\n");
    return 0;
}
