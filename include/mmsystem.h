/**
 * @file mmsystem.h
 * @brief WinMM compatibility subset used by the game runtime.
 * @note Status: PARTIAL
 *
 * MIDI/MCI music is implemented through TinySoundFont + TinyMidiLoader over SDL3 audio.
 * Supported MCI commands: MCI_OPEN (sequencer), MCI_PLAY, MCI_CLOSE, MCI_SET (no-op).
 * midiOut subset: GetNumDevs, Open, SetVolume, Close.
 * CD audio (cdaudio) is gracefully declined.
 */
#ifndef FREE_API_MMSYSTEM_H
#define FREE_API_MMSYSTEM_H

#include <windows.h>
#include <mmiscapi2.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Result code type returned by WinMM APIs.
 * @note Status: IMPLEMENTED
 */
typedef UINT MMRESULT;
/**
 * @brief Identifier type for MCI devices.
 * @note Status: PARTIAL
 */
// TASK-24H-0104/0905: canonical definition site -- include/mciapi.h checks
// this same guard instead of redefining. mmsystem.h is the more
// foundational, actually-included-by-both-games header; mciapi.h is
// currently unused by either game (only a commented-out include exists in
// free-eggbert's movie.cpp).
#ifndef FREE_API_MCIDEVICEID_DEFINED
#define FREE_API_MCIDEVICEID_DEFINED
typedef UINT MCIDEVICEID;
#endif
/**
 * @brief Error code type returned by MCI APIs.
 * @note Status: PARTIAL
 */
typedef DWORD MCIERROR;

/**
 * @brief Extended joystick state structure.
 *
 * Mirrors the legacy WinMM JOYINFOEX layout expected by old games.
 *
 * Only free-eggbert's event.cpp:2069-2125 reads this struct (planetblupi
 * never uses joysticks), and it only reads `dwSize`, `dwFlags`, `dwXpos`,
 * `dwYpos`, and `dwButtons` -- `dwZpos`/`dwRpos`/`dwUpos`/`dwVpos`/
 * `dwButtonNumber`/`dwPOV`/`dwReserved1`/`dwReserved2` are declared purely
 * for real Win32 struct-layout compatibility and are never read by either
 * game. Any future real joystick implementation (TASK-0103) only needs to
 * populate the five fields listed above.
 *
 * @note Status: IMPLEMENTED
 */
typedef struct tagJOYINFOEX {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwXpos;
    DWORD dwYpos;
    DWORD dwZpos;
    DWORD dwRpos;
    DWORD dwUpos;
    DWORD dwVpos;
    DWORD dwButtons;
    DWORD dwButtonNumber;
    DWORD dwPOV;
    DWORD dwReserved1;
    DWORD dwReserved2;
} JOYINFOEX, *PJOYINFOEX, *LPJOYINFOEX;

/**
 * @brief Legacy waveform format base structure.
 * @note Status: PARTIAL
 */
typedef struct _WAVEFORMAT {
    WORD wFormatTag;
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD nBlockAlign;
} WAVEFORMAT, *PWAVEFORMAT, *LPWAVEFORMAT;

// TASK-24H-0107: MCI_OPEN_PARMS/LPMCI_OPEN_PARMS and MCI_PLAY_PARMS/
// LPMCI_PLAY_PARMS are each guarded by a FREE_API_MCI_*_DEFINED macro here
// AND independently aliased to the differently-shaped MCI_DGV_* structs in
// digitalv.h. windows.h includes mmsystem.h (this file) before digitalv.h
// gets a chance to define its own alias, so THIS shape always wins,
// regardless of which header a caller directly includes -- deliberate-by-
// necessity, not accidental: both games' live MIDI/sequencer call sites
// (../free-eggbert/src/sound.cpp:574,713; ../planetblupi/src/sound.cpp:524)
// and src/MidiMusic.cpp:537,597's casts all use exactly this
// (lpstrDeviceType/lpstrElementName-shaped) layout. Confirmed via grep
// (this session) that neither game's AVI-probe code (movie.cpp) ever uses
// the bare MCI_OPEN_PARMS/MCI_PLAY_PARMS alias -- it always references
// MCI_DGV_OPEN_PARMS/LPMCI_DGV_OPEN_PARMS by their DGV-prefixed names
// directly, so no code path anywhere relies on this alias resolving to the
// MCI_DGV_* shape instead.
#ifndef FREE_API_MCI_OPEN_PARMS_DEFINED
#define FREE_API_MCI_OPEN_PARMS_DEFINED
typedef struct _MCI_OPEN_PARMSA {
    DWORD_PTR dwCallback;
    MCIDEVICEID wDeviceID;
    LPCSTR lpstrDeviceType;
    LPCSTR lpstrElementName;
    LPCSTR lpstrAlias;
} MCI_OPEN_PARMSA, *LPMCI_OPEN_PARMSA;

typedef MCI_OPEN_PARMSA MCI_OPEN_PARMS;
typedef LPMCI_OPEN_PARMSA LPMCI_OPEN_PARMS;
#endif

// See the MCI_OPEN_PARMS comment immediately above -- the same include-
// order-determined, confirmed-safe resolution applies here.
#ifndef FREE_API_MCI_PLAY_PARMS_DEFINED
#define FREE_API_MCI_PLAY_PARMS_DEFINED
typedef struct _MCI_PLAY_PARMS {
    DWORD_PTR dwCallback;
    DWORD_PTR dwFrom;
    DWORD_PTR dwTo;
} MCI_PLAY_PARMS, *LPMCI_PLAY_PARMS;
#endif

typedef struct _MCI_SET_PARMS {
    DWORD_PTR dwCallback;
    DWORD dwTimeFormat;
    DWORD dwAudio;
} MCI_SET_PARMS, *LPMCI_SET_PARMS;

/**
 * @brief Opaque MIDI output handle type.
 * @note Status: STUB
 */
typedef HANDLE HMIDIOUT;
typedef HMIDIOUT* LPHMIDIOUT;

/**
 * @name Timer flags and MM messages
 * @brief Constant subset used by the game.
 * @note Status: PARTIAL
 */
/** @{ */
#define TIME_PERIODIC 0x0001

#define MM_MCINOTIFY 0x03B9
#define MCI_NOTIFY_SUCCESSFUL 0x0001
/** @} */

/**
 * @name Joystick and MCI constants
 * @brief WinMM symbolic values consumed by legacy code.
 * @note Status: PARTIAL
 */
/** @{ */
#define JOY_BUTTON1 0x0001
#define JOY_BUTTON2 0x0002
#define JOY_BUTTON3 0x0004
#define JOY_BUTTON4 0x0008

/** @brief WinMM result codes subset. @note Status: PARTIAL */
#define MMSYSERR_NOERROR             0
#define MCIERR_INVALID_DEVICE_ID   259
#define MCIERR_UNSUPPORTED_FUNCTION 268
#define MCIERR_INTERNAL            305

/** @brief Joystick result codes (real Win32 values). @note Status: IMPLEMENTED */
#define JOYERR_NOERROR    0
#define JOYERR_PARMS    165
#define JOYERR_NOCANDO  166
#define JOYERR_UNPLUGGED 167

#define WAVE_FORMAT_PCM 1

// TASK-24H-0105/0905: these 7 constants are also declared identically in
// digitalv.h (which #include <windows.h>s this file transitively, so this
// definition always runs first) -- per-macro #ifndef guards make mmsystem.h
// the canonical source instead of an unguarded, drift-risking duplicate.
#ifndef MCI_OPEN
#define MCI_OPEN 0x0803
#endif
#ifndef MCI_CLOSE
#define MCI_CLOSE 0x0804
#endif
#ifndef MCI_PLAY
#define MCI_PLAY 0x0806
#endif
#define MCI_SET 0x080D
#ifndef MCI_NOTIFY
#define MCI_NOTIFY 0x00000001L
#endif
#ifndef MCI_WAIT
#define MCI_WAIT 0x00000002L
#endif
#ifndef MCI_OPEN_TYPE
#define MCI_OPEN_TYPE 0x00002000L
#endif
#define MCI_OPEN_TYPE_ID 0x00001000L
#ifndef MCI_OPEN_ELEMENT
#define MCI_OPEN_ELEMENT 0x00000200L
#endif
#define MCI_SET_TIME_FORMAT 0x00000400L
#define MCI_FORMAT_TMSF 10
#define MCI_TRACK 0x00000010L
/** @} */

/**
 * @brief Builds a little-endian FOURCC value.
 * @note Status: IMPLEMENTED
 */
#define mmioFOURCC(ch0, ch1, ch2, ch3) \
    ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) | ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24))

/**
 * @brief Starts a WinMM-style periodic timer.
 *
 * Internally mapped to a compatibility timer queue in `free-api`.
 *
 * TASK-24H-0503: `fuEvent`/`TIME_ONESHOT` is accepted but not honored --
 * every timer is always treated as periodic (`TIME_PERIODIC`) regardless of
 * the flag passed. `TIME_ONESHOT` has no defined constant in this header
 * because neither target game uses it; adding it is out of scope without an
 * evidenced call site.
 *
 * @note Status: PARTIAL
 */
MMRESULT WINAPI timeSetEvent(UINT uDelay,
                             UINT uResolution,
                             LPTIMECALLBACK lpTimeProc,
                             DWORD_PTR dwUser,
                             UINT fuEvent);

/**
 * @brief Stops a timer started by `timeSetEvent`.
 *
 * TASK-24H-0501: always returns a literal `1` on failure (unknown timer id,
 * or `uTimerID==0`), not a named `MMRESULT`/`TIMERR_*` constant -- an
 * informal, intentional convention, confirmed unused by either target
 * game's sole call site (a bare, return-value-discarding statement).
 *
 * @note Status: PARTIAL
 */
MMRESULT WINAPI timeKillEvent(UINT uTimerID);

/**
 * @brief Queries joystick state -- real, SDL_Joystick-backed (TASK-0103).
 * Populates `dwXpos`/`dwYpos` (SDL's signed -32768..32767 axis range
 * shifted to Win32's unsigned 0..65535, center 32768) and `dwButtons` bits
 * 0-3 (`JOY_BUTTON1`-`JOY_BUTTON4`) from the first 2 axes / 4 buttons of the
 * `uJoyID`-th connected device -- the only fields free-eggbert reads
 * (event.cpp:2069-2127). Returns `JOYERR_UNPLUGGED` if no such device is
 * connected (both games already handle this gracefully).
 * @note Status: IMPLEMENTED
 */
MMRESULT WINAPI joyGetPosEx(UINT uJoyID, LPJOYINFOEX pji);
/**
 * @brief Returns the number of connected joystick devices (SDL-backed).
 * @note Status: IMPLEMENTED
 */
UINT WINAPI joyGetNumDevs(void);

/**
 * @brief Returns number of MIDI output devices.
 *
 * Returns 1 if the SDL3 audio subsystem is available, 0 otherwise.
 * @note Status: IMPLEMENTED
 */
UINT WINAPI midiOutGetNumDevs(void);
/**
 * @brief Opens a MIDI output device handle.
 *
 * Returns a dummy handle; real audio rendering is handled through MCI.
 * @note Status: IMPLEMENTED
 */
MMRESULT WINAPI midiOutOpen(LPHMIDIOUT phmo, UINT uDeviceID, DWORD_PTR dwCallback, DWORD_PTR dwInstance, DWORD fdwOpen);
/**
 * @brief Sets MIDI output volume.
 *
 * Accepts a WinMM packed left/right 16-bit volume word and maps it to a
 * linear gain (0.0–1.0) applied to the TinySoundFont renderer.
 * @note Status: IMPLEMENTED
 */
MMRESULT WINAPI midiOutSetVolume(HMIDIOUT hmo, DWORD dwVolume);
/**
 * @brief Closes a MIDI output device handle.
 * @note Status: IMPLEMENTED
 */
MMRESULT WINAPI midiOutClose(HMIDIOUT hmo);

/**
 * @brief Sends an MCI command to an opened device.
 *
 * Supported commands:
 * - MCI_OPEN with lpstrDeviceType="sequencer": loads a MIDI file via TinyMidiLoader.
 * - MCI_PLAY: starts playback via TinySoundFont + SDL3 audio stream.
 * - MCI_CLOSE: stops playback and frees MIDI data.
 * - MCI_SET: accepted as no-op (CD-audio time-format; irrelevant for MIDI).
 * - cdaudio device type: gracefully declined with an error code.
 *
 * @note Status: PARTIAL
 * - Looping (MCI_PLAY looping flag): TODO
 * - CD audio: TODO
 */
// TASK-24H-0105/0905: canonical declaration site -- digitalv.h (which
// #include <windows.h>s this file transitively) checks this same guard
// instead of redeclaring an identical signature.
#ifndef FREE_API_MCISENDCOMMANDA_DECLARED
#define FREE_API_MCISENDCOMMANDA_DECLARED
MCIERROR WINAPI mciSendCommandA(MCIDEVICEID mciId, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam);
#define mciSendCommand mciSendCommandA
#endif
/**
 * @brief Converts an MCI error code to a human-readable string.
 * @note Status: IMPLEMENTED
 */
BOOL WINAPI mciGetErrorStringA(MCIERROR mcierr, LPSTR pszText, UINT cchText);

#define mciGetErrorString mciGetErrorStringA

#ifdef __cplusplus
}
#endif

#endif // FREE_API_MMSYSTEM_H