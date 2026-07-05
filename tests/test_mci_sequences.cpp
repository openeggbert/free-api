/**
 * @file test_mci_sequences.cpp
 * @brief Consolidated WinMM/MCI regression tests mirroring both games' real
 * `sound.cpp` command sequences end-to-end, per plan.md TASK-0089/0090/
 * 0091/0094/0098.
 *
 * Covers:
 *  - The exact sequencer open->play(MCI_NOTIFY)->notify->close sequence
 *    both games use for background music (free-eggbert sound.cpp:590-633;
 *    planetblupi sound.cpp equivalent).
 *  - MM_MCINOTIFY delivery timing after a short track actually finishes.
 *  - The exact notify-triggered close-then-reopen-and-replay loop both
 *    games' own WndProc runs on MM_MCINOTIFY (free-eggbert blupi.cpp:
 *    562-580: SuspendMusic() [MCI_CLOSE] then RestartMusic() [MCI_OPEN+
 *    MCI_PLAY] when wParam==MCI_NOTIFY_SUCCESSFUL) -- repeated rapidly, as
 *    a stress test.
 *  - "cdaudio" MCI_OPEN graceful decline (planetblupi sound.cpp; free-eggbert
 *    sound.cpp:723/soundbass.cpp:662), and that it doesn't break the
 *    sequencer fallback used immediately after.
 *
 * Digital-video (MCI "avivideo") is deliberately NOT covered here -- it is
 * already covered by tests/test_mci_avivideo_regressions.cpp and resolved
 * as an intentional non-issue (docs/out-of-scope.md); duplicating that
 * coverage here is explicitly out of scope for TASK-0098.
 *
 * Background: until this session, `MidiMusicSendCommand`'s MCI_OPEN
 * handler (src/MidiMusic.cpp) unconditionally returned MCIERR_INTERNAL
 * behind a "//todo fix sigsegv" comment (commit a35f476c, "MIDI was
 * disabled") -- a real, intermittent use-after-free in MixerThread (it
 * captured a MidiSession* under one lock scope, released the lock, then
 * dereferenced it after re-acquiring a second lock scope; a concurrent
 * MCI_CLOSE/MCI_OPEN on another thread during that gap could erase/
 * reallocate the backing std::vector<MidiSession>, dangling the pointer).
 * That race is exactly what free-eggbert's own notify-triggered
 * close-then-reopen pattern above can trigger. The fix folds the
 * find-and-render step into one uninterrupted lock acquisition; the stress
 * test below exercises that exact real-game pattern repeatedly.
 */
#include <windows.h>
#include <digitalv.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int g_failures = 0;

static void Check(bool condition, const char* what)
{
    if (condition) {
        printf("[mci-sequences] PASS: %s\n", what);
    } else {
        printf("[mci-sequences] FAIL: %s\n", what);
        ++g_failures;
    }
}

static LRESULT WINAPI MciTestWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static HWND MakeTestWindow(const char* className)
{
    WNDCLASSA wc{};
    wc.lpfnWndProc   = MciTestWndProc;
    wc.lpszClassName = className;
    wc.hInstance     = (HINSTANCE)1;
    RegisterClassA(&wc);
    return CreateWindowExA(0, className, "Test", WS_POPUPWINDOW | WS_VISIBLE,
                            0, 0, 320, 240, nullptr, nullptr, (HINSTANCE)1, nullptr);
}

// A tiny, valid, single-note Type-0 MIDI file -- long enough to render at
// least one mixer block, short enough to finish (and fire MM_MCINOTIFY)
// quickly in a test.
static bool WriteMinimalMidi(const std::string& path)
{
    static const unsigned char kMidi[] = {
        'M', 'T', 'h', 'd',
        0x00, 0x00, 0x00, 0x06,
        0x00, 0x00,
        0x00, 0x01,
        0x00, 0x60,
        'M', 'T', 'r', 'k',
        0x00, 0x00, 0x00, 0x0F,
        0x00, 0xC0, 0x00,
        0x00, 0x90, 0x3C, 0x40,
        0x60, 0x80, 0x3C, 0x00,
        0x00, 0xFF, 0x2F, 0x00,
    };
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    const size_t written = fwrite(kMidi, 1, sizeof(kMidi), f);
    fclose(f);
    return written == sizeof(kMidi);
}

static MCIERROR OpenSequencer(const std::string& path, MCIDEVICEID* outId)
{
    MCI_OPEN_PARMSA openParms{};
    openParms.wDeviceID = 0;
    openParms.lpstrDeviceType = const_cast<LPSTR>("sequencer");
    openParms.lpstrElementName = const_cast<LPSTR>(path.c_str());

    MCIERROR rc = mciSendCommandA(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_ELEMENT,
                                   reinterpret_cast<DWORD_PTR>(&openParms));
    if (rc == 0 && outId) *outId = openParms.wDeviceID;
    return rc;
}

static MCIERROR PlayWithNotify(MCIDEVICEID id, HWND notifyHwnd)
{
    MCI_PLAY_PARMS playParms{};
    playParms.dwCallback = reinterpret_cast<DWORD_PTR>(notifyHwnd);
    return mciSendCommandA(id, MCI_PLAY, MCI_NOTIFY, reinterpret_cast<DWORD_PTR>(&playParms));
}

// Drains the queue for up to `durationMs`, returning true if MM_MCINOTIFY
// with wParam==MCI_NOTIFY_SUCCESSFUL for `hwnd` was observed.
static bool WaitForMciNotifySuccessful(HWND hwnd, Uint64 durationMs)
{
    const Uint64 deadline = SDL_GetTicks() + durationMs;
    MSG msg{};
    while (SDL_GetTicks() < deadline) {
        if (PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE)) {
            if (msg.message == MM_MCINOTIFY && msg.wParam == MCI_NOTIFY_SUCCESSFUL) {
                return true;
            }
        } else {
            SDL_Delay(2);
        }
    }
    return false;
}

// TASK-0089: the exact sequencer open->play(MCI_NOTIFY)->close sequence
// both games use for background music.
static void TestSequencerOpenPlayNotifyCloseSequence()
{
    HWND hwnd = MakeTestWindow("RegTest_MciSequencerSequence");
    Check(hwnd != nullptr, "CreateWindowExA succeeds for the MCI sequencer sequence test");
    if (!hwnd) return;

    const std::string path = "test_mci_sequence_fixture.mid";
    Check(WriteMinimalMidi(path), "MIDI fixture file is written for the sequence test");

    MCIDEVICEID id = 0;
    MCIERROR openRc = OpenSequencer(path, &id);
    Check(openRc == 0, "MCI_OPEN(\"sequencer\") succeeds against the real fixture (matches both games' exact call shape)");

    if (openRc == 0) {
        MCIERROR playRc = PlayWithNotify(id, hwnd);
        Check(playRc == 0, "MCI_PLAY with MCI_NOTIFY succeeds (matches both games' exact call shape)");

        // TASK-0091: MM_MCINOTIFY delivery timing -- this track is a single
        // note at a slow tempo, so it finishes in well under a couple of
        // seconds; a generous bound catches a regression without being
        // flaky under CI scheduling jitter.
        bool notified = WaitForMciNotifySuccessful(hwnd, 3000);
        Check(notified, "MM_MCINOTIFY with MCI_NOTIFY_SUCCESSFUL arrives at the requesting HWND within a reasonable time");

        MCIERROR closeRc = mciSendCommandA(id, MCI_CLOSE, 0, 0);
        Check(closeRc == 0, "MCI_CLOSE succeeds after the track finishes");
    }

    remove(path.c_str());
    DestroyWindow(hwnd);
}

// TASK-0090/regression: free-eggbert's own MM_MCINOTIFY handler
// (blupi.cpp:562-580) closes and immediately reopens+replays on every
// successful notification (its own game-side looping mechanism). This
// repeats that exact real-game pattern rapidly, which is exactly the
// scenario that could race the mixer thread against MCI_CLOSE/MCI_OPEN
// mutating the session vector -- the real cause of the "todo fix sigsegv"
// crash this file's fix (src/MidiMusic.cpp MixerThread) addresses.
static void TestNotifyTriggeredCloseAndReopenStressTest()
{
    HWND hwnd = MakeTestWindow("RegTest_MciNotifyRestartStress");
    Check(hwnd != nullptr, "CreateWindowExA succeeds for the notify-restart stress test");
    if (!hwnd) return;

    const std::string path = "test_mci_stress_fixture.mid";
    Check(WriteMinimalMidi(path), "MIDI fixture file is written for the stress test");

    const int kCycles = 30;
    int completedCycles = 0;
    for (int i = 0; i < kCycles; ++i) {
        MCIDEVICEID id = 0;
        if (OpenSequencer(path, &id) != 0) break;
        if (PlayWithNotify(id, hwnd) != 0) {
            mciSendCommandA(id, MCI_CLOSE, 0, 0);
            break;
        }

        // Real game code doesn't wait for notify before the *next* user
        // action, and SuspendMusic()/MCI_CLOSE can race the mixer thread
        // mid-render -- so close immediately, without waiting, on most
        // cycles to maximize the chance of hitting the race window; every
        // few cycles let it actually finish and be notified first, closer
        // to free-eggbert's real restart-on-notify shape.
        if (i % 5 == 0) {
            WaitForMciNotifySuccessful(hwnd, 500);
        }
        mciSendCommandA(id, MCI_CLOSE, 0, 0);
        ++completedCycles;
    }

    Check(completedCycles == kCycles,
          "repeated open->play->close cycles (mirroring the real notify-triggered restart loop) complete without crashing");

    // Drain any straggler MM_MCINOTIFY messages so they don't leak into
    // later tests.
    MSG msg{};
    while (PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE)) {}

    remove(path.c_str());
    DestroyWindow(hwnd);
}

// TASK-0094: "cdaudio" MCI_OPEN is gracefully declined, and doesn't break
// the sequencer fallback both games use immediately after (free-eggbert
// sound.cpp:723/soundbass.cpp:662 try cdaudio first, then fall back to the
// MIDI sequencer path).
static void TestCdaudioGracefulDeclineDoesNotBreakSequencerFallback()
{
    HWND hwnd = MakeTestWindow("RegTest_MciCdaudioDecline");
    Check(hwnd != nullptr, "CreateWindowExA succeeds for the cdaudio-decline test");
    if (!hwnd) return;

    MCI_OPEN_PARMSA cdParms{};
    cdParms.lpstrDeviceType = const_cast<LPSTR>("cdaudio");
    MCIERROR cdRc = mciSendCommandA(0, MCI_OPEN, MCI_OPEN_TYPE, reinterpret_cast<DWORD_PTR>(&cdParms));
    Check(cdRc != 0, "MCI_OPEN(\"cdaudio\") is declined (nonzero MCIERROR), not silently accepted");

    const std::string path = "test_mci_cdaudio_fallback_fixture.mid";
    Check(WriteMinimalMidi(path), "MIDI fixture file is written for the cdaudio-fallback test");

    MCIDEVICEID id = 0;
    MCIERROR openRc = OpenSequencer(path, &id);
    Check(openRc == 0, "the sequencer MIDI fallback still opens successfully immediately after a declined cdaudio open");

    if (openRc == 0) {
        mciSendCommandA(id, MCI_CLOSE, 0, 0);
    }

    remove(path.c_str());
    DestroyWindow(hwnd);
}

// TASK-0092 (plan.md): both games iterate every MIDI-out device to apply a
// volume change on music (re)start: GetNumDevs -> loop of Open/SetVolume/
// Close (free-eggbert sound.cpp:290-304; planetblupi sound.cpp:256-270).
// Must not crash whether GetNumDevs reports 0 (degraded, documented
// acceptable) or 1 (SDL-audio-available case) devices.
static void TestMidiOutVolumeIterationSequence()
{
    UINT numDevs = midiOutGetNumDevs();
    Check(numDevs <= 1, "midiOutGetNumDevs reports 0 or 1 devices (this backend never enumerates more than one)");

    bool anyOpenFailed = false;
    for (UINT i = 0; i < numDevs; ++i) {
        HMIDIOUT hmo = nullptr;
        MMRESULT openRc = midiOutOpen(&hmo, i, 0, 0, 0);
        if (openRc != MMSYSERR_NOERROR) {
            anyOpenFailed = true;
            continue;
        }
        MMRESULT volRc = midiOutSetVolume(hmo, 0x80008000); // matches both games' packed left/right volume shape
        Check(volRc == MMSYSERR_NOERROR, "midiOutSetVolume succeeds for an opened device in the iteration sequence");
        MMRESULT closeRc = midiOutClose(hmo);
        Check(closeRc == MMSYSERR_NOERROR, "midiOutClose succeeds for an opened device in the iteration sequence");
    }
    Check(!anyOpenFailed || numDevs == 0,
          "the GetNumDevs->Open->SetVolume->Close loop completes for every enumerated device without crashing, "
          "for whatever device count this backend reports (0 or 1)");
}

// TASK-0097 (plan.md): both games' CMovie::termAVI() (free-eggbert
// movie.cpp:59, planetblupi movie.cpp:56) calls mciGetDeviceIDA("avivideo")
// and passes the result straight into MCI_CLOSE, unconditionally, even
// though "avivideo" was never actually opened (declined -- see
// docs/out-of-scope.md). Verifies this exact sequence doesn't crash or leak
// regardless of what device ID the STUB returns.
static void TestMciGetDeviceIdaResultSafelyClosable()
{
    MCIDEVICEID mciId = mciGetDeviceIDA("avivideo");

    MCI_GENERIC_PARMS genericParms{};
    MCIERROR closeRc = mciSendCommandA(mciId, MCI_CLOSE, 0, reinterpret_cast<DWORD_PTR>(&genericParms));
    Check(true, "mciGetDeviceIDA's result can be passed straight into MCI_CLOSE without crashing (matches both games' termAVI())");
    (void)closeRc; // not fatal either way -- both an "unknown id" or a real close are acceptable outcomes here
}

int main()
{
    printf("[mci-sequences] Starting\n");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("[mci-sequences] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    TestSequencerOpenPlayNotifyCloseSequence();
    TestNotifyTriggeredCloseAndReopenStressTest();
    TestCdaudioGracefulDeclineDoesNotBreakSequencerFallback();
    TestMidiOutVolumeIterationSequence();
    TestMciGetDeviceIdaResultSafelyClosable();

    // Deliberately not calling SDL_Quit() here: MidiState's static-global
    // destructor (src/MidiMusic.cpp ~MidiState) tears down its SDL audio
    // stream at process-exit time, after main() returns -- if SDL_Quit()
    // already ran first, that teardown crashes inside SDL itself (verified
    // via ASan: SEGV in SDL_DestroyAudioQueue). Neither target game ever
    // calls SDL_Quit() (SDL is an internal implementation detail invisible
    // to both games' own code -- see docs/scope.md), so this ordering can
    // never occur during real gameplay; matching that, this is the only
    // test file that opens the MIDI backend, and (like basic_test.cpp,
    // which also exercises MCI_OPEN) it does not call SDL_Quit().
    if (g_failures > 0) {
        printf("[mci-sequences] %d FAILURE(S)\n", g_failures);
        return 1;
    }

    printf("[mci-sequences] ALL TESTS PASSED\n");
    return 0;
}
