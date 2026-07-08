# Free API

![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)

**Free API** is an experimental C++ project that aims to reimplement a **minimal subset of the Windows API (WinAPI)**, roughly targeting functionality available around **~1998 (Win32 era)**.

The goal is to provide a **small, self-contained compatibility layer** that allows legacy applications and games to run **without depending on Microsoft Windows**.

---

## Overview

Free API provides a **WinAPI-compatible subset** implemented using **SDL3**:

```
WinAPI (subset ~1998)
        ↓
    Free API
        ↓
      SDL 3
```

* **Free API** → defines WinAPI-like headers and behavior
* **SDL 3** → internal implementation detail for windowing, events, and timing

---

## Scope

Free API targets exactly two games — **Free Eggbert** and **Planet Blupi** —
and implements only the WinAPI subset those two games actually use. It is not
Wine and not a general Win32 SDK reimplementation. See
[`docs/scope.md`](docs/scope.md) for the scope policy and [`plan.md`](plan.md)
for the full usage audit.

## Design Principles

* **WinAPI-like public headers** (windows.h, windef.h, etc.)
* **SDL3 used internally** as a portable backend
* Minimalism: only what is needed is implemented
* Focus on **real-world use cases** (legacy games support)

---

## Goals

* Recreate a **minimal subset of WinAPI (~1998)**
* Allow running legacy Win32 applications without Windows
* Provide a **portable runtime layer**
* Keep code **simple, readable, and hackable**
* Maintain **cross-platform support** via SDL3

---

## Features

### Application Entry
* `WinMain` abstraction (mapping to `main`)

### Windowing
* Window creation (`CreateWindowEx`)
* Message loop (`PeekMessage`, `DispatchMessage`)
* Window procedures (`WNDPROC`)

### Input (SDL3 → WinAPI message translation)
* **Mouse motion** — `SDL_EVENT_MOUSE_MOTION` → `WM_MOUSEMOVE` with `lParam` = packed `(y<<16)|x`
* **Mouse buttons** — `SDL_EVENT_MOUSE_BUTTON_DOWN/UP` → `WM_LBUTTONDOWN/UP`, `WM_RBUTTONDOWN/UP`, `WM_MBUTTONDOWN/UP`
* **Mouse button state** — `wParam` contains live `MK_LBUTTON`/`MK_RBUTTON`/`MK_MBUTTON`/`MK_SHIFT`/`MK_CONTROL` flags
* **Keyboard** — `SDL_EVENT_KEY_DOWN/UP` → `WM_KEYDOWN`/`WM_KEYUP` with `wParam` = `VK_*` code
  * Letters A–Z (uppercase VK codes), digits 0–9
  * Navigation: `VK_LEFT/RIGHT/UP/DOWN`, `VK_HOME`, `VK_END`, `VK_PRIOR`, `VK_NEXT`, `VK_INSERT`, `VK_DELETE`
  * Control: `VK_RETURN`, `VK_ESCAPE`, `VK_SPACE`, `VK_TAB`, `VK_BACK`, `VK_SHIFT`, `VK_CONTROL`, `VK_MENU`, `VK_PAUSE`
  * Function keys: `VK_F1`–`VK_F12` (F10 uses `WM_SYSKEYDOWN`/`WM_SYSKEYUP` matching Windows behavior)
* **Text input** — `SDL_EVENT_TEXT_INPUT` → `WM_CHAR` (for name entry screens)
* **Window focus** — `SDL_EVENT_WINDOW_FOCUS_GAINED` → `WM_ACTIVATEAPP(1)`; `SDL_EVENT_WINDOW_FOCUS_LOST` is suppressed (not translated to `WM_ACTIVATEAPP(0)`) to prevent games from entering inactive/suspended state due to spurious desktop focus changes
* **Debug logging** — set env `FREE_API_DEBUG_INPUT=1` (aliases: `FREE_API_DEBUG_MOUSE=1`, `FREE_API_DEBUG_REAL_INPUT=1`) at runtime to log all translated messages, ENQUEUE/DISPATCH events, per-event SDL `windowID` and resolved `HWND`, GetCursorPos and ScreenToClient calls.

#### Input pipeline notes
- Mouse and keyboard events are routed to the `HWND` matching the SDL `windowID` of the event; if no match (e.g. before window creation completes), they fall back to the active/first registered window so startup events are not silently dropped.
- `PeekMessageA` calls `SDL_PumpEvents()` + `PumpSdlEvents()` on **every** invocation (not only when the queue is empty), so real OS mouse/keyboard events cannot be starved by other queued messages staying ahead in the queue. This was the root cause of "test pipeline passes but real game does not see input" reports.
- The `WM_ACTIVATEAPP(0)` suppression is intentional: many SDL environments deliver spurious `FOCUS_LOST` at startup or in headless/virtual environments; sending deactivation would freeze games that guard rendering/input behind `g_bActive`.
- All input events flow through `PeekMessage` → `DispatchMessage` → `WndProc`; no direct callbacks bypass the WinAPI message queue.

#### Input pipeline test
An automated end-to-end test (`tests/test_input_pipeline.cpp`) injects SDL events programmatically and verifies the full pipeline:

```bash
cmake --build build --target test_input_pipeline
./build/bin/test_input_pipeline
# Expected output: [input-pipeline-test] ALL TESTS PASSED
```

Verified: `WM_MOUSEMOVE`, `WM_LBUTTONDOWN/UP`, `WM_RBUTTONDOWN`, `WM_KEYDOWN`/`WM_KEYUP` across a full `VK_*` sweep (letters, digits, arrows, function keys), the `F10`→`WM_SYSKEYDOWN`/`WM_SYSKEYUP` quirk, an unmapped-scancode-produces-no-message case, boundary/negative `lParam` packing, and a 2000-event sustained-input stress test.

### MIDI / MCI Music (`mmsystem.h`) — TinySoundFont + TinyMidiLoader

MIDI music playback is implemented through **TinySoundFont** (v0.9) + **TinyMidiLoader** (v0.7)
rendered to stereo float PCM and fed into an **SDL3 audio stream**.

#### Architecture

```
MCI_OPEN (sequencer, musicXXX.blp)
        ↓
TinyMidiLoader ─ loads MIDI Type 0/1 file
        ↓
Mixer thread ─ advances MIDI time, sends events to TinySoundFont
        ↓
TinySoundFont ─ synthesises stereo float PCM using a SoundFont .sf2
        ↓
SDL3 audio stream ─ one shared device opened on first use
```

#### SoundFont requirement

A **SoundFont 2 (.sf2)** file is required for audio synthesis.
No SoundFont is bundled (to avoid copyright issues).

Lookup order (first found wins):
1. `FREE_API_SOUNDFONT` environment variable
2. `assets/soundfont/default.sf2`
3. `soundfont/default.sf2`

If no SoundFont is found, `MCI_OPEN` still succeeds but playback is silent.
A warning is printed to the SDL log:
```
[midi] WARNING: No SoundFont found. Music will be silent.
Set FREE_API_SOUNDFONT=/path/to/file.sf2 or place default.sf2 in assets/soundfont/ or soundfont/.
```

Free SoundFonts suitable for testing:
- [GeneralUser GS](https://schristiancollins.com/generaluser.php) — freely redistributable, ~30 MB
- [FluidR3_GM](https://packages.debian.org/fluid-soundfont-gm) — available in Debian/Ubuntu packages

#### Supported MCI commands

| Command | Status |
|---------|--------|
| `MCI_OPEN` with `lpstrDeviceType="sequencer"` | Implemented |
| `MCI_PLAY` with `MCI_NOTIFY` | Implemented |
| `MCI_CLOSE` | Implemented |
| `MCI_SET` | Accepted, no-op |
| `cdaudio` device type | Gracefully declined |

#### midiOut subset

| Function | Status |
|----------|--------|
| `midiOutGetNumDevs` | Implemented (returns 1 if SDL audio available) |
| `midiOutOpen` | Implemented (dummy handle) |
| `midiOutSetVolume` | Implemented (maps WinMM packed volume to linear gain) |
| `midiOutClose` | Implemented |

#### Supported MIDI features

- MIDI Type 0 and Type 1 files
- Note on/off, program change, pitch bend, control change
- Channel 9 percussion
- Global volume via `midiOutSetVolume`

#### Limitations / TODOs

- Looping (`MCI_PLAY` with loop flag): TODO — playback stops at end-of-song
- CD audio (`cdaudio`): TODO — gracefully declined
- MCI_NOTIFY: posted when song ends (partial; no timeout handling)
- Concurrent music tracks: not supported (one at a time)
- Web (Emscripten): SDL3 audio may require a user gesture before audio starts

#### Debug logging

Set `FREE_API_DEBUG_MIDI=1` to enable per-call logging:
```bash
FREE_API_DEBUG_MIDI=1 ./SPEEDY_BLUPI_WINDOWS
```

#### Platform support

| Platform | Status |
|----------|--------|
| Linux | Supported via SDL3 |
| Windows | Supported via SDL3 (not WinMM real MIDI) |
| macOS | Supported via SDL3 |
| Android | Supported if SoundFont is packaged in assets |
| Web (Emscripten) | Supported; may need user gesture for audio |

#### Vendored libraries

The following single-header libraries are vendored under `external/`:

- `external/tsf.h` — [TinySoundFont v0.9](https://github.com/schellingb/TinySoundFont) by Bernhard Schelling (MIT)
- `external/tml.h` — [TinyMidiLoader v0.7](https://github.com/schellingb/TinySoundFont) by Bernhard Schelling (MIT)

---

### Multimedia Timer (`mmsystem.h`)
* `timeSetEvent` / `timeKillEvent` — implemented with `SDL_AddTimer` / `SDL_RemoveTimer`. The user-supplied `LPTIMECALLBACK` fires periodically on a private SDL timer thread.
* The internal WinAPI message queue (`g_messageQueue`) is mutex-protected because the timer thread is allowed to call `PostMessage` (e.g. legacy games posting `WM_TIMER`/`WM_UPDATE` from `TimerStep`).
* Without this implementation, games using `MMTIMER` mode (`timeSetEvent(50ms, TimerStep, …, TIME_PERIODIC)`) never received their tick, so they could not animate, redraw, or process input — even though raw SDL mouse/keyboard events were arriving and being translated correctly.

### System Utilities
* Timing (`GetTickCount`, `Sleep`)
* Debugging (`OutputDebugString`)

---

## Project Structure

```
free-api/
├── include/
│    ├── windows.h
│    ├── windef.h
│    ├── winnt.h
│    └── ...
├── src/
│    └── winapi.cpp
└── CMakeLists.txt
```

---

## Build Instructions

```bash
git clone https://github.com/openeggbert/free-api.git
cd free-api

cmake -B build
cmake --build build
```

This is the default, sibling-of-a-target-game build. For every other
supported build mode (standalone with a system-installed SDL3, building
inside `../free-eggbert` or `../planetblupi`, and the relevant CMake
options), see [`docs/cmake-options.md`](docs/cmake-options.md). For running
the test suite (including the required `SDL_VIDEODRIVER`/`SDL_AUDIODRIVER`
environment variables and sanitizer runs), see
[`docs/testing.md`](docs/testing.md).

### Examples

Standalone, runnable demonstrations of specific WinAPI functionality
(window/message loop, timers, input, GDI, MIDI playback) live in
[`examples/`](examples/README.md):

```bash
cmake -B build -DFREE_API_USE_SYSTEM_SDL3=ON -DFREE_API_BUILD_EXAMPLES=ON
cmake --build build
./build/05_midi_playback
```

---

## Project Status

**Work in progress**

Current focus:
* Supporting `free-direct` requirements
* Basic windowing and message pump
* Cross-platform compatibility

---

## License

This project is licensed under **MIT**.

