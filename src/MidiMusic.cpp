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
 * - Looping is not supported (TODO); playback stops at end-of-song.
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

#include <SDL3/SDL.h>

#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>
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

/* MCI error codes (not in the project mmsystem.h subset). */
// MCIERR_* constants are now defined in mmsystem.h
// Keep local aliases for backward compatibility within this file.
#ifndef MCIERR_UNSUPPORTED_FUNCTION
static constexpr MCIERROR MCIERR_UNSUPPORTED_FUNCTION = 268;
#endif
#ifndef MCIERR_INVALID_DEVICE_ID
static constexpr MCIERROR MCIERR_INVALID_DEVICE_ID    = 259;
#endif
#ifndef MCIERR_INTERNAL
static constexpr MCIERROR MCIERR_INTERNAL              = 305;
#endif

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
    std::mutex       mtx;
    tsf*             soundFont   = nullptr;
    SDL_AudioDeviceID device     = 0;
    SDL_AudioStream*  stream     = nullptr;
    std::thread       thread;
    std::atomic<bool> running{false};

    /* Active sessions indexed by MCIDEVICEID. */
    std::vector<MidiSession> sessions;
    MCIDEVICEID nextId = 1;

    /* Global volume: 0.0 – 1.0 */
    float volume = 1.0f;

    /**
     * @brief Destructor: stops the mixer thread and releases SDL audio resources.
     *
     * Called automatically when the static g_midi object is destroyed at program
     * exit.  Without this, std::thread's destructor calls std::terminate() (SIGABRT)
     * if the thread is still joinable.
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

static MidiState g_midi;

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
 * @note Status: IMPLEMENTED
 */
static std::string NormalizeMidiPath(const char* raw)
{
    if (!raw) return {};

    auto fileExists = [](const std::string& path) -> bool {
        return ::access(path.c_str(), F_OK) == 0;
    };

    /* Step 1 – convert backslashes. */
    std::string s(raw);
    for (char& c : s) {
        if (c == '\\') c = '/';
    }

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
 * Runs until g_midi.running is set to false.
 * @note Status: IMPLEMENTED
 */
static void MixerThread()
{
    /* Stereo float buffer for one render block. */
    std::vector<float> pcm(static_cast<size_t>(kBlockFrames) * 2);

    while (g_midi.running.load()) {
        /* Check if anything is playing. */
        MidiSession* active = nullptr;
        {
            std::lock_guard<std::mutex> lk(g_midi.mtx);
            for (auto& s : g_midi.sessions) {
                if (s.playing && !s.finished) {
                    active = &s;
                    break;
                }
            }
        }

        if (!active) {
            SDL_Delay(10);
            continue;
        }

        /* Render one block. */
        {
            std::lock_guard<std::mutex> lk(g_midi.mtx);

            /* Re-check under lock. */
            if (!active->playing || active->finished) {
                continue;
            }

            const double blockMs = (kBlockFrames * 1000.0) / kMixSpec.freq;

            /* Process MIDI events up to the end of this block. */
            while (active->cursor &&
                   active->cursor->time <= active->timeMs + blockMs)
            {
                tsf_channel_set_pan(g_midi.soundFont,
                                    active->cursor->channel, 0.5f);

                switch (active->cursor->type) {
                    case TML_PROGRAM_CHANGE:
                        tsf_channel_set_presetnumber(
                            g_midi.soundFont,
                            active->cursor->channel,
                            active->cursor->program,
                            (active->cursor->channel == 9));
                        break;
                    case TML_NOTE_ON:
                        tsf_channel_note_on(
                            g_midi.soundFont,
                            active->cursor->channel,
                            active->cursor->key,
                            static_cast<float>(active->cursor->velocity) / 127.0f);
                        break;
                    case TML_NOTE_OFF:
                        tsf_channel_note_off(
                            g_midi.soundFont,
                            active->cursor->channel,
                            active->cursor->key);
                        break;
                    case TML_PITCH_BEND:
                        tsf_channel_set_pitchwheel(
                            g_midi.soundFont,
                            active->cursor->channel,
                            active->cursor->pitch_bend);
                        break;
                    case TML_CONTROL_CHANGE:
                        tsf_channel_midi_control(
                            g_midi.soundFont,
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
            tsf_set_output(g_midi.soundFont, TSF_STEREO_INTERLEAVED,
                           kMixSpec.freq, 0.0f);
            tsf_render_float(g_midi.soundFont, pcm.data(), kBlockFrames, 0);

            /* Apply volume. */
            const float vol = g_midi.volume;
            if (vol != 1.0f) {
                for (float& s : pcm) s *= vol;
            }

            /* End of song? */
            if (active->cursor == nullptr) {
                active->playing  = false;
                active->finished = true;

                /* Post MM_MCINOTIFY to the registered window. */
                if (active->notifyHwnd) {
                    MIDI_LOG("MCI_PLAY finished, posting MM_MCINOTIFY to HWND %p",
                             static_cast<void*>(active->notifyHwnd));
                    PostMessageA(active->notifyHwnd, MM_MCINOTIFY,
                                 MCI_NOTIFY_SUCCESSFUL,
                                 static_cast<LPARAM>(active->id));
                }
            }
        }

        /* Submit PCM to SDL. */
        SDL_PutAudioStreamData(
            g_midi.stream,
            pcm.data(),
            static_cast<int>(pcm.size() * sizeof(float)));
    }

    MIDI_LOG("mixer thread exiting");
}

/* -------------------------------------------------------------------------- */
/*  Backend initialisation / teardown                                         */
/* -------------------------------------------------------------------------- */

/**
 * @brief Ensures the SDL audio device and mixer thread are running.
 * @return true on success.
 * @note Status: IMPLEMENTED
 */
static bool EnsureMidiBackend()
{
    if (g_midi.device != 0) return true; /* already open */

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("[midi] SDL_InitSubSystem(AUDIO) failed: %s", SDL_GetError());
        return false;
    }

    g_midi.stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &kMixSpec, nullptr, nullptr);

    if (!g_midi.stream) {
        SDL_Log("[midi] SDL_OpenAudioDeviceStream failed: %s", SDL_GetError());
        return false;
    }

    g_midi.device = SDL_GetAudioStreamDevice(g_midi.stream);
    SDL_ResumeAudioStreamDevice(g_midi.stream);

    /* Load SoundFont. */
    g_midi.soundFont = LoadSoundFont();
    if (g_midi.soundFont) {
        tsf_set_output(g_midi.soundFont, TSF_STEREO_INTERLEAVED,
                       kMixSpec.freq, 0.0f);
        tsf_set_volume(g_midi.soundFont, 1.0f);
    } else {
        MIDI_LOG("audio backend started without SoundFont; music will stay silent");
        return true;
    }

    /* Start mixer thread. */
    g_midi.running.store(true);
    g_midi.thread = std::thread(MixerThread);

    MIDI_LOG("audio backend started (device id=%u)", (unsigned)g_midi.device);
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

    std::lock_guard<std::mutex> lk(g_midi.mtx);
    g_midi.volume = gain;

    if (g_midi.soundFont) {
        tsf_set_volume(g_midi.soundFont, gain);
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
 * Supported commands: MCI_OPEN, MCI_PLAY, MCI_STOP (same as MCI_CLOSE here), MCI_CLOSE, MCI_SET.
 * "cdaudio" device type is gracefully declined with MCIERR_UNSUPPORTED_FUNCTION.
 *
 * @note Status: PARTIAL
 *   - MCI_PLAY looping: TODO
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
            std::lock_guard<std::mutex> lk(g_midi.mtx);
            MidiSession sess;
            sess.id       = g_midi.nextId++;
            sess.filename = path;
            sess.song     = song;
            sess.cursor   = song;
            parms->wDeviceID = sess.id;
            g_midi.sessions.push_back(std::move(sess));
        }

        MIDI_LOG("MCI_OPEN: assigned device id=%u", (unsigned)parms->wDeviceID);
        return 0; /* success */
    }

    if (uMsg == MCI_PLAY) {
        auto* parms = reinterpret_cast<MCI_PLAY_PARMS*>(dwParam);

        std::lock_guard<std::mutex> lk(g_midi.mtx);
        for (auto& s : g_midi.sessions) {
            if (s.id == mciId) {
                /* Stop any current playback on other sessions. */
                for (auto& other : g_midi.sessions) {
                    if (other.id != mciId) {
                        other.playing = false;
                    }
                }

                if (g_midi.soundFont) {
                    tsf_reset(g_midi.soundFont);
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

                if (!g_midi.soundFont) {
                    s.playing = false;
                    s.finished = true;
                    if (s.notifyHwnd) {
                        PostMessageA(s.notifyHwnd, MM_MCINOTIFY,
                                     MCI_NOTIFY_SUCCESSFUL,
                                     static_cast<LPARAM>(s.id));
                    }
                    MIDI_LOG("MCI_PLAY: no SoundFont loaded; treating device id=%u as silent success",
                             (unsigned)mciId);
                    return 0;
                }

                MIDI_LOG("MCI_PLAY: device id=%u notifyHwnd=%p",
                         (unsigned)mciId, static_cast<void*>(s.notifyHwnd));
                return 0;
            }
        }
        MIDI_LOG("MCI_PLAY: unknown device id=%u", (unsigned)mciId);
        return MCIERR_INVALID_DEVICE_ID;
    }

    if (uMsg == MCI_CLOSE) {
        std::lock_guard<std::mutex> lk(g_midi.mtx);
        for (auto it = g_midi.sessions.begin(); it != g_midi.sessions.end(); ++it) {
            if (it->id == mciId) {
                MIDI_LOG("MCI_CLOSE: device id=%u", (unsigned)mciId);
                it->playing = false;
                if (it->song) {
                    tml_free(it->song);
                    it->song   = nullptr;
                    it->cursor = nullptr;
                }
                if (g_midi.soundFont) {
                    tsf_reset(g_midi.soundFont);
                }
                g_midi.sessions.erase(it);
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
