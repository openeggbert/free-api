# Free API — Documentation Index

Free API is a small SDL3-backed compatibility layer that exposes a Win32/WinAPI-like
public surface for old C/C++ games.  It is **not** Wine, **not** a full Windows
emulator, and **not** a complete Win32 SDK replacement.  Only the subset needed by
the target game(s) is implemented.

See [`docs/scope.md`](docs/scope.md) for the project's scope policy: every new
public API must cite a real usage site in `../free-eggbert` or `../planetblupi`.
See [`docs/supported-apis.md`](docs/supported-apis.md) for the current,
hand-maintained table of every implemented symbol and its status, and
[`plan.md`](plan.md) for the full evidence-based usage audit and task backlog.

```text
Legacy game source code
        |
        | includes <windows.h>, <mmsystem.h>, <digitalv.h>, <io.h>, ...
        v
Free API public headers  (include/)
        |
        | implemented in src/*.cpp + src/internal/*.cpp  +  src/MidiMusic.cpp
        v
SDL3 windowing, events, timers, audio  +  POSIX file calls
```

## Detailed documentation

All detailed API documentation lives directly in the source files as Doxygen comments.
The recommended reading order is:

| File | What you find there |
|---|---|
| `include/windows.h` | Main WinAPI façade: windows, messages, input, GDI-like bitmap/DC, file helpers, timers, WinMain bridge |
| `include/mmsystem.h` | WinMM subset: multimedia timers, MIDI output, MCI command API |
| `include/digitalv.h` | MCI Digital Video/sequencer structures and constants |
| `include/io.h` | Legacy file I/O and `_findfirst/_findnext/_findclose` |
| `include/direct.h` | `_chdir`, `_getcwd`, `_mkdir` POSIX wrappers |
| `include/minwindef.h` | Scalar types and `TRUE/FALSE/MAX_PATH` |
| `include/windef.h` | Opaque handle types, `WPARAM/LPARAM/LRESULT`, calling-convention macros |
| `include/winnt.h` | String/char types, `HRESULT`, `GUID`, `IUnknown` alias |
| `include/wtypes.h` | Minimal OLE/WTypes aliases |
| `src/internal/FreeApiMessageQueue.cpp` | Message queue, SDL event translation |
| `src/internal/FreeApiGdi.cpp` + `src/wingdi_*.cpp` | GDI internals |
| `src/internal/FreeApiTimers.cpp` + `src/winuser_timer.cpp` + `src/winmm.cpp` | Timer internals (`SetTimer`/`WM_TIMER` and `timeSetEvent`/`timeKillEvent`) |
| `src/internal/FreeApiPath.cpp` | Path normalization |
| `src/internal/FreeApiDiagnostics.cpp` | Diagnostics |
| `src/winapi.cpp` | Empty; doc-comment redirect map to the files above (see the file itself for the full, current list) |
| `src/MidiMusic.cpp` | MIDI/MCI backend: TinySoundFont + TinyMidiLoader + SDL3 audio, SoundFont lookup, mixer thread, MCI session lifecycle |
| `src/MidiMusic.h` | Private MIDI backend API (not for public consumers) |

### Status vocabulary

Each public symbol carries a `@note Status:` tag:

| Tag | Meaning |
|---|---|
| `IMPLEMENTED` | Useful, non-trivial implementation for the target subset. |
| `PARTIAL` | Works for a narrow subset of flags/formats; differs from real Windows. |
| `STUB` | Placeholder: compiles, returns fixed value or does nothing. |
| `HEADER_ONLY` | Macro, typedef, or inline function in a header; no `.cpp` needed. |
| `INTERNAL` | Implementation detail in `.cpp` or private headers; not a public API. |

## Debug Flags

All flags are environment variables, read once (lazily, on first use) and
cached; set them before launching the game/test binary. Default state for
every flag below is **off** unless noted.

| Flag | Controls | Default |
|---|---|---|
| `FREE_API_DIAGNOSTICS` | Enables periodic `[FREE_API_DIAG]` snapshots (queue size/high-water, timer counts, message/coalescing counters, RSS memory) at startup, `atexit`, and on demand (`FreeApiDiagSnapshot`). Also gates whether the fast-path diagnostic atomics are incremented at all (`FreeApiDiagnosticsFastEnabled`). | Off |
| `FREE_DIRECT_DIAGNOSTICS` | Alias for `FREE_API_DIAGNOSTICS` — either one enables diagnostics (checked together). | Off |
| `FREE_API_DEBUG_GDI` | Enables hot-path GDI logging in `src/wingdi_blit.cpp` (`StretchBlt`/`GetPixel`/`SetPixel` call details). Set to exactly `1`. | Off |
| `FREE_API_DEBUG_INPUT` | Enables verbose input-translation logging (`InputLog`, `src/internal/FreeApiMessageQueue.cpp`) — SDL event → WinAPI message translation detail, capped at the first 20 events. Set to exactly `1`. | Off |
| `FREE_API_DEBUG_MOUSE` | Alias for `FREE_API_DEBUG_INPUT` (either enables the same input logging). | Off |
| `FREE_API_DEBUG_REAL_INPUT` | Alias for `FREE_API_DEBUG_INPUT` (either enables the same input logging). | Off |
| `FREE_API_DEBUG_MIDI` | Enables verbose MIDI/MCI logging (`MIDI_LOG`, `src/MidiMusic.cpp`) — session open/play/close detail, SoundFont lookup, mixer thread state. Set to exactly `1`. | Off |
| `FREE_API_SOUNDFONT` | Path to a `.sf2` SoundFont file for MIDI rendering; not a debug flag, but read the same way (environment variable, lazily). Falls back to `assets/soundfont/default.sf2` then `soundfont/default.sf2`; if none found, MIDI plays silently (not an error). | Unset |

## Build

```bash
cmake -B build -DFREE_API_BUILD_TESTS=ON
cmake --build build
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir build
```

See [`docs/testing.md`](docs/testing.md) for the full test-running
workflow (why the `SDL_VIDEODRIVER`/`SDL_AUDIODRIVER` vars above are
required, running against each target game, sanitizer runs, etc.) and
[`docs/cmake-options.md`](docs/cmake-options.md) for the full build-mode
reference.

## Generating Doxygen HTML

```bash
doxygen Doxyfile   # if a Doxyfile is present
```

Point Doxygen at `include/` and `src/` to extract all API and implementation documentation.
