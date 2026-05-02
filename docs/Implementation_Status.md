# Free API Implementation Status

This status matrix is based on the uploaded headers and implementations. The status here prioritizes actual `.cpp` behavior over older comments in public headers.

## 1. Status definitions

| Status | Meaning |
|---|---|
| **Implemented** | Useful, non-trivial implementation exists for the project target. |
| **Partial** | Implemented for a narrow subset, selected flags, simplified semantics, or game-specific cases. |
| **Stub** | Placeholder behavior only: fixed return, no-op, unsupported, or compile-time compatibility. |
| **Header-only** | Macro, typedef, or inline wrapper. |
| **Internal** | Implemented but not declared in public headers. |

## 2. Header-level summary

| File | Status | Notes |
|---|---:|---|
| `include/minwindef.h` | Header-only / implemented | Basic scalar Win32 typedefs and constants. |
| `include/winnt.h` | Header-only / implemented | Basic string, GUID, HRESULT, and alias typedefs. No real COM behavior. |
| `include/windef.h` | Header-only / implemented | Handles and calling-convention macros. |
| `include/windows.h` | Partial | Central API. Many functions are useful; many are intentionally simplified. |
| `include/mmsystem.h` | Partial | Timer and MIDI/MCI work for narrow target; joystick and many WinMM areas absent. |
| `include/digitalv.h` | Stub/partial declarations | DGV structs/constants exist; video playback unsupported. |
| `include/io.h` | Partial | `_lopen/_lread/_lclose` work; `_find*` is stubbed. |
| `include/direct.h` | Header-only / partial | Thin POSIX wrappers for `_chdir`, `_getcwd`, `_mkdir`. |
| `include/wtypes.h` | Header-only / stub | OLE/WTypes aliases only. |
| `include/commdlg.h` | Stub | Include placeholder only. |
| `include/windowsx.h` | Stub | Include placeholder only. |
| `src/MidiMusic.h` | Internal / partial | Private MIDI backend declarations. |

## 3. `windows.h` / `src/winapi.cpp` functions

### WinBase-like helpers

| Public symbol | Status | Current behavior |
|---|---:|---|
| `Sleep` | Implemented | Uses `std::this_thread::sleep_for`. |
| `GetTickCount` | Implemented | Uses `std::chrono::steady_clock`, returns `DWORD` milliseconds. |
| `GlobalMemoryStatus` | Partial | Fills fixed approximate memory values. |
| `CloseHandle` | Stub/partial | Returns `TRUE`; does not own/free generic handles. |
| `OutputDebugStringA` | Implemented | Prints to `stdout`. |
| `OutputDebugStringW` | Partial | Prints wide chars as narrow chars. |

### Window class, window creation, window state

| Public symbol | Status | Current behavior |
|---|---:|---|
| `RegisterClassA` | Partial | Stores class name to `WNDPROC`; returns `1` on success. |
| `CreateWindowExA` | Partial | Creates SDL window, maps basic styles, stores `HWND -> WNDPROC`, sends `WM_CREATE`. |
| `CreateWindowA` | Partial | Calls `CreateWindowExA` with no extended style. |
| `DestroyWindow` | Partial | Destroys SDL window and removes mapping. Quits SDL video when last window is gone. |
| `ShowWindow` | Partial | Maps hide/show/minimize/maximize/restore to SDL. |
| `UpdateWindow` | Partial | Raises SDL window; does not send `WM_PAINT`. |
| `MoveWindow` | Partial | Sets SDL position and size; ignores repaint semantics. |
| `InvalidateRect` | Stub | Returns `TRUE`; no invalid region tracking. |
| `SetWindowTextA` | Partial | Sets SDL window title. |
| `GetClientRect` | Partial | Returns SDL window size as client rectangle. |
| `AdjustWindowRect` | Stub/partial | No-op success for non-null rect. |
| `SetFocus` | Partial | Stores focus window and raises SDL window. |

### Message queue and dispatch

| Public symbol | Status | Current behavior |
|---|---:|---|
| `PeekMessageA` | Implemented/partial | Pumps SDL events, generates timer messages, returns queued messages. Ignores filters. |
| `GetMessageA` | Partial | Blocks by polling until a message; returns `FALSE` for `WM_QUIT`. |
| `TranslateMessage` | Stub/partial | Returns `TRUE` for non-null pointer; does not synthesize `WM_CHAR`. |
| `DispatchMessageA` | Partial | Calls stored `WNDPROC`, with fallback for null hwnd and one window. |
| `DefWindowProcA` | Partial | Handles `WM_CLOSE` and `WM_DESTROY`; all else returns `0`. |
| `PostQuitMessage` | Implemented | Enqueues `WM_QUIT`. |
| `PostMessageA` | Implemented/partial | Enqueues message into global queue. No thread/window validation. |
| `WaitMessage` | Partial | Pumps SDL, waits briefly, returns `TRUE`. |

### Cursor, mouse, keyboard, and UI helpers

| Public symbol | Status | Current behavior |
|---|---:|---|
| `GetCursorPos` | Partial | Uses `SDL_GetGlobalMouseState`. |
| `ScreenToClient` | Partial | Subtracts SDL window position. |
| `ClientToScreen` | Partial | Adds SDL window position. |
| `SetCursorPos` | Partial | Calls `SDL_WarpMouseGlobal`. |
| `SetCursor` | Stub | Returns provided cursor handle. |
| `ShowCursor` | Stub | Ignores input and returns `0`. |
| `LoadCursorA` | Stub | Returns dummy non-null handle. |
| `LoadIconA` | Stub | Returns dummy non-null handle. |
| `MessageBoxA` | Stub/partial | Prints to `stderr` and returns `1`. |
| `LoadStringA` | Stub/partial | Writes `RES_<id>` into buffer. |
| `GetModuleHandleA` | Stub | Returns dummy module handle `1`. |
| `GetStockBrush` | Stub | Returns dummy handle based on object ID. |
| `GetSystemMetrics` | Partial | Returns hardcoded `1024`, `768`, `24` for selected metrics; else `0`. |

### GDI-like bitmap/DC subset

| Public symbol | Status | Current behavior |
|---|---:|---|
| `LoadImageA` | Partial | Loads BMP files from disk only when `LR_LOADFROMFILE`; converts to internal RGBA32 bitmap. |
| `GetObjectA` | Partial | Fills `BITMAP` for internal compatible bitmap. |
| `DeleteObject` | Partial | Deletes internal compatible bitmap. |
| `CreateBitmap` | Partial | Creates internal RGBA32 bitmap from 8/16/32-bit input. 8-bit uses grayscale placeholder. |
| `CreateCompatibleDC` | Partial | Creates internal memory DC. |
| `SelectObject` | Partial | Selects internal bitmap into memory DC. |
| `DeleteDC` | Partial | Deletes internal DC. |
| `StretchBlt` | Partial | Supports `SRCCOPY` from selected bitmap memory DC to internal surface DC, nearest-neighbor scaling. |
| `GetPixel` | Partial | Reads from compatible DC/selected bitmap. |
| `SetPixel` | Partial | Writes RGB into compatible DC/selected bitmap. |
| `GetDeviceCaps` | Stub/partial | Returns `256` only for `SIZEPALETTE`. |
| `GetSystemPaletteEntries` | Stub/partial | Fills grayscale palette entries. |

### Resource APIs

| Public symbol | Status | Current behavior |
|---|---:|---|
| `FindResourceA` | Stub | Returns `NULL`. |
| `LoadResource` | Stub | Returns `NULL`. |
| `SizeofResource` | Stub | Returns `0`. |
| `LockResource` | Stub/partial | Returns the supplied handle value. |
| `UnlockResource` | Stub | Returns `FALSE`. |
| `FreeResource` | Stub | Returns `FALSE`. |

### File/path/CRT-style helpers

| Public symbol | Status | Current behavior |
|---|---:|---|
| `_lopen` | Partial | Calls POSIX `open` with `O_RDONLY`; ignores mode. |
| `_lread` | Partial | Calls POSIX `read`. |
| `_lclose` | Partial | Calls POSIX `close`. |
| `DeleteFileA` | Partial | Calls `remove` directly. |
| `CreateDirectoryA` | Partial | Normalizes path and calls `mkdir`; ignores security; no recursive parent creation. |
| `wsprintfA` | Partial | Uses `vsnprintf` with fixed size `1024`. |
| `free_api_fopen` / `fopen` wrapper | Header-only / implemented | Normalizes Windows-style paths and uppercase-basename fallback. C++ only. |

### Timers

| Public symbol | Status | Current behavior |
|---|---:|---|
| `SetTimer` | Partial | Stores polling timer; `PeekMessageA` generates `WM_TIMER`. Ignores callback function. |
| `KillTimer` | Partial | Removes stored polling timer. |

### Rect helpers and macros

| Public symbol | Status | Current behavior |
|---|---:|---|
| `SetRect` | Header-only / implemented | Fills `RECT`; false on null. |
| `IntersectRect` | Header-only / implemented | Computes intersection; returns true only if positive area. |
| `UnionRect` | Header-only / implemented | Computes bounding union. |
| `RGB` | Header-only / implemented | Packs RGB into `COLORREF`. |
| `MAKELONG`, `LOWORD`, `HIWORD` | Header-only / implemented | Word packing/extraction helpers. |
| `ZeroMemory`, `FillMemory`, `CopyMemory` | Header-only / implemented | Map to C memory functions. |

### Entry point bridge

| Public symbol | Status | Current behavior |
|---|---:|---|
| `_pgmptr` | Partial | Global char pointer set to `argv[0]` by bridge. |
| `FREE_API_WINMAIN_PROC` | Header-only | Function pointer typedef for WinMain-like entry. |
| `FreeApiRunWinMain` | Implemented/partial | Builds command line and calls WinMain-like function with null handles and `SW_SHOW`. |
| `FREE_API_IMPLEMENT_WINMAIN()` | Header-only / implemented | Non-Windows macro that defines `main` and forwards to `WinMain`. |

## 4. `mmsystem.h` / multimedia functions

| Public symbol | Status | Current behavior |
|---|---:|---|
| `timeSetEvent` | Implemented/partial | Uses `SDL_AddTimer`, invokes callback on SDL timer thread. Behaves periodic; ignores resolution and one-shot semantics. |
| `timeKillEvent` | Implemented/partial | Removes SDL timer and state. |
| `joyGetPosEx` | Stub | Clears structure and returns error-like value `1`. |
| `joyGetNumDevs` | Stub | Returns `0`. |
| `midiOutGetNumDevs` | Implemented/partial | Returns `1` if SDL audio subsystem can initialize. |
| `midiOutOpen` | Implemented/partial | Returns dummy handle; real audio through MCI. |
| `midiOutSetVolume` | Implemented/partial | Converts packed WinMM volume to average gain. |
| `midiOutClose` | Implemented/partial | Returns success. |
| `mciSendCommandA` | Partial | Supports MIDI sequencer open/play/close/set no-op; rejects CD/video/unsupported commands. |
| `mciGetDeviceIDA` | Stub/partial | Returns `1` for any input. |
| `mciGetErrorStringA` | Implemented/partial | Formats known MCI error codes and generic unknown codes. |

## 5. `digitalv.h`

| Public symbol/group | Status | Current behavior |
|---|---:|---|
| `MCI_DGV_*` constants | Header-only / stub | Constants exist for source compatibility. |
| `MCI_DGV_*_PARMS` structures | Header-only / stub | Layouts exist; video functionality not implemented. |
| `mciSendCommandA` declaration | Partial | Actual function is shared with `mmsystem.h`; video commands unsupported. |
| `mciGetDeviceIDA` declaration | Stub/partial | Actual function returns `1`. |

## 6. `io.h`

| Public symbol | Status | Current behavior |
|---|---:|---|
| `_finddata_t` | Header-only / partial | Struct exists for compatibility. |
| `_findfirst` | Stub | Clears output struct and returns `-1`. |
| `_findnext` | Stub | Returns `-1`. |
| `_findclose` | Stub/partial | Returns `0`. |
| `_lopen`, `_lread`, `_lclose` | Partial | Declared here and implemented in `winapi.cpp`. |

## 7. `direct.h`

| Public symbol | Status | Current behavior |
|---|---:|---|
| `_chdir` | Header-only / partial | Calls POSIX `chdir`. |
| `_getcwd` | Header-only / partial | Calls POSIX `getcwd`. |
| `_mkdir` | Header-only / partial | Calls POSIX `mkdir(path, 0777)`. |

## 8. `wtypes.h`, `commdlg.h`, `windowsx.h`

| Public symbol/group | Status | Current behavior |
|---|---:|---|
| `VARTYPE`, `SCODE`, `DATE`, `CLIPFORMAT` | Header-only / stub | Type aliases only. |
| `byte` | Header-only / compatibility | Alias to `BYTE` if not already defined. |
| `commdlg.h` | Stub | Includes `windows.h`; no dialog APIs. |
| `windowsx.h` | Stub | Includes `windows.h`; no helper macros. |

## 9. Internal but important functions/objects

These are not public API but explain how the public behavior works.

| Internal item | Status | Purpose |
|---|---:|---|
| `EnsureVideoSubsystem` | Internal / implemented | Lazily initializes SDL video and input debug flags. |
| `PumpSdlEvents` | Internal / implemented | Converts SDL events to WinAPI messages. |
| `PushMessage` | Internal / implemented | Adds `MSG` to mutex-protected queue. |
| `SdlScancodeToVK` | Internal / partial | Maps SDL scancodes to VK codes. |
| `CompatBitmap` | Internal / partial | Private RGBA32 bitmap object. |
| `CompatDC` | Internal / partial | Private memory/surface DC object. |
| `FreeApiCreateSurfaceDC` | Internal / partial | Creates a surface DC pointing at external RGBA32 pixels. Not declared in public headers. |
| `FreeApiDestroySurfaceDC` | Internal / partial | Deletes a surface DC. Not declared in public headers. |
| `BuildCommandLine` | Internal / implemented | Joins `argv[1...]` into WinMain `lpCmdLine`. |
| `MidiMusic*` functions | Internal / partial | MIDI/MCI backend functions called by public wrappers. |

## 10. Most important gaps for future work

1. Real resource loading from `.rc`/Windows resources.
2. More complete path normalization across all file functions, especially `DeleteFileA` and `_lopen`.
3. Real `TranslateMessage` behavior for keyboard-to-character conversion, or documented reliance on SDL text input.
4. Real cursor show/hide state and cursor objects.
5. Palette-aware `CreateBitmap` / bitmap loading for 8-bit legacy graphics.
6. Recursive directory creation or exact Windows error semantics for `CreateDirectoryA`.
7. Real `SetTimer` callback support when `lpTimerFunc != NULL`.
8. More accurate `GetSystemMetrics` and `GetDeviceCaps`.
9. Joystick/gamepad support, probably mapped from SDL gamepad APIs.
10. MCI status queries, looping, from/to ranges, pause/stop semantics, and optionally digital-video no-op state tracking.
