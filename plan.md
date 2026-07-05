# Free API Minimal Game Compatibility Plan

*Generated 2026-07-04. Based on direct source inspection of `free-api` (this repo) and its two target games, `../free-eggbert` and `../planetblupi`, both of which already build against `free-api` and `free-direct` as sibling source dependencies (not a hypothetical future integration).*

---

## 1. Purpose

Free API is a narrow, source-level Win32/WinAPI compatibility layer whose **only** reason to exist is to let two specific legacy Windows games compile and run without Microsoft Windows:

* **Free Eggbert** (`../free-eggbert`, "Speedy Eggbert 2" port)
* **Planet Blupi** (`../planetblupi`)

Both games already `add_subdirectory()` this repo and link against `free-api` (and the sibling `free-direct`, which emulates DirectDraw/DirectSound/DirectPlay on top of SDL3 and, in places, on top of free-api's own GDI subset). Free API's job is **source-level compatibility for these two games** — nothing more.

Free API is explicitly **not**:

* **Not Wine.** It does not emulate a Windows process, PE loader, registry, or kernel objects.
* **Not a general Win32 SDK reimplementation.** It does not aim for API completeness against any Windows SDK version.
* **Not a platform for arbitrary 1998-era Windows games.** Only the two named games define scope.
* Free API **must stay small**. Its size should track the union of what these two games actually call, not what Win32 offers.
* Free API **must only implement what the two target games actually use**, as demonstrated by direct source evidence (grep/read), not by assumption or by what "a game like this would probably need."
* **Any future API addition must cite a real usage site** in `../free-eggbert` or `../planetblupi` (file + line), or be justified as scope-control cleanup/tests/docs for the existing subset.

This document is built entirely from direct evidence gathered from:

* `free-api`: `README.md`, `Documentation.md`, `CMakeLists.txt`, all of `include/`, `include_non_windows/`, `src/`, `tests/`, `todo/`.
* `../free-eggbert`: `CMakeLists.txt`, `include/`, `src/` (excluding `third_party/`, `dxsdk3/`, `bass/`, `msvc5/`, `android/` vendor trees — except `ddutil.cpp`/`wave.cpp`, which live in `src/` and are legitimately in scope despite being adapted from Microsoft DirectX-SDK sample code).
* `../planetblupi`: `CMakeLists.txt`, `include/`, `src/` (excluding `third_party/`, `android/`).

---

## 2. Hard Scope Rules

1. No API may be added just because it existed in Windows 95/98.
2. No API may be added just because it seems useful.
3. No API may be added for hypothetical future games.
4. No large compatibility framework may be introduced.
5. No generic PE/resource loader may be introduced unless the games require it (they don't — see §8/Resources).
6. No generic Win32 subsystem emulation (registry, COM/OLE, security descriptors, threading primitives beyond what's already used) may be added.
7. SDL3 is allowed only as an internal backend detail behind the WinAPI-shaped public surface; it must never leak into public headers.
8. Public headers should expose only what the two games need to compile and link.
9. Every unused public symbol in `include/` must be classified as one of: **required by target game**, **compile-only compatibility**, **accidental legacy leftover**, or **candidate for removal/hiding** (see §5).
10. Every non-trivial implementation must have a test based on behavior actually used by the games (see §7 "Tests" milestone and §8 "Test tasks").

`DirectDraw`/`DirectSound`/`DirectPlay` (`ddraw.h`, `dsound.h`, `dplay.h`, and all `DD*`/`DS*`/`DP*` symbols) are used extensively by both games but belong to the sibling **free-direct** project, not free-api. They are noted where they co-occur with real WinAPI/GDI/WinMM calls but are explicitly **out of scope** for this plan.

---

## 3. Usage Audit of Target Games

Evidence gathered by direct grep/read of `../free-eggbert` and `../planetblupi` source (2026-07-04). Both games already compile unconditionally against the WinAPI-shaped surface on every shipped target (native/SDL3, Android, Emscripten) — **neither game has a `#ifdef _WIN32` bypass path**, so nothing here can be dismissed as "Windows-only, therefore skippable." Dead code (guarded by hardcoded-`FALSE` feature macros such as free-eggbert's `_CD`/`_LEGACY`/`_BASS`/`THREAD`, or free-eggbert's inverse `MMTIMER=TRUE`, or commented-out call sites) is marked **(dead)** and excluded from priority weighting.

### 3.1 Used headers

| Header | free-eggbert | planetblupi | Notes |
|---|---|---|---|
| `windows.h` | Yes (~12 files) | Yes (~15 files) | Central facade; required by nearly every source file in both games. |
| `windowsx.h` | Yes (blupi.cpp, ddutil.cpp, movie.cpp) | Yes (blupi.cpp, ddutil.cpp, movie.cpp) | In **both** games, only the `GetStockBrush` macro is actually used (once, at startup). `ddutil.cpp`/`movie.cpp` include it but use nothing from it in either game. |
| `wtypes.h` | Yes (`misc.hpp:5`, `blupi.cpp:14`) | Not included | free-eggbert only; types (`VARTYPE`/`SCODE`/`DATE`/`CLIPFORMAT`) compile but are never exercised at runtime in either game. |
| `mmsystem.h` | Yes (`blupi.cpp`, `movie.cpp`) | Yes (`blupi.cpp`, `movie.cpp`) | WinMM subset: MCI, MIDI-out, timers. |
| `digitalv.h` | Yes (`movie.cpp`) | Yes (`movie.cpp`) | MCI digital-video (AVI) + sequencer parameter structs. |
| `commdlg.h` | Yes (`movie.cpp:9`) — **zero API calls** | Yes (`movie.cpp:6`) — **zero API calls** | Vestigial dead include in both games (no `GetOpenFileName` etc. anywhere). Keep as empty stub only. |
| `io.h` | Yes (`event.cpp:12,14`, for `_findfirst`/`_findnext`) | Not included; `_findfirst` family confirmed unused | free-eggbert only. |
| `direct.h` | Not literally included; `_mkdir` is called (`event.cpp:4193`) | Yes (`movie.cpp:10`) — **zero API calls** (`_chdir`/`_mkdir`/`_getcwd` confirmed unused) | Dead/vestigial in planetblupi; free-eggbert needs `_mkdir` to remain declared/working regardless of which header path supplies it. |
| `sys/timeb.h` (`ftime`/`struct timeb`) | Yes (`blupi.cpp`, `pixmap.cpp`) | Not used | free-eggbert only (already covered by `tests/test_timeb.cpp`). |
| `ddraw.h`, `dsound.h`, `dplay.h`/`"dplay.h"` | Yes | Yes | **Out of scope** — belongs to sibling project `free-direct`. Listed only because these headers co-occur with real WinAPI/GDI symbols in the same files. |

### 3.2 Used WinAPI functions

Grouped by area for readability; all rows belong to the same logical "3.2 Used WinAPI functions" evidence set. "Dead" means the call site exists but never executes in the current build (see per-game audits for the exact guard).

#### Window lifecycle / class registration

| Function | free-eggbert | planetblupi | Source location | Required behavior | Current Free API status | Priority |
|---|---|---|---|---|---|---|
| `RegisterClassA` | Yes | Yes | eggbert `blupi.cpp:707,728`; planetblupi `blupi.cpp:620` | Register a `WNDCLASSA` once at startup; class must be usable by `CreateWindow(Ex)A`. | PARTIAL | P0 |
| `CreateWindowExA` | Yes (fullscreen) | Yes (fullscreen) | eggbert `blupi.cpp:733-746`; planetblupi `blupi.cpp:625-638` | Fullscreen popup window, `WS_EX_TOPMOST`, sized from `GetSystemMetrics`. | PARTIAL | P0 |
| `CreateWindowA` | Yes (windowed) | Yes (windowed) | eggbert `blupi.cpp:768-780`; planetblupi `blupi.cpp:653-665` | Windowed mode, `WS_POPUPWINDOW\|WS_CAPTION\|WS_VISIBLE`, parent `HWND_DESKTOP`; must return non-NULL real `HWND`. | PARTIAL | P0 |
| `ShowWindow` | Yes | Yes | eggbert `blupi.cpp:784`; planetblupi `blupi.cpp:677` | Called once at startup. | PARTIAL | P0 |
| `UpdateWindow` | Yes | Yes | both `blupi.cpp` (startup) + both `movie.cpp` (movie open/close) | Force repaint/raise. | PARTIAL | P1 |
| `SetFocus` | Yes | Yes | eggbert `blupi.cpp:786`; planetblupi `blupi.cpp:679` | Called once at startup. | PARTIAL | P1 |
| `DestroyWindow` | Yes (fatal init failure only) | Yes (fatal init failure only) | eggbert `blupi.cpp:664`; planetblupi `blupi.cpp:585` | Rare path; must not crash. | PARTIAL | P2 |
| `GetStockBrush` (windowsx.h) | Yes (once) | Yes (once) | eggbert `blupi.cpp:725`; planetblupi `blupi.cpp:617` | `wc.hbrBackground`; only needs a valid non-null `HBRUSH` (window is always fully covered by the game's own blit before visible). | STUB | P2 |
| `AdjustWindowRect` | Yes — **live**, non-`_LEGACY` branch | Yes — **live**, windowed path | eggbert `blupi.cpp:765`; planetblupi `blupi.cpp:650` | Must size the window so the **client area** is exactly the game's internal resolution (640×480-class). | **STUB** ⚠ | **P0/P1** |
| `GetSystemMetrics` (`SM_CXSCREEN`/`SM_CYSCREEN`/`SM_CYCAPTION`) | Yes | Yes | eggbert `blupi.cpp:740,741,753,754`; planetblupi `blupi.cpp:632,633,645,646,651` | Real screen/caption dimensions at startup. | PARTIAL | P0 |
| `SetRect` (inline macro) | Yes | Yes | eggbert `blupi.cpp:756`; planetblupi `blupi.cpp:648` | Compute centered window rect once. | IMPLEMENTED | P1 |

#### Message loop

| Function | free-eggbert | planetblupi | Source location | Required behavior | Current Free API status | Priority |
|---|---|---|---|---|---|---|
| `PeekMessageA` | Yes (`PM_NOREMOVE` gate every loop) | Yes (same pattern) | eggbert `blupi.cpp:906`; planetblupi `blupi.cpp:904` | Non-blocking poll every loop iteration before blocking `GetMessage`. | IMPLEMENTED | P0 |
| `GetMessageA` | Yes | Yes | eggbert `blupi.cpp:908`; planetblupi `blupi.cpp:906` | **Must return `FALSE`/0 exactly on `WM_QUIT`** — planetblupi relies purely on the return value, never checks `msg.message==WM_QUIT` explicitly. | IMPLEMENTED (verified in `src/winuser_message.cpp`) | P0 |
| `TranslateMessage` / `DispatchMessageA` | Yes | Yes | both, every message | Standard pump; `DispatchMessageA` must synchronously invoke `WndProc`. | PARTIAL | P0 |
| `WaitMessage` | Yes (idle, `!g_bActive`) | Yes (idle, `!g_bActive`) | eggbert `blupi.cpp:921`; planetblupi `blupi.cpp:919` | Must not busy-spin when inactive. | PARTIAL | P1 |
| `PostQuitMessage` | Yes | Yes | eggbert `blupi.cpp:633`; planetblupi `blupi.cpp:564` | Posted from `WM_DESTROY`. | IMPLEMENTED (verified) | P0 |
| `PostMessageA` | Yes (from WinMM timer thread + UI) | Yes (UI: `WM_CLOSE`, custom messages) | eggbert `blupi.cpp:647`, `event.cpp:3265,3500,4220`; planetblupi `event.cpp:4754,4807,5024` | Must be safely callable cross-thread (eggbert's `TimerStep` posts from the WinMM timer thread). | PARTIAL (mutex-protected queue) | P0 |
| `DefWindowProcA` | Yes (fallback for all unhandled + implicit `WM_CLOSE`) | Yes (same) | both, default fallback | **Both games post `WM_CLOSE` but never handle it explicitly** — real Win32 default behavior (`DestroyWindow`→`WM_DESTROY`→`PostQuitMessage`) is required. | **IMPLEMENTED** (verified: `src/winuser_message.cpp:195-220` already does exactly this) | P0 |
| `SetWindowTextA` | Yes (`WM_ACTIVATEAPP`) | Yes (`WM_ACTIVATEAPP`) | eggbert `blupi.cpp:532,541`; planetblupi `blupi.cpp:453,462` | Cosmetic title change; return ignored. | PARTIAL | P2 |
| `MessageBoxA` | Yes (fatal init failure only) | Yes (fatal init failure only) | eggbert `blupi.cpp:661`; planetblupi `blupi.cpp:582` | `MB_OK`; rare. | STUB | P2 |
| `GetClientRect` | Yes | Yes — **hot path**, every frame | eggbert `pixmap.cpp:1499,1842`; planetblupi `pixmap.cpp:975,1315` | Must return real, current client-area size. | PARTIAL | P0 |
| `MoveWindow` | Yes (movie child window) | Yes (movie child window) | eggbert `movie.cpp:71-74`; planetblupi `movie.cpp:68-70` | Reposition MCI movie window. | PARTIAL | P2 |
| `InvalidateRect` | Yes (movie open/close) | Not confirmed | eggbert `movie.cpp:90,160` | Force repaint around movie transitions. | STUB | P2 |

#### Timers

| Function | free-eggbert | planetblupi | Source location | Required behavior | Current Free API status | Priority |
|---|---|---|---|---|---|---|
| `timeSetEvent`/`timeKillEvent` | **Yes — the live frame-pump mechanism** | Not used (confirmed zero hits) | eggbert `blupi.cpp:891,628` | WinMM multimedia timer must fire `TimerStep` reliably at ~50ms and allow safe `PostMessage` from the timer thread. | PARTIAL | **P0 (eggbert)** |
| `SetTimer`/`KillTimer`/`WM_TIMER` | Dead code (`MMTIMER` hardcoded TRUE) | **Yes — the live frame-pump mechanism** | planetblupi `blupi.cpp:900,562,392,416-426` | `WM_TIMER` must be generated reliably while the message loop polls `PeekMessage`/`GetMessage`; drives the entire game tick (`UpdateFrame`/`Display`). | IMPLEMENTED (verified: `src/winuser_timer.cpp` + WM_TIMER synthesis in `FreeApiMessageQueue.cpp`) | **P0 (planetblupi)** |

*Both mechanisms are required — each game uses a different one as its real frame pump; neither is optional.*

#### Input (mouse / keyboard / cursor)

| Function | free-eggbert | planetblupi | Source location | Required behavior | Current Free API status | Priority |
|---|---|---|---|---|---|---|
| `GetCursorPos` | Yes | Yes | eggbert `blupi.cpp:598`, `event.cpp:1865`; planetblupi `blupi.cpp:519`, `event.cpp:1517` | Real screen-space cursor position. | PARTIAL | P0 |
| `ScreenToClient` | Yes | Yes | paired with `GetCursorPos` above | Correct client coordinates. | PARTIAL | P0 |
| `ClientToScreen` | Yes | Yes — **hot path**, every frame | eggbert `pixmap.cpp:168,1500-1501`; planetblupi `pixmap.cpp:976-977,1112-1113,1316-1317` (called every `Display()`) | Must reflect the window's real, current position — recomputed every displayed frame in planetblupi. | PARTIAL | P0 |
| `SetCursorPos` | Yes (continuous, software-cursor mode + demo replay) | Yes (continuous, software-cursor mode + demo replay + menu popups) | eggbert `pixmap.cpp:169`, `event.cpp:3459,5294`; planetblupi `menu.cpp:122`, `pixmap.cpp:137`, `event.cpp:4524,4982` | Load-bearing, not cosmetic — re-warps OS cursor to match game state, potentially every frame. | PARTIAL | P0 |
| `ShowCursor` | Yes | Yes | eggbert `pixmap.cpp:165`, `event.cpp:3448,4931...`; planetblupi `pixmap.cpp:133`, `event.cpp:2777,4971,5003` | **Real** OS cursor visibility toggle — both games hide the OS cursor while drawing their own sprite cursor. | **STUB** ⚠ | **P0/P1** |
| `SetCursor` | Yes (per sprite-swap) | Not confirmed as heavily | eggbert `misc.cpp:60` | Real cursor handle hand-off when not in software-cursor mode. | **STUB** ⚠ | P1 |
| `LoadCursorA`/`LoadCursor` | Yes (12 named IDs, per sprite-swap) | Yes (13 named IDs, per sprite-swap) | eggbert `misc.cpp:48-59`; planetblupi `misc.cpp:56-69` | Only needs a non-null handle — real shape not gameplay-relevant. | STUB (acceptable safe stub) | P2 |
| VK_* keyboard set (`WM_KEYDOWN`/`UP`) | Yes (F1-F12, ESC, RETURN, SHIFT, CONTROL, PAUSE, arrows, HOME/END, SPACE) | Yes (same set + A-Z ASCII cheat codes) | see §3.4 | Correct `VK_*` codes and raw ASCII A-Z passthrough. | IMPLEMENTED | P0 |
| `WM_SYSKEYDOWN`/`UP` + `VK_F10` remap quirk | Yes | Yes | eggbert `blupi.cpp:457,461`; planetblupi `blupi.cpp:402,406` | Real Win32 quirk (F10 alone triggers system menu) — game remaps it itself; free-api only needs to deliver `WM_SYSKEYDOWN`/`UP` correctly. | IMPLEMENTED (per README) | P0 |
| `MK_*` flags in mouse-message `wParam` | **No** (confirmed zero use) | **Yes** (`MK_LBUTTON`,`MK_RBUTTON`,`MK_SHIFT`,`MK_CONTROL`) | planetblupi `event.cpp:3994,3413,3440...` | Live bit flags required in `WM_MOUSEMOVE`'s `wParam`. | IMPLEMENTED (per README) — **untested** | P1 |
| `GetAsyncKeyState`/`GetKeyState` | No (confirmed zero use) | No (confirmed zero use) | — | Not needed by either game. | Not implemented (correctly out of scope) | — |

#### GDI

| Function | free-eggbert | planetblupi | Source location | Required behavior | Current Free API status | Priority |
|---|---|---|---|---|---|---|
| `CreateCompatibleDC`/`DeleteDC` | Yes (throwaway DC) | Yes (throwaway DC) | eggbert `pixmap.cpp:137,152`, `ddutil.cpp:141,166`; planetblupi `ddutil.cpp:198,230`, `pixmap.cpp:282,293` | Memory DC only, paired lifecycle. | PARTIAL | P0 |
| `GetDeviceCaps` (`SIZEPALETTE`) | Yes (once) | Yes (startup + every reactivation) | eggbert `pixmap.cpp:146,428`; planetblupi `pixmap.cpp:287` | Plausible value; branch only on `<257`. | PARTIAL | P0 |
| `GetSystemPaletteEntries` | Not confirmed | Yes | planetblupi `pixmap.cpp:292` | Fill 256 `PALETTEENTRY` (grayscale placeholder acceptable). | PARTIAL | P1 |
| `LoadImageA` | Yes (`LR_LOADFROMFILE`, resource attempt first) | Yes (extension-agnostic — `.blp` files are BMP data) | eggbert `ddutil.cpp:46,49,94,97`; planetblupi `ddutil.cpp:90,95,148,151` | **Primary game-asset loading path for every sprite sheet in both games.** Must decode real BMP bytes regardless of file extension. | PARTIAL | P0 |
| `GetObjectA` | Yes | Yes | eggbert `ddutil.cpp:57,149`; planetblupi `ddutil.cpp:47,104,208` | Must return correct `bmWidth`/`bmHeight` — drives DirectDraw surface sizing in free-direct. | PARTIAL | P0 |
| `SelectObject`/`DeleteObject` | Yes | Yes | both `ddutil.cpp` | Bitmap-into-DC lifecycle around blit. | PARTIAL | P0 |
| `StretchBlt` (`SRCCOPY`) | Yes | Yes | eggbert `ddutil.cpp:162`; planetblupi `ddutil.cpp:224` | **Every sprite sheet asset in both games passes through this at load time.** | PARTIAL | P0 |
| `GetPixel`/`SetPixel` | Yes (`DDColorMatch`) | Yes (`DDColorMatch`) | eggbert `ddutil.cpp:289-290,314`; planetblupi `ddutil.cpp:360,361,391` | Color-key round-trip through the same backing pixel memory DirectDraw's `Lock()` exposes — correctness-critical for sprite transparency in both games. | PARTIAL | P0 |
| `GetModuleHandle` (as `LoadImageA`'s resource-instance arg) | Yes | Yes | both `ddutil.cpp` | Only used as an always-missing resource lookup instance handle — real value not important. | STUB | P2 |
| `_lopen`/`_lread`/`_lclose` | Yes (fallback) | Yes (fallback, the **live** path since `FindResourceA` always misses today) | eggbert `ddutil.cpp:232,237-240`; planetblupi `ddutil.cpp:297,303-306` | Read raw BMP palette bytes from a real file. | PARTIAL | P0 |
| `CreateBitmap` | Not confirmed | Yes (minimap rebuild) | planetblupi `decmap.cpp:578,582` | Build device-dependent bitmap from raw pixel buffer (1bpp or 8bpp). | PARTIAL | P1 |

#### Resources

| Function | free-eggbert | planetblupi | Source location | Required behavior | Current Free API status | Priority |
|---|---|---|---|---|---|---|
| `FindResourceA` (`RT_BITMAP`) | Yes — **always misses today** (no `.rc` compiled) | Yes — **always misses today** (no `.rc` compiled) | eggbert `ddutil.cpp:204`; planetblupi `ddutil.cpp:268` | Miss-then-fallback-to-`_lopen` is the actual live behavior in both games today. | STUB (already correctly "safe") | P1 |
| `LoadResource`/`LockResource`/`SizeofResource`/`UnlockResource`/`FreeResource` | Yes (`wave.cpp`, dead/unreached at runtime) | Yes (`wave.cpp`, dead/unreached at runtime) | both `wave.cpp` | Must type-check/compile; runtime correctness never exercised by either game. | STUB (acceptable) | P3 |
| `LoadStringA` | **Yes — ~90+ call sites, ALL on-screen UI text** | **Yes — ~50+ call sites, ALL on-screen UI text** | eggbert `misc.cpp:36-39` + call sites; planetblupi `misc.cpp:42-45` + call sites | **Must return real, correct string-table text** — tooltips, labels, win/lose text. | **STUB** ⚠ | **P0/P1** |
| `LoadIconA`/`LoadCursorA` (as resource lookups) | Yes (once/per-swap, string names) | Yes (once/per-swap, string names) | see Input table above | Only needs a non-null handle; visual shape not gameplay-relevant. | STUB (acceptable) | P2 |

#### WinMM / MCI / MIDI

| Function | free-eggbert | planetblupi | Source location | Required behavior | Current Free API status | Priority |
|---|---|---|---|---|---|---|
| `mciSendCommandA` — `"sequencer"` (MIDI music) | Yes | Yes | eggbert `sound.cpp:601-624,664,724-763`; planetblupi `sound.cpp:519-621` | Core background-music path in both games. | IMPLEMENTED/PARTIAL | P0 |
| `mciSendCommandA` — `"avivideo"`/digital-video (movies) | Yes | Yes | eggbert `movie.cpp:46,60,83,132-153,198,206`; planetblupi `movie.cpp:43-203` | Both games' `movie.cpp` drive a full `MCI_DGV_OPEN/STATUS/PLAY/PAUSE/CLOSE` sequence for AVI movie playback. | **STUB** ("digital video" explicitly unimplemented per `digitalv.h` doc comment) ⚠ | **Highest-risk item — see §10** |
| `mciGetErrorStringA` | Yes | Yes | both, on MCI failure only | Non-crashing string; exact text not load-bearing. | IMPLEMENTED | P2 |
| `mciGetDeviceIDA` | Yes (movie teardown) | Yes (movie teardown) | eggbert `movie.cpp:59`; planetblupi `movie.cpp:56` | Looks up already-open device ID once at teardown. | STUB (returns 0; low-impact) | P2 |
| `midiOutGetNumDevs`/`midiOutOpen`/`midiOutSetVolume`/`midiOutClose` | Yes (on music (re)start / volume change) | Yes (same) | eggbert `sound.cpp:290-304`; planetblupi `sound.cpp:256-270` | Iterate MIDI devices to apply volume; not per-frame. | IMPLEMENTED | P1 |
| `PlaySound` (real WinMM) | No — shadowed by game's own `PlaySound` | No | eggbert `def.hpp:10` `#undef` | Not needed for either game. | Not implemented (correct) | — |
| `waveOut*` family | No (confirmed zero hits) | No (confirmed zero hits; effects go via DirectSound/free-direct) | — | Not needed. | Not implemented (correct) | — |

#### File / path / resource

| Function | free-eggbert | planetblupi | Source location | Required behavior | Current Free API status | Priority |
|---|---|---|---|---|---|---|
| `fopen`/`fread`/`fwrite`/`fclose` (CRT) | Yes — **only real file I/O mechanism** | Yes — **only real file I/O mechanism** | pervasive in both | `CreateFile`/`ReadFile`/`WriteFile` confirmed unused in both games. | IMPLEMENTED (`free_api_fopen` backslash-normalizing wrapper) | P0 |
| `CreateDirectoryA` | Dead code (`_CD\|\|_LEGACY` FALSE) | **Yes — live**, save-path bootstrap | planetblupi `misc.cpp:220,230` (`AddUserPath`) | Must really create a directory on disk. | PARTIAL | **P0 (planetblupi)** |
| `_mkdir` (direct.h) | Yes (`"\User"` bootstrap) | No (confirmed unused) | eggbert `event.cpp:4193` | Must really create directory. | PARTIAL (via `crt_direct.cpp`) | P1 |
| `_findfirst`/`_findnext`/`_findclose` | Yes (design-mission file picker) | No (confirmed unused) | eggbert `event.cpp:4736,4741,4747` | List `"\User\*.xch"` files — a real, non-startup-blocking feature. | **STUB** (always fails) ⚠ | P2 |
| `_lopen`/`_lread`/`_lclose` | Yes | Yes | see GDI table | Legacy file I/O for palette read. | PARTIAL | P0 |

### 3.3 Used WinAPI types and structs

| Type / struct | free-eggbert | planetblupi | Source location | Required fields / behavior | Current Free API status | Priority |
|---|---|---|---|---|---|---|
| `HWND`, `HINSTANCE`, `HDC`, `HBITMAP`, `HCURSOR`, `HRSRC`, `HGLOBAL`, `HMODULE` | Yes | Yes | pervasive | Opaque handle identity semantics only. | IMPLEMENTED (HEADER_ONLY, all `void*` aliases) | P0 |
| `WNDCLASSA` | Yes (full field set) | Yes (full field set) | eggbert `blupi.cpp:707-727`; planetblupi `blupi.cpp:593,609-619` | `style, lpfnWndProc, cbClsExtra, cbWndExtra, hInstance, hIcon, hCursor, hbrBackground, lpszMenuName, lpszClassName`. | IMPLEMENTED | P0 |
| `MSG` | Yes | Yes | both `WinMain` loops | Standard fields; `wParam` read as exit code by planetblupi. | PARTIAL | P0 |
| `CREATESTRUCTA`/`LPCREATESTRUCT` | Yes (`.hInstance` only) | Yes (`.hInstance` only) | eggbert `blupi.cpp:508`; planetblupi `blupi.cpp:429` | One field read on `WM_CREATE`. | PARTIAL | P1 |
| `POINT` | Yes (pervasive, generic 2D int) | Yes (pervasive, generic 2D int) | both | `.x`/`.y`. | IMPLEMENTED | P0 |
| `RECT` | Yes | Yes | both | `.left/.top/.right/.bottom`; used with `SetRect`/`IntersectRect`/`UnionRect`. | IMPLEMENTED | P0 |
| `WPARAM`/`LPARAM`/`LRESULT`/`UINT` | Yes (also **persisted to disk** in demo-recording files) | Yes (also **persisted to disk** in demo-recording `DemoEvent` structs) | eggbert `event.cpp:5276-5297`; planetblupi `include/event.h:70-77` | Bit-exact Win32 packing required (`LOWORD`=x/`HIWORD`=y for mouse) — demo file compatibility depends on it. | IMPLEMENTED | P0 |
| `COLORREF` | Yes (`RGB()` macro, ~20 sites) | Yes (`RGB()` macro, ~20 sites) | both | Value-only, DirectDraw color-keying — no real GDI paint calls in either game. | IMPLEMENTED | P0 |
| `PALETTEENTRY` | Yes (byte-layout read) | Yes (byte-layout read/write) | eggbert `ddutil.cpp:187`; planetblupi `ddutil.cpp:251,259-262` | `.peRed/.peGreen/.peBlue/.peFlags` — exact byte layout required. | PARTIAL | P0 |
| `BITMAP`, `BITMAPFILEHEADER`, `BITMAPINFOHEADER`, `RGBQUAD` | Yes | Yes | both `ddutil.cpp` | Real byte-layout structs parsed from BMP data. | STUB (structs defined; behavior lives in `.cpp`) | P0 |
| `WAVEFORMATEX`/`WAVEFORMAT`/`PCMWAVEFORMAT` | Yes (`wave.cpp`, dead-at-runtime) | Yes (`wave.cpp`/`sound.cpp`, live for DirectSound buffer setup — free-direct scope) | both | RIFF/WAVE header layout. | PARTIAL | P2 |
| `MMRESULT`, `MCIDEVICEID`, `MCIERROR` | Yes | Yes | both | WinMM/MCI return/id types. | IMPLEMENTED | P0 |
| `MCI_OPEN_PARMS`/`MCI_PLAY_PARMS`/`MCI_GENERIC_PARMS`/`MCI_SET_PARMS` | Yes | Yes | both `sound.cpp` | Sequencer MCI param blocks. | PARTIAL | P0 |
| `MCI_DGV_OPEN_PARMS`/`WINDOW_PARMS`/`STATUS_PARMS`/`PLAY_PARMS`/`PAUSE_PARMS` | Yes | Yes | both `movie.cpp` | Digital-video MCI param blocks — needed for movie playback. | STUB | **High risk — see §10** |
| `JOYINFOEX` | Yes (`.dwSize=52,.dwFlags=255`, reads `.dwXpos/.dwYpos/.dwButtons` every poll) | Not used | eggbert `event.cpp:2069-2125` | Real per-frame analog/button data when a joystick is selected (optional control scheme). | STUB (safe fallback: reports 0 devices) | P2 |
| `struct timeb` | Yes (`.millitm` read) | Not used | eggbert `blupi.cpp:670,676-677` | Coarse startup benchmarking only. | IMPLEMENTED (tested) | P2 |
| `struct _finddata_t` | Yes (`.name` read) | Not used | eggbert `event.cpp:4736-4747` | File enumeration for design-file picker. | HEADER_ONLY (impl is stub) | P2 |
| `SECURITY_ATTRIBUTES` | Dead code | Yes (constructed, fields inert) | planetblupi `misc.cpp:199,217-219` | Passed to `CreateDirectoryA`; ACL semantics not required. | PARTIAL | P2 |
| `GUID` | No real value ever constructed (always `NULL` passed to Direct* `Create` calls) | No real value ever constructed | — | Bare type only; free-direct's concern. | HEADER_ONLY | P3 |

### 3.4 Used constants and macros

| Constant / macro | free-eggbert | planetblupi | Source location | Required meaning | Current Free API status | Priority |
|---|---|---|---|---|---|---|
| `MAX_PATH` | Yes (~20 sites, arithmetic like `MAX_PATH-30` done on it) | Yes (~20 sites) | both | Must be the real value 260. | IMPLEMENTED | P0 |
| `WS_POPUP`,`WS_POPUPWINDOW`,`WS_CAPTION`,`WS_VISIBLE`,`WS_CHILD`,`WS_EX_TOPMOST` | Yes | Yes | both, window creation | Window style flags. | IMPLEMENTED | P0 |
| `CS_HREDRAW`,`CS_VREDRAW` | Yes | Yes | both, `WNDCLASSA.style` | Class style flags. | IMPLEMENTED | P0 |
| `HWND_DESKTOP` | Yes | Yes | both | Parent sentinel for windowed `CreateWindow`. | IMPLEMENTED | P0 |
| `BLACK_BRUSH` | Yes | Yes | both | `GetStockBrush` argument. | IMPLEMENTED | P1 |
| `SM_CXSCREEN`,`SM_CYSCREEN`,`SM_CYCAPTION` | Yes | Yes | both | `GetSystemMetrics` indices. | IMPLEMENTED | P0 |
| `PM_NOREMOVE`,`PM_REMOVE` | Yes | Yes | both | `PeekMessage` flags. | IMPLEMENTED | P0 |
| `TIME_PERIODIC` | Yes (eggbert's live timer flag) | Not used | eggbert `blupi.cpp:891` | `timeSetEvent` periodic flag. | IMPLEMENTED | P0 |
| `MM_MCINOTIFY`, `MCI_NOTIFY_SUCCESSFUL` | Yes | Yes | both | Real notification arrival needed to loop background music. | PARTIAL | P0 |
| `MB_OK` | Yes | Yes | both | `MessageBoxA` flag. | IMPLEMENTED | P2 |
| `RGB(r,g,b)` | Yes (~20 sites) | Yes (~20 sites) | both | `COLORREF` packing macro. | IMPLEMENTED | P0 |
| `LOWORD`/`HIWORD` | Yes | Yes | both | Extract packed x/y or rank/frame from `DWORD`/`LPARAM`. | IMPLEMENTED | P0 |
| `MAKELONG` | Yes (movie hWnd packing) | Not confirmed | eggbert `movie.cpp:192` | Pack `hWnd` into MCI callback field. | IMPLEMENTED | P1 |
| `MAKEINTRESOURCEA` | Yes | Yes | both `wave.cpp` | Integer-resource-id encoding for `FindResourceA`. | IMPLEMENTED | P1 |
| `IMAGE_BITMAP`,`LR_CREATEDIBSECTION`,`LR_LOADFROMFILE` | Yes | Yes | both `ddutil.cpp` | `LoadImageA` flags. | IMPLEMENTED | P0 |
| `OF_READ` | Yes | Yes | both | `_lopen` flag. | IMPLEMENTED | P0 |
| `SRCCOPY`,`CLR_INVALID`,`RT_BITMAP`,`SIZEPALETTE` | Yes | Yes | both | GDI/resource constants. | IMPLEMENTED | P0 |
| `VK_F1`-`VK_F12`,`VK_ESCAPE`,`VK_RETURN`,`VK_SHIFT`,`VK_CONTROL`,`VK_PAUSE`,`VK_LEFT/RIGHT/UP/DOWN`,`VK_HOME`,`VK_END`,`VK_SPACE` | Yes | Yes | both `event.cpp` | Correct virtual-key codes tested in `WM_KEYDOWN`/`UP`. | IMPLEMENTED | P0 |
| `MK_LBUTTON`,`MK_RBUTTON`,`MK_SHIFT`,`MK_CONTROL`,`MK_MBUTTON` | No (confirmed zero use) | Yes (`MK_LBUTTON/RBUTTON/SHIFT/CONTROL`) | planetblupi `event.cpp` | Live bit flags in mouse-message `wParam`. | IMPLEMENTED — untested | P1 |
| `JOY_BUTTON1`-`JOY_BUTTON4` | Yes (tested every poll) | Not used | eggbert `event.cpp:2089-2125` | Bitmask against `(BYTE)joy.dwButtons`. | IMPLEMENTED (constant only; backing data is STUB) | P2 |
| Full MCI command/flag set (`MCI_OPEN`,`MCI_CLOSE`,`MCI_PLAY`,`MCI_PAUSE`,`MCI_SET`,`MCI_STATUS`,`MCI_STATUS_ITEM`,`MCI_NOTIFY`,`MCI_WAIT`,`MCI_OPEN_TYPE`,`MCI_OPEN_TYPE_ID`,`MCI_OPEN_ELEMENT`,`MCI_SET_TIME_FORMAT`,`MCI_FORMAT_TMSF`,`MCI_TRACK`,`MCI_DGV_OPEN_PARENT`,`MCI_DGV_OPEN_WS`,`MCI_DGV_PLAY_REVERSE`,`MCI_DGV_STATUS_HWND`) | Yes | Yes | both | Command/flag values for sequencer + digital-video MCI use. | PARTIAL (sequencer); STUB (digital-video) | Mixed — see §10 |
| `MMSYSERR_NOERROR` | Yes | Yes | both | MIDI API return-code check. | IMPLEMENTED | P1 |
| `mmioFOURCC(...)` | Not confirmed live | Yes (`wave.cpp`) | planetblupi | RIFF chunk-tag macro. | IMPLEMENTED | P2 |

### 3.5 Used message IDs

| Message | free-eggbert | planetblupi | Source location | Required behavior | Current Free API status | Priority |
|---|---|---|---|---|---|---|
| `WM_CREATE` | Yes | Yes | both, once | Deliver `CREATESTRUCT.hInstance`. | IMPLEMENTED | P0 |
| `WM_DESTROY` | Yes | Yes | both | Kill timer, teardown, `PostQuitMessage`. | IMPLEMENTED | P0 |
| `WM_CLOSE` | Yes (posted, never handled explicitly) | Yes (posted, never handled explicitly) | both | Default behavior must `DestroyWindow`+`PostQuitMessage`. | **IMPLEMENTED** (verified `src/winuser_message.cpp:200-208`) | P0 |
| `WM_TIMER` | Dead code (eggbert uses custom `WM_UPDATE` instead) | **Yes — live, every tick** | planetblupi `blupi.cpp:392,416-426` | Reliable periodic delivery drives `UpdateFrame`/`Display`. | IMPLEMENTED | P0 |
| `WM_ACTIVATEAPP` | Yes | Yes | both | `wParam` truthiness toggles active state, surfaces restore, music suspend/resume. | IMPLEMENTED | P0 |
| `WM_SYSCOLORCHANGE`,`WM_QUERYNEWPALETTE`,`WM_PALETTECHANGED`,`WM_DISPLAYCHANGE` | Yes (logged only) | Yes (logged only) | both | No functional handling needed — just deliverable without crashing. | IMPLEMENTED | P2 |
| `MM_MCINOTIFY` | Yes | Yes | both | Must arrive with `wParam=MCI_NOTIFY_SUCCESSFUL` on real playback completion — drives music looping. | PARTIAL | P0 |
| `WM_SETCURSOR` | Yes (`return TRUE`) | Yes (`return TRUE`) | both | Correct "handled" semantics only. | PARTIAL | P1 |
| `WM_LBUTTONDOWN`/`UP`,`WM_RBUTTONDOWN`/`UP` | Yes | Yes | both | Standard click dispatch. | IMPLEMENTED (tested) | P0 |
| `WM_MOUSEMOVE` | Yes (demo-replay-critical) | Yes (demo-replay-critical, hot path) | both | `lParam` must be bit-exact `LOWORD=x,HIWORD=y` — demo files replay raw recorded triples. | IMPLEMENTED (tested) | P0 |
| `WM_NCMOUSEMOVE` | Yes | Yes | both | Cursor-leaves-client-area handling. | PARTIAL | P1 |
| `WM_KEYDOWN`/`WM_KEYUP` | Yes | Yes | both | `VK_*` codes + planetblupi's raw A-Z ASCII cheat sequence. | IMPLEMENTED (tested) | P0 |
| `WM_SYSKEYDOWN`/`WM_SYSKEYUP` | Yes (`VK_F10` only) | Yes (`VK_F10` only) | both | Real delivery so games can remap it themselves. | IMPLEMENTED | P0 |

**Confirmed unused by both games:** `WM_PAINT`, `WM_QUIT` (only via `GetMessage`'s return contract, never referenced by name), `WM_COMMAND`, `WM_SYSCOMMAND`, `WM_CHAR`, `WM_MBUTTONDOWN`/`UP`, `WM_ERASEBKGND`, `WM_SIZE`, `WM_MOVE`.

Custom app messages (`WM_USER+N`, ~150-220 values across both games — `WM_UPDATE`, `WM_DECOR*`, `WM_BUTTON*`, `WM_PHASE_*`, etc.) are **not real WinAPI messages**; only `WM_USER`'s numeric value (0x0400) must be correct so they don't collide with real system messages.

### 3.6 Used GDI behavior

| API / behavior | free-eggbert | planetblupi | Source location | Required behavior | Current Free API status | Priority |
|---|---|---|---|---|---|---|
| Load an arbitrary-named on-disk file as a BMP (extension-agnostic) | Yes | Yes | both `ddutil.cpp` (`DDLoadBitmap`-equivalent) | Decode real BMP bytes regardless of filename extension (`.blp`). | PARTIAL | P0 |
| Memory-DC → surface-DC blit | Yes | Yes | both `ddutil.cpp` (`DDCopyBitmap`-equivalent) | `CreateCompatibleDC→SelectObject→StretchBlt(SRCCOPY)→DeleteDC`, pixel-correct. | PARTIAL | P0 |
| Color-key computation (`GetPixel`/`SetPixel` round-trip) | Yes | Yes | both `ddutil.cpp` (`DDColorMatch`-equivalent) | Must reflect the same backing pixel memory free-direct's `Lock()` exposes. | PARTIAL | P0 |
| Client-rect → screen-rect conversion, every frame | Yes | Yes — hot path | both `pixmap.cpp` `Display()` | Real, current window position/size every frame. | PARTIAL | P0 |
| System palette introspection | Yes | Yes | both, `InitSysPalette`-equivalent | 8-bit-vs-true-color decision; re-run on reactivation. | PARTIAL | P1 |
| Runtime raw-bits bitmap creation | Not confirmed | Yes (minimap) | planetblupi `decmap.cpp` | 1bpp/8bpp `CreateBitmap` from memory buffer. | PARTIAL | P1 |
| Cursor show/hide/position tracking | Yes | Yes | both | Real OS cursor visibility/position must track game's internal mouse state. | **STUB** ⚠ | P0/P1 |

### 3.7 Used WinMM / MCI behavior

| API / behavior | free-eggbert | planetblupi | Source location | Required behavior | Current Free API status | Priority |
|---|---|---|---|---|---|---|
| MIDI sequencer open/play/close (background music) | Yes | Yes | both `sound.cpp` | Core music path — already the best-covered subsystem. | IMPLEMENTED/PARTIAL | P0 |
| `MM_MCINOTIFY`-driven music looping | Yes | Yes | both | Game-side restart-on-notify logic; free-api must deliver the notification reliably (does **not** need internal loop-flag support). | PARTIAL | P0 |
| MIDI volume iteration | Yes | Yes | both `sound.cpp` | Enumerate + set volume on all MIDI-out devices. | IMPLEMENTED | P1 |
| Digital-video (AVI movie) open/status/play/pause/close | Yes | Yes | both `movie.cpp` | Full `MCI_DGV_*` sequence, including handing back a real, movable window handle for the video surface. | **STUB** ⚠ | **Highest risk — see §10** |
| `"cdaudio"` device type | Yes (attempted, alt music path) | Yes (attempted, alt music path) | both `sound.cpp` | Graceful decline without breaking MIDI fallback. | IMPLEMENTED (declined) | P1 |

### 3.8 Used file/path/resource behavior

| API / behavior | free-eggbert | planetblupi | Source location | Required behavior | Current Free API status | Priority |
|---|---|---|---|---|---|---|
| CRT stdio for all persistent game data | Yes | Yes | both | `fopen`/`fread`/`fwrite`/`fclose`, no `CreateFile`/`ReadFile`/`WriteFile` anywhere. | IMPLEMENTED | P0 |
| Backslash-path normalization | Yes | **Yes — inconsistent**: `image\*.blp` uses backslash, `data/`/`sound/`/`movie/` use forward slash | planetblupi (documented across ~30 sites) | Both separator styles must resolve on POSIX filesystems. | IMPLEMENTED (`free_api_fopen` wrapper in `windows.h`) | P0 |
| Real directory creation for save data | Dead code (eggbert) | **Yes — live** (`AddUserPath`) | planetblupi `misc.cpp:220,230` | Must actually create `user_data/`-style directories on disk. | PARTIAL | P0 |
| Design-file picker enumeration | Yes | Not used | eggbert `event.cpp:4736-4747` | Real `_findfirst`/`_findnext` directory listing. | **STUB** (always fails) ⚠ | P2 |
| Resource-section fallback-to-file-read | Yes (always exercised — `.rc` not compiled) | Yes (always exercised — `.rc` not compiled) | both `ddutil.cpp` | `FindResourceA` miss → `_lopen`/`_lread`/`_lclose` palette read. | IMPLEMENTED (as the fallback) | P0 |

---

## 4. Current Free API Audit

| Area | Required by games | Current status | Missing behavior | Risk | Priority |
|---|---|---|---|---|---|
| Build system | Yes | `CMakeLists.txt:11` hardcodes an absolute local path (`/rv/data/development/.../free-eggbert/cmake/ThirdPartySDL.cmake`) to find SDL3; no system-SDL3 option; `FREE_API_BUILD_TESTS` toggle already exists and works. | Portable/relocatable SDL3 acquisition; documented build modes. | High (breaks any clone not at this exact path, breaks CI) | P0 |
| Public headers | Yes | Reasonably scoped already; a few headers (`commdlg.h`, most of `windowsx.h`, `wtypes.h`) are proven-unused-at-runtime stubs kept only for compile compatibility. | Formal classification doc (this plan partly serves that role; §5 continues it). | Low | P1 |
| WinMain bridge | Yes | `src/winmain_bridge.cpp` (509 lines) implements `FreeApiRunWinMain`, `_pgmptr` population, arg-to-`lpCmdLine` join. | Not evidenced as broken; no changes indicated by audits. | Low | P2 |
| Window class registration | Yes | `RegisterClassA` PARTIAL, functionally sufficient for both games' exact `WNDCLASSA` field set. | Nothing evidenced missing. | Low | P0 (already met) |
| Window creation | Yes | `CreateWindowExA`/`CreateWindowA` PARTIAL; `AdjustWindowRect` is **STUB** despite being a **live, non-dead call in both games' windowed-mode path**. | Real client-area-preserving rect adjustment. | **Medium-High** (windowed mode may render with wrong offset/size) | **P0/P1** |
| Message queue | Yes | `GetMessageA`/`PeekMessageA`/`PostMessageA`/`PostQuitMessage`/`DefWindowProcA` (incl. `WM_CLOSE` default) verified correct in `src/winuser_message.cpp` and `src/internal/FreeApiMessageQueue.cpp`. | Mutex-held-during-`SDL_Delay` issue flagged in `todo/free-api-performance-todo.md` (P0 there) — could delay eggbert's WinMM-timer-thread `PostMessage`. | Medium | P0 |
| SDL event translation | Yes | `PumpSdlEvents`/`FreeApiMessageQueue.cpp` translate mouse/keyboard/window events; `WM_MOUSEMOVE` lParam packing verified bit-exact (both games' demo files depend on this). | Nothing evidenced missing for the used subset. | Low | P0 (already met) |
| Keyboard input | Yes | VK_* set covers exactly what both games use; `WM_SYSKEYDOWN`+F10 quirk implemented. | Nothing evidenced missing. | Low | P0 (already met) |
| Mouse input | Yes | Buttons/move implemented and tested; `MK_*` flags implemented per README but **have no dedicated test**, and are required by planetblupi. | Regression test for `MK_*` bits. | Low-Medium | P1 |
| Cursor (`ShowCursor`/`SetCursor`) | Yes | **STUB** — both games call these expecting real OS-cursor show/hide while a software sprite cursor is drawn. | Real SDL cursor-visibility wrapper. | **Medium** (double/visible cursor bug potential) | **P0/P1** |
| Timers | Yes (both mechanisms, one per game) | `timeSetEvent`/`timeKillEvent` (eggbert) and `SetTimer`/`KillTimer`/`WM_TIMER` (planetblupi) both verified implemented and functionally load-bearing. | Dedicated regression tests for each game's exact tick pattern. | Medium (untested critical path) | P0 |
| GDI bitmap/DC subset | Yes | `CreateCompatibleDC`/`SelectObject`/`DeleteObject`/`DeleteDC`/`GetObjectA`/`StretchBlt`/`GetPixel`/`SetPixel` all PARTIAL and functionally exercised by both games' asset pipeline (via free-direct). | Apply already-identified P0/P1 perf/correctness items from `todo/free-api-performance-todo.md` (StretchBlt fast path, GetPixel/SetPixel-vs-Lock() consistency, mutex-during-sleep). | Medium | P0 |
| Image loading subset | Yes | `LoadImageA` PARTIAL, `LR_LOADFROMFILE` only, via `SDL_LoadBMP`; extension-agnostic (matches planetblupi's `.blp` requirement). | Confirm resource-loading branch (`LR_LOADFROMFILE` unset) is genuinely unreachable/unneeded (it is — both games' resource lookups always miss and fall through). | Low | P0 (already met) |
| Resource subset | Yes | `FindResourceA`/`LoadResource`/`LockResource`/`SizeofResource`/`FreeResource`/`UnlockResource` all STUB; **this is an acceptable safe stub** since both games' `.rc` files aren't compiled and the games already correctly fall through to file-based loading. **Exception: `LoadStringA` is also STUB but is NOT safe** — both games source ALL on-screen UI text through it. | Real minimal string-table backing for `LoadStringA`. | **High** (broken/placeholder UI text in both games) | **P0/P1** |
| File/path subset | Yes | `fopen` backslash-normalizing wrapper, `_lopen`/`_lread`/`_lclose`, `CreateDirectoryA` all PARTIAL/IMPLEMENTED and load-bearing (planetblupi's save system). `_findfirst`/`_findnext`/`_findclose` STUB (always fails) — eggbert-only, non-blocking feature gap. | Real directory-listing implementation (P2, eggbert design-file picker only). | Low-Medium | P1/P2 |
| MCI/MIDI subset | Yes | `"sequencer"` MIDI path IMPLEMENTED/PARTIAL and well-documented/tested (TinySoundFont+TinyMidiLoader). **Digital-video (`"avivideo"`) path is STUB** despite both games' `movie.cpp` actively driving the full `MCI_DGV_*` sequence for movie playback. | Decide and implement (or explicitly document as unsupported) minimal AVI/MCI-digital-video behavior. | **High** — see §10 | **Highest-risk item** |
| Joystick subset | Only free-eggbert (optional control scheme) | STUB, always reports 0 devices — a safe, documented fallback since the game degrades to keyboard/mouse when no joystick is found. | Real `SDL_Joystick`/`SDL_Gamepad`-backed implementation if desired. | Low (optional feature, graceful degradation already correct) | P2 |
| Tests | Yes | 3 tests exist: `basic_test` (Sleep/GetTickCount smoke + MCI case-fallback regression), `test_input_pipeline` (full SDL→WndProc pipeline), `test_timeb` (non-Windows `ftime`). Good foundation but doesn't cover GDI blit correctness, `MK_*` flags, `WM_TIMER`/`timeSetEvent` tick patterns, digital-video MCI, or file/path edge cases from real game paths. | Tests for: `AdjustWindowRect`, `ShowCursor`/`SetCursor`, `LoadStringA`, `MK_*` flags, both timer mechanisms, GDI hot-path correctness, backslash/forward-slash mixed paths. | Medium | P0/P1 |
| Documentation | Yes | `README.md`/`Documentation.md` are good and honest about status (`STUB`/`PARTIAL`/`IMPLEMENTED` tags used consistently in headers). `todo/` already contains two concrete, well-scoped backlogs (embedded resources, performance). | Scope-control docs (`docs/scope.md`, `docs/target-games.md`, `docs/out-of-scope.md`) don't exist yet. | Low | P1 |

---

## 5. Out-of-Scope API List

APIs/areas currently present in `free-api` but not proven required by real target-game behavior (only proven required *to compile*, or not required at all):

| Symbol / area | Why it exists now | Used by target games? | Decision | Notes |
|---|---|---|---|---|
| `commdlg.h` (whole file, empty) | Both games `#include` it (vestigial, from adapted DirectX-SDK sample code) | Included, zero API calls in either game | **Keep: harmless compile-only stub** | Do not add any Common Dialog implementation. |
| `windowsx.h` beyond `GetStockBrush` | Placeholder for message-cracking macros | Only `GetStockBrush` is used, by both games | **Keep: required (GetStockBrush) + harmless stub (rest)** | Do not add `GET_X_LPARAM`/`HANDLE_WM_*` macros speculatively. |
| `wtypes.h` (`VARTYPE`/`SCODE`/`DATE`/`CLIPFORMAT`) | free-eggbert includes it; adapted-code vestige | Included (eggbert only), zero runtime use | **Keep: harmless compile-only stub** | Do not implement OLE/Automation behavior. |
| `WM_CHAR` translation (`SDL_EVENT_TEXT_INPUT`) | Implemented per README "for name entry screens" | **Not found used by either game's core source** | **Keep, but flag as unproven** | Do not expand text-input handling further without a concrete usage site. |
| `WM_MBUTTONDOWN`/`WM_MBUTTONUP` translation, `MK_MBUTTON` | Implemented per README | **Not found used by either game** | **Keep, but flag as unproven** | Do not add further mouse-button behavior speculatively. |
| `GlobalMemoryStatus`/`MEMORYSTATUS` | free-eggbert calls it twice (startup TrueColor gate) | free-eggbert only; not found in planetblupi | **Keep: required (eggbert)** | Do not expand fields beyond `dwTotalPhys`. |
| `GetTickCount`, `Sleep` | Implemented, documented `IMPLEMENTED` | **Not proven called by either game's core source directly** — only by free-api's own `tests/basic_test.cpp` | **Keep: harmless, used by free-api's own test infrastructure** | Trivial, already tested; no expansion needed. |
| `FindResourceA`/`LoadResource`/`LockResource`/`SizeofResource`/`FreeResource`/`UnlockResource` (non-bitmap, non-string paths — e.g. embedded `"WAVE"` resources in `wave.cpp`) | Present so `wave.cpp`-style code compiles/links | Called, but proven **dead/unreachable at runtime** in both games (`.rc` not compiled → always misses, no fallback in `wave.cpp`) | **Keep: harmless compile-only stub** | Do not build the full embedded-resource-registry + CMake generator from `todo/Embedded_Resources_FreeAPI.md` unless a concrete game-blocking need is proven. |
| `LoadIconA`/`LoadCursorA` real resource-backed behavior | STUB today | Called by both games, but only a non-null handle is required (shape not gameplay-relevant) | **Keep: harmless compile-only stub** | Do not implement real icon/cursor decoding. |
| `joyGetDevCapsA`/`JOYCAPS` | Not implemented | Confirmed dead/commented-out in eggbert, zero use in planetblupi | **Do not implement** | No live call site exists in either game. |
| `mciGetDeviceIDA` (digitalv.h variant, always returns 0) | Present for movie-teardown lookup | Called once per game at teardown, low-impact | **Keep: harmless compile-only stub** | Do not expand beyond the current trivial behavior. |
| DirectDraw/DirectSound/DirectPlay surface (`ddraw.h`,`dsound.h`,`dplay.h`, all `DD*`/`DS*`/`DP*`) | Used extensively by both games | Yes, but **belongs to sibling project free-direct** | **Out of scope for free-api entirely** | Free API must not grow a DirectX compatibility surface of its own. |
| Unicode (`W`-suffixed) API variants | `UNICODE`-gated macros exist in `winuser.h` | **Neither game builds with `UNICODE` defined**; only `A`-suffixed functions are exercised | **Do not implement** real `W` behavior | Keep the macro aliasing for source compatibility only; do not add wide-string runtime logic. |

**No implementation tasks are created for any row in this table** — only the cleanup/documentation tasks in §7/§8 reference them.

---

## 6. Required Minimal Subset

**Minimal Required Free API Subset** — every row below has direct evidence from §3. `Priority` follows the rules in this document's introduction (P0 = compile/start/render/input/audio-critical; P1 = correctness/stability; P2 = reachable edge cases/diagnostics; P3 = docs/cleanup).

| Symbol / behavior | Category | Required by free-eggbert | Required by planetblupi | Evidence | Required behavior | Current status | Required tests | Priority |
|---|---|---|---|---|---|---|---|---|
| `RegisterClassA` + `WNDCLASSA` | WinUser | Yes | Yes | §3.2/§3.3 | Register class usable by `CreateWindow(Ex)A` | IMPLEMENTED | message-loop test (exists) | P0 |
| `CreateWindowExA` (fullscreen) | WinUser | Yes | Yes | §3.2 | Topmost fullscreen popup | PARTIAL | window-creation test | P0 |
| `CreateWindowA` (windowed) | WinUser | Yes | Yes | §3.2 | Windowed, `HWND_DESKTOP` parent | PARTIAL | window-creation test | P0 |
| `AdjustWindowRect` | WinUser | Yes (live) | Yes (live) | §3.2 | Real client-area-preserving rect math | **STUB** | new regression test | **P0/P1** |
| `ShowWindow`/`UpdateWindow`/`SetFocus` | WinUser | Yes | Yes | §3.2 | Startup sequence | PARTIAL | covered by manual/E2E run | P0 |
| `GetSystemMetrics` (`SM_CXSCREEN/CYSCREEN/CYCAPTION`) | WinUser | Yes | Yes | §3.2/§3.4 | Real screen dimensions | PARTIAL | new test | P0 |
| `PeekMessageA`/`GetMessageA`/`TranslateMessage`/`DispatchMessageA` | WinUser | Yes | Yes | §3.2 | Exact loop shape, `WM_QUIT` contract | IMPLEMENTED | `test_input_pipeline` (partial) + new loop test | P0 |
| `WaitMessage` | WinUser | Yes | Yes | §3.2 | Non-busy idle wait | PARTIAL | new test | P1 |
| `PostQuitMessage`/`PostMessageA` | WinUser | Yes | Yes | §3.2 | Cross-thread-safe posting | IMPLEMENTED | timer-thread stress test | P0 |
| `DefWindowProcA` (`WM_CLOSE`→destroy, `WM_DESTROY`→quit) | WinUser | Yes | Yes | §3.2/§3.5 | Real Win32 default-close behavior | IMPLEMENTED | new regression test | P0 |
| `GetClientRect` | WinUser | Yes | Yes (hot path) | §3.2 | Real current client size | PARTIAL | GDI/blit test | P0 |
| `SetTimer`/`KillTimer`/`WM_TIMER` | WinUser/Timers | dead | **Yes (live)** | §3.2 | Reliable periodic tick via message loop | IMPLEMENTED | new regression test | **P0** |
| `timeSetEvent`/`timeKillEvent` | WinMM/Timers | **Yes (live)** | dead | §3.2 | Reliable ~50ms multimedia timer, thread-safe `PostMessage` | PARTIAL | new regression test | **P0** |
| `GetCursorPos`/`ScreenToClient`/`ClientToScreen`/`SetCursorPos` | WinUser/Input | Yes | Yes (hot path) | §3.2 | Real, current coordinates every frame | PARTIAL | new hot-path test | P0 |
| `ShowCursor`/`SetCursor` | WinUser/Input | Yes | Yes | §3.2 | Real OS cursor visibility toggle | **STUB** | new regression test | **P0/P1** |
| `LoadCursorA`/`LoadIconA` | WinUser/Resources | Yes | Yes | §3.2 | Non-null handle only | STUB (acceptable) | smoke test | P2 |
| VK_* set + `WM_KEYDOWN/UP`/`WM_SYSKEYDOWN/UP`(F10) | WinUser/Input | Yes | Yes | §3.2/§3.4/§3.5 | Correct codes, F10 quirk | IMPLEMENTED | `test_input_pipeline` (partial) + F10 test | P0 |
| `MK_*` flags in `WM_MOUSEMOVE.wParam` | WinUser/Input | No | **Yes** | §3.2/§3.4 | Live bit flags | IMPLEMENTED — untested | new regression test | P1 |
| `WM_MOUSEMOVE`/`WM_LBUTTONDOWN/UP`/`WM_RBUTTONDOWN/UP` | WinUser/Input | Yes | Yes | §3.5 | Bit-exact `lParam` packing (demo-file compatibility) | IMPLEMENTED | `test_input_pipeline` (exists) | P0 |
| `CreateCompatibleDC`/`DeleteDC`/`SelectObject`/`DeleteObject`/`GetObjectA` | GDI | Yes | Yes | §3.2/§3.6 | Correct bitmap dims, paired lifecycle | PARTIAL | new GDI test | P0 |
| `GetDeviceCaps`(`SIZEPALETTE`)/`GetSystemPaletteEntries` | GDI | Yes | Yes | §3.2/§3.6 | Plausible palette-capability values | PARTIAL | new test | P0/P1 |
| `LoadImageA` (`LR_LOADFROMFILE`, extension-agnostic) | GDI | Yes | Yes | §3.2/§3.6 | Decode real BMP bytes from any-named file | PARTIAL | new fixture-based test | P0 |
| `StretchBlt` (`SRCCOPY`) | GDI | Yes | Yes | §3.2/§3.6 | Pixel-correct blit; 1:1 fast path perf | PARTIAL | new test (+ perf-todo items) | P0 |
| `GetPixel`/`SetPixel` | GDI | Yes | Yes | §3.2/§3.6 | Consistent with free-direct `Lock()` backing memory | PARTIAL | new test | P0 |
| `CreateBitmap` | GDI | No | Yes | §3.2/§3.6 | 1bpp/8bpp raw-buffer bitmap creation (minimap) | PARTIAL | new test | P1 |
| `_lopen`/`_lread`/`_lclose` | File | Yes | Yes | §3.2/§3.8 | Real file read (BMP palette fallback path) | PARTIAL | new test | P0 |
| `CreateDirectoryA` | File | dead | **Yes (live)** | §3.2/§3.8 | Real directory creation for save data | PARTIAL | new test | **P0** |
| `_mkdir` | File | Yes | No | §3.2/§3.8 | Real directory creation | PARTIAL | new test | P1 |
| `_findfirst`/`_findnext`/`_findclose` | File | Yes | No | §3.2/§3.8 | Real file enumeration (design-file picker) | **STUB** | new test | P2 |
| Backslash/forward-slash path normalization | File | Yes | Yes (mixed within same game) | §3.8 | Both separator styles resolve on POSIX | IMPLEMENTED | new mixed-path test | P0 |
| `FindResourceA`(`RT_BITMAP`) miss→fallback | Resources | Yes | Yes | §3.2/§3.8 | Correct miss + fallthrough | IMPLEMENTED (as fallback) | covered by GDI test | P0 |
| `LoadStringA` | Resources | **Yes (~90+ sites)** | **Yes (~50+ sites)** | §3.2/§3.8 | Real string-table text for all on-screen UI | **STUB** | new test | **P0/P1** |
| `mciSendCommandA` `"sequencer"` (MIDI music) | WinMM | Yes | Yes | §3.2/§3.7 | Open/play/close background music | IMPLEMENTED/PARTIAL | `basic_test` (partial) + new sequence test | P0 |
| `MM_MCINOTIFY`/`MCI_NOTIFY_SUCCESSFUL` | WinMM | Yes | Yes | §3.5/§3.7 | Reliable end-of-track notification for looping | PARTIAL | new test | P0 |
| `midiOutGetNumDevs/Open/SetVolume/Close` | WinMM | Yes | Yes | §3.2/§3.7 | Volume iteration on (re)start/change | IMPLEMENTED | smoke test | P1 |
| `mciSendCommandA` `"avivideo"`/MCI_DGV_* (movies) | WinMM | Yes | Yes | §3.2/§3.3/§3.7 | Full open/status/play/pause/close, real movable window handle | **STUB** | new test (after scope decision) | **Highest risk — §10** |
| `"cdaudio"` graceful decline | WinMM | Yes | Yes | §3.7 | Decline without breaking MIDI fallback | IMPLEMENTED | covered by existing MCI test | P1 |
| `joyGetPosEx`/`joyGetNumDevs` | WinMM/Joystick | Yes (optional) | No | §3.2/§3.3 | Safe stub (0 devices) OR real analog data | STUB (safe) | optional, P2 only if implemented | P2 |
| `GlobalMemoryStatus` | WinBase | Yes | No | §3.2/§3.3 | Plausible `dwTotalPhys` for TrueColor gate | PARTIAL | smoke test | P1 |
| `fopen`/`fread`/`fwrite`/`fclose` (via wrapper) | File | Yes | Yes | §3.8 | Backslash-normalizing wrapper | IMPLEMENTED | existing + new mixed-path test | P0 |

---

## 7. Task List

Numbering is sequential across all milestones (`TASK-0001`…`TASK-0124`), plus supplemental tasks `TASK-0125`/`TASK-0126` appended after the initial audit (sourced from gaps found while reconciling this list against `NEXT.md`). Every task cites real evidence from §3/§4/§5 or is explicit scope-control cleanup.

Each task carries a `Status:` line once verified against the actual repository state: `DONE` (with the file/test that proves it), `PARTIAL` (some Required work missing), `TODO` (not started), or `NOT-APPLICABLE`/`OBSOLETE` (superseded, with reason). A task with no `Status:` line has not yet been re-verified against current code since this plan was written — treat it as unknown, not as done or not done.

### Milestone A — Scope & Governance

### TASK-0001: Document the "cite a usage site" rule for all future API additions

Status: DONE — Rule stated in Documentation.md:8-9 and docs/scope.md.
Priority: P0
Area: Scope
Type: Documentation
Evidence: Free API internal cleanup
Depends on: None

Problem:
There is currently no written rule forcing future contributors (or future-AI-assisted sessions) to justify new WinAPI surface with a real usage site, which is exactly how free-api could drift into becoming a general WinAPI layer.

Required work:
* Add a short, prominent rule to `Documentation.md` (or a new `CONTRIBUTING.md`) stating that any new public symbol must cite a `file:line` usage site in `../free-eggbert` or `../planetblupi`, or be explicit scope-control cleanup/tests/docs.

Acceptance criteria:
* The rule is visible from the repo root README or a top-level `CONTRIBUTING.md`.
* Existing tests still pass.
* No unrelated API is added.

Out of scope:
* Do not build tooling/CI enforcement in this task (see TASK-0010 for the review checklist).
* Do not restate the entire plan.md content — link to it instead.

### TASK-0002: Add `docs/scope.md`

Status: DONE — docs/scope.md exists, covers purpose + hard scope rules.
Priority: P0
Area: Scope
Type: Documentation
Evidence: Free API internal cleanup
Depends on: TASK-0001

Problem:
The narrow-scope intent (§1/§2 of this plan) needs a stable, linkable home outside `plan.md` so it survives even after the task list is completed and archived.

Required work:
* Create `docs/scope.md` containing §1 "Purpose" and §2 "Hard Scope Rules" from this plan, adapted as living documentation (not a point-in-time plan).

Acceptance criteria:
* `docs/scope.md` exists and states the two target games, the "not Wine" boundaries, and the ten hard scope rules.
* No unrelated API is added.

Out of scope:
* Do not duplicate the full usage-audit tables here — link to `plan.md` or `docs/target-games.md`.

### TASK-0003: Add `docs/target-games.md`

Status: DONE — docs/target-games.md exists, matches required content.
Priority: P0
Area: Scope
Type: Documentation
Evidence: Free API internal cleanup
Depends on: TASK-0002

Problem:
There is no single doc describing what Free Eggbert and Planet Blupi are, where they live, and how they consume free-api, which makes onboarding and future audits harder than necessary.

Required work:
* Create `docs/target-games.md` describing both games (repo paths, build backend `FREEDIRECT`, how they link `free-api`+`free-direct`, and a short pointer to each game's own README/TODO).

Acceptance criteria:
* Doc exists and correctly states both games already depend on `free-api` today (not a future integration).
* No unrelated API is added.

Out of scope:
* Do not include full usage tables — those live in `plan.md` §3.

### TASK-0004: Add `docs/out-of-scope.md`

Status: DONE — docs/out-of-scope.md exists (references plan.md §5 by design).
Priority: P0
Area: Scope
Type: Documentation
Evidence: Free API internal cleanup
Depends on: TASK-0002

Problem:
§5 of this plan classifies currently-present-but-unproven APIs; that classification needs a durable home so future contributors don't accidentally "complete" a stub that was deliberately left minimal.

Required work:
* Create `docs/out-of-scope.md` containing the §5 table (symbol/area, why it exists, used-by-games?, decision, notes).

Acceptance criteria:
* Doc exists and matches §5 of this plan at time of writing.
* No unrelated API is added.

Out of scope:
* Do not add new out-of-scope classifications beyond what's in §5 without new evidence.

### TASK-0005: Add a supported-APIs table doc generated manually from the audit

Status: DONE — `docs/supported-apis.md` created (current status per symbol, updated to reflect this session's fixes, not a stale copy); referenced from `Documentation.md`.
Priority: P1
Area: Scope
Type: Documentation
Evidence: Free API internal cleanup
Depends on: TASK-0002

Problem:
§6 "Minimal Required Free API Subset" is the canonical list but lives inside a dated plan file; a standalone, maintainable doc is needed for ongoing reference.

Required work:
* Create `docs/supported-apis.md` with the §6 table, hand-maintained (not auto-generated) going forward.

Acceptance criteria:
* Doc exists, matches §6 at time of writing, and is referenced from `Documentation.md`.
* No unrelated API is added.

Out of scope:
* Do not build automatic doc-generation tooling in this task.

### TASK-0006: Add a table of compile-only stubs

Status: DONE — `docs/out-of-scope.md` "Compile-only stubs" section added, including the historical-note callout that AdjustWindowRect/ShowCursor/SetCursor/LoadStringA were once flagged "not actually safe" and have since been fixed, and that `_findfirst`/`_findnext`/`_findclose` remain a real (not safe-by-design) gap.
Priority: P1
Area: Scope
Type: Documentation
Evidence: §5 of this plan
Depends on: TASK-0004

Problem:
Compile-only stubs (e.g. `LoadIconA`, `commdlg.h`, `wtypes.h`) are scattered across header doc-comments with no single index, making it hard to tell "safe stub" from "accidental gap" at a glance.

Required work:
* Add a "Compile-only stubs" section to `docs/out-of-scope.md` listing every symbol tagged `STUB` in headers today, with a one-line justification for why the stub is safe (or a flag if it is not, per §4's `AdjustWindowRect`/`ShowCursor`/`LoadStringA` findings).

Acceptance criteria:
* Every header-declared `STUB` symbol appears in the table.
* Symbols identified in §4 as "not actually safe" (`AdjustWindowRect`, `ShowCursor`, `SetCursor`, `LoadStringA`) are explicitly called out as needing real implementation, not left implied "safe."
* No unrelated API is added.

Out of scope:
* Do not fix any of the flagged gaps in this task — that's tracked by TASK-0031/0056/0057/0075.

### TASK-0007: Add a table of unsupported APIs

Status: DONE — "Unsupported APIs" table added to `docs/out-of-scope.md`.
Priority: P2
Area: Scope
Type: Documentation
Evidence: Free API internal cleanup
Depends on: TASK-0004

Problem:
There is no explicit list of commonly-expected WinAPI functions that free-api deliberately does not implement (e.g. `GetAsyncKeyState`, `waveOut*`, `PlaySound`, full resource compiler), which invites future contributors to "helpfully" add them.

Required work:
* Add an "Unsupported APIs" section to `docs/out-of-scope.md` listing confirmed-absent, confirmed-unused-by-both-games symbols from §3 (e.g. `GetAsyncKeyState`, `GetKeyState`, `waveOut*`, `PlaySound`, `QueryPerformanceCounter`, full `.rc`/`.res` compilation, DirectX symbols owned by free-direct).

Acceptance criteria:
* List matches the "confirmed not used" findings cited throughout §3.
* No unrelated API is added.

Out of scope:
* Do not implement any listed symbol as part of this task.

### TASK-0008: Add a policy statement that generic WinAPI expansion is forbidden

Status: DONE — docs/scope.md "The rule" section states this policy.
Priority: P0
Area: Scope
Type: Documentation
Evidence: Free API internal cleanup
Depends on: TASK-0002

Problem:
Without an explicit, quotable policy line, it is easy for future work (including AI-assisted sessions) to rationalize "just add this one more common Win32 function."

Required work:
* Add a one-paragraph policy statement to `docs/scope.md`: no WinAPI surface may be added without a §3-style usage citation from `../free-eggbert` or `../planetblupi`.

Acceptance criteria:
* Statement is present verbatim in `docs/scope.md`.
* No unrelated API is added.

Out of scope:
* Do not add enforcement tooling (linting, CI checks) in this task.

### TASK-0009: Add a policy that APIs may be removed or hidden if unused by target games

Status: DONE — Removal/hiding policy paragraph added to `docs/out-of-scope.md`.
Priority: P1
Area: Scope
Type: Documentation
Evidence: §5 of this plan
Depends on: TASK-0004

Problem:
The reverse policy (removal/hiding is allowed and encouraged, not just additions being restricted) isn't written down, which biases the project toward "keep everything just in case."

Required work:
* Add a policy paragraph to `docs/out-of-scope.md` stating that any symbol proven unused by both target games (per §5's methodology) is a legitimate candidate for removal, hiding behind an opt-in macro, or leaving as a documented stub — contributor's choice, justified case by case.

Acceptance criteria:
* Statement is present in `docs/out-of-scope.md`.
* No unrelated API is added.

Out of scope:
* Do not actually remove any symbol as part of this task.

### TASK-0010: Add a review checklist for future pull requests

Status: DONE — `docs/pr-checklist.md` created, covers all four required questions plus scope-boundary reminders.
Priority: P1
Area: Scope
Type: Documentation
Evidence: Free API internal cleanup
Depends on: TASK-0001, TASK-0008

Problem:
Reviewers (human or AI) need a quick checklist to apply consistently instead of re-deriving the scope philosophy from scratch on every PR.

Required work:
* Add a `docs/pr-checklist.md` (or a section in `CONTRIBUTING.md`) with concrete review questions: "Does this cite a `file:line` in one of the two target games?", "Does this add a new public header/symbol not already justified in `docs/supported-apis.md`?", "Does this add a new third-party dependency?", "Are new/changed behaviors covered by a test derived from actual game usage?".

Acceptance criteria:
* Checklist exists and covers at minimum the four questions above.
* No unrelated API is added.

Out of scope:
* Do not wire this into any automated CI bot in this task.

### Milestone B — Build System

### TASK-0011: Remove hardcoded absolute local path from CMakeLists.txt

Status: DONE — No hardcoded absolute path remains in CMakeLists.txt.
Priority: P0
Area: Build
Type: Bugfix
Evidence: CMakeLists.txt:11
Depends on: None

Problem:
`CMakeLists.txt:11` does `include("/rv/data/development/github.com/openeggbert/free-eggbert/cmake/ThirdPartySDL.cmake")` — an absolute path tied to one specific developer machine, which breaks any other clone location, CI runner, or contributor's machine.

Required work:
* Replace the hardcoded absolute path with a relative path resolution (e.g. `${CMAKE_CURRENT_SOURCE_DIR}/../free-eggbert/cmake/ThirdPartySDL.cmake` guarded by `EXISTS`, or better, a `FREE_API_THIRDPARTY_SDL_CMAKE` cache variable with a sane relative default) so the project configures correctly from any clone location where the sibling layout is preserved.
* Ensure the fallback still produces a `FATAL_ERROR` with an actionable message if SDL3 truly cannot be found, matching current behavior for the "nothing found" case.

Acceptance criteria:
* `grep -r "/rv/data" CMakeLists.txt` (or any other absolute developer-machine path) returns nothing.
* A fresh clone of `free-api` + `free-eggbert` (or `planetblupi`) at any parent directory path configures successfully.
* Existing tests still pass.
* No unrelated API is added.

Out of scope:
* Do not remove the sibling-relative SDL3 vendoring pattern itself — only its hardcoded absolute form.
* Do not add a full package-manager integration (vcpkg/Conan) in this task.

### TASK-0012: Add a `FREE_API_USE_SYSTEM_SDL3` CMake option

Status: DONE — FREE_API_USE_SYSTEM_SDL3 option in CMakeLists.txt:23-31.
Priority: P1
Area: Build
Type: Implementation
Evidence: CMakeLists.txt:10-25
Depends on: TASK-0011

Problem:
The build currently always attempts to vendor SDL3 via the sibling project's `ThirdPartySDL.cmake` if `SDL3::SDL3` isn't already a target; there is no explicit, documented way to opt into a system-installed SDL3/SDL3_image/SDL3_mixer instead.

Required work:
* Add a `FREE_API_USE_SYSTEM_SDL3` CMake option (default `OFF`, preserving current behavior) that, when `ON`, calls `find_package(SDL3 REQUIRED)`/`find_package(SDL3_image REQUIRED)`/`find_package(SDL3_mixer REQUIRED)` instead of vendoring.

Acceptance criteria:
* With the option `OFF` (default), build behavior is unchanged.
* With the option `ON` on a system with SDL3 dev packages installed, the build configures and links against system SDL3.
* Existing tests still pass.
* No unrelated API is added.

Out of scope:
* Do not remove the vendored/sibling-provided SDL3 path — it remains the default.

### TASK-0013: Support parent-project-provided SDL3 targets explicitly

Status: DONE — Parent-provided SDL3 target guards at CMakeLists.txt:27,33; documented.
Priority: P1
Area: Build
Type: Implementation
Evidence: CMakeLists.txt:10,15,19,23 (`if(NOT TARGET SDL3::SDL3)` guards)
Depends on: TASK-0011

Problem:
The existing `if(NOT TARGET SDL3::SDL3)` guards already tolerate a parent project (like free-eggbert/planetblupi's top-level build) providing SDL3 first, but this behavior is undocumented and easy to break accidentally.

Required work:
* Add a code comment plus a `docs/build-modes.md` entry documenting that free-api will use pre-existing `SDL3::SDL3`/`SDL3_image::SDL3_image`/`SDL3_mixer::SDL3_mixer` targets if a parent `add_subdirectory()` already created them, and only vendors/finds them itself otherwise.

Acceptance criteria:
* Documentation accurately describes the existing guard behavior.
* A test build where a parent `CMakeLists.txt` defines `SDL3::SDL3` before `add_subdirectory(free-api)` does not re-vendor SDL3.
* No unrelated API is added.

Out of scope:
* Do not change the guard logic itself unless a real bug is found — this task is about documenting and locking in existing correct behavior.

### TASK-0014: Add and document clear CMake build-mode options

Status: DONE — docs/cmake-options.md covers build modes (different filename than spec).
Priority: P1
Area: Build
Type: Documentation
Evidence: CMakeLists.txt (FREE_API_BUILD_TESTS, EMSCRIPTEN branch)
Depends on: TASK-0012, TASK-0013

Problem:
Build modes (standalone vendored-SDL3, system-SDL3, parent-provided-SDL3, Emscripten) exist or are being added but are not documented in one place.

Required work:
* Create `docs/build-modes.md` enumerating each supported mode, the CMake options/cache variables involved, and one example command line per mode.

Acceptance criteria:
* Doc covers at least: default vendored mode, `FREE_API_USE_SYSTEM_SDL3=ON`, parent-provided mode, Emscripten mode (tests auto-disabled).
* No unrelated API is added.

Out of scope:
* Do not invent new build modes not already implied by existing CMake logic.

### TASK-0015: Document the existing `FREE_API_BUILD_TESTS` toggle

Status: DONE — FREE_API_BUILD_TESTS documented in cmake-options.md "Tests" section.
Priority: P2
Area: Build
Type: Documentation
Evidence: CMakeLists.txt:106-143
Depends on: TASK-0014

Problem:
`FREE_API_BUILD_TESTS` already exists and correctly defaults `OFF` under Emscripten, `ON` otherwise, but this isn't called out anywhere in `README.md`/`Documentation.md`.

Required work:
* Add a short "Testing" section to `Documentation.md` documenting `-DFREE_API_BUILD_TESTS=ON/OFF` and `ctest --test-dir build`.

Acceptance criteria:
* Doc accurately reflects current CMake defaults.
* No unrelated API is added.

Out of scope:
* Do not change the default value or add new test-selection granularity in this task.

### TASK-0016: Add a "fresh clone" smoke check to prevent regressions like TASK-0011

Status: DONE — `cmake/CheckNoHardcodedPaths.cmake` (new), registered as the `check_no_hardcoded_paths` CTest test. Verified it fails on a deliberately reintroduced `/home/...` path and passes on the current, fixed `CMakeLists.txt`.
Priority: P1
Area: Build
Type: Test
Evidence: CMakeLists.txt:11 (the bug fixed by TASK-0011)
Depends on: TASK-0011

Problem:
Nothing currently prevents a future contributor from reintroducing a hardcoded absolute path (or another machine-specific assumption) into `CMakeLists.txt`.

Required work:
* Add a lightweight CI or pre-commit check (a shell script invoked by CTest, or documented manual step) that greps `CMakeLists.txt`/`cmake/*.cmake` for suspicious absolute paths (e.g. `/home/`, `/rv/`, `C:\\`) and fails if found.

Acceptance criteria:
* The check fails on a deliberately reintroduced absolute path.
* The check passes on the current, fixed `CMakeLists.txt`.
* No unrelated API is added.

Out of scope:
* Do not build a full CI pipeline in this task if none exists — a documented/scripted local check is sufficient.

### TASK-0017: Audit and document why each vendored `external/` library is needed

Status: DONE — README "Vendored libraries" section covers this.
Priority: P2
Area: Build
Type: Documentation
Evidence: external/tsf.h, external/tml.h; README.md MIDI section
Depends on: None

Problem:
`external/` vendors TinySoundFont and TinyMidiLoader; their necessity is documented in `README.md` but not cross-linked from a scope-control perspective (i.e. "why is this the only third-party dependency beyond SDL3, and why is it justified").

Required work:
* Add a short note to `docs/scope.md` or `docs/build-modes.md` stating that `tsf.h`/`tml.h` are required for real MIDI playback needed by both games' `"sequencer"` MCI path (§3.7), and that no other third-party dependency should be added without the same level of justification.

Acceptance criteria:
* Note exists and correctly cites both games' MIDI requirement.
* No unrelated API is added.

Out of scope:
* Do not add or evaluate alternative MIDI libraries in this task.

### TASK-0018: Document all supported build modes in README.md

Status: DONE — `README.md`'s Build Instructions section now links to `docs/cmake-options.md`.
Priority: P2
Area: Build
Type: Documentation
Evidence: README.md "Build Instructions" section
Depends on: TASK-0014

Problem:
`README.md`'s current "Build Instructions" section only shows the single default vendored-SDL3 path.

Required work:
* Update `README.md`'s "Build Instructions" section to link to the new `docs/build-modes.md` (TASK-0014) instead of (or in addition to) the single example.

Acceptance criteria:
* `README.md` links to `docs/build-modes.md`.
* No unrelated API is added.

Out of scope:
* Do not restructure the rest of `README.md` in this task.

### Milestone C — Header Hygiene

### TASK-0019: Formalize the header-vs-usage audit as a living document

Status: DONE — `docs/headers.md` created, copies plan.md §3.1's header table with a re-verify note.
Priority: P1
Area: Headers
Type: Audit
Evidence: plan.md §3.1
Depends on: TASK-0005

Problem:
This plan's §3.1 header-usage table is a point-in-time snapshot; it should be captured as a maintainable doc so it doesn't go stale silently.

Required work:
* Copy §3.1's header table into `docs/supported-apis.md` (TASK-0005) or a dedicated `docs/headers.md`, with a note to re-verify against the two games on any future header change.

Acceptance criteria:
* Doc exists and matches §3.1 at time of writing.
* No unrelated API is added.

Out of scope:
* Do not add automated re-verification tooling in this task.

### TASK-0020: Mark unused public declarations out-of-scope in header comments

Status: DONE — All three header comments updated (`include/commdlg.h`, `include/windowsx.h`, `include/wtypes.h`) with explicit "proven unused by both target games" language, citing exact file:line evidence.
Priority: P1
Area: Headers
Type: Documentation
Evidence: include/commdlg.h, include/windowsx.h, include/wtypes.h; §5
Depends on: TASK-0004

Problem:
`commdlg.h`, most of `windowsx.h`, and `wtypes.h` are already marked `STUB` in their doc-comments but don't explicitly say "proven unused by both target games, kept only for compile compatibility," which would help future readers make the same judgment call this audit did.

Required work:
* Update the file-level Doxygen comment in `include/commdlg.h`, `include/windowsx.h`, and `include/wtypes.h` to state explicitly that neither `../free-eggbert` nor `../planetblupi` call any real API from these files at runtime (per this plan's §3.1/§5), and that they exist purely so the `#include` lines in game source compile.

Acceptance criteria:
* All three files' header comments explicitly state the "included but unused" finding.
* No behavior change; existing tests still pass.
* No unrelated API is added.

Out of scope:
* Do not delete these headers — both games still `#include` them.

### TASK-0021: Hide unused `JOYINFOEX` fields' significance behind documentation, not new macros

Status: DONE — Doc-comment added to `JOYINFOEX` in `include/mmsystem.h` listing the exact 5 fields free-eggbert reads.
Priority: P2
Area: Headers
Type: Documentation
Evidence: include/mmsystem.h:43-57; free-eggbert event.cpp:2069-2125
Depends on: None

Problem:
`JOYINFOEX` declares `dwZpos`/`dwRpos`/`dwUpos`/`dwVpos`/`dwButtonNumber`/`dwPOV`/`dwReserved1`/`dwReserved2` for Win32 struct-layout compatibility, but only `dwSize`/`dwFlags`/`dwXpos`/`dwYpos`/`dwButtons` are ever read by free-eggbert (planetblupi doesn't use joysticks at all).

Required work:
* Add a doc-comment clarifying which `JOYINFOEX` fields are actually read by the one consumer game, so future implementers of real joystick support (TASK-0103) know the minimum bar.

Acceptance criteria:
* Doc-comment lists the exact fields free-eggbert reads.
* No unrelated API is added.

Out of scope:
* Do not remove any struct field (real Win32 layout compatibility must be preserved even for unused fields).

### TASK-0022: Clarify the A/W alias policy in winuser.h

Status: DONE — Doc-comment added above the `#ifdef UNICODE` block in `include/winuser.h`.
Priority: P1
Area: Headers
Type: Documentation
Evidence: include/winuser.h:6-44 (`#ifdef UNICODE` block); §5
Depends on: None

Problem:
`winuser.h` defines both `UNICODE` and non-`UNICODE` macro branches, but neither target game ever defines `UNICODE`, so the `W`-suffixed branch is untested and unused dead weight from a behavioral standpoint (though harmless as macros).

Required work:
* Add a doc-comment above the `#ifdef UNICODE` block stating that neither target game builds with `UNICODE` defined, that only the `A`-suffixed functions have real implementations, and that the `W` macro branch exists purely for source-compatibility symmetry, not functional Unicode support.

Acceptance criteria:
* Doc-comment added; no behavior change.
* No unrelated API is added.

Out of scope:
* Do not remove the `UNICODE` macro branch — it's cheap, harmless, and may aid future compile-compatibility.

### TASK-0023: Add an explicit policy against implementing real Unicode/W-variant behavior

Status: DONE — Unicode/`W`-suffixed policy section added to `docs/out-of-scope.md`.
Priority: P1
Area: Headers
Type: Documentation
Evidence: §5 (Unicode row)
Depends on: TASK-0004, TASK-0022

Problem:
Without an explicit rule, a future contributor might see the `W`-suffixed macro aliases and assume real wide-character implementations are expected.

Required work:
* Add the Unicode policy row from §5 to `docs/out-of-scope.md`: do not implement real `W`-suffixed runtime behavior unless a target game is proven to build with `UNICODE` defined.

Acceptance criteria:
* Policy present in `docs/out-of-scope.md`.
* No unrelated API is added.

Out of scope:
* Do not remove existing `W` macro aliases.

### TASK-0024: Add compile smoke tests for the exact headers both games include

Status: DONE — tests/test_header_compile.cpp exists, registered in CTest.
Priority: P0
Area: Tests
Type: Test
Evidence: §3.1 (header table)
Depends on: None

Problem:
There is no test verifying that a single translation unit including the full header set actually used by the two games (`windows.h`, `windowsx.h`, `wtypes.h`, `mmsystem.h`, `digitalv.h`, `commdlg.h`, `io.h`, `direct.h`, `sys/timeb.h`) compiles cleanly together — regressions here would break both games' builds silently.

Required work:
* Add a new test target (e.g. `tests/test_header_compile.cpp`) that `#include`s exactly this header set (in an order matching at least one real game file) and compiles to a trivial `main()` that returns 0.
* Register it in `CMakeLists.txt`/CTest.

Acceptance criteria:
* New test target builds and passes via `ctest --test-dir build`.
* Existing tests still pass.
* No unrelated API is added.

Out of scope:
* Do not include headers neither game uses (e.g. no speculative `winreg.h`, `shellapi.h`).

### TASK-0025: Audit and document intentional macro definitions to avoid pollution

Status: DONE — `include/windows.h`'s `fopen` wrapper and `include/rpcndr.h`'s `byte` alias both now explicitly framed as deliberate, narrowly-scoped exceptions.
Priority: P2
Area: Headers
Type: Documentation
Evidence: include/windows.h:129-170 (`fopen` wrapper), include/rpcndr.h (`byte`)
Depends on: None

Problem:
`windows.h` `#define`s `fopen` globally to a path-normalizing wrapper, and `byte` is aliased in a couple of headers — both are intentional, narrowly-scoped exceptions to "don't pollute global macros," but this intent isn't documented as a deliberate, audited decision.

Required work:
* Add a doc-comment (already partially present for `fopen`) explicitly framing both the `fopen` redefinition and the `byte` alias as deliberate, narrowly-justified exceptions tied to a specific game requirement (planetblupi's backslash paths for `fopen`; legacy CRT compatibility for `byte`), not a general policy of macro-heavy compatibility.

Acceptance criteria:
* Doc-comments updated; no behavior change.
* No unrelated API is added.

Out of scope:
* Do not remove the `fopen` wrapper — it is load-bearing for planetblupi (§3.8).

### TASK-0026: Add a static/compile-time check that public headers never leak SDL types

Status: DONE — `tests/test_header_compile.cpp` already links only `freeapi_compat_headers` (no SDL3/`external/` include path); doc-comment added making explicit that its successful compile IS the proof of no SDL leak, checked every `ctest` run.
Priority: P2
Area: Headers
Type: Test
Evidence: Documentation.md ("must not expose SDL types")
Depends on: TASK-0024

Problem:
The project's own documentation states public headers must not expose SDL types, but nothing actively verifies this beyond code review discipline.

Required work:
* Extend `tests/test_header_compile.cpp` (TASK-0024) or add a small standalone test that includes only `include/` public headers (no `-I` path to SDL3 or `external/`) and confirms it still compiles, proving no public header transitively requires SDL3/tsf.h/tml.h include paths.

Acceptance criteria:
* Test compiles public headers without SDL3/`external/` on the include path.
* No unrelated API is added.

Out of scope:
* Do not restructure `freeapi_compat_headers` target scoping in this task unless the test reveals an actual leak.

### Milestone D — WinUser / Windowing / Message Loop

### TASK-0027: Verify `RegisterClassA` against both games' exact `WNDCLASSA` field usage

Status: DONE — `tests/test_winuser_regressions.cpp` (`TestRegisterClassAWithFullFieldSet`) registers all 10 fields and creates a window against the class; `ctest` passes.
Priority: P0
Area: WinUser
Type: Test
Evidence: free-eggbert blupi.cpp:707-728; planetblupi blupi.cpp:593,609-619
Depends on: None

Problem:
Both games populate the full `WNDCLASSA` field set (`style, lpfnWndProc, cbClsExtra, cbWndExtra, hInstance, hIcon, hCursor, hbrBackground, lpszMenuName, lpszClassName`) but there's no test proving `RegisterClassA` handles every one of these fields without crashing or silently dropping behavior.

Required work:
* Add a test registering a `WNDCLASSA` with every field populated (matching both games' shape) and verify subsequent `CreateWindowA`/`CreateWindowExA` against that class name succeeds.

Acceptance criteria:
* New test passes.
* Existing tests still pass.
* No unrelated API is added.

Out of scope:
* Do not add `WNDCLASSEXA`/`RegisterClassExA` support — neither game uses it.

### TASK-0028: Verify `CreateWindowExA` fullscreen path matches both games' exact call shape

Status: DONE — `tests/test_winuser_regressions.cpp` (`TestCreateWindowExAFullscreenPath`); `ctest` passes.
Priority: P0
Area: WinUser
Type: Test
Evidence: free-eggbert blupi.cpp:733-746; planetblupi blupi.cpp:625-638
Depends on: TASK-0027

Problem:
Both games call `CreateWindowExA` with `WS_EX_TOPMOST`/`WS_POPUP` and sizes derived from `GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN)` for fullscreen mode; this exact shape isn't covered by an existing test.

Required work:
* Add a test creating a window with this exact style/size combination and verify a non-null `HWND` is returned and its reported client size is sane.

Acceptance criteria:
* New test passes.
* No unrelated API is added.

Out of scope:
* Do not test arbitrary/unused style combinations.

### TASK-0029: Verify `CreateWindowA` windowed path matches both games' exact call shape

Status: DONE — test_winuser_regressions.cpp:80-93 covers the windowed path.
Priority: P0
Area: WinUser
Type: Test
Evidence: free-eggbert blupi.cpp:768-780; planetblupi blupi.cpp:653-665
Depends on: TASK-0027

Problem:
Both games' windowed-mode path (`WS_POPUPWINDOW|WS_CAPTION|WS_VISIBLE`, parent `HWND_DESKTOP`) isn't covered by an existing test, and both games check the return value for `NULL` and abort startup if it fails.

Required work:
* Add a test creating a window with this exact style combination and parent `HWND_DESKTOP`, verifying a non-null `HWND`.

Acceptance criteria:
* New test passes.
* No unrelated API is added.

Out of scope:
* Do not implement real parent/child window hierarchy semantics beyond returning a valid handle.

### TASK-0030: Investigate current `AdjustWindowRect` STUB against both games' live windowed-mode requirement

Status: DONE — src/winuser_misc.cpp:74 comment + NEXT.md document the investigation.
Priority: P0
Area: WinUser
Type: Audit
Evidence: include/winuser.h:373 (`@note Status: STUB`); free-eggbert blupi.cpp:765; planetblupi blupi.cpp:650
Depends on: TASK-0029

Problem:
`AdjustWindowRect` is currently a no-op `STUB`, but both games call it live (not dead code) in windowed mode to size the window so its **client area** matches the game's fixed internal resolution — a no-op here likely produces a window whose client area is off by the caption-bar/border size, visibly clipping or offsetting the rendered game.

Required work:
* Reproduce the effect in a minimal test: create a windowed-mode window using each game's exact style flags and current `AdjustWindowRect` STUB, and measure whether the resulting client rect matches the requested size.
* Confirm/deny the visible-bug hypothesis with evidence (does SDL's window-creation-by-total-size already happen to work around this, or not).

Acceptance criteria:
* A written finding (test output + short note) confirms whether real users are currently seeing a clipped/offset windowed-mode view in either game.
* No unrelated API is added.

Out of scope:
* Do not implement the fix in this task — implementation is TASK-0031.

### TASK-0031: Implement real `AdjustWindowRect` behavior for the windowed-mode case

Status: OBSOLETE — Contradicted by TASK-0030's finding; NEXT.md explicitly forbids "fixing" this.
Priority: P1
Area: WinUser
Type: Implementation
Evidence: free-eggbert blupi.cpp:765; planetblupi blupi.cpp:650; TASK-0030 finding
Depends on: TASK-0030

Problem:
Per TASK-0030, both games depend on `AdjustWindowRect` producing a correctly-sized outer window rect so the client area matches the game's fixed resolution; a permanent no-op risks a visibly wrong windowed-mode presentation.

Required work:
* Implement `AdjustWindowRect` to grow/shrink the input `RECT` by a caption-height/border-width delta appropriate for the current SDL-backed window decoration model (can be a fixed, documented approximation — real Win32 pixel-perfect chrome dimensions are not required, only "client area ends up correct within SDL's actual window content area").
* Only handle the style flags both games actually pass (`WS_POPUPWINDOW|WS_CAPTION`, `bMenu=FALSE`) — do not generalize to arbitrary style/menu combinations.

Acceptance criteria:
* New test (extending TASK-0030's) shows the resulting client rect matches the requested size for both games' exact style flags.
* Existing tests still pass.
* No unrelated API is added.

Out of scope:
* Do not implement general-purpose window-chrome-size calculation for styles/menu combinations neither game uses.

### TASK-0032: Verify `ShowWindow`/`UpdateWindow`/`SetFocus` startup sequence

Status: DONE — `tests/test_winuser_regressions.cpp` (`TestShowWindowUpdateWindowSetFocusSequence`); `ctest` passes.
Priority: P1
Area: WinUser
Type: Test
Evidence: free-eggbert blupi.cpp:784-786; planetblupi blupi.cpp:677-679
Depends on: TASK-0028, TASK-0029

Problem:
The exact three-call startup sequence both games use isn't covered by an isolated test (only indirectly via manual end-to-end runs).

Required work:
* Add a test exercising `ShowWindow(hwnd, SW_SHOW)` → `UpdateWindow(hwnd)` → `SetFocus(hwnd)` in sequence and verifying no crash/error return.

Acceptance criteria:
* New test passes.
* No unrelated API is added.

Out of scope:
* Do not test other `nCmdShow` values neither game passes.

### TASK-0033: Verify `DestroyWindow` fatal-init-failure path doesn't crash

Status: DONE — `tests/test_winuser_regressions.cpp` (`TestDestroyWindowFatalInitFailurePathDoesNotCrash`, 500-iteration create-then-immediately-destroy cycle); `ctest` passes.
Priority: P2
Area: WinUser
Type: Test
Evidence: free-eggbert blupi.cpp:664; planetblupi blupi.cpp:585
Depends on: None

Problem:
Both games call `DestroyWindow` only on an unrecoverable init failure; this rare path has no test coverage and a crash here would turn a soft failure into a hard crash.

Required work:
* Add a test creating then immediately `DestroyWindow`-ing a window, verifying clean return and that internal window-registry state is properly cleared (per `todo/free-api-performance-todo.md`'s "Verify Window Lookup Map Maintenance" item).

Acceptance criteria:
* New test passes.
* No unrelated API is added.

Out of scope:
* Do not implement child-window destruction cascades — neither game has child windows.

### TASK-0034: Verify `WM_CREATE` delivers `CREATESTRUCT.hInstance` correctly

Status: DONE — `tests/test_winuser_regressions.cpp` (`TestWmCreateDeliversCorrectHInstance`); `ctest` passes.
Priority: P1
Area: WinUser
Type: Test
Evidence: free-eggbert blupi.cpp:508; planetblupi blupi.cpp:428-430
Depends on: TASK-0028, TASK-0029

Problem:
Both games read exactly one field (`hInstance`) out of `LPCREATESTRUCT` on `WM_CREATE`; there's no regression test proving this field survives the trip from `CreateWindow(Ex)A`'s `hInstance` argument through to the delivered message.

Required work:
* Extend `test_input_pipeline.cpp` or add a new test verifying a `WM_CREATE` message's `lParam`, cast to `LPCREATESTRUCT`, has `.hInstance` equal to the value passed to `CreateWindowExA`/`CreateWindowA`.

Acceptance criteria:
* New/extended test passes.
* No unrelated API is added.

Out of scope:
* Do not verify other `CREATESTRUCT` fields neither game reads.

### TASK-0035: Verify `WM_DESTROY` → timer-kill → teardown → `PostQuitMessage` sequence

Status: DONE — `tests/test_timer_regressions.cpp` (`TestWmDestroyKillsSetTimerThenPostsQuit`, `TestWmDestroyKillsTimeSetEventThenPostsQuit`), both timer mechanisms; `ctest` passes, stable across 5 runs.
Priority: P0
Area: WinUser
Type: Test
Evidence: free-eggbert blupi.cpp:626-633; planetblupi blupi.cpp:561-565
Depends on: TASK-0041, TASK-0043

Problem:
Both games' `WM_DESTROY` handler kills their live timer mechanism and posts `WM_QUIT`; a regression here would either leak a running timer or hang the shutdown sequence.

Required work:
* Add a test that starts a timer (`SetTimer` or `timeSetEvent`, matching each game's mechanism), simulates `WM_DESTROY`-equivalent teardown (kill timer, `PostQuitMessage`), and verifies `GetMessageA` subsequently returns `FALSE`.

Acceptance criteria:
* New test passes for both timer mechanisms.
* No unrelated API is added.

Out of scope:
* Do not test unrelated teardown of GDI/resource state in this task.

### TASK-0036: Add a regression test locking in `DefWindowProcA`'s `WM_CLOSE` default behavior

Status: DONE — TestDefWindowProcHandlesWmClose covers default WM_CLOSE behavior.
Priority: P0
Area: WinUser
Type: Test
Evidence: src/winuser_message.cpp:200-208; free-eggbert event.cpp:3265,3500,4220; planetblupi event.cpp:4754,4807,5024
Depends on: None

Problem:
Both games post `WM_CLOSE` from deep UI code paths and rely entirely on the *default* window procedure behavior (`DestroyWindow`+`PostQuitMessage`) to actually exit — this is already correctly implemented in `src/winuser_message.cpp:200-208` but has no dedicated regression test, so a future refactor could silently break both games' quit path.

Required work:
* Add a test that posts `WM_CLOSE` to a real window, drains the message queue via `PeekMessageA`/`DispatchMessageA` (not overriding the default `WndProc` behavior for this message), and verifies the window is destroyed and a subsequent `GetMessageA` returns `FALSE` (i.e. `WM_QUIT` was posted).

Acceptance criteria:
* New test passes and would fail if `WM_CLOSE`'s default handling were removed/changed.
* No unrelated API is added.

Out of scope:
* Do not add explicit `WM_CLOSE` handling to test `WndProc`s — the point is to test the *default* path both games rely on.

### TASK-0037: Add a message-loop test mimicking both games' exact loop shape

Status: DONE — `tests/test_planetblupi_loop.cpp` and `tests/test_eggbert_loop.cpp` reproduce the exact idiom end-to-end (including mixed real+posted message ordering); `ctest` passes, stable across 5 runs.
Priority: P0
Area: WinUser
Type: Test
Evidence: free-eggbert blupi.cpp:904-921; planetblupi blupi.cpp:904-919
Depends on: TASK-0036

Problem:
Both games use an identical, specific loop idiom (`PeekMessage(..., PM_NOREMOVE)` as a non-blocking gate, then blocking `GetMessage`, then `TranslateMessage`/`DispatchMessage`, with a `WaitMessage` idle branch when inactive) that isn't exercised end-to-end by `test_input_pipeline.cpp` (which uses a simpler `PeekMessage(PM_REMOVE)`-only drain loop).

Required work:
* Add a new test replicating the exact `PeekMessage(PM_NOREMOVE)` → `GetMessage` → `Translate`/`Dispatch` idiom, injecting a mix of real (SDL-originated) and posted (custom) messages, verifying correct ordering and delivery.

Acceptance criteria:
* New test passes.
* No unrelated API is added.

Out of scope:
* Do not model Emscripten's `emscripten_sleep(0)` idle branch — that's a platform-specific detail of the games' own code, not free-api's.

### TASK-0038: Verify `WaitMessage` does not busy-spin when idle

Status: DONE — `tests/test_winuser_regressions.cpp` (`TestWaitMessageDoesNotBusySpin`, wall-clock iteration-count bound plus a cross-thread post-then-observe check); `ctest` passes, stable across 5 runs.
Priority: P1
Area: WinUser
Type: Test
Evidence: free-eggbert blupi.cpp:921; planetblupi blupi.cpp:919
Depends on: TASK-0037

Problem:
Both games call `WaitMessage` only when the window is inactive (`!g_bActive`), expecting it to idle without consuming CPU until a new message arrives; there's no test verifying this.

Required work:
* Add a test that calls `WaitMessage` on an empty queue in a background thread, posts a message after a short delay from the main thread, and verifies `WaitMessage` returns promptly after the post (not immediately, and not after an excessive busy-wait).

Acceptance criteria:
* New test passes and demonstrates non-busy waiting (e.g. via a CPU-time or wall-clock-vs-sleep assertion).
* No unrelated API is added.

Out of scope:
* Do not add configurable wait timeouts — neither game uses them.

### TASK-0039: Add a stress test for cross-thread `PostMessageA` safety

Status: DONE — `tests/test_timer_regressions.cpp` (`TestCrossThreadPostMessageAStressTest`, real `timeSetEvent` background thread vs. main-thread `PeekMessageA` drain, 300 messages, verified stable across 5 runs); `ctest` passes.
Priority: P0
Area: WinUser
Type: Test
Evidence: free-eggbert blupi.cpp:641-648 (`TimerStep` posts from WinMM timer thread)
Depends on: TASK-0043

Problem:
free-eggbert's live frame-pump mechanism posts `WM_UPDATE` from a WinMM multimedia-timer thread into the main message queue every ~50ms; a race condition here would cause dropped ticks or corruption, directly breaking the game's frame pump.

Required work:
* Add a test that starts a `timeSetEvent`-based periodic timer whose callback calls `PostMessageA`, while the main thread concurrently drains the queue via `PeekMessageA`, running for enough iterations to catch races (e.g. a few hundred ticks), verifying no lost/corrupted messages and no crash.

Acceptance criteria:
* New test passes reliably across multiple runs (no flakiness from races).
* No unrelated API is added.

Out of scope:
* Do not test more than one concurrent timer thread — neither game needs it.

### TASK-0040: Apply and verify the P0 message-queue-mutex-during-sleep fix from the performance TODO

Status: DONE — Fix already applied (`src/winuser_message.cpp` `PeekMessageA` releases the mutex before `SDL_Delay`); now verified by TASK-0039's stress test.
Priority: P0
Area: WinUser
Type: Bugfix
Evidence: todo/free-api-performance-todo.md ("P0: Do Not Hold the Message Queue Mutex While Sleeping"); free-eggbert blupi.cpp:891 (timeSetEvent-driven PostMessage)
Depends on: TASK-0039

Problem:
`PeekMessageA` may currently hold `g_messageQueueMutex` while calling `SDL_Delay(1)` when the queue is empty, which can delay the WinMM timer thread's `PostMessageA` call (free-eggbert's real frame-pump path) by up to the sleep duration on every idle poll.

Required work:
* Apply the exact fix already specified in `todo/free-api-performance-todo.md`: check-and-pop the queue while holding the mutex, release the mutex, *then* `SDL_Delay(1)` if empty.
* Do not change the visible message-loop behavior otherwise.

Acceptance criteria:
* Code review confirms the mutex is never held across `SDL_Delay`.
* TASK-0039's stress test still passes (and ideally demonstrates lower worst-case latency).
* Existing tests still pass.
* No unrelated API is added.

Out of scope:
* Do not otherwise redesign the message queue's locking strategy beyond this specific fix.

### Milestone E — Timers

### TASK-0041: Add a regression test for `SetTimer`/`KillTimer`/`WM_TIMER` matching planetblupi's exact tick pattern

Status: DONE — `tests/test_timer_regressions.cpp` (`TestSetTimerFiresWmTimerAtApproximateInterval`); `ctest` passes.
Priority: P0
Area: WinUser
Type: Test
Evidence: planetblupi blupi.cpp:900,562,392,416-426
Depends on: None

Problem:
`SetTimer`/`KillTimer` and `WM_TIMER` synthesis are already implemented (`src/winuser_timer.cpp`, `FreeApiMessageQueue.cpp`) and verified by code inspection to be correct, but there is no automated test proving `WM_TIMER` fires reliably at the configured interval while the message loop polls `PeekMessage`/`GetMessage` — this is planetblupi's entire game-tick mechanism.

Required work:
* Add a test that calls `SetTimer(hwnd, 1, N, NULL)`, runs a `PeekMessage`-driven loop for a bounded number of iterations/wall-clock time, and verifies `WM_TIMER` messages arrive at approximately the requested interval (within reasonable jitter tolerance).

Acceptance criteria:
* New test passes.
* No unrelated API is added.

Out of scope:
* Do not test the `lpTimerFunc` callback parameter — both games ignore it (rely on `WM_TIMER` message delivery only).

### TASK-0042: Verify `KillTimer` actually stops further `WM_TIMER` delivery

Status: DONE — `tests/test_timer_regressions.cpp` (`TestKillTimerStopsWmTimerDelivery`); `ctest` passes.
Priority: P1
Area: WinUser
Type: Test
Evidence: planetblupi blupi.cpp:562 (`WM_DESTROY` calls `KillTimer`)
Depends on: TASK-0041

Problem:
No existing test confirms that once `KillTimer` is called, no further `WM_TIMER` messages for that timer ID are generated — a leak here would post stray messages after planetblupi's shutdown sequence begins.

Required work:
* Extend TASK-0041's test to call `KillTimer` mid-run and verify no further `WM_TIMER` messages for that ID appear afterward.

Acceptance criteria:
* New/extended test passes.
* No unrelated API is added.

Out of scope:
* None beyond this specific behavior.

### TASK-0043: Add a regression test for `timeSetEvent`/`timeKillEvent` matching free-eggbert's exact tick pattern

Status: DONE — `tests/test_timer_regressions.cpp` (`TestTimeSetEventFiresCallbackAtApproximateInterval`); `ctest` passes.
Priority: P0
Area: WinMM
Type: Test
Evidence: free-eggbert blupi.cpp:891,628,641-648
Depends on: None

Problem:
`timeSetEvent` is free-eggbert's entire frame-pump mechanism (its `WM_TIMER` path is dead code); like planetblupi's `SetTimer` path, it's implemented but untested end-to-end.

Required work:
* Add a test that calls `timeSetEvent(N, N/4, callback, 0, TIME_PERIODIC)`, verifies the callback fires repeatedly at approximately the requested interval, and that the callback can safely call `PostMessageA` (mirroring `TimerStep`) without corrupting the queue.

Acceptance criteria:
* New test passes.
* No unrelated API is added.

Out of scope:
* Do not test `timeSetEvent`'s one-shot (`TIME_ONESHOT`) mode — neither game uses it.

### TASK-0044: Verify `timeKillEvent` actually stops the periodic callback

Status: DONE — `tests/test_timer_regressions.cpp` (`TestTimeKillEventStopsCallback`); `ctest` passes.
Priority: P1
Area: WinMM
Type: Test
Evidence: free-eggbert blupi.cpp:628
Depends on: TASK-0043

Problem:
No existing test confirms `timeKillEvent` reliably stops further callback invocations — a lingering callback thread after game shutdown could crash on teardown-related use-after-free of game state.

Required work:
* Extend TASK-0043's test to call `timeKillEvent` and verify no further callback invocations occur afterward (e.g. via an atomic counter checked after a delay).

Acceptance criteria:
* New/extended test passes.
* No unrelated API is added.

Out of scope:
* None beyond this specific behavior.

### TASK-0045: Document the two distinct, game-specific live timer mechanisms

Status: DONE — docs/target-games.md + NEXT.md document both timer mechanisms.
Priority: P1
Area: Docs
Type: Documentation
Evidence: §3.2 (Timers table); free-eggbert vs. planetblupi divergence
Depends on: TASK-0041, TASK-0043

Problem:
It would be easy for a future contributor to assume only one timer mechanism (whichever they encounter first) is "the" free-api timer path and neglect or accidentally break the other, since each is the *sole* live tick mechanism for a different game.

Required work:
* Add a clear note to `Documentation.md` (or `docs/target-games.md`) stating: free-eggbert's live tick path is `timeSetEvent`/`timeKillEvent` (its `SetTimer`/`WM_TIMER` call sites are dead code); planetblupi's live tick path is `SetTimer`/`KillTimer`/`WM_TIMER` (it never calls `timeSetEvent`). Both must keep working.

Acceptance criteria:
* Note added and cross-referenced from both timer-related test files' comments.
* No unrelated API is added.

Out of scope:
* Do not unify the two mechanisms into one code path — they are genuinely different WinMM/WinUser APIs with different semantics.

### TASK-0046: Gate `SetTimer`'s unconditional per-call `SDL_Log`

Status: DONE — src/winuser_timer.cpp:29,46 gates SetTimer's SDL_Log.
Priority: P2
Area: Logging
Type: Bugfix
Evidence: src/winuser_timer.cpp:29 (`SDL_Log` fires on every `SetTimer` call, ungated)
Depends on: TASK-0105

Problem:
`SetTimer` currently logs unconditionally on every call via `SDL_Log`; while not a hot per-frame path (called once at startup by planetblupi), it's inconsistent with the gated-logging policy applied elsewhere and adds noise when diagnostics aren't requested.

Required work:
* Gate the existing `SDL_Log` calls in `src/winuser_timer.cpp` behind the same diagnostics-enabled check used elsewhere (e.g. `FreeApiDiagnosticsEnabled()`), matching the pattern already used for other subsystems.

Acceptance criteria:
* `SetTimer`/`KillTimer` no longer log unconditionally; logging still occurs when diagnostics are enabled.
* Existing tests still pass.
* No unrelated API is added.

Out of scope:
* Do not remove the log entirely — just gate it.

### Milestone F — Input

### TASK-0047: Add a regression test locking in `WM_MOUSEMOVE`'s bit-exact lParam packing

Status: DONE — `tests/test_input_pipeline.cpp` extended with (0,0), near-0xFFFF, and negative-as-unsigned boundary cases; `ctest` passes.
Priority: P0
Area: WinUser
Type: Test
Evidence: free-eggbert event.cpp:5276-5297 (demo replay); planetblupi include/event.h:70-77 (`DemoEvent` persistence)
Depends on: None

Problem:
`test_input_pipeline.cpp` already asserts `LOWORD`/`HIWORD` packing for one coordinate pair (123,456), but both games persist raw `(message,wParam,lParam)` triples to disk for demo recording/playback, so any packing deviation — especially at boundary values — would desync demo files.

Required work:
* Extend the existing input-pipeline test (or add a new one) covering boundary/edge coordinate values (0,0 / large values near 0xFFFF / negative-as-unsigned edge cases) to confirm `LOWORD(lParam)=x, HIWORD(lParam)=y` holds exactly, matching real Win32 semantics.

Acceptance criteria:
* Extended/new test passes.
* No unrelated API is added.

Out of scope:
* Do not add sub-pixel or floating-point coordinate support — both games use integer client coordinates.

### TASK-0048: Add a regression test for `MK_*` mouse-button-state flags

Status: DONE — `tests/test_winuser_regressions.cpp` (`TestMouseMoveLParamPackingAndModifierFlags` extended with an MK_RBUTTON drag case). MK_SHIFT/MK_CONTROL remain confirmed-not-automatable in this headless environment (SDL_PushEvent doesn't update SDL_GetKeyboardState()); MK_LBUTTON/MK_RBUTTON (the testable subset) now both covered.
Priority: P1
Area: WinUser
Type: Test
Evidence: planetblupi event.cpp:3994,3413,3440,3472,3504 (`MK_LBUTTON`/`MK_RBUTTON`/`MK_SHIFT`/`MK_CONTROL`)
Depends on: None

Problem:
`MK_*` flags are documented as `IMPLEMENTED` in `winuser.h` and the README, and are required live by planetblupi (not by free-eggbert, which never reads them), but no existing test exercises them.

Required work:
* Add a test injecting mouse-move/button SDL events with modifier keys (Shift/Ctrl) held and verifying the corresponding `WM_MOUSEMOVE`/`WM_*BUTTONDOWN` message's `wParam` carries the correct `MK_LBUTTON`/`MK_RBUTTON`/`MK_SHIFT`/`MK_CONTROL` bits.

Acceptance criteria:
* New test passes.
* No unrelated API is added.

Out of scope:
* Do not test `MK_MBUTTON`/`MK_XBUTTON1/2` combinations beyond what's already implemented — no evidence either game needs them tested further than existing coverage.

### TASK-0049: Extend `test_input_pipeline.cpp` with `WM_SYSKEYDOWN`+`VK_F10` regression coverage

Status: DONE — `tests/test_input_pipeline.cpp` (F10 delivered as WM_SYSKEYDOWN/WM_SYSKEYUP with wParam==VK_F10, and confirmed NOT also delivered as WM_KEYDOWN); `ctest` passes.
Priority: P1
Area: WinUser
Type: Test
Evidence: free-eggbert blupi.cpp:457,461; planetblupi blupi.cpp:402,406
Depends on: None

Problem:
Both games implement an identical quirk (test for `WM_SYSKEYDOWN`/`UP` with `wParam==VK_F10`, then treat it like `WM_KEYDOWN`/`UP`), which requires free-api to correctly deliver real `WM_SYSKEYDOWN`/`WM_SYSKEYUP` for the F10 key specifically — not yet covered by an automated test.

Required work:
* Add a test case to `test_input_pipeline.cpp` injecting an F10 key press/release and verifying `WM_SYSKEYDOWN`/`WM_SYSKEYUP` (not `WM_KEYDOWN`/`WM_KEYUP`) are delivered with `wParam==VK_F10`.

Acceptance criteria:
* New test case passes.
* Existing test cases still pass.
* No unrelated API is added.

Out of scope:
* Do not test other Alt-key/system-menu combinations neither game handles.

### TASK-0050: Add regression coverage for the full VK_* set both games test

Status: DONE — `tests/test_input_pipeline.cpp` extended with F1-F9/F11/F12, RETURN, SHIFT, CONTROL, PAUSE, LEFT/RIGHT/UP/DOWN, HOME, END (SPACE/ESCAPE were already covered); full §3.4 set now covered (F10 covered separately via TASK-0049's WM_SYSKEYDOWN test).
Priority: P1
Area: WinUser
Type: Test
Evidence: §3.4 (VK_* constants table)
Depends on: None

Problem:
Individual VK codes (F1-F12, ESC, RETURN, SHIFT, CONTROL, PAUSE, arrows, HOME, END, SPACE, A-Z) are implemented but only a few (SPACE, ESCAPE) are covered by `test_input_pipeline.cpp` today.

Required work:
* Extend `test_input_pipeline.cpp` with key-down/up cases for the remaining VK codes both games actually test, per §3.4.

Acceptance criteria:
* Extended test covers every VK constant listed in §3.4's table.
* No unrelated API is added.

Out of scope:
* Do not add VK codes not listed in §3.4 (e.g. numpad, media keys) — no evidence either game uses them.

### TASK-0051: Investigate current `ShowCursor`/`SetCursor` STUB against both games' live cursor-visibility requirement

Status: DONE — ShowCursor/SetCursor STUB investigated and fixed (commit fa6616a).
Priority: P0
Area: WinUser
Type: Audit
Evidence: include/winuser.h:385,393 (`@note Status: STUB`); free-eggbert pixmap.cpp:165, event.cpp:3448,4931...; planetblupi pixmap.cpp:133, event.cpp:2777,4971,5003
Depends on: None

Problem:
`ShowCursor` and `SetCursor` are currently no-op `STUB`s, but both games call `ShowCursor(FALSE)` when entering software-sprite-cursor mode (`MOUSETYPEGRA`) expecting the *real* OS cursor to disappear — a no-op here means the real OS arrow cursor remains visible on top of/alongside the game's custom-drawn cursor sprite, a real and immediately visible bug in normal gameplay.

Required work:
* Confirm the hypothesis with a manual/E2E check (per the `/verify` skill) running either game and observing whether a duplicate/OS cursor is visible during normal play.
* Document the finding.

Acceptance criteria:
* A written finding confirms or refutes the visible-double-cursor hypothesis.
* No unrelated API is added.

Out of scope:
* Do not implement the fix in this task — implementation is TASK-0052/0053.

### TASK-0052: Implement real `ShowCursor` via SDL cursor visibility

Status: DONE — src/winuser_cursor.cpp implements real, SDL-backed ShowCursor.
Priority: P1
Area: WinUser
Type: Implementation
Evidence: TASK-0051 finding; free-eggbert/planetblupi cursor-visibility call sites
Depends on: TASK-0051

Problem:
Per TASK-0051, both games depend on `ShowCursor` actually hiding/showing the real OS cursor.

Required work:
* Implement `ShowCursor(BOOL bShow)` to call `SDL_ShowCursor()`/`SDL_HideCursor()` (or the SDL3-current equivalent) on the active window, maintaining the Win32 display-counter semantics only to the extent both games rely on it (i.e. treat it as a simple show/hide toggle — neither game nests calls expecting an exact counter value beyond boolean visibility, per the audits).

Acceptance criteria:
* New test (extending TASK-0051) shows the OS cursor is actually hidden/shown by SDL after calling `ShowCursor(FALSE)`/`ShowCursor(TRUE)`.
* Existing tests still pass.
* No unrelated API is added.

Out of scope:
* Do not implement the exact Win32 nested-counter return-value semantics beyond what both games' call patterns require (simple toggle, not a counter both games inspect).

### TASK-0053: Implement minimal real `SetCursor` behavior

Status: DONE — Same commit implements real SetCursor.
Priority: P1
Area: WinUser
Type: Implementation
Evidence: free-eggbert misc.cpp:60; TASK-0051 finding
Depends on: TASK-0052

Problem:
`SetCursor` is currently a no-op `STUB`; free-eggbert calls it (with a handle from `LoadCursorA`) only when *not* in software-cursor mode, expecting the OS cursor shape/visibility to reflect the call.

Required work:
* Implement `SetCursor` to at minimum ensure the OS cursor is visible (complementing `ShowCursor`) when called with a non-null handle; real per-shape cursor rendering is not required since `LoadCursorA` itself is an acceptable safe stub (§5) — a generic visible-arrow-cursor result is sufficient.

Acceptance criteria:
* New test verifies calling `SetCursor` with a `LoadCursorA`-returned handle results in a visible OS cursor.
* Existing tests still pass.
* No unrelated API is added.

Out of scope:
* Do not implement real per-shape (arrow vs. hand vs. crosshair) cursor rendering — no evidence either game's gameplay depends on the exact shape.

### TASK-0054: Add a regression test for `ShowCursor`/`SetCursor` visibility toggling

Status: DONE — TestShowCursorHidesAndShowsRealCursor, TestSetCursorReturnsPreviousHandle.
Priority: P1
Area: Tests
Type: Test
Evidence: TASK-0052, TASK-0053
Depends on: TASK-0052, TASK-0053

Problem:
Once implemented, `ShowCursor`/`SetCursor` need a durable regression test so a future SDL upgrade or refactor doesn't silently regress cursor visibility for both games.

Required work:
* Add a dedicated test toggling `ShowCursor(FALSE)`/`ShowCursor(TRUE)` and calling `SetCursor` with a loaded cursor handle, verifying SDL's reported cursor-visibility state at each step.

Acceptance criteria:
* New test passes and is registered in CTest.
* No unrelated API is added.

Out of scope:
* None beyond this specific behavior.

### TASK-0055: Verify `GetCursorPos`/`ScreenToClient`/`ClientToScreen`/`SetCursorPos` hot-path correctness

Status: DONE — `tests/test_winuser_regressions.cpp` (`TestClientToScreenTracksWindowPositionNotStale` incl. perf sanity bound, `TestSetCursorPosAndGetCursorPosRoundTrip`); `ctest` passes. SetCursorPos/GetCursorPos round-trip is only asserted when the sandbox's SDL dummy driver supports the warp (it doesn't, so that specific check SKIPs — GetCursorPos's return type/behavior is still verified).
Priority: P0
Area: WinUser
Type: Test
Evidence: planetblupi pixmap.cpp:975-990 (`Display()`, every frame), 1100-1123 (`MouseQuickDraw`)
Depends on: None

Problem:
Planetblupi calls `ClientToScreen` (twice) every single displayed frame inside `Display()`, and `SetCursorPos` potentially every mouse-move in `MOUSETYPEWINPOS` mode — these are hot-path, not startup-only, calls; correctness and reasonable performance both matter.

Required work:
* Add a test simulating repeated `GetClientRect`+`ClientToScreen` calls across a window move/resize, verifying the returned screen coordinates track the window's real position each time (not a cached/stale value).
* Add a basic performance sanity check (e.g. N calls complete within a generous time bound) to catch an accidental O(n) regression in a hot path.

Acceptance criteria:
* New test passes.
* No unrelated API is added.

Out of scope:
* Do not add formal performance benchmarking infrastructure — a simple bounded-time sanity check is sufficient.

### TASK-0056: Verify `LoadCursorA`/`LoadIconA` return non-null handles for all names both games use

Status: DONE — `tests/test_winuser_regressions.cpp` (`TestLoadCursorAAndLoadIconAReturnNonNullForAllRealNames`), covering all 14 real cursor names (union of both games) plus the one real icon name.
Priority: P2
Area: Resources
Type: Test
Evidence: free-eggbert misc.cpp:48-59 (12 names); planetblupi misc.cpp:56-69 (13 names)
Depends on: None

Problem:
Both games call `LoadCursorA` with a fixed set of named cursor IDs (`"IDC_ARROW"`, `"IDC_POINTER"`, etc.); the current STUB should return a non-null handle for every one of these specific names to avoid a null-handle edge case in game code that doesn't null-check.

Required work:
* Add a test enumerating the exact name strings from both audits (§3.2 Input table) and verifying each returns a non-null `HCURSOR`/`HICON`.

Acceptance criteria:
* New test passes for every named ID cited in the audit.
* No unrelated API is added.

Out of scope:
* Do not implement real per-shape cursor/icon rendering.

### TASK-0057: Verify `MK_*`/VK_* input pipeline performance under sustained input (no per-event allocation regressions)

Status: DONE — `tests/test_input_pipeline.cpp` extended with a 2000-event sustained mouse/keyboard stress test, bounded memory (per-iteration `g_received.clear()`) and time (< 5s, actual ~11ms); `ctest` passes.
Priority: P2
Area: WinUser
Type: Test
Evidence: todo/free-api-performance-todo.md (general hot-path guidance)
Depends on: TASK-0048, TASK-0050

Problem:
Input translation runs continuously during gameplay in both games; while no specific bug is evidenced, the existing performance TODO's general guidance (avoid hot-path allocation/logging) applies to the input path as much as to GDI.

Required work:
* Add a sustained-input stress test (hundreds of injected mouse/keyboard events processed via the real pipeline) verifying no crash, no unbounded memory growth, and reasonable wall-clock time.

Acceptance criteria:
* New test passes.
* No unrelated API is added.

Out of scope:
* Do not add micro-benchmarking/profiling infrastructure beyond a simple bounded-time/bounded-memory check.

### TASK-0058: Document that `GetAsyncKeyState`/`GetKeyState` are confirmed unneeded

Status: DONE — Entry added to `docs/out-of-scope.md`'s "Unsupported APIs" table.
Priority: P3
Area: Docs
Type: Documentation
Evidence: §3.2 ("confirmed zero use" in both games)
Depends on: TASK-0007

Problem:
These are common Win32 input functions a future contributor might assume are needed; both audits explicitly confirm zero usage in either game (all input is message-driven).

Required work:
* Add an entry to `docs/out-of-scope.md`'s "Unsupported APIs" table (TASK-0007) citing this finding.

Acceptance criteria:
* Entry present.
* No unrelated API is added.

Out of scope:
* Do not implement these functions.

### Milestone G — GDI

### TASK-0059: Verify `CreateCompatibleDC`/`DeleteDC` throwaway-DC lifecycle

Status: DONE — `tests/test_gdi_regressions.cpp` (`TestCreateCompatibleDcRepeatedLifecycleDoesNotLeak`, 5000-iteration cycle verified against the internal live-DC diagnostic counter); `ctest` passes.
Priority: P0
Area: GDI
Type: Test
Evidence: free-eggbert pixmap.cpp:137,152, ddutil.cpp:141,166; planetblupi ddutil.cpp:198,230, pixmap.cpp:282,293
Depends on: None

Problem:
Both games repeatedly create-then-destroy a memory DC purely to host a bitmap for `GetDeviceCaps`/`GetObject`/`SelectObject`/`StretchBlt`; there's no test verifying repeated create/destroy cycles don't leak internal state.

Required work:
* Add a test looping `CreateCompatibleDC(NULL)`→(select/query)→`DeleteDC` many times, verifying no crash and no unbounded internal-registry growth.

Acceptance criteria:
* New test passes.
* No unrelated API is added.

Out of scope:
* Do not test `CreateCompatibleDC` with a non-NULL source DC — neither game uses that form.

### TASK-0060: Verify `GetDeviceCaps(SIZEPALETTE)` plausible-value contract

Status: DONE — **Found and fixed a real bug while writing this test.** `GetDeviceCaps(SIZEPALETTE)` previously always returned 256 (`src/wingdi_misc.cpp`), which is real Win32's actual-hardware-palette (<=8bpp) value, not the TrueColor-host value (0). Both games' branching logic only agrees when the value is exactly 0 (free-eggbert pixmap.cpp:146 vs. pixmap.cpp:428/planetblupi pixmap.cpp:287 disagree for any other nonzero value — analyzed in the new test's comment). The old 256 forced free-eggbert's true-color decor off and forced planetblupi's minimap onto its untested 8-bit-indexed `CreateBitmap` path (TASK-0125) instead of the true-color 16-bit path. Fixed to return 0; regression test `tests/test_gdi_regressions.cpp` (`TestGetDeviceCapsSizePaletteReportsTrueColorHost`) locks this in; `ctest` passes. **Needs a manual visual playtest** of planetblupi's minimap and free-eggbert's true-color rendering to confirm the corrected path looks right (cannot be verified visually in this headless sandbox).
Priority: P0
Area: GDI
Type: Test
Evidence: free-eggbert pixmap.cpp:146,428; planetblupi pixmap.cpp:287
Depends on: TASK-0059

Problem:
Both games branch their TrueColor-vs-paletted rendering path on whether `GetDeviceCaps(hdc, SIZEPALETTE) < 257`; no test currently locks in this contract.

Required work:
* Add a test verifying `GetDeviceCaps(hdc, SIZEPALETTE)` returns a value both games would correctly interpret as "not an 8-bit palette display" (i.e. `>= 257`, matching a modern TrueColor host).

Acceptance criteria:
* New test passes.
* No unrelated API is added.

Out of scope:
* Do not implement other `GetDeviceCaps` indices neither game queries.

### TASK-0061: Verify `GetSystemPaletteEntries` fills a plausible 256-entry table

Status: DONE — `tests/test_gdi_regressions.cpp` (`TestGetSystemPaletteEntriesFills256WellFormedEntries`); `ctest` passes.
Priority: P1
Area: GDI
Type: Test
Evidence: planetblupi pixmap.cpp:292
Depends on: TASK-0059

Problem:
Planetblupi reads 256 `PALETTEENTRY` values from this call into `m_sysPal`; the current grayscale-placeholder implementation should at minimum not crash and fill exactly 256 entries.

Required work:
* Add a test verifying `GetSystemPaletteEntries(hdc, 0, 256, buffer)` fills exactly 256 well-formed `PALETTEENTRY` structs.

Acceptance criteria:
* New test passes.
* No unrelated API is added.

Out of scope:
* Do not implement real palette-matching accuracy — grayscale placeholder is sufficient per current scope.

### TASK-0062: Verify `LoadImageA` extension-agnostic BMP decoding

Status: DONE — `tests/test_gdi_regressions.cpp` (`TestLoadImageADecodesNonBmpExtensionAndGetObjectAReportsCorrectDimensions`, using an in-memory-generated BMP fixture saved with a `.blp` extension); `ctest` passes.
Priority: P0
Area: GDI
Type: Test
Evidence: planetblupi ddutil.cpp:90,95,148,151 (`.blp` files are BMP data under a different extension)
Depends on: None

Problem:
`LoadImageA` uses `SDL_LoadBMP` internally; both games load every sprite-sheet asset through this path with non-`.bmp` file extensions (`.blp`), so the implementation must not make any extension-based assumptions.

Required work:
* Add a test fixture: a real BMP byte stream saved with a `.blp` (or other non-`.bmp`) extension, and verify `LoadImageA(..., LR_LOADFROMFILE, ...)` correctly decodes it.

Acceptance criteria:
* New test passes using a non-`.bmp`-named fixture file.
* No unrelated API is added.

Out of scope:
* Do not add support for image formats other than BMP — neither game uses any other format through this path.

### TASK-0063: Verify `GetObjectA` returns correct `bmWidth`/`bmHeight` for loaded bitmaps

Status: DONE — same test as TASK-0062 (`TestLoadImageADecodesNonBmpExtensionAndGetObjectAReportsCorrectDimensions`) also asserts `GetObjectA`'s `bmWidth`/`bmHeight` match the known fixture dimensions; `ctest` passes.
Priority: P0
Area: GDI
Type: Test
Evidence: free-eggbert ddutil.cpp:57,149; planetblupi ddutil.cpp:47,104,208
Depends on: TASK-0062

Problem:
Both games use `GetObjectA` to read back a loaded `HBITMAP`'s dimensions, which then drives DirectDraw surface sizing in free-direct; an incorrect value here would misallocate or mis-scale every sprite sheet in both games.

Required work:
* Add a test loading a known-dimension BMP fixture via `LoadImageA` then verifying `GetObjectA(hbm, sizeof(BITMAP), &bm)` reports the exact matching `bmWidth`/`bmHeight`.

Acceptance criteria:
* New test passes.
* No unrelated API is added.

Out of scope:
* Do not verify other `BITMAP` fields (`bmPlanes`/`bmBitsPixel`/`bmBits`) beyond what both games read.

### TASK-0064: Verify `SelectObject`/`DeleteObject` bitmap-into-DC lifecycle

Status: DONE — `tests/test_gdi_regressions.cpp` (`TestSelectObjectDeleteObjectBitmapIntoDcLifecycle`, reproducing `DDCopyBitmap`'s exact create->select->blit->delete sequence across 500 iterations); `ctest` passes.
Priority: P0
Area: GDI
Type: Test
Evidence: free-eggbert ddutil.cpp:144; planetblupi ddutil.cpp:202
Depends on: TASK-0059, TASK-0063

Problem:
Both games select a loaded bitmap into a memory DC before blitting, then delete it after; no existing test verifies the full select→blit→delete sequence in one pass.

Required work:
* Add a test performing exactly this sequence and verifying no crash/leak across repeated iterations.

Acceptance criteria:
* New test passes.
* No unrelated API is added.

Out of scope:
* Do not test selecting non-bitmap GDI objects — neither game does this.

### TASK-0065: Verify and optimize `StretchBlt` per the performance TODO's P0 items

Status: DONE — StretchBlt already implemented and locked in by test_gdi_regressions.cpp.
Priority: P0
Area: GDI
Type: Bugfix
Evidence: todo/free-api-performance-todo.md ("P0: Improve the 1:1 StretchBlt Fast Path", "P1: Fix Scaled StretchBlt Source Clipping Semantics"); free-eggbert ddutil.cpp:162; planetblupi ddutil.cpp:224
Depends on: TASK-0064

Problem:
`StretchBlt` is the central image-loading primitive for both games (every sprite sheet passes through it), and the existing, already-scoped performance TODO identifies concrete correctness/perf risks (hot-path logging, slow per-pixel 1:1 copies, scaled-path clipping semantics, per-call allocation in scaled paths) that should be fixed as part of bringing this P0 subsystem up to the bar this plan sets.

Required work:
* Apply the specific fixes already described in `todo/free-api-performance-todo.md`: detect and fast-path the 1:1 `SRCCOPY` case; fix scaled-path source-clipping semantics without silently changing edge behavior; avoid per-blit heap allocation in the scaled path; remove/gate hot-path `SDL_Log` calls.

Acceptance criteria:
* 1:1 blits copy the expected rectangle exactly (new test).
* Clipped 1:1 blits do not write outside destination bounds (new test).
* Scaled blits preserve documented, intentional clipping behavior (new test).
* No hot-path log spam during normal blitting.
* Existing tests still pass.
* No unrelated API is added.

Out of scope:
* Do not replace the GDI-like software blit path with SDL_Renderer/SDL_GPU/OpenGL, per the performance TODO's explicit constraint.

### TASK-0066: Verify `GetPixel`/`SetPixel` consistency with free-direct's `Lock()`-exposed backing memory

Status: DONE — TestGetSetPixelRoundTripOnSurfaceDc verifies the shared-memory Surface-DC path.
Priority: P0
Area: GDI
Type: Bugfix
Evidence: todo/free-api-performance-todo.md ("P0: Keep GetPixel/SetPixel Correct for DirectDraw Color Matching"); free-eggbert ddutil.cpp:289-290,314; planetblupi ddutil.cpp:360,361,391
Depends on: TASK-0059

Problem:
Both games' color-key computation (`DDColorMatch`-equivalent) writes a probe color via `SetPixel`, reads it back via a DirectDraw `Lock()`-exposed pointer, then restores the original — this only works if `GetPixel`/`SetPixel` operate on the exact same backing pixel memory free-direct's `Lock()` exposes.

Required work:
* Verify (and fix if needed) that `GetDC`/`SetPixel`/`GetPixel`/`ReleaseDC` operate on the same buffer a subsequent `Lock()` call would expose, per the exact sequence documented in the performance TODO.

Acceptance criteria:
* A test round-trips `SetPixel`→(simulated `Lock()`-equivalent read)→verifies the same value, then restores and re-verifies.
* Existing tests still pass.
* No unrelated API is added.

Out of scope:
* Do not micro-optimize `GetPixel`/`SetPixel` speed — correctness is the priority per the performance TODO's own framing (these are called rarely, not per-frame).

### TASK-0067: Verify `CreateBitmap` raw-pixel-buffer path for planetblupi's minimap

Status: DONE — test_gdi_regressions.cpp covers the raw-buffer path (extended by TASK-0125).
Priority: P1
Area: GDI
Type: Test
Evidence: planetblupi decmap.cpp:576-594
Depends on: None

Problem:
Planetblupi rebuilds its minimap by calling `CreateBitmap` from an in-memory 1bpp-or-8bpp pixel buffer depending on `g_bPalette`; this path is not covered by an existing test.

Required work:
* Add a test calling `CreateBitmap` with both a 1-plane/1-bit and an 8-bit-per-pixel raw buffer, verifying a valid `HBITMAP` is returned and `GetObjectA` reports matching dimensions.

Acceptance criteria:
* New test passes for both bit-depth cases.
* No unrelated API is added.

Out of scope:
* Do not implement additional bit depths neither game uses.

### TASK-0068: Add fixture-based GDI regression tests per the performance TODO's test list

Status: DONE — `tests/test_gdi_regressions.cpp` (`TestStretchBlt1to1OutOfRangeSourceRectClipsSafely`, covering negative source origin and source-rect-past-bitmap-edge); the rest of the list (1:1 exact copy, clip safety, scaled nearest-neighbor, GetPixel/SetPixel vs Lock, PeekMessageA mutex, timeSetEvent-while-idle) was already covered. `ctest` passes.
Priority: P1
Area: Tests
Type: Test
Evidence: todo/free-api-performance-todo.md ("P2: Add Focused Performance/Correctness Tests")
Depends on: TASK-0065, TASK-0066

Problem:
The performance TODO already enumerates a specific, well-scoped test list that hasn't been implemented yet.

Required work:
* Implement the listed tests: 1:1 `StretchBlt` exact-rect copy; clipped 1:1 `StretchBlt` bounds safety; scaled `StretchBlt` nearest-neighbor behavior; out-of-range source rectangle handling; `GetPixel`/`SetPixel` vs. `Lock()`-consistency; `PeekMessageA` not holding the queue mutex while sleeping (ties to TASK-0040); `timeSetEvent` callbacks posting while idle (ties to TASK-0043).

Acceptance criteria:
* All listed tests exist, pass, and are registered in CTest.
* No unrelated API is added.

Out of scope:
* Do not add tests requiring a full game launch, per the performance TODO's explicit constraint.

### TASK-0069: Gate hot-path GDI logging behind `FREE_API_DEBUG_GDI`

Status: DONE — FreeApiGdiDebugEnabled() gates hot-path GDI logging in src/wingdi_blit.cpp.
Priority: P1
Area: Logging
Type: Bugfix
Evidence: todo/free-api-performance-todo.md ("P0: Remove Hot-Path Rendering Logs")
Depends on: TASK-0065

Problem:
`StretchBlt` and related hot-path GDI functions must not log on every call during normal gameplay; the performance TODO already specifies the exact fix (a cached `FREE_API_DEBUG_GDI` environment-variable check).

Required work:
* Implement the `FreeApiGdiDebugEnabled()` helper exactly as specified in the performance TODO and gate all per-blit/per-pixel logging behind it.

Acceptance criteria:
* Normal gameplay (env var unset) produces no per-frame GDI log spam.
* Setting `FREE_API_DEBUG_GDI=1` restores detailed logging.
* Existing tests still pass.
* No unrelated API is added.

Out of scope:
* Do not remove error logging for genuinely unsupported calls — only gate the noisy success-path logs.

### TASK-0070: Document the GDI subset's exact boundary with free-direct

Status: DONE — "Boundary with free-direct" section added to `docs/scope.md` with the concrete `StretchBlt` vs. `IDirectDrawSurface::Blt` example.
Priority: P2
Area: Docs
Type: Documentation
Evidence: §3.6; §5 (DirectDraw row)
Depends on: TASK-0004

Problem:
It's easy to conflate free-api's GDI subset (real Win32 `HDC`/`HBITMAP`/`StretchBlt`/`GetPixel`/`SetPixel`) with free-direct's DirectDraw surface/blit APIs, since both games' `ddutil.cpp`-style code mixes calls to both in the same functions.

Required work:
* Add a short note to `docs/out-of-scope.md` or `docs/scope.md` clarifying exactly which GDI calls are free-api's responsibility (per §3.6) versus which DirectDraw calls belong to free-direct.

Acceptance criteria:
* Note added with a concrete example (e.g. "`StretchBlt` is free-api's; `IDirectDrawSurface::Blt` is free-direct's").
* No unrelated API is added.

Out of scope:
* Do not document free-direct's internal implementation — only the boundary.

### Milestone H — Resources

### TASK-0071: Document the current, real, safe-fallback behavior of the resource subsystem

Status: DONE — "Resource subsystem: FindResourceA miss → file-based fallback" section added to `docs/out-of-scope.md`, with file:line citations for both games' palette and sprite-sheet fallback chains.
Priority: P1
Area: Resources
Type: Documentation
Evidence: §3.8; free-eggbert ddutil.cpp:204; planetblupi ddutil.cpp:268
Depends on: TASK-0004

Problem:
`FindResourceA`/`LoadResource`/etc. being `STUB` sounds alarming out of context, but per this audit both games' `.rc` files are not compiled by their own CMake builds, so `FindResourceA` always misses today and both games *already* correctly fall through to file-based loading (`_lopen`-based palette read, `LoadImageA(LR_LOADFROMFILE)`) — this is the actual, currently-working behavior, not a hypothetical.

Required work:
* Document this exact chain (miss→fallback) in `docs/out-of-scope.md` so it's clear the STUB is intentional and currently sufficient, not an oversight.

Acceptance criteria:
* Doc explicitly states both games' `.rc` files aren't compiled and describes the miss→fallback chain with file:line citations.
* No unrelated API is added.

Out of scope:
* Do not build the full embedded-resource-registry system from `todo/Embedded_Resources_FreeAPI.md` based on this task alone.

### TASK-0072: Decide and document the future of `todo/Embedded_Resources_FreeAPI.md`

Status: DONE — Decision note (DEFERRED) added atop `todo/Embedded_Resources_FreeAPI.md`.
Priority: P1
Area: Resources
Type: Documentation
Evidence: todo/Embedded_Resources_FreeAPI.md; TASK-0071 finding
Depends on: TASK-0071

Problem:
A detailed, well-scoped design for a full embedded-resource registry + CMake generator already exists in `todo/`, but per this audit, neither game is currently blocked by missing resource support — both fall through to working file-based paths. Building the full system now would be scope creep unless a concrete, evidenced need emerges.

Required work:
* Add a decision note at the top of `todo/Embedded_Resources_FreeAPI.md` (or move it to `docs/deferred/`) stating: this design is **deferred** pending a concrete, evidenced game requirement (e.g. if `.rc` compilation is ever added to either game's build and a resource lookup becomes load-bearing); do not implement speculatively.

Acceptance criteria:
* Decision note added.
* No unrelated API is added.

Out of scope:
* Do not delete the design doc — it remains valuable if the need ever arises.

### TASK-0073: Add a regression test for `FindResourceA` miss → `_lopen` fallback path

Status: DONE — `tests/test_file_regressions.cpp` (`TestFindResourceAMissesThenLopenLreadLcloseDecodesRealBmpHeader`); `ctest` passes.
Priority: P0
Area: Resources
Type: Test
Evidence: free-eggbert ddutil.cpp:204,232-240; planetblupi ddutil.cpp:268,297,303-306
Depends on: TASK-0071

Problem:
The miss→fallback chain is the actual live path exercised by every bitmap-palette load in both games, but has no dedicated test isolating just this behavior.

Required work:
* Add a test verifying `FindResourceA(hInstance, MAKEINTRESOURCEA(id), RT_BITMAP)` returns `NULL` (matching current safe-stub behavior) and that a subsequent `_lopen`/`_lread`/`_lclose` sequence against a real file succeeds.

Acceptance criteria:
* New test passes.
* No unrelated API is added.

Out of scope:
* Do not implement a real resource-section backing for `RT_BITMAP` in this task.

### TASK-0074: Investigate current `LoadStringA` STUB against both games' load-bearing text requirement

Status: DONE — LoadStringA STUB investigated and fixed (commit b8b6667).
Priority: P0
Area: Resources
Type: Audit
Evidence: include/winuser.h:499 (`@note Status: STUB`); free-eggbert misc.cpp:36-39 + ~90 call sites; planetblupi misc.cpp:42-45 + ~50 call sites
Depends on: None

Problem:
Unlike the other resource functions, `LoadStringA` is **not** a safe stub — both games call it dozens of times to fetch every piece of on-screen UI text (tooltips, button labels, win/lose/error messages), so a placeholder-string STUB means both games currently show broken/placeholder UI text wherever `LoadStringA` is called.

Required work:
* Run either game (per the `/verify` skill) and observe actual on-screen text at a few `LoadStringA`-driven UI surfaces (menu, in-game HUD/tooltip, a win/lose screen) to confirm whether text is currently missing/placeholder or coincidentally already sourced from somewhere else.
* Document the finding, including which specific string IDs are exercised at first-screen/menu (the true P0 boot-blocking subset) versus deeper-menu/rare strings.

Acceptance criteria:
* A written finding documents the actual current on-screen text behavior for both games with specifics (screenshot or transcript), and lists the minimal string-ID set needed for first-screen/menu correctness.
* No unrelated API is added.

Out of scope:
* Do not implement the fix in this task — implementation is TASK-0075.

### TASK-0075: Implement a minimal real `LoadStringA` backing store

Status: DONE — cmake/ExtractStringTable.cmake + FreeApiStringTable implement the real backing store.
Priority: P1
Area: Resources
Type: Implementation
Evidence: TASK-0074 finding; free-eggbert/planetblupi `resource.h`/`.rc` string tables
Depends on: TASK-0074

Problem:
Per TASK-0074, both games need `LoadStringA` to return real text, but neither game's `.rc` file is compiled today, so there is no existing resource-section data to source it from.

Required work:
* Implement the narrowest possible backing store sufficient for both games: either (a) a small, hand-maintained table (generated once from each game's `.rc`/`resource.h` files, scoped only to IDs actually referenced per TASK-0074's finding) compiled into free-api or provided by each game itself, or (b) confirm and document if the games already carry this data some other way (per TASK-0074's investigation) and only a small adapter is needed.
* Explicitly do not build the general CMake `.rc`-parsing generator from `todo/Embedded_Resources_FreeAPI.md` — scope this to the minimum needed.

Acceptance criteria:
* `LoadStringA` returns correct, real text for every string ID exercised at first-screen/menu in both games (per TASK-0074's minimal set).
* New test verifies specific IDs return specific expected text.
* Existing tests still pass.
* No unrelated API is added.

Out of scope:
* Do not build a general-purpose `.rc`/`resource.h` parser or CMake generator — only the minimal string data both games need.
* Do not implement `LoadStringW`.

### TASK-0076: Confirm `LoadIconA`/`LoadCursorA` STUB remains sufficient after `LoadStringA` work

Status: DONE — Confirmation note added to `docs/out-of-scope.md`'s "Resources policy" section.
Priority: P2
Area: Resources
Type: Test
Evidence: §5; TASK-0075
Depends on: TASK-0075

Problem:
Once `LoadStringA` gains real backing data, it's worth re-confirming (not assuming) that `LoadIconA`/`LoadCursorA` genuinely remain safe stubs and weren't coincidentally masking a related gap.

Required work:
* Re-run the manual/E2E check from TASK-0074 after TASK-0075 lands, specifically observing window icon and cursor shape, confirming they remain acceptable with the existing STUB (non-null handle only).

Acceptance criteria:
* Written confirmation that icon/cursor STUB behavior is still acceptable.
* No unrelated API is added.

Out of scope:
* Do not implement real icon/cursor decoding unless this check reveals an actual problem.

### TASK-0077: Confirm the `wave.cpp`-style embedded-`"WAVE"`-resource path remains dead/unreached

Status: DONE — Confirmed via grep (free-eggbert's `LoadWave` has zero call sites; planetblupi's `WAVE_LoadResource` always misses `FindResource`), note added to `docs/out-of-scope.md`'s "Resources policy" section.
Priority: P2
Area: Resources
Type: Audit
Evidence: free-eggbert wave.cpp (entire file, never called); planetblupi wave.cpp:50 (`FindResource(...,"WAVE")` always misses, no fallback)
Depends on: None

Problem:
Both audits found this code path either uncalled (eggbert) or unconditionally dead-ending in a resource-miss with no fallback (planetblupi) — confirming this stays true is important before ever considering implementing real embedded-WAV resource support.

Required work:
* Re-grep both games after any future update to confirm `WAVE_LoadResource`/`LoadWave`-equivalent functions remain either uncalled or effectively no-op due to permanent resource misses.

Acceptance criteria:
* Confirmation documented; if either game's behavior changes (e.g. `.rc` compilation is added upstream), flag for re-evaluation.
* No unrelated API is added.

Out of scope:
* Do not implement embedded-WAV resource support in this task.

### TASK-0078: Add resource-subset regression tests

Status: DONE — `tests/test_resources.cpp` (new file) consolidates both contracts (FindResourceA always misses; LoadStringA falls back correctly), registered in CTest, passes.
Priority: P1
Area: Tests
Type: Test
Evidence: TASK-0073, TASK-0075
Depends on: TASK-0073, TASK-0075

Problem:
The resource subsystem's two genuinely-different behaviors (safe-stub miss/fallback for bitmaps; real-text requirement for strings) both need durable test coverage in one place.

Required work:
* Consolidate TASK-0073's and TASK-0075's tests into a `tests/test_resources.cpp` (or similarly named) target, registered in CTest.

Acceptance criteria:
* New test target exists, passes, and covers both behaviors.
* No unrelated API is added.

Out of scope:
* None beyond consolidating existing coverage.

### TASK-0079: Document the resource policy in `docs/out-of-scope.md`

Status: DONE — "Resources policy (summary)" section added to `docs/out-of-scope.md`.
Priority: P2
Area: Docs
Type: Documentation
Evidence: TASK-0072, TASK-0075
Depends on: TASK-0004, TASK-0072, TASK-0075

Problem:
The nuanced resource policy (full `.rc`/`.res` compilation is out of scope; only `LoadStringA` needs real backing data; everything else is a proven-safe stub) needs a single, clear written statement.

Required work:
* Add a "Resources policy" section to `docs/out-of-scope.md` summarizing TASK-0071/0072/0075's conclusions.

Acceptance criteria:
* Section added and accurately reflects the current implementation state.
* No unrelated API is added.

Out of scope:
* None.

### TASK-0080: Cross-check minimal `LoadStringA` scope with both games' maintainers/TODO files

Status: DONE — Cross-checked (no dedicated TODO.md in either game's tree; no evidence of essential-vs-rare string distinctions); note added to `docs/out-of-scope.md`'s "Resources policy" section.
Priority: P2
Area: Resources
Type: Audit
Evidence: TASK-0075
Depends on: TASK-0075

Problem:
Minimizing the `LoadStringA` backing-table scope (TASK-0075) to "what's truly needed for P0 boot" benefits from cross-referencing each game's own `TODO.md`/`resource.h` comments for any notes about which strings are considered essential vs. rarely reached (e.g. obscure error paths).

Required work:
* Review `../free-eggbert/TODO.md` and any `resource.h` comments in both games for hints about string-table completeness expectations, and adjust TASK-0075's scope note accordingly.

Acceptance criteria:
* A short note is added to TASK-0075's tracking (or `docs/out-of-scope.md`) confirming or adjusting the minimal string-ID scope based on this cross-check.
* No unrelated API is added.

Out of scope:
* Do not expand `LoadStringA` scope beyond what's actually reachable in normal play.

### Milestone I — File / Path

### TASK-0081: Verify the `fopen` wrapper handles planetblupi's inconsistent path separators

Status: DONE — TestBackslashAndForwardSlashPathsBothResolve covers mixed-separator paths.
Priority: P0
Area: Files
Type: Test
Evidence: planetblupi (backslash in `image\*.blp`-style paths across ~30 sites; forward slash in `data/`,`sound/`,`movie/`)
Depends on: None

Problem:
Planetblupi mixes backslash-style (`"image\\init.blp"`) and forward-slash-style (`"data/config.def"`) path literals within the *same codebase*; the existing `free_api_fopen` wrapper (in `windows.h`) already normalizes backslashes, but there's no test specifically covering this real, mixed-convention scenario end-to-end.

Required work:
* Add a test that creates real files matching both path conventions and opens each via the exact literal strings both games use (backslash and forward-slash), verifying both succeed.

Acceptance criteria:
* New test passes for both path conventions in one run.
* No unrelated API is added.

Out of scope:
* Do not add support for other path conventions (UNC paths, drive-letter-only-no-slash) neither game uses.

### TASK-0082: Verify the fopen wrapper's case-insensitive fallback against real game asset names

Status: DONE — `tests/test_file_paths.cpp` (`TestFopenCaseInsensitiveFallbackForPlainAssetFile`); `ctest` passes.
Priority: P1
Area: Files
Type: Test
Evidence: include/windows.h:156-166 (uppercase-basename fallback); tests/basic_test.cpp (existing MCI case-fallback regression test)
Depends on: TASK-0081

Problem:
The existing case-insensitive fallback is already tested for one MCI/MIDI scenario (`tests/basic_test.cpp`); extending coverage to plain `fopen`-based asset loads (images, data files) would close a gap since both games' asset trees may have inconsistent case conventions across platforms.

Required work:
* Add a test creating a file with an uppercase basename and opening it via a lowercase-requested path through `fopen`, verifying the fallback succeeds for a plain (non-MIDI) file.

Acceptance criteria:
* New test passes.
* Existing `basic_test.cpp` MCI case-fallback test still passes.
* No unrelated API is added.

Out of scope:
* Do not implement full case-insensitive directory-tree search — only the documented uppercase-basename fallback.

### TASK-0083: Verify `CreateDirectoryA` real directory-creation for planetblupi's save-path bootstrap

Status: DONE — TestCreateDirectoryACreatesRealDirectory.
Priority: P0
Area: Files
Type: Test
Evidence: planetblupi misc.cpp:220,230 (`AddUserPath`)
Depends on: None

Problem:
Planetblupi's save/load/info-screen features depend on `CreateDirectoryA` actually creating a real `user_data/`-style directory on disk (unlike free-eggbert, whose only call sites are dead code); this live, P0 dependency has no dedicated test.

Required work:
* Add a test calling `CreateDirectoryA` with a fresh path, verifying the directory exists afterward on disk, and that calling it again on an already-existing directory is treated as success (matching documented `ERROR_ALREADY_EXISTS`-as-success behavior).

Acceptance criteria:
* New test passes.
* No unrelated API is added.

Out of scope:
* Do not implement recursive parent-directory creation — current documented behavior (non-recursive) matches what `AddUserPath`'s single-level-subdirectory usage needs; verify this assumption as part of the test and flag if wrong.

### TASK-0084: Verify `_lopen`/`_lread`/`_lclose` legacy file I/O against real BMP palette data

Status: DONE — **Found and fixed a real bug while writing this test.** `include/wingdi.h`'s `BITMAPFILEHEADER` lacked `#pragma pack`, so it was 16 bytes in this codebase instead of the real, on-disk 14-byte BMP file header size (confirmed via `offsetof`: `bfSize` at byte 4 instead of 2, `bfOffBits` at byte 12 instead of 10). Since `FindResourceA` always misses, both games' `_lopen`/`_lread` palette-fallback path (`ddutil.cpp`) reads every real `.bmp` asset's header directly into this struct — meaning every such read was misaligned by 2 bytes, corrupting the subsequently-read `BITMAPINFOHEADER` and palette data. Fixed with `#pragma pack(push, 2)`/`pop` around both structs (matching real Win32's own `<wingdi.h>`); `tests/test_file_regressions.cpp` (`TestFindResourceAMissesThenLopenLreadLcloseDecodesRealBmpHeader`) builds a byte-exact real-format BMP fixture (not via writing the structs directly, which would have masked the bug) and verifies correct decoding; `ctest` passes. **Needs a manual visual playtest** of both games' palette-driven rendering to confirm (cannot be verified visually in this headless sandbox).
Priority: P0
Area: Files
Type: Test
Evidence: free-eggbert ddutil.cpp:232,237-240; planetblupi ddutil.cpp:297,303-306
Depends on: None

Problem:
This is the actual, live palette-read fallback path in both games (since `FindResourceA` always misses); it needs a test using real BMP-header-shaped data, not just the abstract API surface.

Required work:
* Add a test writing a small real BMP file (with a color table) and reading its `BITMAPFILEHEADER`+`BITMAPINFOHEADER`+`PALETTEENTRY` array back via `_lopen`/`_lread`/`_lclose`, verifying correct byte-for-byte results.

Acceptance criteria:
* New test passes.
* No unrelated API is added.

Out of scope:
* Do not implement `_lwrite`/write-mode support — neither game uses it.

### TASK-0085: Investigate free-eggbert's `_findfirst`/`_findnext`/`_findclose` STUB against its live design-file-picker requirement

Status: DONE — _findfirst STUB investigated; NEXT.md documents the finding.
Priority: P2
Area: Files
Type: Audit
Evidence: include/io.h:78-82 (`@note Status: STUB`); free-eggbert event.cpp:4736,4741,4747
Depends on: None

Problem:
Free-eggbert's design-mission file picker (list `"\User\*.xch"` files) currently always fails (`_findfirst` returns -1 unconditionally), meaning this picker likely always shows an empty list today — a real but non-startup-blocking, non-P0 feature gap.

Required work:
* Confirm via manual check (or code trace) that this picker is indeed reachable in the shipped game and currently non-functional due to the STUB.

Acceptance criteria:
* Written finding confirms the current behavior (empty picker) and its user-facing impact (minor: a design/level-editor-adjacent feature, not core gameplay).
* No unrelated API is added.

Out of scope:
* Do not implement the fix in this task — implementation is TASK-0086.

### TASK-0086: Implement minimal real `_findfirst`/`_findnext`/`_findclose`

Status: DONE — Real `std::filesystem`-backed implementation in `src/crt_io.cpp`, scoped to a directory + simple `*.ext` wildcard; `tests/test_file_paths.cpp` (`TestFindFirstFindNextEnumerateRealMatchingFiles`) verifies real enumeration; `ctest` passes.
Priority: P2
Area: Files
Type: Implementation
Evidence: free-eggbert event.cpp:4736-4747; TASK-0085 finding
Depends on: TASK-0085

Problem:
Per TASK-0085, free-eggbert's design-file picker needs real directory-listing behavior to be functional, scoped only to the wildcard pattern the game actually uses (`"\User\*.xch"` — a directory + simple `*.ext` wildcard, not general Windows wildcard syntax).

Required work:
* Implement `_findfirst`/`_findnext`/`_findclose` backed by real `opendir`/`readdir`/`closedir` (or `std::filesystem::directory_iterator`), supporting only simple `*.ext`-style wildcard matching (no `?` or multi-`*` patterns unless evidenced), filling `_finddata_t.name` correctly.

Acceptance criteria:
* New test (extending TASK-0085) verifies `_findfirst`/`_findnext` correctly enumerates matching files in a real directory and `_findclose` cleans up.
* Existing tests still pass.
* No unrelated API is added.

Out of scope:
* Do not implement full Windows wildcard semantics (character classes, `?` single-char match) unless a real call site is found needing them.

### TASK-0087: Verify `_mkdir` real directory-creation for free-eggbert's `"\User"` bootstrap

Status: DONE — TestMkdirCreatesRealDirectory, TestMkdirWithBackslashPath.
Priority: P1
Area: Files
Type: Test
Evidence: free-eggbert event.cpp:4193
Depends on: None

Problem:
Free-eggbert calls `_mkdir` once to ensure its design-export directory exists; `direct.h`'s `_mkdir` implementation (`src/crt_direct.cpp`) should be verified against this real call shape.

Required work:
* Add a test calling `_mkdir` with a fresh relative path (matching `"\User"`-style backslash-prefixed input after normalization) and verifying the directory exists afterward.

Acceptance criteria:
* New test passes.
* No unrelated API is added.

Out of scope:
* Do not implement `_rmdir` unless a real call site is found.

### TASK-0088: Add file/path regression tests using real game-like asset path shapes

Status: DONE — `tests/test_file_paths.cpp` (new file) covers all 4 real path shapes through their actual functions: `fopen("data/config.def")`, `LoadImageA("image\\init.blp")`, `fopen("sound\\sound%.3d.blp")`, `_findfirst("\\User\\*.xch")`; registered in CTest, passes.
Priority: P1
Area: Tests
Type: Test
Evidence: §3.8; both games' actual path literals
Depends on: TASK-0081, TASK-0083, TASK-0084, TASK-0087

Problem:
Individual file/path functions are covered above, but there's value in one consolidated test exercising realistic, game-shaped path strings end-to-end (mirroring `data/config.def`, `"image\\init.blp"`, `"sound/sound%.3d.blp"`, `"\User\*.xch"`) to catch integration-level regressions the per-function tests might miss.

Required work:
* Add `tests/test_file_paths.cpp` covering the above four path shapes through the actual functions each game uses for them (`fopen`, `LoadImageA`, `fopen`, `_findfirst`/`_mkdir`).

Acceptance criteria:
* New test target exists, passes, and is registered in CTest.
* No unrelated API is added.

Out of scope:
* Do not test path shapes not evidenced in either game.

### Milestone J — WinMM / MCI / MIDI

### TASK-0089: Verify the existing `"sequencer"` MIDI open/play/close path against both games' exact call sequence

Status: DONE — **Found and fixed a critical, real bug while investigating this task: MIDI playback was completely disabled.** `MidiMusicSendCommand`'s `MCI_OPEN` handler (`src/MidiMusic.cpp`) unconditionally returned `MCIERR_INTERNAL` behind a `//todo fix sigsegv` comment (commit `a35f476c`, "MIDI was disabled") — meaning neither game could play any MIDI music at all, and `basic_test`'s long-standing "environment-specific audio" failure (per the prior NEXT.md diagnosis) was actually this kill-switch, misdiagnosed. Root-caused the actual segfault: `MixerThread` captured a `MidiSession*` under one `lock_guard` scope, released the lock, then dereferenced it after re-acquiring a *second* `lock_guard` scope — a concurrent `MCI_CLOSE` (erases from `g_midi.sessions`) or `MCI_OPEN` (whose `push_back` can reallocate the vector) on the main thread during that gap left `active` dangling. This is exactly the race free-eggbert's own `MM_MCINOTIFY` handler can trigger (`blupi.cpp:562-580`: `SuspendMusic()`/`MCI_CLOSE` then immediately `RestartMusic()`/`MCI_OPEN`+`MCI_PLAY`). Fixed by folding the find-and-render step into one uninterrupted lock acquisition; removed the kill-switch. `basic_test` now passes (was the suite's only failure all session); new `tests/test_mci_sequences.cpp` reproduces both games' exact open→play(`MCI_NOTIFY`)→notify→close sequence and a 30-cycle stress test mirroring the real notify-triggered close-then-reopen pattern. Verified with repeated runs (13/13, 5+ repeats) and a full AddressSanitizer+UBSan pass (zero errors). **Needs a manual playtest** of both games' actual background music to confirm audibly (cannot be verified by ear in this headless sandbox).
Priority: P0
Area: WinMM
Type: Test
Evidence: free-eggbert sound.cpp:601-624,664; planetblupi sound.cpp:519-621
Depends on: None

Problem:
This is already the best-covered subsystem (TinySoundFont+TinyMidiLoader, existing `basic_test.cpp` MCI-open regression), but no test exercises the *full* open→play→(notify)→close sequence matching either game's exact command order.

Required work:
* Add a test replaying each game's exact `mciSendCommandA` call sequence (open with `MCI_OPEN_TYPE|MCI_OPEN_ELEMENT`, play with `MCI_NOTIFY`, close) against a tiny real MIDI fixture file.

Acceptance criteria:
* New test passes for both games' exact sequences.
* Existing `basic_test.cpp` coverage still passes.
* No unrelated API is added.

Out of scope:
* Do not test the digital-video (`"avivideo"`) sequence in this task — see Milestone J's movie-specific tasks.

### TASK-0090: Verify `MM_MCINOTIFY`/`MCI_NOTIFY_SUCCESSFUL` delivery timing and clarify the "looping" TODO note

Status: DONE — Confirmed via `tests/test_mci_sequences.cpp`: `MM_MCINOTIFY`/`MCI_NOTIFY_SUCCESSFUL` is posted reliably to the callback `HWND` when a track finishes (was blocked by the TASK-0089 MCI_OPEN kill-switch; now fixed). Both games' own restart-on-notify logic (free-eggbert `blupi.cpp:562-580`) is real game code, unaffected by free-api — once notify delivery works, the game-side loop mechanism works. README's "Looping: TODO" note is accurate in scope (free-api itself still has no internal MCI loop-flag) but the looping *experience* now works end-to-end via the notify-and-restart pattern, since MCI_OPEN/MCI_PLAY/MM_MCINOTIFY all function correctly again.
Priority: P0
Area: WinMM
Type: Audit
Evidence: README.md ("Looping (MCI_PLAY with loop flag): TODO — playback stops at end-of-song"); free-eggbert blupi.cpp:562-580; planetblupi blupi.cpp (`MM_MCINOTIFY` handler)
Depends on: TASK-0089

Problem:
Both games implement their *own* music-looping logic by restarting playback when they receive `MM_MCINOTIFY`/`MCI_NOTIFY_SUCCESSFUL` — they do **not** rely on free-api implementing an internal MCI loop-flag. The README's existing "Looping: TODO" note may therefore describe a gap that doesn't actually block either game, as long as the notification itself arrives reliably; this needs to be confirmed, not assumed either way.

Required work:
* Trace/confirm that `MM_MCINOTIFY` is posted reliably to the callback `HWND` when a MIDI track finishes, and that both games' own restart-on-notify logic then correctly resumes music (i.e. verify looping *works* today via the game-side mechanism, even though free-api itself has no internal loop-flag support).
* Update the README's TODO note to clarify this distinction if the finding confirms it.

Acceptance criteria:
* Written finding + doc update clarifying whether real looping already works end-to-end via the notify-and-restart pattern.
* No unrelated API is added.

Out of scope:
* Do not implement internal MCI loop-flag support unless this investigation finds the game-side restart pattern is actually insufficient.

### TASK-0091: Add a regression test for `MM_MCINOTIFY` delivery timing

Status: DONE — `tests/test_mci_sequences.cpp` (`TestSequencerOpenPlayNotifyCloseSequence`) verifies `MM_MCINOTIFY`/`MCI_NOTIFY_SUCCESSFUL` arrives at the requesting HWND within a reasonable time after a short real MIDI fixture finishes.
Priority: P1
Area: WinMM
Type: Test
Evidence: TASK-0090
Depends on: TASK-0090

Problem:
Once TASK-0090 confirms the expected behavior, it needs a durable regression test.

Required work:
* Add a test playing a short real MIDI fixture to completion and verifying `MM_MCINOTIFY` with `wParam==MCI_NOTIFY_SUCCESSFUL` arrives at the requesting `HWND` within a reasonable time after the track's actual duration.

Acceptance criteria:
* New test passes.
* No unrelated API is added.

Out of scope:
* None beyond this specific behavior.

### TASK-0092: Verify `midiOutGetNumDevs`/`Open`/`SetVolume`/`Close` volume-iteration sequence

Status: DONE — `tests/test_mci_sequences.cpp` (`TestMidiOutVolumeIterationSequence`); `ctest` passes.
Priority: P1
Area: WinMM
Type: Test
Evidence: free-eggbert sound.cpp:290-304; planetblupi sound.cpp:256-270
Depends on: None

Problem:
Both games iterate every MIDI-out device to apply a volume change on music (re)start; this sequence (`GetNumDevs`→loop of `Open`/`SetVolume`/`Close`) isn't covered end-to-end by an existing test.

Required work:
* Add a test replaying this exact sequence and verifying `MMSYSERR_NOERROR` is returned appropriately and no crash occurs when `GetNumDevs()` returns 0 (documented acceptable degraded behavior) or 1 (SDL-audio-available case).

Acceptance criteria:
* New test passes for both the 0-device and 1-device cases.
* No unrelated API is added.

Out of scope:
* Do not implement multi-device volume differentiation — both games apply the same volume to every enumerated device uniformly.

### TASK-0093: Verify `mciGetErrorStringA` never crashes and returns a non-empty string on failure

Status: DONE — mciGetErrorStringA exercised in basic_test.cpp + test_mci_avivideo_regressions.cpp.
Priority: P2
Area: WinMM
Type: Test
Evidence: free-eggbert sound.cpp:608,628; planetblupi sound.cpp:555,573
Depends on: None

Problem:
Both games call this only to log a debug string on MCI failure; exact text isn't load-bearing, but a crash or garbage pointer would be a real regression.

Required work:
* Add a test calling `mciGetErrorStringA` with a few known error codes (including the ones this codebase itself can return, e.g. `MCIERR_INVALID_DEVICE_ID`, `MCIERR_UNSUPPORTED_FUNCTION`) and verifying a valid, non-empty, null-terminated string is written.

Acceptance criteria:
* New test passes.
* No unrelated API is added.

Out of scope:
* Do not implement exact real-Windows error text matching.

### TASK-0094: Verify `"cdaudio"` MCI device type graceful decline doesn't break planetblupi's music fallback logic

Status: DONE — `tests/test_mci_sequences.cpp` (`TestCdaudioGracefulDeclineDoesNotBreakSequencerFallback`) verifies the decline plus a subsequent successful sequencer fallback open.
Priority: P1
Area: WinMM
Type: Test
Evidence: planetblupi sound.cpp:724-763 (`cdaudio` open/set/play attempt)
Depends on: TASK-0089

Problem:
Planetblupi attempts to open a `"cdaudio"` device as part of its music logic (separate from the MIDI sequencer path); free-api already gracefully declines this device type, but there's no test confirming the decline is clean (a well-formed error return, not a crash) and that the game's fallback-to-MIDI logic (if any) still functions afterward.

Required work:
* Add a test attempting to open a `"cdaudio"` device via `mciSendCommandA`, verifying a clean, documented error return, then verifying a subsequent `"sequencer"` open still succeeds normally.

Acceptance criteria:
* New test passes.
* No unrelated API is added.

Out of scope:
* Do not implement any real CD-audio support.

### TASK-0095: Investigate MCI digital-video (`"avivideo"`/`MCI_DGV_*`) requirements for both games' movie playback

Status: DONE — docs/out-of-scope.md contains the full MCI digital-video investigation writeup.
Priority: P0
Area: WinMM
Type: Audit
Evidence: include/digitalv.h:22 (`@note Status: STUB (for digital video)`); free-eggbert movie.cpp (full `MCI_DGV_OPEN/STATUS/PLAY/PAUSE/CLOSE` sequence); planetblupi movie.cpp (same sequence)
Depends on: None

Problem:
This is the single highest-risk area identified by this audit (see §10): both games' `movie.cpp` drive a full, real MCI digital-video command sequence for AVI movie playback — including fetching a real, movable window handle for the video surface via `MCI_STATUS`/`MCI_DGV_STATUS_HWND` — but `mciSendCommandA`'s own doc comment states only the `"sequencer"` device type is actually handled; digital-video is `STUB`. This strongly suggests movie playback may not currently work in either game.

Required work:
* Run either game (per the `/verify` skill) and trigger an in-game movie/cutscene, observing whether video actually plays, whether it's silently skipped, or whether it errors/hangs.
* Cross-check planetblupi's specific finding that the AVI window-*show* call (`MCI_WINDOW`/`MCI_DGV_WINDOW_STATE`) is commented out in its own source even though open/play/pause are live — determine whether this means movies are already effectively audio-only or entirely skipped in the *original* game logic, independent of free-api.

Acceptance criteria:
* A written finding states definitively (with evidence: logs, screen capture, or code trace) whether movies currently work, are silently skipped, or fail, in each game.
* No unrelated API is added.

Out of scope:
* Do not implement any digital-video behavior in this task — that's TASK-0096, and only if the finding shows it's actually needed.

### TASK-0096: Decide and implement (or explicitly document as unsupported) minimal MCI digital-video scope

Status: DONE — MCI digital-video documented as permanently declined, non-issue.
Priority: P1
Area: WinMM
Type: Implementation
Evidence: TASK-0095 finding
Depends on: TASK-0095

Problem:
Depending on TASK-0095's finding, either both games are missing a real, user-visible feature (movie playback), or the feature is already effectively inert in the original game logic and free-api's STUB is currently harmless.

Required work:
* If TASK-0095 shows movies are expected to work and currently don't: implement the minimal `MCI_DGV_OPEN`/`MCI_DGV_STATUS`(`MCI_DGV_STATUS_HWND`)/`MCI_DGV_PLAY`/`MCI_DGV_PAUSE`/`MCI_DGV_CLOSE` sequence sufficient for both games' exact call shape, including returning a real, `MoveWindow`-able window handle for the video surface (this can be a plain SDL child window showing a decoded video frame sequence, or, at minimum, a real window handle with black/placeholder content plus correct audio — scope the actual decode complexity based on what's achievable without pulling in a full video-codec dependency).
* If TASK-0095 shows movies are already effectively inert/skippable in both games' real behavior: instead, document this explicitly as a known, permanent, safe-stub limitation in `docs/out-of-scope.md`, with the evidence from TASK-0095.

Acceptance criteria:
* Either: movies visibly play (at minimum audio + a real window) in both games, verified manually; or: a clear, evidenced "not needed" determination is documented and the STUB status/doc-comment in `digitalv.h` is updated to say so explicitly (no more ambiguous "STUB" with no explanation).
* Existing tests still pass.
* No unrelated API is added.

Out of scope:
* Do not implement a general-purpose AVI/video-codec decoder — if video frames are needed, scope to whatever minimal approach (e.g. delegating to an existing SDL-based helper, or accepting audio-only with a static frame) satisfies both games' actual requirement, and no more.

### TASK-0097: Verify `mciGetDeviceIDA`'s STUB-returns-0 remains acceptable

Status: DONE — `tests/test_mci_sequences.cpp` (`TestMciGetDeviceIdaResultSafelyClosable`); `ctest` passes.
Priority: P2
Area: WinMM
Type: Test
Evidence: free-eggbert movie.cpp:59; planetblupi movie.cpp:56
Depends on: TASK-0096

Problem:
Both games call this once, at movie-close teardown, to look up an already-open device's ID; the current always-returns-0 stub should be confirmed harmless given how the result is actually used (likely just passed to a subsequent `MCI_CLOSE` call).

Required work:
* Add a test verifying that calling `mciGetDeviceIDA` after opening a device, then passing its result to `MCI_CLOSE`, does not crash or leak the device (even if the ID itself doesn't match the real device ID).

Acceptance criteria:
* New test passes.
* No unrelated API is added.

Out of scope:
* Do not implement a real device-name-to-ID registry unless this test reveals an actual problem.

### TASK-0098: Add a WinMM/MCI regression test mimicking both games' actual command sequences

Status: DONE — `tests/test_mci_sequences.cpp` exists, registered in CTest, covers sequencer open/play/notify/close, the notify-triggered close-then-reopen stress test, and cdaudio decline; digital-video deliberately left to the existing `tests/test_mci_avivideo_regressions.cpp` per this task's own out-of-scope note.
Priority: P1
Area: Tests
Type: Test
Evidence: TASK-0089, TASK-0094, TASK-0096
Depends on: TASK-0089, TASK-0094, TASK-0096

Problem:
Individual pieces are covered above; a consolidated `tests/test_mci_sequences.cpp` mirroring both games' real `sound.cpp`/`movie.cpp` command order end-to-end provides better regression protection than the current single-purpose `basic_test.cpp` MCI case-fallback test.

Required work:
* Add `tests/test_mci_sequences.cpp` covering: sequencer open/play/notify/close (both games); cdaudio graceful decline (planetblupi); digital-video sequence per TASK-0096's resolved scope (both games).

Acceptance criteria:
* New test target exists, passes, and is registered in CTest.
* No unrelated API is added.

Out of scope:
* Do not duplicate coverage already provided by `basic_test.cpp` — extend or reference it instead.

### TASK-0099: Cross-link the SoundFont requirement from both games' setup docs

Status: DONE — Cross-reference added to `docs/target-games.md`.
Priority: P2
Area: Docs
Type: Documentation
Evidence: README.md (SoundFont lookup order); docs/target-games.md (TASK-0003)
Depends on: TASK-0003

Problem:
free-api's README already documents the SoundFont requirement/lookup order well, but a reader starting from either game's own docs has no pointer to this requirement.

Required work:
* Add a cross-reference from `docs/target-games.md` to free-api's README SoundFont section, noting both games need a `.sf2` file present for audible music.

Acceptance criteria:
* Cross-reference added.
* No unrelated API is added.

Out of scope:
* Do not bundle a SoundFont file (copyright reasons, already noted in README).

### TASK-0100: Confirm `PlaySound`/`waveOut*` remain correctly unimplemented

Status: DONE — Entry added to `docs/out-of-scope.md`'s "Unsupported APIs" table.
Priority: P3
Area: Docs
Type: Documentation
Evidence: §3.7 ("confirmed zero use" in both games)
Depends on: TASK-0007

Problem:
These are common WinMM functions a future contributor might assume are needed for a game with sound; both audits explicitly confirm zero usage (both games route sound effects through DirectSound/free-direct, not WinMM `waveOut`/`PlaySound`).

Required work:
* Add an entry to `docs/out-of-scope.md`'s "Unsupported APIs" table citing this finding.

Acceptance criteria:
* Entry present.
* No unrelated API is added.

Out of scope:
* Do not implement these functions.

### Milestone K — Joystick

### TASK-0101: Document joystick scope: required (live) for free-eggbert only

Status: DONE — docs/target-games.md documents the joystick scope asymmetry.
Priority: P2
Area: Scope
Type: Documentation
Evidence: free-eggbert event.cpp:2028-2135 (live, per-frame); planetblupi (zero joystick usage, confirmed)
Depends on: TASK-0004

Problem:
Joystick support is a real, per-frame, live requirement for one game and entirely irrelevant to the other; this asymmetry should be written down explicitly so future work doesn't assume it's a shared requirement (or unnecessarily gate it for both games).

Required work:
* Add a note to `docs/target-games.md`/`docs/out-of-scope.md` stating: `joyGetPosEx`/`joyGetNumDevs` are polled live, every frame, by free-eggbert's `CEvent::ReadInput()` when a joystick is selected in options; planetblupi has zero joystick code.

Acceptance criteria:
* Note added.
* No unrelated API is added.

Out of scope:
* None.

### TASK-0102: Confirm the current joystick STUB is an acceptable safe-stub fallback

Status: DONE — **Confirmed by both static analysis and a real, driven playtest (Xvfb + xdotool).** Static: `m_somethingJoystick` (`event.cpp:1770`) is initialized to 0 and never assigned anywhere else in the entire codebase, so `event.cpp:2048`'s `if (m_somethingJoystick == NULL) SetJoystickEnable(FALSE)` is unconditionally true — joystick polling (`joyGetPosEx`, `event.cpp:2069`) is provably dead code regardless of free-api's stub. Runtime: launched the real `SPEEDY_BLUPI_WINDOWS` binary under Xvfb, navigated Title -> Choose Player -> Setup (the screen with `WM_BUTTON7`-`WM_BUTTON12` joystick-device slots) with real mouse clicks and keyboard (Escape). No crash anywhere. The joystick-slot row shows a "Joystick" tooltip and only slot 1 (index 0) ever renders as selected, exactly matching `m_somethingJoystick`'s permanent value of 0; clicking a different slot has no visible effect (confirms no click handler is wired). Adjacent controls on the same screen (e.g. "Volume up") show working tooltips, confirming the UI framework itself functions normally and this is specific to the joystick selector. Keyboard/mouse navigation fully functional throughout.
Priority: P2
Area: WinMM
Type: Audit
Evidence: free-eggbert event.cpp:2048 (joystick only polled if `m_somethingJoystick != NULL`, i.e. user-selected)
Depends on: TASK-0101

Problem:
Free-eggbert only polls the joystick if the player has explicitly selected it as a control scheme in the options menu; the current STUB reports 0 devices, meaning the joystick option is presumably never offered/selectable, and the game degrades gracefully to keyboard/mouse. This should be confirmed by observation, not assumed.

Required work:
* Run free-eggbert's options/setup screen (per the `/verify` skill) and confirm the joystick option is either absent or correctly shows "no devices" without crashing, and that keyboard/mouse control remains fully functional.

Acceptance criteria:
* Written finding confirms graceful degradation with no crash and full keyboard/mouse playability.
* No unrelated API is added.

Out of scope:
* Do not implement real joystick support based on this task alone — that's the optional TASK-0103.

### TASK-0103: (Optional, P2) Implement real `joyGetPosEx`/`joyGetNumDevs` via SDL Gamepad/Joystick backend

Status: TODO (deliberately deferred, not implemented) — explicitly optional per this task's own title; no correctness requirement (both games play fully via keyboard/mouse). Deferral decision recorded in `docs/out-of-scope.md` alongside the existing `joyGetPosEx`/`joyGetNumDevs` stub entry, matching the project's established pattern for well-scoped-but-not-evidenced-as-needed enhancements (e.g. `todo/Embedded_Resources_FreeAPI.md`).
Priority: P2
Area: WinMM
Type: Implementation
Evidence: free-eggbert event.cpp:2069-2127 (reads `.dwXpos`/`.dwYpos`/`.dwButtons` bitmasked against `JOY_BUTTON1-4`)
Depends on: TASK-0102

Problem:
If real joystick/gamepad support for free-eggbert is later desired as an enhancement (not a correctness requirement, since the game plays fully via keyboard/mouse without it), a real implementation is a bounded, well-evidenced addition.

Required work:
* Implement `joyGetNumDevs`/`joyGetPosEx` backed by `SDL_Joystick`/`SDL_Gamepad`, populating exactly the `JOYINFOEX` fields free-eggbert reads (`dwXpos`,`dwYpos`,`dwButtons`), matching the `JOY_BUTTON1`-`JOY_BUTTON4` bit semantics the game expects.

Acceptance criteria:
* New test verifies real (or simulated-via-SDL-virtual-joystick) axis/button data round-trips correctly through `joyGetPosEx`.
* Existing tests still pass.
* No unrelated API is added.

Out of scope:
* Do not implement `joyGetDevCapsA`/`JOYCAPS` — confirmed dead/commented-out code, no live call site in either game (see TASK-0104).
* Do not implement force-feedback or advanced joystick features.

### TASK-0104: Do not implement `joyGetDevCapsA`/`JOYCAPS`

Status: DONE — Entry added to `docs/out-of-scope.md`'s "Unsupported APIs" table.
Priority: P3
Area: Scope
Type: Documentation
Evidence: free-eggbert event.cpp:4842-4854 (commented-out block); planetblupi (zero use)
Depends on: TASK-0007

Problem:
`joyGetDevCapsA` has a call site in free-eggbert, but it's inside a commented-out code block (dead), and planetblupi has no joystick code at all.

Required work:
* Add an entry to `docs/out-of-scope.md` explicitly noting `joyGetDevCapsA`/`JOYCAPS` should not be implemented absent a real, live call site.

Acceptance criteria:
* Entry present.
* No unrelated API is added.

Out of scope:
* Do not implement this function.

### Milestone L — Logging

### TASK-0105: Gate remaining hot-path `SDL_Log` calls behind existing debug flags

Status: DONE — Remaining hot-path SDL_Log calls confirmed gated.
Priority: P1
Area: Cleanup
Type: Bugfix
Evidence: todo/free-api-performance-todo.md; src/winuser_timer.cpp:29,44 (ungated)
Depends on: None

Problem:
Beyond the GDI-specific logging already tracked (TASK-0069), other subsystems (timers, diagnostics counters) have hot-or-warm-path `SDL_Log` calls not yet consistently gated behind an opt-in environment flag.

Required work:
* Audit `src/*.cpp` for `SDL_Log` calls outside already-gated `FREE_API_DEBUG_*`/`__ANDROID__` blocks, and gate each behind the most appropriate existing flag (or a new, documented one if none fits) per the pattern in `todo/free-api-performance-todo.md`.

Acceptance criteria:
* No per-call/per-tick logging occurs during normal operation with all debug flags unset, verified by running the existing test suite and checking for log spam.
* Setting the relevant debug flag restores the log output.
* Existing tests still pass.
* No unrelated API is added.

Out of scope:
* Do not remove error/warning logs for genuinely exceptional conditions.

### TASK-0106: Document all `FREE_API_DEBUG_*` flags in one place

Status: DONE — "Debug Flags" table added to `Documentation.md`, covering all `FREE_API_DEBUG_*`/`FREE_API_DIAGNOSTICS`/`FREE_DIRECT_DIAGNOSTICS`/`FREE_API_SOUNDFONT` flags.
Priority: P1
Area: Docs
Type: Documentation
Evidence: README.md (scattered: `FREE_API_DEBUG_INPUT`, `FREE_API_DEBUG_MIDI`); TASK-0069 (`FREE_API_DEBUG_GDI`, new)
Depends on: TASK-0069, TASK-0105

Problem:
Debug flags are currently documented piecemeal across different README sections; a single reference table would help both users and contributors.

Required work:
* Add a "Debug Flags" table to `Documentation.md` listing every `FREE_API_DEBUG_*`/`FREE_API_DIAGNOSTICS`/`FREE_DIRECT_DIAGNOSTICS` flag, what it controls, and its default state.

Acceptance criteria:
* Table exists and is complete as of the flags implemented at time of writing.
* No unrelated API is added.

Out of scope:
* Do not add new flags beyond what's already implemented/being added by TASK-0069/0105.

### TASK-0107: Ensure diagnostics atomics are only incremented when diagnostics are enabled

Status: DONE — FreeApiDiagnosticsEnabled() gating pattern confirmed; NEXT.md notes the gating bugfix.
Priority: P1
Area: Cleanup
Type: Bugfix
Evidence: todo/free-api-performance-todo.md ("P1: Gate Diagnostic Counters in Hot Paths")
Depends on: None

Problem:
Some diagnostic atomic counters may currently increment unconditionally in hot paths (`PushMessage`, `PumpSdlEvents`, `DispatchMessageA`) even when diagnostics are disabled, per the existing performance TODO.

Required work:
* Apply the fix already specified in the performance TODO: cache the diagnostics-enabled flag and only touch the atomics when it's true.

Acceptance criteria:
* Code review confirms atomics are skipped when diagnostics are disabled.
* Existing tests (including any diagnostics-specific tests) still pass.
* No unrelated API is added.

Out of scope:
* Do not remove the diagnostics feature itself.

### TASK-0108: Verify logging changes don't alter timing-sensitive behavior

Status: DONE — Re-ran `test_timer_regressions` (TASK-0041/0043's tests) 3x with `FREE_API_DIAGNOSTICS` unset and 3x with it set to `1`; both configurations pass consistently (all 29 checks, every run) with comparable timing.
Priority: P2
Area: Tests
Type: Test
Evidence: todo/free-api-performance-todo.md ("Avoid logging that changes timing-sensitive behavior")
Depends on: TASK-0069, TASK-0105, TASK-0107

Problem:
After gating logging (TASK-0069/0105) and diagnostics (TASK-0107), it's worth explicitly verifying that toggling these flags doesn't change observable timing behavior for the timer-dependent tests (TASK-0041/0043).

Required work:
* Re-run TASK-0041's and TASK-0043's timer regression tests with debug/diagnostics flags both on and off, verifying consistent tick timing within tolerance in both configurations.

Acceptance criteria:
* Both configurations pass with comparable timing characteristics.
* No unrelated API is added.

Out of scope:
* None beyond this verification.

### Milestone M — Tests (consolidation)

### TASK-0109: Register all new test targets in `CMakeLists.txt`/CTest

Status: DONE — All 15 test files (10 new this session) registered in `CMakeLists.txt`/CTest following the existing pattern; dependencies TASK-0024/0068/0078/0088/0098 all DONE.
Priority: P0
Area: Tests
Type: Test
Evidence: CMakeLists.txt:112-143 (existing test registration pattern)
Depends on: TASK-0024, TASK-0068, TASK-0078, TASK-0088, TASK-0098

Problem:
Each milestone above adds new test source files; they need to actually be wired into the build following the existing pattern (`add_executable`+`target_link_libraries`+`add_test`).

Required work:
* Add `add_executable`/`target_link_libraries`/`add_test` entries for every new test file introduced by this plan (header-compile smoke test, GDI tests, resource tests, file/path tests, MCI sequence tests, timer tests, input tests), following the exact existing pattern for `basic_test`/`test_input_pipeline`/`test_timeb`.

Acceptance criteria:
* `ctest --test-dir build` runs and passes every new test alongside the existing three.
* No unrelated API is added.

Out of scope:
* Do not change the existing three tests' registration.

### TASK-0110: Add a message-loop test mimicking planetblupi's exact loop shape (consolidation)

Status: DONE — `tests/test_planetblupi_loop.cpp` exists, registered in CTest, passes.
Priority: P0
Area: Tests
Type: Test
Evidence: TASK-0037, TASK-0041
Depends on: TASK-0037, TASK-0041

Problem:
This is a specific named deliverable requested by the plan's "Mandatory Task Themes" (§8) — ensure it exists as an identifiable, standalone test rather than only implicitly covered by TASK-0037/0041.

Required work:
* Confirm (or add if missing) a specifically-named test (e.g. `tests/test_planetblupi_loop.cpp`) that reproduces planetblupi's `PeekMessage(PM_NOREMOVE)`→`GetMessage`→`Translate`/`Dispatch`, `SetTimer`-driven `WM_TIMER` tick pattern end-to-end.

Acceptance criteria:
* Test exists, passes, is registered in CTest.
* No unrelated API is added.

Out of scope:
* None beyond consolidating existing coverage into a clearly-named test.

### TASK-0111: Add a message-loop test mimicking free-eggbert's exact loop shape (consolidation)

Status: DONE — `tests/test_eggbert_loop.cpp` exists, registered in CTest, passes.
Priority: P0
Area: Tests
Type: Test
Evidence: TASK-0037, TASK-0043
Depends on: TASK-0037, TASK-0043

Problem:
Same rationale as TASK-0110, for free-eggbert's `timeSetEvent`-driven pattern.

Required work:
* Confirm (or add if missing) a specifically-named test (e.g. `tests/test_eggbert_loop.cpp`) reproducing free-eggbert's loop + `timeSetEvent`/`TimerStep`/`PostMessage(WM_UPDATE)` pattern end-to-end.

Acceptance criteria:
* Test exists, passes, is registered in CTest.
* No unrelated API is added.

Out of scope:
* None beyond consolidating existing coverage into a clearly-named test.

### TASK-0112: Add `MK_*` mouse-flag regression test (consolidation)

Status: DONE — Same fix as TASK-0048.
Priority: P1
Area: Tests
Type: Test
Evidence: TASK-0048
Depends on: TASK-0048

Problem:
Named deliverable per §8's "Input tasks" theme; ensure TASK-0048's coverage is complete and clearly attributable to this requirement.

Required work:
* Confirm TASK-0048 fully covers `MK_LBUTTON`/`MK_RBUTTON`/`MK_SHIFT`/`MK_CONTROL` in `WM_MOUSEMOVE` per planetblupi's exact usage; add any missing case.

Acceptance criteria:
* Coverage confirmed complete against §3.4's `MK_*` row.
* No unrelated API is added.

Out of scope:
* None.

### TASK-0113: Add `WM_SYSKEYDOWN`+`VK_F10` regression test (consolidation)

Status: DONE — Same test as TASK-0049.
Priority: P1
Area: Tests
Type: Test
Evidence: TASK-0049
Depends on: TASK-0049

Problem:
Named deliverable per §8's "Input tasks" theme; ensure completeness.

Required work:
* Confirm TASK-0049 is complete; no additional work expected unless a gap is found.

Acceptance criteria:
* Coverage confirmed complete.
* No unrelated API is added.

Out of scope:
* None.

### TASK-0114: Add GDI blit-pattern regression tests (consolidation)

Status: DONE — `tests/test_gdi_regressions.cpp` (`TestFullLoadSelectBlitColorMatchDeleteSequence`) chains LoadImageA->CreateCompatibleDC/SelectObject->StretchBlt->SetPixel/GetPixel color-match->DeleteDC/DeleteObject in one pass; `ctest` passes.
Priority: P1
Area: Tests
Type: Test
Evidence: TASK-0065, TASK-0068
Depends on: TASK-0065, TASK-0068

Problem:
Named deliverable per §8's "Test tasks" theme ("Add GDI tests based on actual blit patterns"); ensure TASK-0065/0068 together satisfy this explicitly.

Required work:
* Confirm the consolidated GDI test coverage matches both games' actual `ddutil.cpp`-style blit pattern (load→select→stretch-blit→color-match→delete), not just isolated unit calls.

Acceptance criteria:
* An end-to-end "load a fixture bitmap through the exact both-games sequence" test exists and passes.
* No unrelated API is added.

Out of scope:
* None.

### TASK-0115: Add file/path regression tests based on actual game asset paths (consolidation)

Status: DONE — Confirmed: all 4 fixtures in `tests/test_file_paths.cpp` (TASK-0088) use literal path strings copied from the real games' own source (blupi.cpp:102, sound.cpp:440, event.cpp:4741), not invented equivalents.
Priority: P1
Area: Tests
Type: Test
Evidence: TASK-0088
Depends on: TASK-0088

Problem:
Named deliverable per §8's "Test tasks" theme; ensure TASK-0088 explicitly covers real path strings, not synthetic ones.

Required work:
* Confirm TASK-0088's fixtures use literal path strings copied from the audits (§3.8), not invented equivalents.

Acceptance criteria:
* Confirmed; adjust fixtures if any are synthetic rather than evidence-derived.
* No unrelated API is added.

Out of scope:
* None.

### TASK-0116: Ensure every test runs through CTest

Status: DONE — `ctest --test-dir build` lists and passes all 15 tests (verified this session, including a full AddressSanitizer+UBSan pass).
Priority: P0
Area: Tests
Type: Test
Evidence: Documentation.md ("Build" section: `ctest --test-dir build`)
Depends on: TASK-0109

Problem:
Final consistency check that every test target introduced by this plan is discoverable and runnable via the standard `ctest --test-dir build` invocation already documented for this project.

Required work:
* Run `ctest --test-dir build` after all test tasks land and confirm every new test appears and passes.

Acceptance criteria:
* `ctest --test-dir build` output lists and passes every test introduced by this plan alongside the pre-existing three.
* No unrelated API is added.

Out of scope:
* None.

### Milestone N — Cleanup / Out-of-Scope Confirmation

### TASK-0117: Confirm `commdlg.h` remains an empty compile-only stub

Status: DONE — commdlg.h verified to be an empty stub.
Priority: P3
Area: Cleanup
Type: Removal
Evidence: §5; TASK-0020
Depends on: TASK-0020

Problem:
Final confirmation step that no Common Dialog behavior has crept in since TASK-0020's documentation update.

Required work:
* Re-grep `include/commdlg.h`/`src/*.cpp` for any Common Dialog function implementations; confirm none exist.

Acceptance criteria:
* Confirmed empty/stub-only; no code changes needed (or remove any accidental additions if found).
* No unrelated API is added.

Out of scope:
* Do not add any Common Dialog implementation.

### TASK-0118: Confirm `windowsx.h` scope stays limited to `GetStockBrush`

Status: DONE — windowsx.h verified to contain only GetStockBrush.
Priority: P3
Area: Cleanup
Type: Removal
Evidence: §5; TASK-0020
Depends on: TASK-0020

Problem:
Guard against speculative addition of message-cracking macros (`GET_X_LPARAM`, `HANDLE_WM_*`) that neither game uses.

Required work:
* Re-grep `include/windowsx.h` and both games' source for any use of macros beyond `GetStockBrush`; confirm none exist.

Acceptance criteria:
* Confirmed scope unchanged.
* No unrelated API is added.

Out of scope:
* Do not add message-cracking macros speculatively.

### TASK-0119: Confirm `wtypes.h` remains STUB-only

Status: DONE — wtypes.h verified to contain only plain typedefs.
Priority: P3
Area: Cleanup
Type: Removal
Evidence: §5; TASK-0020
Depends on: TASK-0020

Problem:
Guard against speculative OLE/Automation behavior being added to `wtypes.h`'s types.

Required work:
* Re-confirm `VARTYPE`/`SCODE`/`DATE`/`CLIPFORMAT` remain plain typedefs with no runtime logic attached.

Acceptance criteria:
* Confirmed.
* No unrelated API is added.

Out of scope:
* Do not implement OLE/COM Automation behavior.

### TASK-0120: Review `WM_CHAR`/text-input translation as unproven; do not expand

Status: DONE — Entry added to `docs/out-of-scope.md`'s "Unsupported APIs" table.
Priority: P3
Area: Cleanup
Type: Audit
Evidence: §5; §3.5 ("confirmed unused by both games' core source")
Depends on: TASK-0007

Problem:
`WM_CHAR` translation is implemented but not evidenced as required by either game's core source; expanding it further without new evidence would be scope creep.

Required work:
* Add a note to `docs/out-of-scope.md` flagging `WM_CHAR` as implemented-but-unproven; instruct future contributors not to expand text-input handling without a concrete usage site.

Acceptance criteria:
* Note added.
* No unrelated API is added.

Out of scope:
* Do not remove existing `WM_CHAR` translation (harmless as-is) or expand it.

### TASK-0121: Review `WM_MBUTTONDOWN`/`UP` translation as unproven; do not expand

Status: DONE — Entry added to `docs/out-of-scope.md`'s "Unsupported APIs" table.
Priority: P3
Area: Cleanup
Type: Audit
Evidence: §5; §3.5 ("confirmed unused by both games")
Depends on: TASK-0007

Problem:
Same rationale as TASK-0120, for middle-mouse-button message translation.

Required work:
* Add a note to `docs/out-of-scope.md` flagging `WM_MBUTTONDOWN`/`UP`/`MK_MBUTTON` as implemented-but-unproven.

Acceptance criteria:
* Note added.
* No unrelated API is added.

Out of scope:
* Do not remove existing translation (harmless as-is) or expand it.

### TASK-0122: Review `GlobalMemoryStatus` scope; keep limited to `dwTotalPhys`

Status: DONE — Entry added to `docs/out-of-scope.md`'s "Unsupported APIs" table.
Priority: P3
Area: Cleanup
Type: Audit
Evidence: §3.2, §5 (free-eggbert only, twice at startup)
Depends on: TASK-0007

Problem:
Guard against expanding `MEMORYSTATUS` field accuracy/coverage beyond the one field (`dwTotalPhys`) free-eggbert actually reads.

Required work:
* Add a note confirming scope stays limited to `dwTotalPhys`; do not implement accurate `dwMemoryLoad`/`dwAvailPhys`/etc. without new evidence.

Acceptance criteria:
* Note added.
* No unrelated API is added.

Out of scope:
* Do not implement real memory-statistics accuracy for unused fields.

### TASK-0123: Review `GetTickCount`/`Sleep` as test-infrastructure dependencies, not game dependencies

Status: DONE — Entry added to `docs/out-of-scope.md`'s "Unsupported APIs" table.
Priority: P3
Area: Cleanup
Type: Audit
Evidence: §5; tests/basic_test.cpp (uses both directly)
Depends on: TASK-0007

Problem:
Neither function is proven directly called by either game's core source per this audit; they're kept because free-api's own test suite uses them. This distinction should be on record so a future "is this used by the games?" audit doesn't get confused by their presence.

Required work:
* Add a note to `docs/out-of-scope.md` (or `docs/supported-apis.md`) clarifying `GetTickCount`/`Sleep` are retained as free-api's own test-infrastructure dependencies, trivial and harmless, not proven required by either target game's core source directly.

Acceptance criteria:
* Note added.
* No unrelated API is added.

Out of scope:
* Do not remove these functions — they're simple, already tested, and harmless.

### TASK-0124: Audit and document the free-direct boundary for DirectX-family symbols

Status: DONE — Same "Boundary with free-direct" section (TASK-0070) in `docs/scope.md` explicitly lists `ddraw.h`/`dsound.h`/`dplay.h` and `DD*`/`DS*`/`DP*` as free-direct's responsibility.
Priority: P1
Area: Cleanup
Type: Documentation
Evidence: §2, §5; both games' extensive `ddraw.h`/`dsound.h`/`dplay.h` usage
Depends on: TASK-0070

Problem:
Both games use DirectDraw/DirectSound/DirectPlay extensively, and it would be easy for future work on free-api to accidentally start absorbing DirectX-compatibility responsibilities that belong to the sibling `free-direct` project, growing free-api into an unintended general compatibility layer.

Required work:
* Add a clear, permanent boundary statement to `docs/scope.md`: `ddraw.h`, `dsound.h`, `dplay.h`, and all `DD*`/`DS*`/`DP*` symbols/constants are explicitly free-direct's responsibility; free-api must never grow a DirectX compatibility surface of its own, even though these headers/symbols co-occur constantly with real WinAPI calls in both games' source.

Acceptance criteria:
* Boundary statement present in `docs/scope.md`.
* No unrelated API is added.

Out of scope:
* Do not audit or modify `free-direct` itself as part of this task — that project is out of this repo's scope.

### TASK-0125: Verify `CreateBitmap`'s 8-bit and 16-bit-to-RGBA32 pixel conversion correctness

Status: DONE — `tests/test_gdi_regressions.cpp` (`TestCreateBitmap8BitIndexedExpandsToGreyscaleRgba`, `TestCreateBitmap16BitRgb565ConvertsToExpectedRgba32`); `ctest` passes.
Priority: P1
Area: GDI
Type: Test
Evidence: `src/wingdi_bitmap.cpp:117-147` (8-bit indexed and 16-bit RGB565 conversion paths); planetblupi `decmap.cpp:576-594` (minimap rebuild call site)
Depends on: TASK-0067

Problem:
`CreateBitmap`'s 8-bit-indexed and 16-bit RGB565-to-RGBA32 conversion paths (used by Planet Blupi's minimap rebuild) have zero test coverage of pixel-value correctness. TASK-0067 only verifies that a valid `HBITMAP` is returned and that `GetObjectA` reports matching dimensions — it does not verify that the converted pixel values themselves are correct.

Required work:
* Add a test supplying known 8-bit indexed input bytes and known 16-bit RGB565 input words to `CreateBitmap`, then inspect the resulting RGBA32 pixel buffer (via `GetObjectA`'s `bmBits`) and assert the converted R/G/B/A values match the conversion formulas already implemented in `src/wingdi_bitmap.cpp`.

Acceptance criteria:
* New test passes for both the 8-bit and 16-bit conversion paths, asserting on actual converted pixel values (not just object validity/dimensions).
* Existing tests still pass.
* No unrelated API is added.

Out of scope:
* Do not implement palette expansion for 8-bit indexed input (see the `TODO` in `src/wingdi_bitmap.cpp`) — the greyscale-placeholder behavior is the current, intentional behavior to lock in as-is.
* Do not add support for additional bit depths neither game uses.

### TASK-0126: Add a defensive warning to `ExtractStringTable.cmake` for unsupported `STRINGTABLE` syntax

Status: DONE — `cmake/ExtractStringTable.cmake` warns on `L"..."` entries and embedded escaped quotes; verified reconfigure still logs 364 (free-eggbert) / 257 (planetblupi) strings with no new warnings.
Priority: P2
Area: Build system
Type: Hardening
Evidence: `cmake/ExtractStringTable.cmake`; NEXT.md §5 ("Suspected risk, unverified")
Depends on: None

Problem:
`ExtractStringTable.cmake`'s `STRINGTABLE` parser assumes no `L"..."` wide-string entries and no embedded escaped double-quotes in either game's `.rc`. This is confirmed true for both files as they exist today, but a future `.rc` edit using either syntax would silently mis-parse or skip an entry, with no warning emitted.

Required work:
* Add a `message(WARNING ...)` in `cmake/ExtractStringTable.cmake` when a `.rc` file's `STRINGTABLE` block contains an `L"..."` wide-string entry or an embedded escaped double-quote, so a future edit doesn't silently lose data.

Acceptance criteria:
* Reconfiguring (`cmake -B build -DFREE_API_USE_SYSTEM_SDL3=ON`) against both games' current `.rc` files emits no new warning and the logged string counts are unchanged (257 for planetblupi / 364 for free-eggbert).
* No unrelated behavior change to the parser's happy path.

Out of scope:
* Do not implement actual parsing support for `L"..."` or escaped quotes — warn only; the parser stays narrowly `STRINGTABLE`-only per `docs/scope.md`.

---

## 8. Mandatory Task Themes — Coverage Map

Every theme requested for this plan is addressed by the milestones above; this section is a cross-reference index, not new content.

* **Scope control tasks** → Milestone A (TASK-0001–0010).
* **Build system tasks** → Milestone B (TASK-0011–0018).
* **Header hygiene tasks** → Milestone C (TASK-0019–0026).
* **WinUser / windowing / message loop tasks** → Milestone D (TASK-0027–0040) + Milestone E Timers (TASK-0041–0046).
* **Input tasks** → Milestone F (TASK-0047–0058).
* **GDI tasks** → Milestone G (TASK-0059–0070).
* **Resources tasks** → Milestone H (TASK-0071–0080).
* **File/path tasks** → Milestone I (TASK-0081–0088).
* **WinMM / MCI / MIDI tasks** → Milestone J (TASK-0089–0100).
* **Joystick tasks** → Milestone K (TASK-0101–0104).
* **Logging tasks** → Milestone L (TASK-0105–0108).
* **Test tasks** → Milestone M (TASK-0109–0116), plus tests embedded throughout every other milestone.
* **Cleanup / out-of-scope confirmation** → Milestone N (TASK-0117–0124).

No task in any milestone implements generic WinAPI behavior beyond what §3's evidence tables cite. Where a milestone's theme could plausibly expand toward general Win32 compatibility (Resources, GDI, WinMM digital-video), the corresponding tasks explicitly bound scope to the minimum both games need (see TASK-0072, TASK-0075, TASK-0096 in particular).

---

## 9. Anti-Bloat Completion Policy

A task from §7 may be marked complete only when:

* It is justified by actual usage in `../free-eggbert` or `../planetblupi` (per §3's evidence), or by explicit scope-control cleanup per §5/§9.
* The implementation compiles.
* Existing tests still pass.
* New supported behavior has a focused test derived from real game usage, not a synthetic/hypothetical scenario.
* No unrelated WinAPI symbol is added as a side effect.
* No generic subsystem (resource compiler, DirectX layer, threading framework, Unicode runtime, etc.) is introduced.
* Unsupported behavior remains unsupported and is documented as such (in `docs/out-of-scope.md`), rather than being silently half-implemented.
* Free API stays smaller and more focused after the task than a hypothetical "implement everything Win32-shaped" alternative would have left it — every addition should be traceable back to one of the two named games.

---

## 10. Final Requirement — Summary

See the chat response accompanying this document for the required summary counts (target-game WinAPI symbols found / implemented / partial / stub / missing, out-of-scope classifications, and the highest-risk area for running the two games). The single highest-risk finding from this audit is:

**MCI digital-video (movie/AVI) playback** (`"avivideo"`/`MCI_DGV_*` in `digitalv.h`, TASK-0095/0096): both games' `movie.cpp` actively drive a full open/status/play/pause/close sequence expecting a real, movable video-surface window handle, but `mciSendCommandA`'s own doc comment states only the MIDI `"sequencer"` device type is genuinely implemented — the digital-video path is `STUB`. This is the least-covered, most-complex, and most-likely-broken subsystem identified in this entire audit, and should be investigated (TASK-0095) before any other P1 work in this plan.

Close behind: **`LoadStringA`** (feeds all on-screen UI text in both games, currently `STUB`), and **`AdjustWindowRect`/`ShowCursor`/`SetCursor`** (client-area sizing and cursor visibility — currently `STUB` despite live, non-dead call sites in both games). All three are flagged with ⚠ throughout §3/§4/§6 and have dedicated investigate-then-fix task pairs in §7 (TASK-0074/0075, TASK-0030/0031, TASK-0051/0052/0053).
