/**
 * @file MidiMusic.cpp
 * @brief MIDI/MCI music backend for free-api using TinySoundFont + TinyMidiLoader over SDL3 audio.
 *
 * This module implements a narrow subset of the Windows MCI sequencer and WinMM midiOut APIs
 * sufficient to play MIDI music files used by the game.  The game opens MIDI files via
 * mciSendCommand(MCI_OPEN, ...) with lpstrDeviceType="sequencer", plays them via
 * mciSendCommand(MCI_PLAY, ...) with MCI_NOTIFY, and closes them via MCI_CLOSE.
 *
 * Architecture:
 * - One shared SDL3 audio output stream is opened on first use.
 * - A single background mixing thread advances MIDI time, renders PCM via TinySoundFont, and
 *   feeds SDL_PutAudioStreamData.
 * - midiOutSetVolume() adjusts a global gain applied during rendering.
 * - Native MCI-level looping is intentionally unimplemented: both target
 *   games' own MM_MCINOTIFY handlers re-issue playback themselves
 *   (MCI_CLOSE then MCI_OPEN+MCI_PLAY) whenever a song ends, so music
 *   genuinely loops end-to-end during real gameplay -- this is a
 *   deliberate scope decision, not a missing feature (TASK-24H-0901).
 * - CD audio (lpstrDeviceType="cdaudio") is gracefully declined.
 * - MCI_NOTIFY: when the song ends, MM_MCINOTIFY is posted to the callback HWND.
 *
 * SoundFont lookup order (first found wins):
 *   1. FREE_API_SOUNDFONT environment variable
 *   2. assets/soundfont/default.sf2
 *   3. soundfont/default.sf2
 *   If none found, MCI_OPEN succeeds but playback is silent with a debug warning.
 *
 * Debug logging: set FREE_API_DEBUG_MIDI=1 in the environment.
 *
 * @note Status: PARTIAL
 */

#define TSF_IMPLEMENTATION
#define TML_IMPLEMENTATION

#include "../external/tsf.h"
#include "../external/tml.h"

#include "mmsystem.h"
#include "windows.h"
#include "internal/FreeApiPath.hpp"

#include <SDL3/SDL.h>

#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

/* -------------------------------------------------------------------------- */
/*  Debug logging                                                              */
/* -------------------------------------------------------------------------- */

static bool midiDebugEnabled()
{
    static int cached = -1;
    if (cached < 0) {
        cached = SDL_getenv("FREE_API_DEBUG_MIDI") ? 1 : 0;
    }
    return cached != 0;
}

#define MIDI_LOG(fmt, ...) \
    do { if (midiDebugEnabled()) SDL_Log("[midi] " fmt, ##__VA_ARGS__); } while (0)

/* -------------------------------------------------------------------------- */
/*  Constants                                                                  */
/* -------------------------------------------------------------------------- */

/** SDL3 output format used by the mixer. */
static const SDL_AudioSpec kMixSpec = {SDL_AUDIO_F32, 2, 44100};

/** Size of each render block in frames. */
static constexpr int kBlockFrames = 512;

/* -------------------------------------------------------------------------- */
/*  SoundFont loader                                                           */
/* -------------------------------------------------------------------------- */

/** Candidate SoundFont paths tried in order. */
static const char* kSfCandidates[] = {
    nullptr,                        /* slot 0 — filled from env at runtime */
    "assets/soundfont/default.sf2",
    "soundfont/default.sf2",
    nullptr
};

static tsf* LoadSoundFont()
{
    kSfCandidates[0] = SDL_getenv("FREE_API_SOUNDFONT");

    for (int i = 0; kSfCandidates[i] != nullptr; ++i) {
        const char* path = kSfCandidates[i];
        if (!path || path[0] == '\0') continue;

        tsf* sf = tsf_load_filename(path);
        if (sf) {
            MIDI_LOG("SoundFont loaded from: %s", path);
            return sf;
        }
        MIDI_LOG("SoundFont not found at: %s", path);
    }

    SDL_Log("[midi] WARNING: No SoundFont found. Music will be silent. "
            "Set FREE_API_SOUNDFONT=/path/to/file.sf2 or place default.sf2 in "
            "assets/soundfont/ or soundfont/.");
    return nullptr;
}

/* -------------------------------------------------------------------------- */
/*  Mixer / renderer                                                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief Internal MIDI playback session.
 *
 * Created by MCI_OPEN, destroyed by MCI_CLOSE.
 */
struct MidiSession {
    MCIDEVICEID id = 0;
    std::string filename;
    tml_message* song     = nullptr; /*!< head of TinyMidiLoader message chain */
    tml_message* cursor   = nullptr; /*!< current playback position            */
    double       timeMs   = 0.0;     /*!< elapsed playback time in milliseconds */
    bool         playing  = false;
    bool         finished = false;
    HWND         notifyHwnd = nullptr; /*!< window to receive MM_MCINOTIFY     */
};

/**
 * @brief Shared MIDI audio state.
 *
 * Only one song plays at a time (the game never overlaps music tracks).
 */
struct MidiState {
    // TASK-24H-1233/1245: required lock order -- this mutex must never be
    // held while acquiring the message-queue mutex (i.e. never call
    // PostMessageA/PostQuitMessage while `mtx` is locked). Capture whatever
    // notify data is needed while holding `mtx`, release it, then post. See
    // MixerThread's and MidiMusicSendCommand's MCI_PLAY handler's matching
    // needsNotify/notifyTarget/notifyId pattern for the established shape --
    // apply the same pattern to any future call site added here.
    std::mutex       mtx;
    tsf*             soundFont   = nullptr;
    SDL_AudioDeviceID device     = 0;
    SDL_AudioStream*  stream     = nullptr;
    std::thread       thread;
    std::atomic<bool> running{false};

    /* TASK-24H-1109: latches true on the first EnsureMidiBackend() failure
     * so its failure log fires at most once per process instead of once per
     * MCI_OPEN call (one per song/track load -- many times per real
     * playthrough on a machine with no usable audio device). The failure is
     * still surfaced (not silenced) on that first occurrence; only the
     * repeat-logging is deduplicated. */
    bool backendInitFailed = false;

    /* Active sessions indexed by MCIDEVICEID. */
    std::vector<MidiSession> sessions;
    MCIDEVICEID nextId = 1;

    /* Global volume: 0.0 – 1.0 */
    float volume = 1.0f;

    /**
     * @brief Destructor: stops the mixer thread and releases SDL audio resources.
     *
     * Called automatically when the GetMidiState() function-local static is
     * destroyed at program exit.  Without this, std::thread's destructor calls
     * std::terminate() (SIGABRT) if the thread is still joinable.
     */
    ~MidiState() {
        /* Signal the mixer thread to stop and wait for it. */
        running.store(false);
        if (thread.joinable()) {
            thread.join();
        }

        /* Free all MIDI sessions. */
        for (auto& s : sessions) {
            if (s.song) {
                tml_free(s.song);
                s.song   = nullptr;
                s.cursor = nullptr;
            }
        }
        sessions.clear();

        /* Release TinySoundFont. */
        if (soundFont) {
            tsf_close(soundFont);
            soundFont = nullptr;
        }

        /* Release SDL audio stream and device. */
        if (stream) {
            SDL_DestroyAudioStream(stream);
            stream = nullptr;
            device = 0;
        }
    }
};

// TASK-24H-1232: a function-local static (Meyer's singleton) rather than a
// plain namespace-scope static. g_messageQueue/g_messageQueueMutex
// (FreeApiMessageQueue.cpp) are ordinary namespace-scope statics, whose
// dynamic initialization completes before main() runs; this object's first
// construction happens lazily, on the first call to GetMidiState() during
// main()'s execution, which the language guarantees completes strictly
// after all namespace-scope statics' construction. Per [basic.start.term],
// static-duration objects are destroyed in the reverse order their
// construction completed, and that ordering rule holds across the whole
// program, not just within one translation unit -- so this object's
// destructor (which stops and joins the mixer thread before releasing any
// of its own resources) is guaranteed to run before g_messageQueue/
// g_messageQueueMutex are destroyed, closing the link-order-dependent
// teardown race audit.md's Finding R1 described. See docs/out-of-scope.md
// for the fuller writeup.
static MidiState& GetMidiState()
{
    static MidiState instance;
    return instance;
}

/* -------------------------------------------------------------------------- */
/*  Path normalisation (backslash → forward slash, case-fallback)            */
/* -------------------------------------------------------------------------- */

/**
 * @brief Converts backslashes to forward slashes, then resolves the path.
 *
 * On case-sensitive file systems (Linux) the game-generated path uses lowercase
 * while the actual files on disk may be UPPERCASE. We try the path as-is first,
 * then retry uppercase variants for progressively larger path suffixes.
 *
 * TASK-24H-0706: step 1 now calls the shared FreeApi::Internal::NormalizeFilesystemPath
 * (src/internal/FreeApiPath.cpp) directly, matching TASK-24H-0705's precedent for
 * free_api_fopen, instead of a hand-rolled backslash-only loop. This does more than
 * the old loop did -- it also strips a leading drive-letter prefix ("X:", never
 * produced by either game's MIDI-path construction, a safe no-op here) and strips
 * leading slashes (relevant only if a caller ever passes an ABSOLUTE path). Verified
 * safe: both games' real MCI_OPEN element-name construction (CSound::PlayMusic,
 * GetCurrentDir()+strcat()) always produces a path relative to _pgmptr's directory,
 * and this project's own documented invocation (docs/target-game-verification.md)
 * runs both games via a relative path ("./bin/..."), so _pgmptr (argv[0] on the
 * non-Windows path both games actually run on) is relative and this never strips a
 * real leading slash in the documented, tested usage -- the same assumption
 * TASK-24H-0705 already made (and has run safely under) for free_api_fopen.
 *
 * @note Status: IMPLEMENTED
 */
static std::string NormalizeMidiPath(const char* raw)
{
    if (!raw) return {};

    auto fileExists = [](const std::string& path) -> bool {
        std::error_code ec;
        return std::filesystem::exists(path, ec);
    };

    /* Step 1 – shared prefix normalization (see doc comment above). */
    std::string s = FreeApi::Internal::NormalizeFilesystemPath(raw);

    /* Step 2 – try the path as-is (handles already-correct paths). */
    if (fileExists(s)) {
        return s;
    }

    /*
     * Step 3 – retry with uppercase fallbacks.
     *
     * We uppercase progressively larger suffixes of the path (from basename
     * to parent folders), e.g.:
     *   /.../bin/sound/music000.blp
     *   /.../bin/sound/MUSIC000.BLP
     *   /.../bin/SOUND/MUSIC000.BLP
     *
     * This keeps absolute path prefixes intact while handling uppercase asset
     * directories/files on case-sensitive file systems.
     */
    std::vector<size_t> componentStarts;
    componentStarts.push_back(0);
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '/' && i + 1 < s.size()) {
            componentStarts.push_back(i + 1);
        }
    }

    for (auto it = componentStarts.rbegin(); it != componentStarts.rend(); ++it) {
        std::string upper = s;
        for (size_t i = *it; i < upper.size(); ++i) {
            if (upper[i] != '/') {
                upper[i] = static_cast<char>(toupper(static_cast<unsigned char>(upper[i])));
            }
        }

        if (upper != s && fileExists(upper)) {
            MIDI_LOG("NormalizeMidiPath: using uppercase fallback '%s'", upper.c_str());
            return upper;
        }
    }

    /* Return original normalised path; tml_load_filename will report the error. */
    return s;
}

/* -------------------------------------------------------------------------- */
/*  Mixing thread                                                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Background thread: renders MIDI → PCM → SDL audio stream.
 *
 * Runs until GetMidiState().running is set to false.
 * @note Status: IMPLEMENTED
 */
static void MixerThread()
{
    /* Stereo float buffer for one render block. */
    std::vector<float> pcm(static_cast<size_t>(kBlockFrames) * 2);

    while (GetMidiState().running.load()) {
        bool rendered = false;

        // TASK-24H-1233: notify data (target HWND + device id) for a
        // just-finished song, captured while the lock below is held and
        // acted on (PostMessageA) only after it's released -- PostMessageA
        // acquires the message-queue mutex and does unrelated coalescing/
        // diagnostics work; it must never run while GetMidiState().mtx is
        // held (see the lock-order comment on GetMidiState().mtx's
        // declaration).
        bool needsNotify = false;
        HWND notifyTarget = nullptr;
        MCIDEVICEID notifyId = 0;

        // Find the active session and render its next block under a
        // single, uninterrupted lock acquisition. A prior version looked
        // up `active` in one lock_guard scope, released the lock, then
        // dereferenced it after re-acquiring a second lock_guard scope --
        // if MCI_CLOSE (which erases from GetMidiState().sessions) or MCI_OPEN
        // (whose push_back can reallocate the vector) ran on another
        // thread during that released-lock gap, `active` became a
        // dangling pointer. That was the real, intermittent SIGSEGV behind
        // this file's MCI_OPEN kill-switch (now removed).
        {
            std::lock_guard<std::mutex> lk(GetMidiState().mtx);

            MidiSession* active = nullptr;
            for (auto& s : GetMidiState().sessions) {
                if (s.playing && !s.finished) {
                    active = &s;
                    break;
                }
            }

            if (active) {
                rendered = true;
                const double blockMs = (kBlockFrames * 1000.0) / kMixSpec.freq;

                /* Process MIDI events up to the end of this block. */
                while (active->cursor &&
                       active->cursor->time <= active->timeMs + blockMs)
                {
                    tsf_channel_set_pan(GetMidiState().soundFont,
                                        active->cursor->channel, 0.5f);

                    switch (active->cursor->type) {
                        case TML_PROGRAM_CHANGE:
                            tsf_channel_set_presetnumber(
                                GetMidiState().soundFont,
                                active->cursor->channel,
                                active->cursor->program,
                                (active->cursor->channel == 9));
                            break;
                        case TML_NOTE_ON:
                            tsf_channel_note_on(
                                GetMidiState().soundFont,
                                active->cursor->channel,
                                active->cursor->key,
                                static_cast<float>(active->cursor->velocity) / 127.0f);
                            break;
                        case TML_NOTE_OFF:
                            tsf_channel_note_off(
                                GetMidiState().soundFont,
                                active->cursor->channel,
                                active->cursor->key);
                            break;
                        case TML_PITCH_BEND:
                            tsf_channel_set_pitchwheel(
                                GetMidiState().soundFont,
                                active->cursor->channel,
                                active->cursor->pitch_bend);
                            break;
                        case TML_CONTROL_CHANGE:
                            tsf_channel_midi_control(
                                GetMidiState().soundFont,
                                active->cursor->channel,
                                active->cursor->control,
                                active->cursor->control_value);
                            break;
                        default:
                            break;
                    }
                    active->cursor = active->cursor->next;
                }
                active->timeMs += blockMs;

                /* Render audio. */
                tsf_set_output(GetMidiState().soundFont, TSF_STEREO_INTERLEAVED,
                               kMixSpec.freq, 0.0f);
                tsf_render_float(GetMidiState().soundFont, pcm.data(), kBlockFrames, 0);

                /* Apply volume. */
                const float vol = GetMidiState().volume;
                if (vol != 1.0f) {
                    for (float& s : pcm) s *= vol;
                }

                /* End of song? */
                if (active->cursor == nullptr) {
                    active->playing  = false;
                    active->finished = true;

                    /* Capture MM_MCINOTIFY target/id; posted after the lock
                     * below is released. */
                    if (active->notifyHwnd) {
                        needsNotify  = true;
                        notifyTarget = active->notifyHwnd;
                        notifyId     = active->id;
                    }
                }
            }
        }

        if (needsNotify) {
            MIDI_LOG("MCI_PLAY finished, posting MM_MCINOTIFY to HWND %p",
                     static_cast<void*>(notifyTarget));
            PostMessageA(notifyTarget, MM_MCINOTIFY,
                         MCI_NOTIFY_SUCCESSFUL,
                         static_cast<LPARAM>(notifyId));
        }

        if (rendered) {
            /* Submit PCM to SDL. */
            SDL_PutAudioStreamData(
                GetMidiState().stream,
                pcm.data(),
                static_cast<int>(pcm.size() * sizeof(float)));
        } else {
            SDL_Delay(10);
        }
    }

    MIDI_LOG("mixer thread exiting");
}

/* -------------------------------------------------------------------------- */
/*  Backend initialisation / teardown                                         */
/* -------------------------------------------------------------------------- */

/**
 * @brief Ensures the SDL audio device and mixer thread are running.
 *
 * On failure, the underlying SDL error is logged only on the FIRST call
 * that fails (TASK-24H-1109) -- every subsequent MCI_OPEN on a machine with
 * a persistently unusable audio device would otherwise re-invoke this
 * function and re-log the identical failure once per song/track load.
 * @return true on success.
 * @note Status: IMPLEMENTED
 */
static bool EnsureMidiBackend()
{
    if (GetMidiState().device != 0) return true; /* already open */
    if (GetMidiState().backendInitFailed) return false; /* already failed once; don't re-log */

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("[midi] SDL_InitSubSystem(AUDIO) failed: %s", SDL_GetError());
        GetMidiState().backendInitFailed = true;
        return false;
    }

    GetMidiState().stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &kMixSpec, nullptr, nullptr);

    if (!GetMidiState().stream) {
        SDL_Log("[midi] SDL_OpenAudioDeviceStream failed: %s", SDL_GetError());
        GetMidiState().backendInitFailed = true;
        return false;
    }

    GetMidiState().device = SDL_GetAudioStreamDevice(GetMidiState().stream);
    SDL_ResumeAudioStreamDevice(GetMidiState().stream);

    /* Load SoundFont. */
    GetMidiState().soundFont = LoadSoundFont();
    if (GetMidiState().soundFont) {
        tsf_set_output(GetMidiState().soundFont, TSF_STEREO_INTERLEAVED,
                       kMixSpec.freq, 0.0f);
        tsf_set_volume(GetMidiState().soundFont, 1.0f);
    } else {
        MIDI_LOG("audio backend started without SoundFont; music will stay silent");
        return true;
    }

    /* Start mixer thread. */
    GetMidiState().running.store(true);
    GetMidiState().thread = std::thread(MixerThread);

    MIDI_LOG("audio backend started (device id=%u)", (unsigned)GetMidiState().device);
    return true;
}

/* -------------------------------------------------------------------------- */
/*  Public API — called from winapi.cpp                                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief Returns 1 if the MIDI backend can initialise, 0 otherwise.
 * @note Status: IMPLEMENTED
 */
UINT MidiMusicGetNumDevs()
{
    /* Attempt a lightweight init check without allocating the full backend. */
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return 0;
    return 1;
}

/**
 * @brief Opens a MIDI output device (dummy handle; real rendering is through MCI).
 * @note Status: IMPLEMENTED
 */
MMRESULT MidiMusicOutOpen(LPHMIDIOUT phmo)
{
    if (phmo) {
        *phmo = reinterpret_cast<HMIDIOUT>(static_cast<uintptr_t>(1));
    }
    return MMSYSERR_NOERROR;
}

/**
 * @brief Sets the global MIDI/music volume from a WinMM packed left/right word.
 *
 * WinMM packs left channel in the low 16 bits and right in the high 16 bits.
 * Both channels are typically equal for music; we take the average.
 * Range: 0x0000–0xFFFF per channel → mapped to 0.0–1.0 linear gain.
 *
 * @note Status: IMPLEMENTED
 */
MMRESULT MidiMusicSetVolume(DWORD dwVolume)
{
    const float left  = static_cast<float>(dwVolume & 0xFFFF) / 65535.0f;
    const float right = static_cast<float>((dwVolume >> 16) & 0xFFFF) / 65535.0f;
    const float gain  = (left + right) * 0.5f;

    std::lock_guard<std::mutex> lk(GetMidiState().mtx);
    GetMidiState().volume = gain;

    if (GetMidiState().soundFont) {
        tsf_set_volume(GetMidiState().soundFont, gain);
    }

    MIDI_LOG("midiOutSetVolume: left=%.3f right=%.3f → gain=%.3f", left, right, gain);
    return MMSYSERR_NOERROR;
}

/**
 * @brief Closes the dummy MIDI output handle.
 * @note Status: IMPLEMENTED
 */
MMRESULT MidiMusicOutClose()
{
    return MMSYSERR_NOERROR;
}

/**
 * @brief Handles mciSendCommand for MIDI sequencer / CD-audio / generic MCI.
 *
 * Supported commands: MCI_OPEN, MCI_PLAY, MCI_CLOSE, MCI_SET. MCI_STOP is
 * neither defined in include/ nor handled here -- no evidenced call site in
 * either target game; an MCI_STOP call would fall through to the unhandled-
 * message branch below and return MCIERR_UNSUPPORTED_FUNCTION (TASK-24H-0908).
 * "cdaudio" device type is gracefully declined with MCIERR_UNSUPPORTED_FUNCTION.
 *
 * @note Status: PARTIAL
 *   - Native MCI-level looping (MCI_PLAY with a loop flag) is intentionally
 *     unimplemented: both target games' MM_MCINOTIFY handlers re-issue
 *     playback themselves on song-end, so looping works end-to-end during
 *     real gameplay without it (TASK-24H-0901).
 *   - MCI_SET: accepted, no-op (CD audio time-format; not relevant for MIDI)
 */
MCIERROR MidiMusicSendCommand(MCIDEVICEID mciId, UINT uMsg,
                               DWORD_PTR fdwCommand, DWORD_PTR dwParam)
{
    MIDI_LOG("mciSendCommand id=%u msg=0x%04X flags=0x%08X",
             (unsigned)mciId, uMsg, (unsigned)fdwCommand);

    if (uMsg == MCI_OPEN) {
        auto* parms = reinterpret_cast<MCI_OPEN_PARMSA*>(dwParam);
        if (!parms) return MCIERR_INTERNAL;

        const char* devType = parms->lpstrDeviceType ? parms->lpstrDeviceType : "";

        /* Decline CD audio — the game handles this branch already. */
        if (SDL_strcasecmp(devType, "cdaudio") == 0) {
            MIDI_LOG("MCI_OPEN: cdaudio requested — declined (not implemented)");
            return MCIERR_UNSUPPORTED_FUNCTION;
        }

        const bool isSequencer = (SDL_strcasecmp(devType, "sequencer") == 0)
                               || (fdwCommand & MCI_OPEN_ELEMENT);

        std::string path;
        if (fdwCommand & MCI_OPEN_ELEMENT) {
            path = NormalizeMidiPath(parms->lpstrElementName);
        }

        MIDI_LOG("MCI_OPEN: type='%s' file='%s'", devType, path.c_str());

        if (!isSequencer) {
            MIDI_LOG("MCI_OPEN: unknown device type '%s' — declined", devType);
            return MCIERR_UNSUPPORTED_FUNCTION;
        }

        if (!EnsureMidiBackend()) {
            return MCIERR_INTERNAL;
        }

        /* Load MIDI file. */
        tml_message* song = nullptr;
        if (!path.empty()) {
            song = tml_load_filename(path.c_str());
            if (!song) {
                SDL_Log("[midi] MCI_OPEN: failed to load MIDI file '%s'", path.c_str());
                return MCIERR_INTERNAL;
            }
            MIDI_LOG("MCI_OPEN: MIDI loaded '%s'", path.c_str());
        } else {
            MIDI_LOG("MCI_OPEN: no element name; creating empty session");
        }

        /* Register session. */
        {
            std::lock_guard<std::mutex> lk(GetMidiState().mtx);
            MidiSession sess;
            sess.id       = GetMidiState().nextId++;
            sess.filename = path;
            sess.song     = song;
            sess.cursor   = song;
            parms->wDeviceID = sess.id;
            GetMidiState().sessions.push_back(std::move(sess));
        }

        MIDI_LOG("MCI_OPEN: assigned device id=%u", (unsigned)parms->wDeviceID);
        return 0; /* success */
    }

    if (uMsg == MCI_PLAY) {
        auto* parms = reinterpret_cast<MCI_PLAY_PARMS*>(dwParam);

        // TASK-24H-1245: notify data captured while GetMidiState().mtx is
        // held, acted on (PostMessageA) only after the lock below is
        // released -- see the lock-order comment on GetMidiState().mtx's
        // declaration and MixerThread's matching capture-then-post pattern
        // (this is the second call site TASK-24H-1233 left untouched).
        bool needsNotify = false;
        HWND notifyTarget = nullptr;
        MCIDEVICEID notifyId = 0;
        bool sessionFound = false;
        bool silentSuccess = false;

        {
            std::lock_guard<std::mutex> lk(GetMidiState().mtx);
            for (auto& s : GetMidiState().sessions) {
                if (s.id == mciId) {
                    sessionFound = true;

                    /* Stop any current playback on other sessions. */
                    for (auto& other : GetMidiState().sessions) {
                        if (other.id != mciId) {
                            other.playing = false;
                        }
                    }

                    if (GetMidiState().soundFont) {
                        tsf_reset(GetMidiState().soundFont);
                    }

                    s.cursor   = s.song;
                    s.timeMs   = 0.0;
                    s.playing  = true;
                    s.finished = false;

                    if (parms && (fdwCommand & MCI_NOTIFY)) {
                        s.notifyHwnd = reinterpret_cast<HWND>(parms->dwCallback);
                    } else {
                        s.notifyHwnd = nullptr;
                    }

                    if (!GetMidiState().soundFont) {
                        s.playing = false;
                        s.finished = true;
                        silentSuccess = true;
                        if (s.notifyHwnd) {
                            needsNotify  = true;
                            notifyTarget = s.notifyHwnd;
                            notifyId     = s.id;
                        }
                    } else {
                        MIDI_LOG("MCI_PLAY: device id=%u notifyHwnd=%p",
                                 (unsigned)mciId, static_cast<void*>(s.notifyHwnd));
                    }
                    break;
                }
            }
        }

        if (needsNotify) {
            PostMessageA(notifyTarget, MM_MCINOTIFY,
                         MCI_NOTIFY_SUCCESSFUL,
                         static_cast<LPARAM>(notifyId));
        }

        if (sessionFound) {
            if (silentSuccess) {
                MIDI_LOG("MCI_PLAY: no SoundFont loaded; treating device id=%u as silent success",
                         (unsigned)mciId);
            }
            return 0;
        }

        MIDI_LOG("MCI_PLAY: unknown device id=%u", (unsigned)mciId);
        return MCIERR_INVALID_DEVICE_ID;
    }

    if (uMsg == MCI_CLOSE) {
        std::lock_guard<std::mutex> lk(GetMidiState().mtx);
        for (auto it = GetMidiState().sessions.begin(); it != GetMidiState().sessions.end(); ++it) {
            if (it->id == mciId) {
                MIDI_LOG("MCI_CLOSE: device id=%u", (unsigned)mciId);
                it->playing = false;
                if (it->song) {
                    tml_free(it->song);
                    it->song   = nullptr;
                    it->cursor = nullptr;
                }
                if (GetMidiState().soundFont) {
                    tsf_reset(GetMidiState().soundFont);
                }
                GetMidiState().sessions.erase(it);
                return 0;
            }
        }
        /* Unknown id on CLOSE is not fatal. */
        MIDI_LOG("MCI_CLOSE: unknown device id=%u (ignored)", (unsigned)mciId);
        return 0;
    }

    if (uMsg == MCI_SET) {
        /* The game uses MCI_SET only for CD-audio time format; ignore for MIDI. */
        MIDI_LOG("MCI_SET: id=%u (no-op)", (unsigned)mciId);
        return 0;
    }

    MIDI_LOG("mciSendCommand: unhandled msg=0x%04X", uMsg);
    return MCIERR_UNSUPPORTED_FUNCTION;
}

/**
 * @brief Converts an MCI error code to a human-readable string.
 * @note Status: PARTIAL
 */
BOOL MidiMusicGetErrorString(MCIERROR mcierr, LPSTR pszText, UINT cchText)
{
    if (!pszText || cchText == 0) return FALSE;

    switch (mcierr) {
        case 0:                               SDL_snprintf(pszText, cchText, "Success"); break;
        case MCIERR_UNSUPPORTED_FUNCTION:     SDL_snprintf(pszText, cchText, "Unsupported MCI function"); break;
        case MCIERR_INVALID_DEVICE_ID:        SDL_snprintf(pszText, cchText, "Invalid MCI device ID"); break;
        case MCIERR_INTERNAL:                 SDL_snprintf(pszText, cchText, "Internal MCI error"); break;
        default:                              SDL_snprintf(pszText, cchText, "MCI error %lu", (unsigned long)mcierr); break;
    }
    return TRUE;
}
