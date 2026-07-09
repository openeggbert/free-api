# Free API — Public Surface Classification Audit (TASK-24H-0115)

A durable, checkable consolidation of every public declaration in
`include/*.h` and `include_non_windows/*.h`, each assigned exactly one of
five classifications per `docs/scope.md`'s "The rule":

* **required-by-game** — cited by a real `file:line` in `../free-eggbert`
  or `../planetblupi`.
* **test-infrastructure-only** — called only by `tests/`, never by either
  game (`docs/scope.md`'s named exception, `TASK-24H-0114`).
* **free-direct-bridge** — consumed only by `../free-direct` acting as the
  games' bridge (`docs/scope.md`'s "Boundary with `free-direct`" section).
* **permanent-documented-stub** — a compile-only/safe-placeholder stub,
  documented in `docs/out-of-scope.md`'s "Compile-only stubs" table.
* **vestigial-but-harmless** — proven unused by both games, kept for a
  documented reason.

This consolidates classification work already done piecemeal across
`docs/scope.md`, `docs/out-of-scope.md`, and `docs/supported-apis.md`
rather than re-deriving it — those three remain the authoritative sources
for the *reasoning* behind each classification; this table exists so the
next audit has a single place to check "is this symbol still classified
the way I remember," not a place that duplicates the full rationale.
Constant families that share one classification and one rationale (e.g.
all `VK_*` keycodes) are grouped into one row, matching
`docs/supported-apis.md`'s own convention, rather than one row per
`#define`.

**This table itself is a point-in-time snapshot (`TASK-24H-0115`) and is
not automatically re-verified — it can drift.** `TASK-24H-1238` added the
mechanical, always-current enforcement this table can't provide on its
own: `cmake/CheckPublicSurfaceBaseline.cmake` (run via the
`check_public_surface_baseline` CTest test) extracts every public
declaration currently in `include/*.h`/`include_non_windows/*.h` and fails
loudly, naming the symbol, if anything appears that isn't already listed
in `cmake/known-public-symbols.txt` — see `docs/scope.md`'s "Automated
enforcement" section. That check confirms *a symbol is classified at all*
(by requiring an explicit baseline addition, which the citation rule then
gates); it does not verify classification *correctness* the way this
table's evidence column does. Use this table for the reasoning behind an
existing symbol's classification; trust the CTest check, not this table's
symbol *list*, for whether the header set has grown since this table was
written.

Real per-symbol usage verification for this table was done directly
against `../free-eggbert/src` and `../planetblupi/src` (not assumed from
prior docs) wherever an existing doc didn't already carry a `file:line`
citation.

## `include/basestd.h`, `include/minwindef.h`, `include/windef.h`, `include/winnt.h`, `include/winerror.h`

Pure typedef/constant/macro shims with no independent behavior of their
own — every type (`INT_PTR` family, `DWORD`/`BOOL`/`BYTE`/handle aliases,
`RECT`/`POINT`, `CHAR`/`WCHAR`/`LPSTR`/`GUID`/`HRESULT` family,
`E_FAIL`/`ERROR_*`) required-by-game transitively (every WinAPI call
either game makes uses these foundational types) except the explicitly
vestigial ones already called out below.

| Symbol | Header | Classification | Evidence |
|---|---|---|---|
| `INT_PTR`/`UINT_PTR`/`LONG_PTR`/`ULONG_PTR`/`DWORD_PTR` + pointer forms | basestd.h | required-by-game | Foundational pointer-sized types; used transitively by every WinAPI signature. |
| `MAX_PATH`, `NULL`, `TRUE`, `FALSE`, `CALLBACK`/`WINAPI`/`WINAPIV`/`APIENTRY`/`APIPRIVATE`/`PASCAL`, `__stdcall`/`__cdecl` | minwindef.h | required-by-game | Foundational constants/calling-convention macros used throughout both games' source. |
| `DWORD`, `BOOL`, `BYTE`, `WORD`, `FLOAT`+`P`/`LP` forms, `INT`, `UINT`, `LRESULT`, `WPARAM`, `LPARAM`, `ATOM` | minwindef.h | required-by-game | Foundational scalar types used throughout. |
| `HANDLE`, `HGLOBAL`, `HINSTANCE`, `HMODULE`, `HRSRC`, `HWND`, `HDC`, `HGDIOBJ`, `HBRUSH`, `HBITMAP`, `HICON`, `HCURSOR`, `HMENU` | minwindef.h | required-by-game | Opaque handle types passed to real WinAPI calls both games make. |
| `HPALETTE` | minwindef.h | permanent-documented-stub | `docs/out-of-scope.md` "Compile-only stubs" — no palette API exists anywhere in free-api. |
| `HFONT` | minwindef.h | permanent-documented-stub | `docs/out-of-scope.md` "Compile-only stubs" — no font API exists anywhere in free-api. |
| `COLORREF` | windef.h | required-by-game | Used by `GetPixel`/`SetPixel`/`RGB()`, both live GDI paths. |
| `RECT`/`PRECT`/`LPRECT`, `POINT`/`PPOINT`/`LPPOINT` | windef.h | required-by-game | Used by `GetClientRect`/`AdjustWindowRect`/`GetCursorPos` etc., all live. |
| `MAKELONG`, `LOWORD`, `HIWORD` | windef.h | required-by-game | Standard WM_* param-packing macros; both games' message handlers use them. |
| `CHAR`, `WCHAR`, `BOOLEAN`, `LPSTR`/`LPCSTR`/family, `LPWSTR`/`LPCWSTR`/family, `LONG`, `HRESULT`, `ULONG`/`PULONG`, `USHORT`/`PUSHORT`, `UCHAR`/`PUCHAR`, `PSZ`, `PVOID`, `TCHAR`/`LPTSTR`/`LPCTSTR` | winnt.h | required-by-game | Foundational string/scalar types used throughout both games' WinAPI call sites. |
| `IUnknown` | winnt.h | permanent-documented-stub | `docs/out-of-scope.md` "Compile-only stubs" (COM-family vestige, `TASK-24H-0112`). |
| `GUID`/`LPGUID`/`LPCGUID`, `IID`/`LPIID`/`REFIID`, `CLSID`/`LPCLSID`/`REFCLSID` | winnt.h | permanent-documented-stub | Same COM-family vestige, `docs/out-of-scope.md` (`TASK-24H-0112`). |
| `E_FAIL` | winerror.h | vestigial-but-harmless | Declared for header-shape compatibility; grep confirms zero use in either game or free-api's own src. |
| `ERROR_ALREADY_EXISTS` | winerror.h | required-by-game (indirectly) | Set by `CreateDirectoryA`'s own already-exists path (currently dead code per `docs/out-of-scope.md` — see that entry); the constant itself has no direct game citation, but is part of a documented, evidenced code path. |
| `ERROR_INVALID_PARAMETER` | winerror.h | vestigial-but-harmless | Declared for header-shape compatibility; grep confirms zero use in either game. Used internally by free-api's own `_lopen`/similar error paths in some builds, not by either game directly. |

## `include/commdlg.h`, `include/windowsx.h`, `include/wtypes.h`, `include/rpcndr.h`

| Symbol | Header | Classification | Evidence |
|---|---|---|---|
| Everything in `commdlg.h` | commdlg.h | permanent-documented-stub | `docs/out-of-scope.md` "Compile-only stubs" — proven zero API calls in either game's `movie.cpp` include site. |
| `GetStockBrush` | windowsx.h | permanent-documented-stub | `docs/out-of-scope.md` "Compile-only stubs" — real call sites exist (`../free-eggbert/src/blupi.cpp:725`, `../planetblupi/src/blupi.cpp:617`) but the stub return value is never inspected (window always fully covered before becoming visible). |
| `VARTYPE`, `SCODE`, `DATE`, `CLIPFORMAT` | wtypes.h | permanent-documented-stub | `docs/out-of-scope.md` "Compile-only stubs" — compile-only, free-eggbert only, never exercised at runtime. |
| `byte` macro (wtypes.h and rpcndr.h both define it, guarded) | wtypes.h, rpcndr.h | permanent-documented-stub | Legacy CRT compatibility alias; `docs/out-of-scope.md` groups this with the COM-family vestige rationale. |

## `include/debugapi.h`, `include/handleapi.h`, `include/synchapi.h`, `include/sysinfoapi.h`

| Symbol | Header | Classification | Evidence |
|---|---|---|---|
| `OutputDebugStringA` | debugapi.h | required-by-game | `docs/supported-apis.md`: `../free-eggbert/src/misc.cpp:32`, `../planetblupi/src/wave.cpp:234,264,268`. |
| `OutputDebugStringW` | debugapi.h | vestigial-but-harmless | `docs/out-of-scope.md` "Unicode / W-suffixed API variants" — zero evidenced call sites in either game; kept as a real (non-stub) implementation deliberately (`TASK-24H-0109/1215`). |
| `CloseHandle` | handleapi.h | **vestigial-but-harmless — undocumented (see Mismatches below)** | Confirmed zero call sites anywhere: neither game, no test, no free-api `src/` call site beyond its own definition (`src/winbase.cpp:59`). |
| `Sleep` | synchapi.h | test-infrastructure-only | `docs/scope.md`'s own worked example; `tests/basic_test.cpp`, `tests/test_winuser_regressions.cpp`. Zero call sites in either game. |
| `GetTickCount` | sysinfoapi.h | test-infrastructure-only | `docs/scope.md`'s own worked example; `tests/basic_test.cpp`. Zero call sites in either game. |

## `include/io.h`, `include/direct.h`

| Symbol | Header | Classification | Evidence |
|---|---|---|---|
| `_access`/`access` (Windows-only forward declarations, `#if defined(_WIN32)`) | io.h | required-by-game (Windows-target only, inactive on current build platforms) | Declared only for a real-Windows build target, per the header's own comment; not reachable/testable on the Linux/macOS/Web build this project actually ships. |
| `_MAX_FNAME`, `_finddata_t` | io.h | required-by-game | Supports `_findfirst`/`_findnext` below. |
| `_findfirst`/`_findnext`/`_findclose` | io.h | required-by-game | `docs/supported-apis.md`: free-eggbert's design-mission picker, `event.cpp:4741-4747`. planetblupi: zero call sites (confirmed dead, same row). |
| `_lopen`/`_lread`/`_lclose` (declared in `winbase.h`, transitively visible via `io.h`'s `#include <windows.h>`, `TASK-24H-0103/0713`) | winbase.h | required-by-game | `docs/supported-apis.md`: both games' `ddutil.cpp`. |
| `_chdir`, `_getcwd`, `_mkdir` | direct.h | mixed — see notes | `_mkdir`: required-by-game (`../free-eggbert/src/event.cpp:4193`). `_chdir`/`_getcwd`: vestigial-but-harmless, confirmed zero call sites in either game, kept-and-documented decision (`docs/headers.md`, `TASK-24H-0110/1216`) since `direct.h` must stay for the live `_mkdir` anyway. |

## `include/winbase.h`

| Symbol | Header | Classification | Evidence |
|---|---|---|---|
| `SECURITY_ATTRIBUTES` | winbase.h | required-by-game | Parameter type for `CreateDirectoryA`, both games' `AddUserPath` call sites. |
| `MEMORYSTATUS`, `GlobalMemoryStatus` | winbase.h | required-by-game | `docs/supported-apis.md`: free-eggbert only (`dwTotalPhys` gate). |
| `ZeroMemory`/`FillMemory`/`CopyMemory` | winbase.h | required-by-game | Standard macros; used throughout both games' buffer-manipulation code. |
| `FreeResource`, `LockResource`, `UnlockResource`, `LoadResource`, `SizeofResource` | winbase.h | permanent-documented-stub | `docs/out-of-scope.md` "Compile-only stubs" table — permanent safe miss/no-op, direct test coverage added this session (`TASK-24H-0803`). |
| `GetModuleHandleA` | winbase.h | permanent-documented-stub | `docs/out-of-scope.md` "Compile-only stubs" — real call sites exist (`../planetblupi/src/ddutil.cpp:90,148`, `../free-eggbert/src/ddutil.cpp:46`, via the `GetModuleHandle` macro) but the returned handle is only ever passed straight into `LoadImageA`, never inspected. |
| `FindResourceA` | winbase.h | permanent-documented-stub | `docs/out-of-scope.md`/`docs/supported-apis.md` — permanent, correct, evidenced always-miss. |
| `CreateDirectoryA` | winbase.h | required-by-game | `docs/supported-apis.md`: dead in free-eggbert, live in planetblupi. |
| `RemoveDirectoryA` | winbase.h | test-infrastructure-only | Zero call sites in either game; used by `tests/basic_test.cpp`. (`TASK-24H-0710` already tracks formally documenting this — currently open.) |
| `DeleteFileA` | winbase.h | required-by-game | `../free-eggbert/src/event.cpp:5170`. |
| `SetEnvironmentVariableA` | winbase.h | test-infrastructure-only | `docs/scope.md`'s own worked example; `tests/basic_test.cpp` only. |
| `GetLastError`/`SetLastError` | winbase.h | test-infrastructure-only (with internal free-api usage) | `docs/scope.md`'s own worked example. Zero call sites in either game. `SetLastError` is also called internally by free-api's own `CreateDirectoryA`/other implementations (`src/winbase_file.cpp`) to fulfill part of their own contract — that internal usage doesn't change the game-facing classification. |

## `include/mciapi.h`, `include/digitalv.h`, `include/mmsystem.h`, `include/mmiscapi2.h`

| Symbol | Header | Classification | Evidence |
|---|---|---|---|
| `MCIDEVICEID` (canonical in mmsystem.h, guarded-duplicate in mciapi.h/digitalv.h) | mmsystem.h (canonical) | required-by-game | Foundational MCI type; both games' `movie.cpp`/`sound.cpp`. |
| `mciapi.h` itself (whole file) | mciapi.h | vestigial-but-harmless | Header's own doc comment: not included by either game (only a commented-out reference in `../free-eggbert/src/movie.cpp`); kept standalone-compilable for Win32-header-shape compatibility. |
| `MCI_GENERIC_PARMS`, `MCI_STATUS_PARMS`, `MCI_DGV_OPEN_PARMSA`/`MCI_DGV_OPEN_PARMS`, `MCI_DGV_WINDOW_PARMSA`/`MCI_DGV_WINDOW_PARMS`, `MCI_DGV_STATUS_PARMSA`/`MCI_DGV_STATUS_PARMS`, `MCI_DGV_PLAY_PARMS` | digitalv.h | required-by-game | Both games' `movie.cpp` AVI-probe call shapes (dead-reach post-probe, but the structs themselves are real declared/used parameter types at the one live `MCI_OPEN` call — see `docs/out-of-scope.md`'s AVI section). |
| `MCI_OPEN`/`MCI_CLOSE`/`MCI_PLAY`/`MCI_NOTIFY`/`MCI_WAIT`/`MCI_OPEN_TYPE`/`MCI_OPEN_ELEMENT` (canonical in mmsystem.h, guarded-duplicate in digitalv.h) | mmsystem.h (canonical) | required-by-game | Both games' live sequencer MCI calls (`docs/supported-apis.md`). |
| `MCI_STATUS`, `MCI_PAUSE`, `MCI_STATUS_ITEM`, `MCI_DGV_OPEN_PARENT`/`MCI_DGV_OPEN_WS`/`MCI_DGV_STATUS_HWND`/`MCI_DGV_PLAY_REVERSE` (digitalv.h only) | digitalv.h | required-by-game | AVI-probe call shape constants, both games' `movie.cpp`. |
| `mciSendCommandA`/`mciSendCommand` (canonical in mmsystem.h, guarded-duplicate in digitalv.h) | mmsystem.h (canonical) | required-by-game | Both games, live sequencer path + dead-reach AVI probe. |
| `mciGetDeviceIDA`/`mciGetDeviceID` | digitalv.h | required-by-game | `../free-eggbert/src/movie.cpp:59` (`termAVI()`); see also `TASK-24H-0808`'s device-id collision test this session. |
| `MMRESULT`, `MCIERROR` | mmsystem.h | required-by-game | Return types of the above, live. |
| `JOYINFOEX` | mmsystem.h | required-by-game | `docs/out-of-scope.md`: populated by `joyGetPosEx`, read by free-eggbert (dead-reach — see joystick classification below, but the struct itself is a real, tested, populated type). |
| `_WAVEFORMAT` | mmsystem.h | vestigial-but-harmless | Declared for header-shape completeness; no `waveOut*` API is implemented (`docs/out-of-scope.md` "Unsupported APIs" — both games use DirectSound/`free-direct`, not WinMM `waveOut`). |
| `MCI_OPEN_PARMSA`/`MCI_OPEN_PARMS`/`LPMCI_OPEN_PARMS`, `MCI_PLAY_PARMS`, `MCI_SET_PARMS` | mmsystem.h | required-by-game | Both games' live sequencer `MCI_OPEN`/`MCI_PLAY` call shapes. |
| `HMIDIOUT`/`LPHMIDIOUT` | mmsystem.h | required-by-game | Both games' `sound.cpp` MIDI-out device iteration. |
| `TIME_PERIODIC` | mmsystem.h | required-by-game | `../free-eggbert/src/blupi.cpp:891` (`timeSetEvent` call). |
| `MM_MCINOTIFY`, `MCI_NOTIFY_SUCCESSFUL` | mmsystem.h | required-by-game | Both games' `WM_MCINOTIFY`-driven replay-loop handlers. |
| `JOY_BUTTON1`-`4` | mmsystem.h | required-by-game (dead-reach) | Populate `JOYINFOEX::dwButtons`; free-eggbert's joystick code path is confirmed structurally unreachable (`docs/target-games.md`, `m_somethingJoystick` always `0`) but the constants are real, tested values. |
| `MMSYSERR_NOERROR`, `MCIERR_INVALID_DEVICE_ID`, `MCIERR_UNSUPPORTED_FUNCTION`, `MCIERR_INTERNAL` | mmsystem.h | required-by-game | Real MCI return-code paths exercised by both games' sequencer/AVI-probe call sites. |
| `JOYERR_NOERROR`/`JOYERR_PARMS`/`JOYERR_NOCANDO`/`JOYERR_UNPLUGGED` | mmsystem.h | required-by-game (dead-reach) | Same joystick dead-reach status as `JOY_BUTTON*` above. |
| `WAVE_FORMAT_PCM` | mmsystem.h | vestigial-but-harmless | Same rationale as `_WAVEFORMAT` above — no `waveOut*` implementation exists. |
| `MCI_OPEN_TYPE_ID`, `MCI_SET_TIME_FORMAT`, `MCI_FORMAT_TMSF`, `MCI_TRACK` (mmsystem.h-only, not duplicated in digitalv.h) | mmsystem.h | required-by-game | AVI-probe/sequencer call-shape constants, both games. |
| `mmioFOURCC` | mmsystem.h | vestigial-but-harmless | Declared for header-shape completeness; grep confirms zero use in either game. |
| `timeSetEvent`/`timeKillEvent` | mmsystem.h | required-by-game | `docs/supported-apis.md`: free-eggbert live, planetblupi dead. |
| `joyGetPosEx`/`joyGetNumDevs` | mmsystem.h | required-by-game (dead-reach) | `docs/supported-apis.md`: free-eggbert's own call path is dead (`m_somethingJoystick` always `0`, `TASK-24H-1001`); a real, tested `SDL_Joystick`-backed implementation is kept anyway (`TASK-0103`). |
| `midiOutGetNumDevs`/`midiOutOpen`/`midiOutSetVolume`/`midiOutClose` | mmsystem.h | required-by-game | Both games' `sound.cpp`, confirmed this session (`sound.cpp:290,293,299,304` free-eggbert; `sound.cpp:256,259,265,270` planetblupi). |
| `mciGetErrorStringA`/`mciGetErrorString` | mmsystem.h | required-by-game | `../free-eggbert/src/sound.cpp:608`. Zero use in planetblupi (one-game citation is sufficient per `docs/scope.md`'s rule). |
| `LPTIMECALLBACK` | mmiscapi2.h | required-by-game | Callback type for `timeSetEvent`, live. |

## `include/winuser.h`

Message-loop/window/input surface. Most rows below are already covered in
full detail by `docs/supported-apis.md`'s table (cross-referenced, not
repeated here); only symbols not already carrying a `docs/supported-apis.md`
row are given fresh evidence.

| Symbol | Classification | Evidence |
|---|---|---|
| `RegisterClassA`, `CreateWindowExA`, `CreateWindowA`, `AdjustWindowRect`, `ShowWindow`/`UpdateWindow`/`SetFocus`, `GetSystemMetrics`, `PeekMessageA`/`GetMessageA`/`TranslateMessage`/`DispatchMessageA`, `WaitMessage`, `PostQuitMessage`/`PostMessageA`, `wsprintfA`, `DefWindowProcA`, `DestroyWindow`, `SetWindowTextA`, `GetClientRect`, `SetTimer`/`KillTimer`, `GetCursorPos`/`ScreenToClient`/`ClientToScreen`/`SetCursorPos`, `ShowCursor`/`SetCursor`, VK_* set, `MK_*` flags, `WM_MOUSEMOVE`/button messages | required-by-game | `docs/supported-apis.md` (full detail per symbol). |
| `LoadCursorA`/`LoadIconA` | permanent-documented-stub | `docs/supported-apis.md`: "STUB (acceptable)". |
| `MoveWindow` | required-by-game (dead-reach) | `docs/supported-apis.md`/`docs/out-of-scope.md`: only call sites are inside dead-reach `movie.cpp`. |
| `InvalidateRect` | permanent-documented-stub | `docs/out-of-scope.md` "Compile-only stubs" — real call sites (`../free-eggbert/src/movie.cpp:90,160`) but a no-op since both games redraw every frame regardless. |
| `MessageBoxA`, `MB_OK` | permanent-documented-stub | `docs/out-of-scope.md` "Compile-only stubs" — real call site (`../free-eggbert/src/blupi.cpp:661`) but only reached on fatal init failure. |
| `LoadImageA`, `LoadStringA` | required-by-game | `docs/supported-apis.md` (full detail). |
| `WNDPROC`, `WNDCLASSA`/`WNDCLASS`/`PWNDCLASS`/`LPWNDCLASS`, `MSG`/`LPMSG`, `CREATESTRUCTA`/`LPCREATESTRUCTA` | required-by-game | Parameter/callback types for the functions above, all live. |
| `HWND_DESKTOP` | required-by-game | Legacy desktop pseudo-window handle constant, part of `CreateWindowExA` call shape both games use. |
| `PM_NOREMOVE`/`PM_REMOVE` | required-by-game | `PeekMessageA` mode constants, both games' message pumps. |
| `WS_VISIBLE`/`WS_POPUP`/`WS_CHILD`/`WS_CAPTION`/`WS_POPUPWINDOW`/`WS_OVERLAPPEDWINDOW`, `WS_EX_TOPMOST` | required-by-game | `CreateWindowExA` style constants, both games (`docs/out-of-scope.md`'s `dwExStyle`/resizable-window entries). |
| `CS_VREDRAW`/`CS_HREDRAW` | required-by-game | `WNDCLASSA::style`, both games' window-class registration. |
| `WM_NULL`/`WM_CREATE`/`WM_DESTROY`/`WM_CLOSE`/`WM_QUIT`/`WM_SYSCOLORCHANGE`/`WM_ACTIVATEAPP`/`WM_SETCURSOR`/`WM_DISPLAYCHANGE`/`WM_KEYDOWN`/`WM_KEYUP`/`WM_SYSKEYDOWN`/`WM_SYSKEYUP`/`WM_TIMER`/`WM_QUERYNEWPALETTE`/`WM_PALETTECHANGED`/`WM_CHAR`/`WM_USER` | required-by-game | Both games' `WndProc` message-handling switch statements. |
| `WM_NCMOUSEMOVE` | required-by-game (never generated) | `docs/out-of-scope.md`: planetblupi has a real handler for this message ID, but free-api's translation never produces it — confirmed non-issue, not a gap. |
| `WM_MBUTTONDOWN`/`WM_MBUTTONUP`, `MK_MBUTTON` | required-by-game, but unproven — do not expand | `docs/out-of-scope.md` "Unsupported APIs" — implemented but confirmed unused by either game's core source. |
| `WM_MOUSEWHEEL` | vestigial-but-harmless | Declared for header-shape completeness; not confirmed as a real translated event anywhere in `src/`. |

## `include/wingdi.h`

| Symbol | Classification | Evidence |
|---|---|---|
| `SRCCOPY`, `CreateBitmap`, `CreateCompatibleDC`/`DeleteDC`/`DeleteObject`, `GetDeviceCaps`, `GetPixel`/`SetPixel`, `GetSystemPaletteEntries`, `SelectObject`, `StretchBlt`, `GetObjectA` | required-by-game | `docs/supported-apis.md` (full detail per symbol). |
| `tagBITMAP`/`BITMAP`, `tagRGBQUAD`/`RGBQUAD`, `tagBITMAPINFOHEADER`/`BITMAPINFOHEADER`, `tagBITMAPFILEHEADER`/`BITMAPFILEHEADER` | required-by-game | Real on-disk BMP structures both games' `ddutil.cpp` decode directly (`tests/test_file_regressions.cpp`'s BMP-header decode test). |
| `tagPALETTEENTRY`/`PALETTEENTRY` | required-by-game | Populated by `GetSystemPaletteEntries`, live. |
| `RGB` | required-by-game | Standard color-packing macro used throughout both games' GDI code. |
| `BLACK_BRUSH` | vestigial-but-harmless | Declared for header-shape completeness; no brush-creation API exists to consume it. |
| `CLR_INVALID` | vestigial-but-harmless | Declared for header-shape completeness; grep confirms zero use in either game. |
| `SIZEPALETTE` | required-by-game | `GetDeviceCaps` index constant, live (`docs/supported-apis.md`). |

## `include/free_api_bridge.h`

| Symbol | Classification | Evidence |
|---|---|---|
| `FreeApiCreateSurfaceDC`, `FreeApiDestroySurfaceDC`, `FreeApiSetWindowFullscreen` | free-direct-bridge | `docs/scope.md`'s "The actual bridge-exception surface" — the three named, documented bridge functions. |

## `include/windows.h`

| Symbol | Classification | Evidence |
|---|---|---|
| `FREE_API_WINMAIN_PROC`, `FreeApiRunWinMain`, `FREE_API_IMPLEMENT_WINMAIN` | free-direct-bridge | `docs/scope.md`'s "The actual bridge-exception surface" names `FreeApiRunWinMain` explicitly. Note: both games structurally launch *through* this (they each define a real `WinMain` free-api's `src/winmain_bridge.cpp` weak `main()` calls via `FreeApiRunWinMain(&WinMain, ...)`), but neither game's own source contains the literal token `FreeApiRunWinMain` — consistent with why `docs/scope.md` treats it as a bridge-exception symbol rather than requiring a direct game-source citation. |
| `_pgmptr` (non-Windows-only declaration) | required-by-game | Populated by `src/winmain_bridge.cpp` for old CRT-startup-code compatibility both games' source expects to exist. |
| `free_api_fopen`, the global `fopen`→`free_api_fopen` macro redirect | required-by-game | `docs/headers.md`'s "Why windows.h globally redefines fopen" — both games call plain `fopen()` pervasively with backslash paths. |

## Mismatches found (not fixed, filed for follow-up)

1. **`CloseHandle` (`include/handleapi.h`) has zero call sites anywhere** —
   not in either target game, not in any test, not in free-api's own `src/`
   beyond its own definition (`src/winbase_file.cpp` and
   `src/winbase.cpp:59`). It doesn't fit any of `docs/out-of-scope.md`'s
   documented stub/vestigial rows, and `docs/supported-apis.md` has no row
   for it either — it is genuinely undocumented, and its true
   classification is **vestigial-but-harmless**, not the
   **test-infrastructure-only** category `TASK-24H-0114`'s own problem
   text implied it belonged to (that text listed `CloseHandle` alongside
   `GetLastError`/`SetLastError`/`RemoveDirectoryA`/etc. as if all were
   test-only — only `CloseHandle` turns out to have zero test usage too).
   Filed as `TASK-24H-1228` below.

No other mismatch was found between this table and `docs/supported-apis.md`/
`docs/out-of-scope.md` — every other symbol's classification here agrees
with (or directly cites) an existing entry in one of those two files.
