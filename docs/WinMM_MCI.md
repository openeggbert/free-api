# Free API WinMM and MCI Subset

This document explains the multimedia part of Free API: `mmsystem.h`, `digitalv.h`, multimedia timers, MIDI output, and MCI command handling.

## 1. Headers and responsibilities

| Header/source | Role |
|---|---|
| `include/mmsystem.h` | Public WinMM subset: timers, joystick structure, MIDI output functions, MCI command functions and constants. |
| `include/digitalv.h` | Public MCI Digital Video constants and structures. Mostly placeholder. |
| `src/winapi.cpp` | Implements public wrappers: `timeSetEvent`, `timeKillEvent`, `joyGetPosEx`, `joyGetNumDevs`, `midiOut*`, `mciSendCommandA`, `mciGetDeviceIDA`, `mciGetErrorStringA`. |
| `src/MidiMusic.cpp` | Implements the internal MIDI/MCI sequencer backend using TinyMidiLoader + TinySoundFont + SDL3 audio. |
| `src/MidiMusic.h` | Private declarations used by `winapi.cpp`; not public API. |

## 2. WinMM scalar types and structures

| Symbol | Meaning |
|---|---|
| `MMRESULT` | Result code type for multimedia functions. Defined as `UINT`. |
| `MCIDEVICEID` | MCI device identifier. Defined as `UINT`. |
| `MCIERROR` | MCI error code type. Defined as `DWORD`. |
| `HMIDIOUT` | Opaque MIDI output handle. Alias of `HANDLE`. |
| `LPHMIDIOUT` | Pointer to a MIDI output handle. |

### `JOYINFOEX`

`JOYINFOEX` mirrors the legacy WinMM joystick state layout: axes, buttons, POV, and reserved fields. The structure exists, but joystick input is not implemented.

### `WAVEFORMAT`

`WAVEFORMAT` exposes the classic base waveform format fields:

- `wFormatTag`
- `nChannels`
- `nSamplesPerSec`
- `nAvgBytesPerSec`
- `nBlockAlign`

It is currently a compatibility declaration, not a full waveform-audio implementation.

### MCI parameter structures

`mmsystem.h` exposes:

- `MCI_OPEN_PARMSA`
- `MCI_PLAY_PARMS`
- `MCI_SET_PARMS`

`digitalv.h` additionally exposes MCI Digital Video variants:

- `MCI_GENERIC_PARMS`
- `MCI_STATUS_PARMS`
- `MCI_DGV_OPEN_PARMSA`
- `MCI_DGV_WINDOW_PARMSA`
- `MCI_DGV_STATUS_PARMSA`
- `MCI_DGV_PLAY_PARMS`
- `MCI_DGV_PAUSE_PARMS`

The Digital Video structures are present so old source compiles. Actual video playback is not implemented.

## 3. Multimedia timer: `timeSetEvent` and `timeKillEvent`

### Public API

```c
MMRESULT WINAPI timeSetEvent(
    UINT uDelay,
    UINT uResolution,
    LPTIMECALLBACK lpTimeProc,
    DWORD_PTR dwUser,
    UINT fuEvent);

MMRESULT WINAPI timeKillEvent(UINT uTimerID);
```

### How it works

`timeSetEvent` creates an SDL timer with `SDL_AddTimer`. The SDL timer fires on an SDL timer thread and calls a small bridge function. That bridge calls the user-supplied `LPTIMECALLBACK`:

```text
timeSetEvent
    -> SDL_AddTimer
        -> FreeApiMmTimerBridge on SDL timer thread
            -> user LPTIMECALLBACK
```

The callback receives:

- `uTimerID`: the Free API timer ID
- `uMsg`: currently `0`
- `dwUser`: the user value passed to `timeSetEvent`, truncated to `DWORD` when passed to the callback type
- `dw1`, `dw2`: currently `0`

### Important behavior

- `uDelay == 0` or null callback returns `0` failure.
- `uResolution` is ignored.
- `fuEvent` is ignored except that this implementation behaves as a periodic timer. One-shot behavior is not implemented.
- `timeKillEvent` removes the SDL timer and erases internal state.
- The message queue is mutex-protected because timer callbacks may call `PostMessageA` from another thread.

### Why this matters for old games

Many old Win32 games use multimedia timers instead of normal `WM_TIMER` timers because they wanted steadier frame/update timing. A typical pattern is:

```c
id = timeSetEvent(50, 0, TimerStep, user, TIME_PERIODIC);
```

The callback then posts a custom update message to the main window. If `timeSetEvent` is only a stub, the game may create a window but never animate, render, or process input updates.

## 4. Joystick subset

| API | Behavior |
|---|---|
| `joyGetNumDevs()` | Returns `0`. |
| `joyGetPosEx(UINT, LPJOYINFOEX)` | Clears the structure and returns error-like value `1`. |

This intentionally reports no joystick support. Keyboard and mouse input are handled through SDL-to-WinAPI messages instead.

## 5. MIDI output subset

The public `midiOut*` functions are thin wrappers around the internal MIDI music module.

| API | Behavior |
|---|---|
| `midiOutGetNumDevs()` | Returns `1` if SDL audio can initialize, otherwise `0`. |
| `midiOutOpen(...)` | Sets a dummy MIDI handle and returns success. Real rendering happens through MCI sequencer playback. |
| `midiOutSetVolume(HMIDIOUT, DWORD)` | Reads packed left/right 16-bit WinMM volume and stores an average linear gain from `0.0` to `1.0`. |
| `midiOutClose(HMIDIOUT)` | Returns success; no per-handle teardown. |

Learning note: in classic WinMM, `midiOut*` is a lower-level MIDI output API. The game subset here mainly needs global MIDI availability and volume behavior; actual song playback is done by MCI.

## 6. MCI command API

### Public API

```c
MCIERROR WINAPI mciSendCommandA(
    MCIDEVICEID mciId,
    UINT uMsg,
    DWORD_PTR fdwCommand,
    DWORD_PTR dwParam);

MCIDEVICEID WINAPI mciGetDeviceIDA(LPCSTR lpszDevice);
BOOL WINAPI mciGetErrorStringA(MCIERROR mcierr, LPSTR pszText, UINT cchText);
```

### Main supported commands

| Command | Status | Behavior |
|---|---:|---|
| `MCI_OPEN` with `lpstrDeviceType="sequencer"` | Partial/implemented for MIDI | Loads the MIDI file and assigns an `MCIDEVICEID`. |
| `MCI_OPEN` with `MCI_OPEN_ELEMENT` | Partial/implemented for MIDI | Treats the element name as a MIDI file path. |
| `MCI_PLAY` | Partial/implemented | Starts playback for the opened MIDI session. Stops any other session. |
| `MCI_PLAY` with `MCI_NOTIFY` | Partial | Stores callback window and posts `MM_MCINOTIFY` when song ends. |
| `MCI_CLOSE` | Implemented | Stops playback, frees song data, removes session. Unknown IDs are ignored. |
| `MCI_SET` | Stub/no-op success | Accepted because the game uses it for CD-audio time format. |
| `cdaudio` device type | Stub/unsupported | Returns `MCIERR_UNSUPPORTED_FUNCTION`. |
| `avivideo` / device-type-only video open | Stub/unsupported | Returns `MCIERR_UNSUPPORTED_FUNCTION` without dereferencing a possibly truncated pointer. |
| Other MCI commands | Stub/unsupported | Returns `MCIERR_UNSUPPORTED_FUNCTION`. |

### Special `mciSendCommandA` guard for video

`src/winapi.cpp` contains a deliberate check for:

```text
uMsg == MCI_OPEN
fdwCommand has MCI_OPEN_TYPE
fdwCommand does not have MCI_OPEN_ELEMENT
```

This path is used by video initialization such as `avivideo`. The implementation rejects it before dereferencing `dwParam`. The code comment explains that some legacy source truncates a struct pointer to `DWORD`; dereferencing that on 64-bit Linux could crash. Returning `MCIERR_UNSUPPORTED_FUNCTION` lets the game disable movie playback gracefully.

## 7. MIDI backend architecture

The internal MIDI backend uses:

- `external/tml.h` — TinyMidiLoader
- `external/tsf.h` — TinySoundFont
- SDL3 audio stream
- One background mixer thread

```text
MCI_OPEN
    -> normalize path
    -> tml_load_filename
    -> create MidiSession

MCI_PLAY
    -> select active MidiSession
    -> reset TinySoundFont
    -> mixer thread advances MIDI events
    -> TinySoundFont renders float stereo PCM
    -> SDL_PutAudioStreamData

Song finished
    -> optionally PostMessageA(hwnd, MM_MCINOTIFY, MCI_NOTIFY_SUCCESSFUL, deviceId)

MCI_CLOSE
    -> free tml song
    -> remove MidiSession
```

### Audio format

The mixer uses stereo float output:

- Sample format: `SDL_AUDIO_F32`
- Channels: `2`
- Frequency: `44100 Hz`
- Block size: `512` frames

### Supported MIDI event types

The mixer handles:

- Program change
- Note on
- Note off
- Pitch bend
- Control change
- Channel 9 percussion preset selection

Unsupported or simplified features depend on what TinyMidiLoader/TinySoundFont expose and what the small mixer handles.

## 8. SoundFont lookup

A SoundFont 2 (`.sf2`) file is needed for real MIDI synthesis. The project does not bundle one.

Lookup order:

1. `FREE_API_SOUNDFONT` environment variable
2. `assets/soundfont/default.sf2`
3. `soundfont/default.sf2`

If no SoundFont is found, the code logs a warning. According to the current source, `MCI_OPEN` can still create a session, but playback effectively has no useful synthesis. For actual music, set `FREE_API_SOUNDFONT` or package a default `.sf2` file.

Example:

```bash
FREE_API_SOUNDFONT=/path/to/GeneralUser.sf2 ./game
```

## 9. MIDI path normalization

The internal `NormalizeMidiPath` function:

1. Converts backslashes to forward slashes.
2. Tries the path as-is.
3. Tries uppercase fallback for progressively larger suffixes.

Example fallback sequence:

```text
sound/music000.blp
sound/MUSIC000.BLP
SOUND/MUSIC000.BLP
```

This is useful on Linux when old game data uses uppercase file names but the game constructs lowercase Windows paths.

## 10. Digital Video subset

`digitalv.h` declares constants and structures for MCI Digital Video:

- `MCI_STATUS`
- `MCI_PAUSE`
- `MCI_DGV_OPEN_PARENT`
- `MCI_DGV_OPEN_WS`
- `MCI_DGV_STATUS_HWND`
- `MCI_DGV_PLAY_REVERSE`
- DGV open/window/status/play/pause structures

However, the actual implementation does not play AVI/digital video. It rejects video opening so the game can continue without videos.

## 11. MCI error strings

`mciGetErrorStringA` delegates to the internal MIDI backend and handles at least:

- `0` -> success
- `MCIERR_UNSUPPORTED_FUNCTION`
- `MCIERR_INVALID_DEVICE_ID`
- `MCIERR_INTERNAL`
- unknown error values -> formatted as `MCI error N`

## 12. Debugging multimedia

Set:

```bash
FREE_API_DEBUG_MIDI=1
```

This enables MIDI/MCI debug logging.

For timer/message-loop problems, also consider input logging:

```bash
FREE_API_DEBUG_INPUT=1
```

## 13. Known limitations and risks

- Only one MIDI song is intended to play at a time.
- Looping playback is not implemented.
- `MCI_PLAY` `from/to` ranges are not honored.
- `MCI_STATUS` is declared in `digitalv.h`, but not implemented for real query behavior.
- CD audio is not implemented.
- AVI/video playback is not implemented.
- No real MIDI output device is opened per `HMIDIOUT` handle.
- The multimedia timer callback runs on another thread; any game callback must avoid unsafe assumptions and normally should only queue messages or do thread-safe work.
