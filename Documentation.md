# Free API Documentation

Documentation generated from the uploaded `free-api-develop.zip` source tree.

Free API is a small SDL3-backed compatibility layer that exposes a **Win32/WinAPI-like public surface** for old C/C++ games. It is not a full Windows emulator, not Wine, and not a complete Win32 SDK replacement. It implements only the subset currently needed by the surrounding `free-direct` / legacy-game work.

## Main idea

```text
Legacy game source code
        |
        | includes <windows.h>, <mmsystem.h>, <io.h>, ...
        v
Free API public headers
        |
        | implemented mostly in src/winapi.cpp and src/MidiMusic.cpp
        v
SDL3 windowing, events, timers, audio, POSIX file calls
```

The public headers intentionally look like old Windows headers. Internally, handles and objects are translated into small compatibility objects, SDL windows, SDL timers, SDL audio streams, POSIX file descriptors, or fixed dummy values.

## Read this documentation in this order

1. [docs/WinAPI_Subset.md](docs/WinAPI_Subset.md) — the core WinAPI subset: types, handles, windows, messages, input, GDI-like bitmap/DC helpers, files, resources, and path handling.
2. [docs/WinMM_MCI.md](docs/WinMM_MCI.md) — multimedia APIs: `timeSetEvent`, `midiOut*`, `mciSendCommandA`, MIDI playback, SoundFont lookup, joystick stubs, and digital-video placeholders.
3. [docs/Implementation_Status.md](docs/Implementation_Status.md) — function-by-function status table based on both headers and `.cpp` implementations.

## Status vocabulary used in these docs

| Status | Meaning |
|---|---|
| **Implemented** | Non-trivial implementation exists and is usable for the intended narrow compatibility target. It may still be smaller than real Windows. |
| **Partial** | Some behavior exists, but only for selected flags, data formats, message types, or game-specific usage. |
| **Stub** | The symbol exists mainly so code compiles or a game can gracefully skip a feature. It returns a fixed value, prints, does nothing, or reports unsupported behavior. |
| **Header-only** | Implemented as a macro, typedef, or inline function in a public header. |
| **Internal** | Present in `.cpp` or private headers, but not part of the public headers. |

Important: several comments inside the current public headers are conservative or outdated. For example, `SetTimer`, `MoveWindow`, `SetWindowTextA`, `GetCursorPos`, and several GDI-like functions are marked as stubs in headers, but the `.cpp` file contains real partial implementations. The status tables in this documentation use the **actual implementation** as the primary source.

## Public headers in the uploaded tree

| Header | Purpose | Current role |
|---|---|---|
| `include/windows.h` | Main WinAPI façade. Includes base types, windowing, messages, input constants, GDI-like declarations, resources, file helpers, and the non-Windows `WinMain` bridge macro. | Central public header. Many APIs are partial but functional. |
| `include/mmsystem.h` | WinMM subset: multimedia timers, joystick types, MIDI output, MCI command API. | Timer and MIDI/MCI are functional for the game subset; joystick is a stub. |
| `include/digitalv.h` | MCI Digital Video structures and constants. | Mostly placeholder; AVI/video playback is intentionally unsupported. |
| `include/io.h` | Old low-level file and `_findfirst` compatibility declarations. | `_lopen/_lread/_lclose` are implemented; `_findfirst/_findnext/_findclose` are stubs. |
| `include/direct.h` | Minimal MSVC `direct.h` compatibility for `_chdir`, `_getcwd`, `_mkdir`. | Header-only POSIX wrappers. |
| `include/minwindef.h` | Basic Win32 scalar typedefs and `TRUE/FALSE/MAX_PATH`. | Header-only type compatibility. |
| `include/windef.h` | Opaque handle typedefs, pointer-sized integer typedefs, `WPARAM/LPARAM/LRESULT`, calling-convention macros. | Header-only type compatibility. |
| `include/winnt.h` | Character, string, `HRESULT`, `GUID`, `IUnknown`, Unicode alias types. | Header-only type compatibility. |
| `include/wtypes.h` | Small OLE/WTypes aliases plus `byte`. | Header-only placeholder. |
| `include/commdlg.h` | Common dialog placeholder. | Stub include only. |
| `include/windowsx.h` | Windowsx helper placeholder. | Stub include only. |

## Source files that implement behavior

| Source file | Responsibility |
|---|---|
| `src/winapi.cpp` | Most WinAPI behavior: SDL window creation, message queue, SDL input translation, WinBase helpers, GDI-like bitmap/DC objects, file helpers, resources stubs, user timers, multimedia timer bridge, public WinMM wrappers, `FreeApiRunWinMain`. |
| `src/MidiMusic.cpp` | Internal MIDI/MCI music backend using TinySoundFont + TinyMidiLoader + SDL3 audio. |
| `src/MidiMusic.h` | Private declarations used by `winapi.cpp`. Not intended for public users. |
| `src/winmain_bridge.cpp` | Currently only includes `windows.h`; the actual bridge implementation is in `winapi.cpp`. |
| `external/tsf.h`, `external/tml.h` | Vendored TinySoundFont and TinyMidiLoader single-header libraries. |

## High-level implementation summary

### Implemented or useful now

- Old-style `WinMain` entry bridging on non-Windows through `FREE_API_IMPLEMENT_WINMAIN()` and `FreeApiRunWinMain`.
- `RegisterClassA`, `CreateWindowExA`, `CreateWindowA`, `DestroyWindow`, `ShowWindow`, `UpdateWindow`, `SetWindowTextA`, `MoveWindow`, `GetClientRect`, `SetFocus` over SDL3 windows.
- `PeekMessageA`, `GetMessageA`, `DispatchMessageA`, `PostMessageA`, `PostQuitMessage`, `WaitMessage` with a process-local queue.
- SDL3 event translation into WinAPI messages for mouse movement, mouse buttons, keyboard keys, text input, quit, close, and focus gain.
- `Sleep`, `GetTickCount`, `OutputDebugStringA/W`, approximate `GlobalMemoryStatus`.
- Basic GDI-like bitmap and memory-DC support: `LoadImageA` for BMP files, `CreateBitmap`, `CreateCompatibleDC`, `SelectObject`, `StretchBlt`, `GetPixel`, `SetPixel`, `DeleteObject`, `DeleteDC`, `GetObjectA`.
- `timeSetEvent` / `timeKillEvent` using SDL timers.
- `SetTimer` / `KillTimer` using polling in `PeekMessageA` to post `WM_TIMER`.
- MIDI/MCI playback for `sequencer` devices using `.mid`-like data loaded through TinyMidiLoader and synthesized through TinySoundFont.
- Path normalization wrappers for Windows-style backslash paths in some file-loading paths.

### Mostly placeholders or intentionally incomplete

- Real resources (`FindResourceA`, `LoadResource`, `SizeofResource`, `FreeResource`) are not implemented.
- Common dialogs are not implemented.
- Joystick APIs report no joystick.
- MCI Digital Video / AVI playback is not implemented; device-type-only video open is rejected so the game can skip video.
- Most real GDI drawing, palettes, DIB sections, brushes, icons, cursors, and device capabilities are simplified or fake.
- Unicode/Wide APIs are mostly aliases or absent except `OutputDebugStringW`; the main implementation is ANSI-oriented.
- Security descriptors are ignored.

## Mental model for learning this subset of WinAPI

The most important old-WinAPI ideas represented here are:

1. **Handles are opaque tokens.** In real WinAPI, `HWND`, `HBITMAP`, `HDC`, etc. are opaque handles owned by Windows. In Free API they are mostly `void*` pointing to SDL windows or private C++ structs.
2. **Window classes map to window procedures.** `RegisterClassA` stores a class name and `WNDPROC`. `CreateWindowExA` looks up the class and creates an SDL window, then sends `WM_CREATE` to the stored procedure.
3. **The message loop drives the program.** SDL events are pumped in `PeekMessageA`, translated into WinAPI-like `MSG` entries, then passed to the game through `DispatchMessageA`.
4. **Input is message-based.** Mouse and keyboard input is not delivered by direct callbacks; it becomes `WM_MOUSEMOVE`, `WM_LBUTTONDOWN`, `WM_KEYDOWN`, `WM_CHAR`, etc.
5. **GDI objects are selected into device contexts.** This subset supports enough of the `HBITMAP` + memory `HDC` + `SelectObject` + `StretchBlt` model to move bitmap pixels into a destination surface.
6. **WinMM timers differ from normal window timers.** `timeSetEvent` calls a callback from an SDL timer thread, while `SetTimer` posts `WM_TIMER` messages during `PeekMessageA` polling.
7. **MCI is command-based multimedia.** The game opens a device with `MCI_OPEN`, starts it with `MCI_PLAY`, and closes it with `MCI_CLOSE`; this implementation supports that for MIDI sequencer playback.

## Build context

The uploaded `CMakeLists.txt` builds a C++20 library named `free-api` and links SDL3 privately. Tests can be enabled through `FREE_API_BUILD_TESTS`.

```bash
cmake -B build -DFREE_API_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

The repository contains tests for a basic sleep/MIDI regression case and for the SDL input-to-WinAPI message pipeline.

## Suggested future documentation improvements

- Move the function-by-function status table into Doxygen comments and keep it generated from source.
- Mark each public function with `@note Status: IMPLEMENTED`, `PARTIAL`, or `STUB`, based on actual implementation rather than older placeholder comments.
- Split `windows.h` into smaller internal headers later if it becomes hard to navigate.
- Add examples: minimal window, message loop, timer callback, bitmap blit, and MCI MIDI playback.
- Add a compatibility target table for each game that uses Free API.
