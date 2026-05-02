# Free API — Documentation Index

Free API is a small SDL3-backed compatibility layer that exposes a Win32/WinAPI-like
public surface for old C/C++ games.  It is **not** Wine, **not** a full Windows
emulator, and **not** a complete Win32 SDK replacement.  Only the subset needed by
the target game(s) is implemented.

```text
Legacy game source code
        |
        | includes <windows.h>, <mmsystem.h>, <digitalv.h>, <io.h>, ...
        v
Free API public headers  (include/)
        |
        | implemented in src/winapi.cpp  +  src/MidiMusic.cpp
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
| `src/winapi.cpp` | Implementation details: message queue, SDL event translation, GDI internals, timer internals, path normalization, diagnostics |
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

## Build

```bash
cmake -B build -DFREE_API_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

## Generating Doxygen HTML

```bash
doxygen Doxyfile   # if a Doxyfile is present
```

Point Doxygen at `include/` and `src/` to extract all API and implementation documentation.
