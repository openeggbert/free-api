# 24-Hour Deep Audit — free-api

Produced during a user-approved 24-hour autonomous stabilization session
(see `plan.md` § "24-Hour Autonomous Stabilization Backlog" for the task
backlog this audit feeds). Repository state at start: commit `757164d`,
branch `develop`, working tree clean. Sibling repos audited as checked out
at session start: `../free-eggbert` (`develop`, commit `dae5652f`, with a
pre-existing uncommitted `DOC.md` deletion/`dxsdk3` submodule change left
untouched — confirmed by the user to be unrelated in-progress work,
migrated to `planetblupi.openeggbert.com`), `../planetblupi`
(`feature/free_direct`, commit `db61ffe7`, audited as-is per user
instruction), `../free-direct` (`develop`, commit `d466c440`, clean).

This audit was produced by six parallel, independent, from-scratch
research passes (not by trusting `plan.md`'s prior audit or any doc
blindly): (1) full `../free-eggbert` source usage sweep, (2) full
`../planetblupi` source usage sweep, (3) full public-header symbol
inventory (`include/`, `include_non_windows/`), (4) full `src/`
implementation-correctness audit across 29 subsystems, (5) `../free-direct`
bridge usage audit + MCI AVI feasibility research, (6) documentation/
`todo/` staleness check + `tests/` coverage inventory. Every claim below
cites `file:line` evidence from one of those passes; nothing here is
speculation.

---

## 1. Executive Verdict

**free-api is small and scoped correctly.** Two fully independent,
from-scratch source sweeps of both target games (not a re-read of the old
audit) confirm the same conclusion the prior audit reached: every public
symbol free-api implements traces to a real call site in one of the two
games, with the sole documented `free-direct`-bridge exception — which
this session found is itself slightly under-documented (see below).
Neither game was found to need any WinAPI surface free-api doesn't already
have a story for (implemented, stub, or explicitly declined).

**It is close to complete for the two target games.** Both games build,
link, and run against the current tree; 17/17 tests pass in all three
build modes (target-game-via-Ninja, target-game-via-Make, standalone).
The remaining gaps are narrow and already tracked (`plan.md`
`TASK-0001`–`TASK-0012` from the prior session) or newly surfaced by this
session's deeper implementation-correctness pass (§4–§5 below).

**Risky areas, ranked by what could actually bite a player or a future
maintainer:**

1. **`StretchBlt`'s scaled-path X/Y clamp asymmetry is a real, live,
   render-hot-path bug with zero regression coverage** — out-of-range
   source X is clamped to the nearest edge pixel, out-of-range source Y
   instead leaves the destination row untouched (`src/wingdi_blit.cpp:118-119`
   vs `:126`). Already tracked (`plan.md` `TASK-0003`), but this session's
   test-coverage sweep confirms no test exercises the OOB-source-Y case —
   this is the single highest-value P0 in the new backlog.
2. **The `free-direct` bridge surface is bigger, and less documented, than
   previously known.** Only `FreeApiRunWinMain` has a public header
   declaration; `FreeApiCreateSurfaceDC`/`FreeApiDestroySurfaceDC` (already
   tracked, `TASK-0002`) AND a previously-undocumented fourth function,
   `FreeApiSetWindowFullscreen` (`../free-direct/src/directdraw/DirectDraw.cpp:22,1149,1189,1191,1373`),
   are consumed by `free-direct` with zero header declaration anywhere.
   Separately, `free-direct` also reaches directly into an **internal**
   free-api implementation detail, `FreeApi::Platform::ReadRssKB`
   (`Diagnostics.cpp:18,72`), which isn't part of the bridge exception at
   all. Both are real, working, unenforced cross-repo coupling.
3. **`PeekMessageA` fully ignores its filter arguments**
   (`src/winuser_message.cpp:25-29`) and **`WaitMessage` isn't a true
   blocking wait** — it returns `TRUE` unconditionally after a single
   ~1ms delay even if the queue is still empty
   (`src/winuser_message.cpp:235-257`). Both are harmless today (neither
   game passes non-zero filters; both games' idle loops tolerate the
   short-poll semantics) but are latent traps if either assumption ever
   changes.
4. **`MK_SHIFT`/`MK_CONTROL` in planetblupi drive real gameplay
   (drag-select multi-unit highlight and level-editor flood-fill,
   respectively — two *different* features, correcting a prior
   assumption that both were the same drag-highlight feature) and have
   zero automated OR human test coverage.** SDL's headless keyboard-state
   query makes this genuinely hard to test automatically
   (`tests/test_winuser_regressions.cpp:27-37` documents why); this
   session found the exact keyboard path to reach live gameplay
   (`Enter × 4` from process launch, see §2) which should make a human
   playtest tractable next session.
5. **`RegisterClassExA`/`WNDCLASSEXA` do not exist anywhere in free-api.**
   Neither game uses them (both use the plain `WNDCLASSA`/`RegisterClassA`
   form), so this is not a gap today — but it means any future WinAPI
   surface expansion request for these should be scrutinized hard, since
   there's no existing partial implementation to extend.
6. Smaller, already-tracked or newly-found correctness/hygiene gaps:
   `_findfirst` session-table leak (`TASK-0009`), `GetDeviceCaps` ignoring
   its `index` argument entirely rather than just returning 0 for
   `SIZEPALETTE` (`src/wingdi_misc.cpp:5-15`), `CreateBitmap`'s
   self-documented 8-bit-greyscale-only gap (no real palette decode,
   `src/wingdi_bitmap.cpp:117-131`), four independent non-shared
   path-normalization implementations (`src/internal/FreeApiPath.cpp` ×2,
   `include/windows.h:140-176`, `src/MidiMusic.cpp:219-275`), and
   inconsistent logging gating (`winuser_window.cpp` and `LoadImageA`'s
   success path log unconditionally, unlike the rest of the codebase's
   disciplined `FreeApiDiagnosticsEnabled()`/`FreeApiGdiDebugEnabled()`
   gating).
7. Two docs are now measurably stale: `README.md`'s "Project Structure"
   section still describes `src/winapi.cpp` as the implementation
   (it's been an empty redirect-comment file since a May 2026 split into
   ~25 files) and overstates MIDI looping/`cdaudio` decline as open
   "TODO" limitations when both are confirmed-working/intentional;
   `Documentation.md`'s reading-order table has the same stale
   `winapi.cpp` claim.

**What should NOT be expanded**, reconfirmed by this session's dedicated
feasibility pass and the two independent game-usage sweeps:

* **MCI AVI/digital-video** — both games' own code already treats a
  missing AVI driver as a fully safe, silent, no-side-effect skip (traced
  exactly in both games' source this session, see §2/§5). There is no
  player-visible defect today. Implementing real playback would add a
  brand-new codec/video-decode subsystem (Cinepak + MS Video 1, confirmed
  from planetblupi's real `.avi` assets) that nothing else in free-api
  resembles — this session's feasibility research recommends declining
  permanently, unchanged from the existing decision.
* **Joystick beyond the current backend** — planetblupi never references
  joystick code at all (zero hits, confirmed this session); free-eggbert's
  real SDL-backed backend exists but is permanently unreachable at runtime
  (`m_somethingJoystick` is only ever assigned `0`, confirmed via
  exhaustive grep — no other assignment exists anywhere in the game's
  source).
* **General `.rc`/`.res` compilation, Common Dialogs, OLE/COM, real
  Unicode/`W` APIs, DirectDraw/DirectSound/DirectPlay inside free-api** —
  all reconfirmed unused/out-of-scope by both usage sweeps; DirectX-family
  headers are confirmed implemented only in the sibling `../free-direct`
  repo, not free-api, which is exactly the intended split.
* **`RegisterClassExA`/`WNDCLASSEXA`** — no evidenced need; do not add
  speculatively.

---

## 2. Target-Game Usage Re-Audit

Both games' source trees were re-scanned from scratch this session
(not by trusting old docs). Scope: all `.cpp`/`.h` under each game's
`src/`/`include/`, excluding build dirs, `third_party/`, and (for
free-eggbert) `dxsdk3/`/`bass/`/`android/`/`msvc5/`. A critical
build-configuration fact governs several rows below: **free-eggbert
hardcodes several `#define`s that dead-code entire call sites** —
`MMTIMER=TRUE` (`src/blupi.cpp:47`), `_CD=FALSE`, `_LEGACY=FALSE`,
`_BASS=FALSE`, `_DEMO=FALSE`, `_EGAMES=FALSE` (`include/def.hpp:6` family)
— none overridden by `CMakeLists.txt`. A textual grep alone would
over-report free-eggbert's live surface; every row below distinguishes
**live** from **dead code, compiled out**.

### 2.1 Used headers

| Header | free-eggbert | planetblupi | Evidence | Notes |
|---|---|---|---|---|
| `windows.h` | Yes | Yes | eggbert: `blupi.cpp:12` +10 sites; blupi: `fifo.cpp:4` +12 sites (some capitalized `Windows.h`) | both games' universal entry point |
| `windowsx.h` | Yes | Yes | eggbert: `blupi.cpp:13`; blupi: `blupi.cpp:15` | message-cracker macros; no confirmed direct macro call site in either game |
| `wtypes.h` | Yes | No | `blupi.cpp:14` | |
| `mmsystem.h` | Yes | Yes (transitive only) | eggbert: `blupi.cpp:16`; blupi: `sound.cpp` calls MCI/MIDI APIs but never `#include`s this header directly — reached transitively | flagged: planetblupi's `sound.cpp` usage is not self-declared |
| `digitalv.h` | Yes | Yes | eggbert: `movie.cpp:15`; blupi: `movie.cpp:12` | AVI/digital-video structs only |
| `commdlg.h` | Yes | Yes | eggbert: `movie.cpp:9`; blupi: `movie.cpp:6` | included, but no Common Dialog API actually called in either game |
| `direct.h` | Yes | Yes (included, unused) | eggbert: `event.cpp:11` (live, for `_mkdir`); blupi: `movie.cpp:10` (no `_mkdir`/`_chdir`/`_getcwd` call site found) | |
| `io.h` | Yes | No | `event.cpp:12,14` | for `_findfirst`/`_findnext` |
| `ddraw.h` | Yes | Yes | both games, multiple `.cpp` files | implemented by `../free-direct`, not free-api — correct split |
| `dsound.h` | Yes | Yes | both games | same — `../free-direct`'s responsibility |
| `dplay.h` | Yes | No (not found) | `network.hpp:5` | same — `../free-direct`'s responsibility |
| `winuser.h`/`wingdi.h`/`winbase.h` (direct) | No | No | — | both games only ever reach these transitively through `windows.h` |

### 2.2 Used WinAPI functions

Consolidated across both games; **Live** = reachable at runtime in the
actual compiled/linked build; **Dead** = present in source but compiled
out or structurally unreachable.

| Symbol | free-eggbert | planetblupi | Evidence | Required behavior | Current free-api status | Risk | Tests |
|---|---|---|---|---|---|---|---|
| `RegisterClassA` (via `RegisterClass` macro) | Live | Live | eggbert `blupi.cpp:728`; blupi `blupi.cpp:620` | register one `WNDCLASSA` | PARTIAL — stores only `WNDPROC`, drops other fields | Low | `test_winuser_regressions.cpp:119` |
| `RegisterClassExA` | — | — | not used by either | n/a | **not implemented at all** | Low (unused) | — |
| `CreateWindowExA`/`CreateWindowA` | Live | Live | eggbert `blupi.cpp:733,766`; blupi `blupi.cpp:625,654` | one window, fullscreen or windowed | PARTIAL | Low | `test_winuser_regressions.cpp:153` |
| `AdjustWindowRect` | Live (one branch; `_LEGACY` branch dead) | Live | eggbert `blupi.cpp:765`; blupi `blupi.cpp:650` | intentional identity transform | IMPLEMENTED (by design) | Low | covered incidentally |
| `ShowWindow`/`UpdateWindow`/`SetFocus` | Live | Live | eggbert `blupi.cpp:784-786`; blupi `blupi.cpp:677-679` | startup sequence | PARTIAL | Low | `test_winuser_regressions.cpp:559` |
| `DestroyWindow` | Live | Live (error path only) | eggbert `blupi.cpp:664`; blupi `blupi.cpp:585` | sync `WM_DESTROY` dispatch | PARTIAL, confirmed synchronous | Medium (multi-window fallback fragile) | `test_winuser_regressions.cpp:234` |
| `PeekMessageA`/`GetMessageA`/`TranslateMessage`/`DispatchMessageA` | Live | Live | main loops in both `blupi.cpp` | message pump | PeekMessageA IMPLEMENTED but ignores filters; GetMessageA PARTIAL (poll-based blocking) | **Medium** (filter-ignoring is a latent trap) | `test_winuser_regressions.cpp:268,311` |
| `WaitMessage` | Live | Live | eggbert `blupi.cpp:921`; blupi `blupi.cpp:919` | block until message | PARTIAL — not a true blocking wait, single ~1ms delay then unconditional `TRUE` | **Medium** | `test_winuser_regressions.cpp:631` |
| `DefWindowProcA` | Live | Live | eggbert `blupi.cpp:638`; blupi `blupi.cpp:568` | fallback + `WM_CLOSE`→`DestroyWindow` | PARTIAL | Low | `test_winuser_regressions.cpp:185` |
| `PostMessageA` | Live | Live | eggbert `blupi.cpp:647` (timer tick) +5; blupi `button.cpp:354` +5 | enqueue message | PARTIAL, mutex-protected, no validation | Low | `test_winuser_regressions.cpp:648` |
| `PostQuitMessage` | Live | Live | eggbert `blupi.cpp:633`; blupi `blupi.cpp:564` | post `WM_QUIT` | IMPLEMENTED | Low | `test_winuser_regressions.cpp:311` |
| `SetTimer`/`KillTimer` | **Dead** (`MMTIMER=TRUE` always) | **Live — sole frame pump** | eggbert `blupi.cpp:893,630` (dead); blupi `blupi.cpp:900,562` (live) | polling-timer, fires only inside `PeekMessageA`/`GetMessageA` | PARTIAL | Low | `test_timer_regressions.cpp`, `test_planetblupi_loop.cpp` |
| `timeSetEvent`/`timeKillEvent` | **Live — sole frame pump** | **Not used at all** | eggbert `blupi.cpp:891,628` | real SDL-timer-thread-backed periodic callback | PARTIAL | Low | `test_timer_regressions.cpp`, `test_eggbert_loop.cpp` |
| `GetClientRect` | Live | Live, **per-frame** | eggbert `pixmap.cpp:1499,1842` (occasional); blupi `pixmap.cpp:975` (inside `Display()`, every tick) | logical client size | PARTIAL | Low | incidental |
| `ClientToScreen`/`ScreenToClient` | Live, per-frame (mouse tracking) | Live, per-frame (blupi `pixmap.cpp:976-977` inside `Display()`) | eggbert `event.cpp:1866` +6; blupi `pixmap.cpp:136,976` +6 | window-position-aware coord translation | PARTIAL | Low | `test_winuser_regressions.cpp:463` |
| `GetCursorPos`/`SetCursorPos` | Live | Live | eggbert `blupi.cpp:598`; blupi `blupi.cpp:519` | via `SDL_GetGlobalMouseState`/`SDL_WarpMouseGlobal` | PARTIAL | Low | `test_winuser_regressions.cpp:540` |
| `ShowCursor`/`SetCursor` | Live | Live | eggbert `misc.cpp:48-60`; blupi `misc.cpp:56-70` | counter/handle-bookkeeping semantics | IMPLEMENTED/PARTIAL, both contracts confirmed correct | Low | `test_winuser_regressions.cpp:717,731` |
| `LoadCursorA`/`LoadIconA` | Live | Live | both, startup + per-sprite-change | non-null stub only | STUB (permanent, documented) | Low | `test_winuser_regressions.cpp:750` |
| `GetSystemMetrics` | Live | Live | both, startup | fixed `SM_CXSCREEN`/`CYSCREEN`/`CYCAPTION` | PARTIAL | Low | — |
| `SetWindowTextA` | Live | Live | eggbert `blupi.cpp:532,541`; blupi `blupi.cpp:453,462` | `WM_ACTIVATEAPP` title change | PARTIAL | Low | — |
| `MessageBoxA` | — | Live (fatal-init-error path only) | `blupi.cpp:582` | STUB display | STUB | Low | — |
| `InvalidateRect` | Live (movie only, unreachable — AVI always fails) | Live (movie only, same) | `movie.cpp` | STUB | STUB | Low (unreachable) | — |
| `LoadImageA` | Live | Live | eggbert `ddutil.cpp:46,49,94,97`; blupi `ddutil.cpp:90,95,148,151` | BMP-from-file only | PARTIAL | **Medium** (weaker path-normalization than other file APIs, §4.15) | incidental |
| `FindResourceA`/`LoadResource`/`LockResource` | Live | Live | eggbert `wave.cpp:52-53`,`ddutil.cpp:204-208`; blupi `wave.cpp:50-52`,`ddutil.cpp:268-271` | always-miss stub, both games fall back to file loading | STUB (permanent, documented, working end-to-end) | Low | `test_resources.cpp:35` |
| `CreateCompatibleDC`/`SelectObject`/`DeleteDC`/`DeleteObject` | Live | Live | `ddutil.cpp`/`pixmap.cpp` both games | in-memory DC/bitmap model | PARTIAL | Low | `test_gdi_regressions.cpp` |
| `GetDeviceCaps` | Live (`SIZEPALETTE` only) | Live (`SIZEPALETTE` only) | eggbert `pixmap.cpp:146,428`; blupi `pixmap.cpp:287` | 0 for `SIZEPALETTE` | IMPLEMENTED for that index; **ignores `index` entirely for every other value** | **Medium** | none found |
| `GetPixel`/`SetPixel` | Live | Live | both, `ddutil.cpp` | color-key matching | PARTIAL | Low | `test_gdi_regressions.cpp` |
| `StretchBlt` | Live | Live | eggbert `ddutil.cpp:162`; blupi `ddutil.cpp:224` | `SRCCOPY` scale/copy | PARTIAL — **confirmed X/Y clamp asymmetry bug**, see §4.16 | **High** | 1:1/clipped covered; OOB-source-Y scaled case **not covered** |
| `CreateBitmap` | — (not used) | Live | `decmap.cpp:578,582` | minimap generation | PARTIAL — 8-bit path is greyscale-only, self-documented gap | Medium | none found |
| `_lopen`/`_lread`/`_lclose` | Live | Live | eggbert `ddutil.cpp:232-240`; blupi `ddutil.cpp:297-306` | read-only file access | PARTIAL | Low | `test_file_regressions.cpp` |
| `_mkdir` | Live | — (not used; uses `CreateDirectoryA` instead) | `event.cpp:4193`, live call at `event.cpp:5610` | create `\User` dir | PARTIAL, path-normalization confirmed fixed | Low | `test_file_regressions.cpp` |
| `CreateDirectoryA` | **Dead** (`AddUserPath()` body gated `#if _CD\|\|_LEGACY`, both FALSE, though called live from 8 sites) | **Live** | eggbert `misc.cpp:185,194` (dead); blupi `misc.cpp:220,230` (live) | create save-path tree | PARTIAL, path-normalization confirmed fixed | Low | `test_file_regressions.cpp` |
| `DeleteFileA` | Live | Live | eggbert `event.cpp:5170`; blupi `event.cpp:4394` | demo-file cleanup | PARTIAL | Low | none found by name |
| `_findfirst`/`_findnext` | **Live** | **Not used at all** | `event.cpp:4741,4747` | `\User\*.xch` enumeration | IMPLEMENTED, but **`_findclose` never called** — confirmed leak | Medium (bounded, low-frequency) | happy-path only; leak scenario untested |
| `_findclose` | **Never called anywhere in free-eggbert** | n/a (unused) | exhaustive grep, zero hits | — | IMPLEMENTED (the leak is a caller-side gap, not a free-api defect) | see above | not exercised |
| `OutputDebugStringA` (+ `OutputDebugString` macro) | Live | Live | both, error paths + game's own `OutputDebug()` wrapper | ANSI debug text | IMPLEMENTED | Low | — |
| `midiOutGetNumDevs`/`Open`/`SetVolume`/`Close` | Live | Live | eggbert `sound.cpp:290-304`; blupi `sound.cpp:256-270` | device enumeration/volume | IMPLEMENTED | Low | `test_mci_sequences.cpp` |
| `mciSendCommandA` (MIDI/sequencer path) | Live | Live | eggbert `sound.cpp:601-761`; blupi `sound.cpp:524-610` | MIDI playback via `"sequencer"` device | PARTIAL, real TinySoundFont-backed playback | Low | `test_mci_sequences.cpp` |
| `mciSendCommandA` (AVI probe) | Live (always fails) | Live (always fails) | eggbert `movie.cpp:46` etc.; blupi `movie.cpp:29-45` etc. | graceful, safe decline | IMPLEMENTED (permanent decline) | Low, confirmed safe both games | `test_mci_avivideo_regressions.cpp` |
| `mciGetDeviceIDA` | Dead-reach (only if AVI opens, never does) | — | `movie.cpp:59` | — | STUB (always returns 0) | Low (unreachable) | — |
| `joyGetNumDevs` | Live (result unused) | — (not used) | `event.cpp:4841` | device count | IMPLEMENTED | Low | `test_joystick_regressions.cpp` |
| `joyGetPosEx` | **Dead-reach** (`m_somethingJoystick` always NULL) | — (not used) | `event.cpp:2071` | joystick state | IMPLEMENTED, real SDL backend | Low (unreachable) | `test_joystick_regressions.cpp` |
| `joyGetDevCapsA` | **Not even compiled** (inside a `/* */` comment block) | — | `event.cpp:4842-4853` | — | out of scope, no live call site anywhere | None | — |

### 2.3 Used WinAPI struct/type references

| Type | free-eggbert | planetblupi | Notes |
|---|---|---|---|
| `MSG` | Yes | Yes | main-loop local |
| `WNDCLASSA` | Yes | Yes | **`WNDCLASSEXA` used by neither** |
| `RECT`, `POINT` | Yes, pervasive | Yes, pervasive | planetblupi reuses `POINT` as its general cell/position type, far beyond mouse coords |
| `BITMAPFILEHEADER`/`BITMAPINFOHEADER` | Yes | Yes | `_lread`-based bitmap fallback path |
| `CREATESTRUCTA`/`LPCREATESTRUCT` | Yes | Yes | `WM_CREATE` handler |
| `PAINTSTRUCT` | No | No | **confirmed unused by both** — neither game uses `WM_PAINT`/`BeginPaint`/`EndPaint` at all |
| `SECURITY_ATTRIBUTES` | Dead (inside dead `AddUserPath` body) | **Live** | blupi's `CreateDirectoryA` call actually uses it |
| `MEMORYSTATUS` | Yes | No | `ReadConfig`/`Benchmark` |
| `JOYINFOEX` | Dead-reach | No | see §2.2 |
| `MCI_DGV_*` struct family (AVI) | Yes, always-fails path | Yes, always-fails path | see §2.7 |
| `MCI_OPEN_PARMS`/`MCI_PLAY_PARMS`/`MCI_STATUS_PARMS` (sequencer) | Yes, live | Yes, live | MIDI playback |

### 2.4 Used constants/macros

| Category | free-eggbert | planetblupi | Notes |
|---|---|---|---|
| `VK_*` | F1-F12, END, ESCAPE, RETURN, SHIFT, CONTROL, PAUSE, LEFT/UP/RIGHT/DOWN, HOME, SPACE | F1-F12 (some dead, `#if 0`), END, ESCAPE, RETURN, LEFT/RIGHT/UP/DOWN, HOME, SPACE, PAUSE, CONTROL, `'A'`-`'Z'` | both games' full VK_* usage is covered by free-api's declared subset |
| `MK_*` | **None used at all** | `MK_SHIFT` (drag-select, `event.cpp:3440,3472,3504`), `MK_CONTROL` (level-editor flood-fill, `event.cpp:3843,3877,3908` — **different feature**, corrects a prior assumption these were the same), `MK_RBUTTON`/`MK_LBUTTON` | see §1 risk #4 |
| `SM_CXSCREEN`/`CYSCREEN`/`CYCAPTION` | Yes | Yes | startup only |
| `WS_*` style flags | Yes (popup/visible/caption/overlappedwindow) | Yes | `CreateWindowExA` |
| `PM_NOREMOVE`/`PM_REMOVE` | Yes | Yes | `PeekMessageA` |
| `MCI_OPEN_TYPE`/`MCI_OPEN_ELEMENT`/`MCI_NOTIFY`/`MCI_WAIT` | Yes | Yes | MCI |
| `MCI_DGV_OPEN_PARENT`/`OPEN_WS`/`STATUS_HWND`/`PLAY_REVERSE` | Yes (AVI, always-fails path) | Yes (same) | dead-weight structs, kept only because the probe call shape references them |

### 2.5 Used message IDs (real WinAPI `WM_*`, not app-custom `WM_USER+N` codes)

Both games handle the same core set:
`WM_CREATE`, `WM_DESTROY`, `WM_ACTIVATEAPP`, `WM_SYSCOLORCHANGE`,
`WM_QUERYNEWPALETTE`, `WM_PALETTECHANGED`, `WM_DISPLAYCHANGE`,
`MM_MCINOTIFY`, `WM_SETCURSOR`, `WM_LBUTTONDOWN/UP`, `WM_RBUTTONDOWN/UP`,
`WM_MOUSEMOVE`, `WM_KEYDOWN/UP`, `WM_SYSKEYDOWN/UP` (F10-only special
case, remapped to plain KEYDOWN/UP in both games), `WM_NCMOUSEMOVE`
(planetblupi only), `WM_CLOSE` (posted, not case-handled, in both).
`WM_TIMER`: **case label present in both games' `WndProc`, but only
actually fires in planetblupi** — in free-eggbert it's structurally
unreachable dead code because `SetTimer` is never called (see §2.2).
Every `WM_PHASE_*`/`WM_BUTTON*`/`WM_ACTION_*`/`WM_DECOR*`/`WM_UPDATE`-style
symbol in either game is an **application-defined** `WM_USER`-offset
constant defined in the game's own headers — not part of free-api's
surface at all (only `WM_USER` itself needs to be correct, which it is).

### 2.6 Used GDI behavior

Both games route all bitmap loading through a DirectDraw-era
compatibility layer (`ddutil.cpp` in both), which itself calls free-api's
GDI subset (`LoadImageA`→`SDL_LoadBMP`, `StretchBlt`, `GetPixel`/`SetPixel`
for color-keying, `CreateCompatibleDC`/`SelectObject`/`DeleteDC`/
`DeleteObject` for scratch DCs). planetblupi additionally uses
`CreateBitmap` for its minimap (`decmap.cpp:578,582` — the 8-bit-greyscale
gap, §4.19, is a real risk here if the minimap's source data is
palette-indexed with non-greyscale colors). Real on-disk `BITMAPFILEHEADER`/
`BITMAPINFOHEADER` layout matters for the `_lread`-based fallback path
in both games (`#pragma pack(push, 2)`, confirmed still correct this
session, §4.14).

### 2.7 Used WinMM/MCI behavior

Both games probe `"avivideo"` at startup (`CMovie::initAVI()`, near-identical
code in both games' `movie.cpp`) and gracefully, silently skip cutscenes
when it's unavailable — traced end-to-end this session in both games'
source (§1 risk assessment, §5 feasibility). Both games separately use
MCI's `"sequencer"` device type for real MIDI playback via `sound.cpp`
(unrelated to the AVI path) — this is live, working, and covered by
`test_mci_sequences.cpp`. Neither game opens a `"cdaudio"` MCI device
(confirmed via grep in both — planetblupi's is truly absent; free-eggbert's
only CD-audio-adjacent code lives in the entirely-dead `soundbass.cpp`).

### 2.8 Used file/path behavior

The two games diverge here more than expected: free-eggbert uses
`_findfirst`/`_findnext` (leaked, no `_findclose`, §2.2) for its
design-mission picker and `_mkdir` (not `CreateDirectoryA`, which is dead
code in free-eggbert specifically) for its `\User` directory; planetblupi
uses **the opposite pair** — live `CreateDirectoryA` for its save-path
tree and **no file-enumeration API at all** (all its data files are
opened via deterministic constructed filenames like `world%.3d.blp`).
Both use `_lopen`/`_lread`/`_lclose` for a legacy bitmap-file fallback
path, and both use plain `fopen` (routed through free-api's
`free_api_fopen` normalization wrapper) for everything else.

### 2.9 Used resource/string behavior

Both games funnel every `LoadString` call through a game-local 3-argument
wrapper (`misc.{cpp,hpp}` in both) that calls the real 4-argument
`LoadStringA`. free-eggbert has ~100+ call sites across a 100–3202 ID range
(some via raw numeric literals rather than named `TX_*` constants);
planetblupi has 30+ call sites and, uniquely, this session **precisely
reconciled** its 257-unique-used-ID figure: 239 IDs referenced by literal
name plus 18 more reached only via offset arithmetic (`TX_ACTION_GO+rank`,
`TX_LOST1+GetWorld()%5`, etc.) — 239+18 = exactly 257, an exact match
against every `STRINGTABLE` entry in all three of planetblupi's `.rc`
files (English/German/French, all 257 entries, zero duplicates). Both
games also use `FindResourceA`/`LoadResource` for non-string assets
(`RT_BITMAP`, and a custom `"WAVE"` resource type) — both always miss
under free-api (by design) and both games fall back to file-based
loading, confirmed working end-to-end.

### 2.10 Used joystick behavior

**planetblupi: none — zero references anywhere, confirmed by exhaustive
case-insensitive grep.** free-eggbert has real joystick-adjacent code
(`joyGetNumDevs`, `joyGetPosEx`, a `JOYINFOEX` struct, a `m_somethingJoystick`
gate) but it is **structurally unreachable at runtime**: the gate flag is
assigned `0` exactly once (its constructor) and never reassigned anywhere
else in the entire codebase, and the one function that would consume
`joyGetDevCapsA`'s result is inside a literal `/* ... */` C-style comment
block, not even compiled. free-api's real SDL-backed joystick
implementation is therefore, from both games' actual runtime perspective,
dead weight kept alive only to satisfy free-eggbert's link-time symbol
requirement.

---

## 3. Public Surface Audit

Full inventory of every symbol declared in `include/` (23 files) and
`include_non_windows/` (2 files, both trivial case-insensitive-filesystem
shims). ~25 files, several hundred symbols total. Grouped by header;
**Decision** column reflects this session's finding, cross-referenced
against §2's usage sweep.

**Documentation convention:** most files use a
`/** @brief ... @note Status: {STUB|PARTIAL|IMPLEMENTED|HEADER_ONLY} */`
comment per declaration. Seven files have **no** such annotations at all:
`basestd.h`, `mciapi.h`, `minwindef.h`, `winerror.h`,
`include_non_windows/sys/timeb.h`, `include_non_windows/Windows.h`,
`include_non_windows/WinUser.h`. The last three are trivial (foundational
typedefs or pure `#include` shims), but `winerror.h` and `mciapi.h` sit
alongside fully-annotated siblings and should be normalized (new backlog
task, P3).

**SDL-leak check: clean.** `grep -rn "SDL_" include/ include_non_windows/`
returns only comments/prose (e.g. `include/windows.h:30`'s own
`@note This header must not expose SDL types.`) — no `SDL_*` type appears
in any declaration. The internal (non-public) `src/internal/FreeApiGdi.hpp`
does forward-declare `SDL_Surface`, but it lives outside `include/`, so
the public-header rule holds.

### 3.1 `basestd.h` — foundational integer-pointer typedefs

`INT_PTR`/`PINT_PTR`/`UINT_PTR`/`PUINT_PTR`/`LONG_PTR`/`PLONG_PTR`/
`ULONG_PTR`/`PULONG_PTR`/`DWORD_PTR`/`PDWORD_PTR` — all **Keep**
(foundational, pervasively used). Missing `@note Status:` — **Document**
(P3).

### 3.2 `commdlg.h` — empty stub

No symbols; pure `#include <windows.h>` compatibility shim so legacy
`#include <commdlg.h>` sites compile (both games include it, neither
calls any Common Dialog API). **Keep as-is** — this is the correct,
minimal shape for a permanently out-of-scope header.

### 3.3 `debugapi.h`

| Symbol | Required by eggbert | Required by blupi | free-direct | Test/compile-only | Status | Decision |
|---|---|---|---|---|---|---|
| `OutputDebugStringA` | Yes | Yes | — | — | IMPLEMENTED | Keep |
| `OutputDebugStringW` | No | No | — | — | IMPLEMENTED | **Scope risk** — real `W`-suffixed function despite project's documented ANSI-only orientation and zero evidenced call site in either game. Backlog: verify zero real callers, then downgrade to a documented stub or keep-with-explicit-justification-note (P2). |
| `OutputDebugString` (macro) | Yes (via macro) | Yes | — | — | — | Keep |

### 3.4 `digitalv.h` — MCI Digital Video / sequencer subset

All `MCI_OPEN`/`CLOSE`/`PLAY`/`STATUS`/`PAUSE`/`NOTIFY`/`WAIT`/
`OPEN_TYPE`/`OPEN_ELEMENT`/`STATUS_ITEM` command/flag constants: **Keep**
(live, sequencer path). All `MCI_DGV_*` structs/constants
(`MCI_DGV_OPEN_PARMSA`, `WINDOW_PARMSA`, `STATUS_PARMSA`, `PLAY_PARMS`,
`PAUSE_PARMS`, `OPEN_PARENT`, `OPEN_WS`, `STATUS_HWND`, `PLAY_REVERSE`):
**Keep** — required only because both games' `initAVI()` call shape
references them at compile time, even though the runtime path always
fails immediately; removing them would break compilation, not add scope.
`mciSendCommandA`: **Keep** (live). `mciGetDeviceIDA`: **Keep, document
as permanently STUB** (dead-reach only, always returns 0). Duplicate
declarations vs `mmsystem.h` (`mciSendCommandA`, `MCI_OPEN`/`CLOSE`/
`PLAY`/`NOTIFY`/`WAIT`/`OPEN_TYPE`/`OPEN_ELEMENT`, `MCI_OPEN_PARMS`/
`MCI_PLAY_PARMS` include-order-fragile aliasing): **Fix** — consolidate
to a single definition (P2, new backlog task; see §5 risk list).

### 3.5 `direct.h`

`_chdir`, `_getcwd`: **zero call sites in either game** — Decision:
**document as intentionally-unused-but-kept** (per user's confirmed
default: keep+document, no removal without stronger evidence). `_mkdir`:
**Keep** (live, free-eggbert).

### 3.6 `handleapi.h`

`CloseHandle`: no call site in either game (test-only, per
`docs/supported-apis.md`'s already-accepted pattern). **Keep, document**
(covered by existing `TASK-0005` in prior backlog).

### 3.7 `io.h`

`F_OK`/`X_OK`/`W_OK`/`R_OK`, `_access`/`access` (Windows-only declarations):
test-only. `_lopen`/`_lread`/`_lclose`: **Keep** (live both games) — **but
duplicated verbatim in `winbase.h`, fix** (P2). `_finddata_t`/`_findfirst`/
`_findnext`/`_findclose`: **Keep** (live free-eggbert; `_findclose` itself
has zero real callers in either game, kept for API completeness/future
correctness of any caller that *does* call it, and used by free-api's own
test suite).

### 3.8 `mciapi.h`

`MCIDEVICEID`: **duplicate typedef, identical to `mmsystem.h`'s** — **Fix**
(consolidate, P2). Missing `@note Status:` — **Document** (P3).

### 3.9 `minwindef.h` — foundational Win32 scalar/handle types

Nearly all entries **Keep** (pervasively required). Two flagged as scope
risks: `HFONT`, `HPALETTE` — **no function anywhere takes or returns
either; no font/palette-creation API exists at all.** Decision: **Keep,
document as vestigial-but-harmless** (they cost nothing at runtime and
removing them risks an unnoticed compile break in `free-direct` or a
future game; downgrade to "remove later" only if a stronger signal
emerges). Missing `@note Status:` — **Document** (P3).

### 3.10 `mmiscapi2.h`

`LPTIMECALLBACK`: **Keep** (live, `timeSetEvent`).

### 3.11 `mmsystem.h` — WinMM compatibility subset

Bulk of the WinMM surface: **Keep**, all traced to live or documented-dead
call sites in §2. Notable: `JOYINFOEX`/`JOY_BUTTON1-4`/`JOYERR_*`: **Keep**
(free-eggbert link-time requirement, runtime-dead — see §2.10). Duplicate
`MCIDEVICEID`/`mciSendCommandA`/several `MCI_*` constants vs `digitalv.h`:
same **Fix** as §3.4/§3.8.

### 3.12 `rpcndr.h` / `wtypes.h`

`byte` macro: **duplicate, identical, in both files** — **Fix** (P3,
trivial consolidation). `wtypes.h`'s `VARTYPE`/`SCODE`/`DATE`/
`CLIPFORMAT`: header's own comment already says "proven unused... at
runtime" — **Keep as documented permanent stub**, correct as-is.

### 3.13 `synchapi.h` / `sysinfoapi.h`

`Sleep`, `GetTickCount`: test-only (no call site in either game's own
logic — `GetTickCount` specifically confirmed zero hits in free-eggbert,
which uses SDL's own tick function instead). **Keep, document** (existing
`TASK-0005` pattern).

### 3.14 `winbase.h`

`ZeroMemory`/`FillMemory`/`CopyMemory`: utility macros, **Keep**.
`_SECURITY_ATTRIBUTES`/`MEMORYSTATUS`/`GlobalMemoryStatus`: **Keep**, live
in at least one game each. `FreeResource`/`LockResource`/`UnlockResource`/
`LoadResource`/`SizeofResource`/`FindResourceA`/`GetModuleHandleA`: **Keep,
documented permanent stub** (§2.9, working-as-designed miss-then-fallback).
`_lopen`/`_lread`/`_lclose`: **duplicate of `io.h`, Fix** (see §3.7).
`CreateDirectoryA`/`RemoveDirectoryA`/`DeleteFileA`: **Keep**, live/tested.
`SetEnvironmentVariableA`/`GetLastError`/`SetLastError`: test-only, **Keep,
document**.

### 3.15 `windef.h`

`COLORREF`, `RECT`, `POINT`, `MAKELONG`/`LOWORD`/`HIWORD`: all **Keep**,
pervasively required by both games.

### 3.16 `windows.h` — central aggregator

`FreeApiRunWinMain`: the one bridge symbol that **does** have a public
header declaration (`include/windows.h:99`). `free_api_fopen`/the global
`#define fopen free_api_fopen`: **Keep**, but flagged — this silently
redefines the standard-library `fopen` symbol for every C++ translation
unit that includes `<windows.h>`, a broad-reaching mechanism for a
narrow path-normalization need. **Document explicitly** in
`docs/headers.md` why this shape was chosen (P2, new backlog task) rather
than changing it — it works and is tested, but its blast radius deserves
an explicit rationale note for future maintainers.

### 3.17 `windowsx.h`

Only real symbol: `GetStockBrush` (both games' `wc.hbrBackground`).
Message-cracking macros are otherwise stubs. **Keep.**

### 3.18 `winerror.h`

`E_FAIL`/`ERROR_ALREADY_EXISTS`/`ERROR_INVALID_PARAMETER`: **Keep**, all
used (`ERROR_ALREADY_EXISTS` by `CreateDirectoryA`'s already-exists path).
Missing `@note Status:` — **Document** (P3).

### 3.19 `wingdi.h`

Full GDI struct/function set: **Keep**, all traced to §2.6 usage. `HPALETTE`-
adjacent `PALETTEENTRY`/`GetSystemPaletteEntries`: **Keep**, live
(grayscale-approximation, documented).

### 3.20 `winnt.h`

Foundational char/string typedefs: mostly **Keep** (pervasive). Flagged
as genuinely vestigial, zero usage anywhere in `include/`: `HFONT`/
`HPALETTE` (already covered §3.9), `IUnknown`/`GUID`/`LPGUID`/`LPCGUID`/
`IID`/`LPIID`/`REFIID`/`CLSID`/`LPCLSID`/`REFCLSID` (header's own comment:
"no COM behavior implemented"), `PSZ`, `PVOID`, `BOOLEAN`,
`LPCH`/`PCH`/`NPSTR`, `LPWCH`/`PWCH`/`NWPSTR`/`PWCHAR`/`PWSTR`. Decision
for the COM-family types: **Keep, document as permanently-vestigial**
(same reasoning as `wtypes.h` — cheap, harmless, and a documented decision
beats a silent removal that might break an unknown downstream consumer).
The scalar-typedef stragglers (`PSZ`,`PVOID`,`BOOLEAN`, unused string
pointer aliases): **Keep, no action** — these are standard-shape Win32
typedefs low-risk enough not to warrant individual tracking.

### 3.21 `winuser.h` — largest header (563 lines)

Bulk of WinUser surface: **Keep**, traced to live call sites throughout
§2. Two structural findings:

* **`UNICODE`-branch macros reference functions that don't exist anywhere**
  (`SetWindowTextW`, `PostMessageW`, `MessageBoxW`, `LoadStringW`,
  `GetModuleHandleW`, `LoadImageW`, `GetObjectW`, `RegisterClassW`,
  `CreateWindowExW`, `CreateWindowW`, `PeekMessageW`, `GetMessageW`,
  `DispatchMessageW`, `DefWindowProcW`, `LoadCursorW`, `LoadIconW`) —
  confirmed deliberate per the header's own comment; if `UNICODE` were
  ever defined, the build would fail loudly rather than silently
  misbehave. **Keep as-is** — this is a correct, if unusual, way to keep
  the door shut on real Unicode support.
* `RegisterClassExA`/`WNDCLASSEXA`: **do not exist** (§1 risk #5,
  reconfirmed §2.2) — no action, not a gap.

### 3.22 `wtypes.h` — see §3.12.

### 3.23 Symbols NOT in any public header despite being consumed by `free-direct`

Cross-referencing this section against the R5 bridge-usage findings:
**`FreeApiCreateSurfaceDC`, `FreeApiDestroySurfaceDC`, and
`FreeApiSetWindowFullscreen` have zero declarations anywhere in
`include/` or `include_non_windows/`.** They're defined in
`src/wingdi_dc.cpp:12,30,44` and declared only in the **internal**
`src/internal/FreeApiGdi.hpp`. Every consumer — free-api's own
`examples/04_gdi_minimap.cpp`, `tests/test_gdi_regressions.cpp`, and
`../free-direct/src/directdraw/DirectDraw.cpp` — hand-declares its own
local `extern "C"` copy. **Decision: Fix** — add one authoritative public
header declaration for all three (not just the two already tracked as
`TASK-0002`); coordinate the `free-direct`-side switch to use it. This is
now the single largest, best-evidenced item in the public-surface audit
(see §5 risk #2).

---

## 4. Implementation Correctness Audit

Organized by the 29 subsystems this session's dedicated implementation
pass covered. Each entry: what works / partial / deliberately
unsupported / risky / tests exist / tests missing / must not generalize.

**4.1 Build system and target-game detection** (`CMakeLists.txt`).
Works: three-tier SDL3 acquisition with `FATAL_ERROR` if none resolve;
`FREE_API_TARGET_GAME` keyed off `CMAKE_PROJECT_NAME` correctly
disambiguates "which game is driving this build" from "which sibling
directories merely exist" — the only reliable signal, confirmed still
necessary since both siblings normally coexist on disk. Risky: none
found — the detection logic is well-guarded. Must not generalize: do not
add a fourth target-game mode without the same rigor (evidence-cited
known-string-ID verification, `FATAL_ERROR` on ambiguity).

**4.2 Public header hygiene.** Works: confirmed clean, zero `SDL_*` types
in any public header; enforced automatically by `test_header_compile.cpp`,
which links only against the public `freeapi_compat_headers` interface
target. No action needed.

**4.3 WinMain bridge** (`src/winmain_bridge.cpp`). Works: `FreeApiRunWinMain`
validates its entry point, repairs `_pgmptr` (a real MinGW workaround via
constructor-priority `GetModuleFileNameA` capture), builds the command
line, and calls through. Risky: the Windows `_pgmptr` patch depends on
MinGW's macro-based `_pgmptr` accepting direct assignment — the code's own
comment flags this may not always work depending on link path; unresolved
edge case with no Windows CI evidence in this environment. Tests missing:
no dedicated `test_winmain_*` file — only incidentally covered by every
test that links free-api at all.

**4.4 Window class registration** (`src/winuser_window.cpp:13-21`). Works:
`RegisterClassA` stores the `WNDPROC`, keyed by class name; correctly
rejects null class name/proc. Partial: silently discards `hIcon`/
`hCursor`/`hbrBackground`/style/extra-bytes fields — acceptable since
neither game reads them back. Confirmed gap, not a bug: `RegisterClassExA`
does not exist anywhere (§2.2, §3.21) — any game code calling it would
fail to *compile*, not silently degrade. Must not generalize: do not add
`RegisterClassExA` without an evidenced call site.

**4.5 Window creation and destruction** (`src/winuser_window.cpp`).
Works: `CreateWindowExA` correctly maps `dwStyle` to SDL window flags,
deliberately never sets `SDL_WINDOW_RESIZABLE` (documented compositor
`WM_CLOSE` quirk workaround). **Confirmed: `WM_CREATE`/`WM_DESTROY` are
both dispatched synchronously, inline** — not posted to the queue —
matching the project's documented invariant exactly; the code comment
explains this is required so both games' `KillTimer`/`timeKillEvent`
calls (invoked from their `WM_DESTROY` handler) actually stop the frame
pump before the message loop exits. Risky: `DispatchMessageA`'s
null-`hwnd` fallback dispatches to "whatever single window happens to be
registered first" — fine for the single-window case both games use,
fragile if ever extended. Must not generalize: do not add multi-window
support without revisiting this fallback.

**4.6 Message queue** (`src/internal/FreeApiMessageQueue.cpp`). Works:
`WM_MOUSEMOVE`/`WM_TIMER` coalescing (in-place update / drop-if-duplicate)
correctly prevents unbounded queue growth; `PumpSdlEvents` correctly
converts to renderer logical space for letterbox/scaled presentation
modes. Risky, undocumented until now: `SDL_EVENT_WINDOW_FOCUS_LOST` is
**deliberately suppressed** — no `WM_ACTIVATEAPP(0)` is ever sent — to
avoid the game freezing on spurious focus-loss events. This is a real,
permanent Win32 behavior deviation; it works, but wasn't previously
written up in `docs/out-of-scope.md`. New backlog task: document it
there explicitly (P2).

**4.7 `PeekMessageA`/`GetMessageA`/`WaitMessage`** (`src/winuser_message.cpp`).
**`PeekMessageA` fully ignores `hWnd`/`wMsgFilterMin`/`wMsgFilterMax`**
— every call behaves as an unfiltered peek regardless of arguments passed.
Confirmed via explicit `(void)`-casts in the source; never sleeps/blocks
(a documented design invariant). `GetMessageA` is the real blocking
primitive, implemented as a `PeekMessageA`-based spin-loop with
`SDL_Delay(1)` — functionally blocking via 1ms polling, not an OS
wait/condvar. **`WaitMessage` is NOT a true blocking wait**: checks the
queue twice (pre/post a single pump), sleeps once for ~1ms only if both
checks miss, and **returns `TRUE` unconditionally** even if the queue is
still empty after that. Functionally adequate for both games' current
idle-loop usage (verified: neither game's behavior depends on
`WaitMessage`'s true-blocking semantics), but a real spec deviation.
Tests exist for the removeflag/quit/no-busy-spin behavior; filter-ignoring
is documented by test comment but not independently asserted as a
positive claim. Must not generalize: do not implement real filter support
without an evidenced call site passing non-zero filters.

**4.8 SDL event translation** (`src/internal/FreeApiSdlVideo.cpp`).
Works: lazy, idempotent `SDL_INIT_VIDEO` init/shutdown tied to
window-registry population. No risky findings.

**4.9 Keyboard input** (`src/internal/FreeApiMessageQueue.cpp:120-329`).
Works: hand-maintained scancode→`VK_*` table covers exactly both games'
evidenced usage (§2.4); unmapped keys are silently dropped (not forwarded
with a garbage code) — correct, safe default. F10 is the only key
special-cased to `WM_SYSKEYDOWN`/`UP`, matching both games' actual usage
exactly (neither uses Alt-combo SYSKEY behavior). `WM_CHAR` only forwards
ASCII (`<128`); non-ASCII UTF-8 bytes are dropped — fine, neither game
needs text input beyond ASCII. Tests: `test_input_pipeline.cpp` covers
the F10 case explicitly (proving it's *not* also delivered as plain
KEYDOWN). Must not generalize: no general Alt-key-held SYSKEY detection
without evidence.

**4.10 Mouse input** (`src/internal/FreeApiMessageQueue.cpp:330-406`).
**Confirmed bit-exact `WM_MOUSEMOVE` lParam packing** (LOWORD=x,
HIWORD=y) — this is the project's most safety-critical invariant (demo-
file replay compatibility) and it holds. `MK_SHIFT`/`MK_CONTROL` are
computed **live on every mouse move** via `SDL_GetKeyboardState`, not
latched at button-press time — explicitly matching planetblupi's
`CEvent::PlayMove` requirement for live shift-state during drag (§2.4).
This is the one subsystem where the implementation is more careful than
its test coverage: the underlying mechanism is correct and well-commented,
but §4.29/§5 flag that the actual gameplay feature it enables (drag-select
highlight) has zero automated or human verification.

**4.11 Cursor handling** (`src/winuser_cursor.cpp`). Works: `ShowCursor`'s
signed-counter-return contract and `SetCursor`'s previous-handle-return
contract are both confirmed to match real Win32 semantics exactly.
`LoadCursorA`/`LoadIconA` are documented permanent non-null stubs — correct
as-is, neither game inspects real cursor/icon shape. No risky findings.

**4.12 Timers: `SetTimer`/`KillTimer`/`WM_TIMER`** (`src/winuser_timer.cpp`).
Works: auto-ID generation, minimum-1ms interval clamp, correct "only
fires while the message loop polls" semantics (matching the header's own
documented caveat). `lpTimerFunc` is deliberately ignored — only
`WM_TIMER` messages are produced, never direct callback invocation,
matching both games' usage (neither passes a real callback). No risky
findings; this is planetblupi's entire, correctly-implemented frame pump.

**4.13 Multimedia timers: `timeSetEvent`/`timeKillEvent`** (`src/winmm.cpp:60-185`).
**Confirmed genuinely independent from §4.12** — backed by a real
`SDL_AddTimer` background thread, separate ID space (shared counter with
`SetTimer`, separate storage map), separate mutex. The timer-callback
bridge re-looks-up its entry by ID on every fire specifically to avoid a
documented prior use-after-free bug (fixed, verified still fixed). This
is free-eggbert's entire, correctly-implemented frame pump. Minor
inconsistency, not user-visible: `timeKillEvent` returns `1` (not
`MMSYSERR_NOERROR`/0) for both the trivial-ID and not-found cases while
its success path returns 0 — cosmetic, low priority.

**4.14 GDI bitmap/DC model** (`src/internal/FreeApiGdi.hpp`/`.cpp`,
`src/wingdi_dc.cpp`). Works: magic-tagged opaque structs provide real
type-safety against handle confusion between `CompatBitmap`/`CompatDC`.
**Confirmed still open: `FreeApiCreateSurfaceDC`/`FreeApiDestroySurfaceDC`
have zero header declaration anywhere** (§3.23) — every caller
hand-declares its own `extern "C"` copy. Already tracked (`TASK-0002`),
this session additionally found a third undeclared bridge function
(`FreeApiSetWindowFullscreen`) with the identical problem. `SelectObject`
returns `NULL` for any non-bitmap object rather than the real
"no-op, return default" Win32 behavior — acceptable since neither game
selects anything but bitmaps.

**4.15 `LoadImageA`** (`src/wingdi_bitmap.cpp:13-49`). Works: `IMAGE_BITMAP`
+ `LR_LOADFROMFILE` path, RGBA32 conversion, optional nearest-neighbor
rescale. `LR_CREATEDIBSECTION` is accepted but has no distinct code path
— always converts to RGBA32 regardless, which happens to be a
superset-compatible behavior for both call shapes either game uses
(matches `docs/out-of-scope.md`'s documented reasoning). **Risky, newly
found:** uses the weaker `NormalizePath` (backslash-only) instead of the
`NormalizeFilesystemPath` used everywhere else (drive-letter strip,
leading-slash strip) — an inconsistency versus `_lopen`/`CreateDirectoryA`/
`_mkdir`/`_findfirst`. Not currently a proven bug (no evidenced game call
site uses a rooted-looking bitmap path), but worth aligning for
consistency (P2, new backlog task). Tests missing: no dedicated
`LoadImageA`-from-real-file regression test (only indirect coverage via
`FreeApiCreateSurfaceDC` fixtures).

**4.16 `StretchBlt`** (`src/wingdi_blit.cpp:13-147`). Works: `SRCCOPY`-only
1:1 fast path (bulk `memcpy` per clipped row, no per-pixel branching) is
correct and well-tested. **Confirmed, still open, exact current lines:**
the scaled path's source-X sampling clamps out-of-range values to the
nearest edge column (`:118-119`), while source-Y sampling instead
`continue`s the entire destination row, **leaving it completely
untouched** (`:126`) rather than clamping. Same geometric situation
(scale/offset pushing a sampled coordinate out of bounds), two different
outcomes depending on which axis triggered it. This is the single
highest-confidence, highest-value bug in the whole audit — real,
reproducible, in the render hot path, and **zero regression test exercises
either the X-clamp or the Y-skip behavior specifically** (existing
`test_gdi_regressions.cpp` covers 1:1/clipped/OOB/scaled paths, but not
this particular asymmetry). Logging is correctly gated behind
`FreeApiGdiDebugEnabled()` throughout — not a quiet-by-default concern.

**4.17 `GetPixel`/`SetPixel`** (`src/wingdi_blit.cpp:149-206`). Works:
both handle Surface- and Memory-DC kinds correctly, safe
non-crashing degenerate behavior on out-of-bounds/invalid DC. `SetPixel`
correctly forces alpha to 255 regardless of prior value, matching
`COLORREF`'s meaning. No risky findings.

**4.18 Palette behavior — `GetDeviceCaps`** (`src/wingdi_misc.cpp:5-15`).
Works: returns `0` for `SIZEPALETTE`, confirmed correct and intentional
(both games' TrueColor-vs-palette branching needs exactly this). **Risky,
newly precise finding: `index` is ignored entirely** — the function
returns `0` for literally *any* index value, not just `SIZEPALETTE`. Not
a proven bug today (neither game queries any other index, confirmed by
§2 usage sweep), but the implementation is narrower than its own
signature suggests. Document explicitly that this is scoped to the one
proven-used index (P3, doc clarification only — no behavior change
needed, since no other index is ever queried).

**4.19 `CreateBitmap`** (`src/wingdi_bitmap.cpp:97-157`). Works: 16-bit
RGB565 and 32-bit paths are both correctly implemented. **Confirmed,
self-documented gap:** the 8-bit path treats indexed pixel data as pure
greyscale (`TODO: apply palette if one is set`, line 119) — no real
palette lookup. This matters specifically for planetblupi's minimap
(`decmap.cpp:578,582`, the only `CreateBitmap` call site in either game)
— if the minimap's source data is genuinely palette-indexed with
non-greyscale colors, it will render wrong. **Needs verification**
whether planetblupi's minimap data is actually 8-bit-indexed-non-greyscale
in practice (new backlog task, P1 — this is the kind of "confirmed gap,
undetermined severity" that deserves investigation before a fix).

**4.20 File/path normalization.** **Confirmed: four independent,
non-shared implementations** exist for materially the same
backslash/drive-letter/leading-slash logic:
`FreeApi::Internal::NormalizePath` (backslash-only, used only by
`LoadImageA`, §4.15), `FreeApi::Internal::NormalizeFilesystemPath` (full
normalization, used by most file I/O), `free_api_fopen`'s own
independently-reimplemented version (plus a case-insensitive-basename
fallback the other two lack), and `NormalizeMidiPath` (yet another
independent case-fallback strategy). None call into each other. Not a
correctness bug today (each is individually correct for its own call
sites, confirmed by tests), but a real duplication/fragility risk for
future maintenance — a fix to one won't propagate to the others. New
backlog task: consolidate to a shared core normalization function with
per-caller opt-in flags for the extra behaviors (case-fallback, etc.)
(P2 — non-trivial refactor, needs care not to change any current
behavior).

**4.21 `_lopen`/`_lread`/`_lclose`** (`src/winbase_file.cpp:37-85`). Works:
correct handle numbering (starts at 3, mirrors stdin/stdout/stderr
reservation), `_lopen` is always read-only regardless of `iReadWrite`
(fine, no game ever requests write). **Newly found, low-severity:**
`g_openFiles`/`g_nextFileHandle` have **no mutex**, unlike the equivalent
`_findfirst` session table — a latent data race if ever called
concurrently. Neither game is multi-threaded around file I/O (confirmed
by the evidence read), so this is theoretical today; document as a known
limitation rather than add unneeded locking overhead (P3).

**4.22 `_mkdir`/`CreateDirectoryA`** (`src/crt_direct.cpp`,
`src/winbase_file.cpp:97-129`). Both route through
`NormalizeFilesystemPath` — **confirmed the prior leading-backslash-escape
fix still holds.** `CreateDirectoryA` correctly distinguishes
"already exists" (via `std::filesystem::exists` check) from a real
failure, matching real Win32 semantics for that case. No risky findings.

**4.23 `_findfirst`/`_findnext`/`_findclose`** (`src/crt_io.cpp:44-138`).
**Confirmed, exact mechanics traced:** `_findfirst` pre-computes the full
match list up front; `_findnext`'s exhaustion path (`index >=
matches.size()`) does **not** erase the session entry; only `_findclose`
erases. Since `g_nextFindHandle` only ever increments, **any caller that
drains via `_findnext` to exhaustion without calling `_findclose` leaks
exactly one entry, permanently, for the process lifetime** — and
free-eggbert's design-mission picker is confirmed to do exactly this on
every screen visit (§2.8). Bounded (one leaked entry per player visit to
one specific screen, not a hot path) but real. Test coverage: happy-path
only (`test_file_paths.cpp` always calls `_findclose`) — **the
leak-inducing scenario itself is not exercised by any test.**

**4.24 `LoadStringA` and STRINGTABLE extraction** (`src/winuser_misc.cpp:20-50`,
`cmake/ExtractStringTable.cmake`). Works: falls back to a `"RES_%u"`
placeholder (never garbage/empty) on miss; the CMake extractor is
correctly scoped to STRINGTABLE-only parsing (not a general `.rc`
compiler, confirmed by its own header comment); fail-loud gating
(`REQUIRE_STRINGS`/`VERIFY_ID`/`USED_IDS_FILE`) is correctly restricted to
target-game builds only, leaving standalone builds on the safe
placeholder-fallback path. This session's independent numeric
re-verification confirms the 308/257 unique-used-ID manifests are exactly
accurate (§2.9). No risky findings — this subsystem is in excellent
shape.

**4.25 `FindResourceA` and safe resource stubs** (`src/winbase_file.cpp:149-186`).
Works exactly as documented: permanent, deliberate always-miss stub;
both games' own code already falls through to file-based loading on the
miss, confirmed end-to-end working by `test_file_regressions.cpp`. No
risky findings, no action needed — this is the correct shape for a
narrow, evidence-scoped stub.

**4.26 WinMM/MIDI sequencer path** (`src/MidiMusic.cpp`). Works: real
TinySoundFont+TinyMidiLoader-backed playback, correct SoundFont
fallback-search chain, correct "missing soundfont degrades to silent
success + still posts `MM_MCINOTIFY`" behavior (keeps the game's
notify-driven replay loop from stalling), correct single-lock-scope fix
for a documented prior dangling-pointer bug. Deliberately unsupported:
looping (explicit `TODO`, `Status: PARTIAL`) — **but this session's
docs-staleness pass confirms this is not actually a functional gap**:
both games' own `MM_MCINOTIFY` handlers re-issue playback themselves on
song-end, so music genuinely does loop end-to-end in real gameplay
despite the internal comment's phrasing (§6, `TASK-0008`). Tests exist:
`test_mci_sequences.cpp`'s 30-cycle close/reopen stress test directly
guards a real prior use-after-free regression.

**4.27 MCI AVI/digital-video decision** (`src/winmm.cpp:282-306`). Works
exactly as documented and re-confirmed by this session's independent
feasibility research (§5, §6): a precisely-targeted probe-shape check
(`MCI_OPEN_TYPE` set, `MCI_OPEN_ELEMENT` not set) returns
`MCIERR_UNSUPPORTED_FUNCTION`, and this session traced both games' own
`CMovie`/`CEvent` code to confirm the resulting skip is fully safe with
no side effects. Additionally sidesteps a real 64-bit pointer-truncation
hazard in planetblupi's own code (not free-api's bug, but the early
return avoids ever exercising it). No action needed — this is a model
example of a well-evidenced, well-tested, deliberately-scoped decision.

**4.28 Joystick support** (`src/winmm.cpp:14-58,187-254`). Works: real
SDL_Joystick-backed implementation, correct axis-range remapping
(`-32768..32767`→`0..65535`), correct button-bit mapping, safe
null/too-small-struct rejection. Risky, low-severity: initializes
`SDL_INIT_JOYSTICK` as a side effect of any call and never tears it down
(no `SDL_QuitSubSystem(SDL_INIT_JOYSTICK)` anywhere) — harmless in
practice (SDL's joystick subsystem is cheap to keep initialized) but
worth noting since free-eggbert's `joyGetNumDevs()` call is inside code
that could run every frame. Confirmed (§2.10): despite being fully real
and correct, this backend is runtime-dead weight from both games'
perspective. Must not generalize: no gamepad/DirectInput/raw-input
expansion without new evidence.

**4.29 Diagnostics/logging** (gating audit across `src/`). Well-gated,
hot-path-safe: the message loop itself and `StretchBlt` (the actual
per-frame blit hot path) have zero unconditional logging — every
`SDL_Log` in both is behind `FreeApiGdiDebugEnabled()`/
`FreeApiDiagnosticsEnabled()`. **Confirmed gap, matches user's "quiet by
default" requirement for this session:** `winuser_window.cpp` is the
single largest source of unconditional logging (~12 sites across
`CreateWindowExA`/`ShowWindow`/`UpdateWindow`/`SetFocus` — roughly a
dozen log lines fire on every single game launch, no opt-out short of a
code change); `LoadImageA`'s **success** path logs unconditionally on
every single bitmap load (`wingdi_bitmap.cpp:47`), not just failures.
Several other unconditional sites exist but are legitimately rare
(startup-once/failure-path-only): `FreeApiSdlVideo.cpp` init,
`FreeApiGdi.cpp`'s surface-conversion-failure log, `timeSetEvent`'s
success log (fires once per successful call, which for free-eggbert is
startup-once), `MidiMusic.cpp`'s no-soundfont/backend-init-failure logs.
This session's backlog treats `winuser_window.cpp` and `LoadImageA`'s
success-path log as the two P1 items worth actually gating (both fire on
every normal run, unlike the genuinely-rare failure-path logs elsewhere)
— see §5/§6.

---

## 5. Known Risk List

Prioritized; each item cross-references the exact user-supplied checklist
item it resolves, where applicable.

1. **P0 — `StretchBlt` scaled-path X/Y clamp asymmetry, zero regression
   coverage** (checklist: "`StretchBlt` scaled-path correctness and
   clipping"). Confirmed live, in the render hot path, exact current
   lines `src/wingdi_blit.cpp:118-119` (X-clamp) vs `:126` (Y-skip).
   Highest-value fix in this audit. → backlog `TASK-24H` P0 items.
2. **P0/P1 — `free-direct` bridge surface bigger than documented**
   (checklist: "`FreeApiCreateSurfaceDC`/`FreeApiDestroySurfaceDC` shared
   declaration with `free-direct`"). Confirmed: three functions
   (`FreeApiCreateSurfaceDC`, `FreeApiDestroySurfaceDC`,
   `FreeApiSetWindowFullscreen`) have zero public header declaration;
   `free-direct` also reaches into the fully-internal
   `FreeApi::Platform::ReadRssKB`. Needs a shared header (coordination
   with `../free-direct`) plus an updated scope-exception doc listing all
   four/consumed-internals honestly. → backlog P1 items (cross-repo
   coordination needed, cannot be P0 solo-repo work).
3. **P1 — `_findfirst` session-table leak, confirmed mechanics, untested
   leak scenario** (checklist item, matches exactly). →
   backlog P1/P2 items (fix-or-document decision, per user's confirmed
   "pick one, document the choice" pattern from the prior session).
4. **P1 — `GetSystemMetrics` fixed values** (checklist item). Re-confirmed
   this session: both games only ever query `SM_CXSCREEN`/`SM_CYSCREEN`/
   `SM_CYCAPTION` at startup — fixed values are correct for this evidenced
   usage; **no bug found**, downgrade from the checklist's implied
   concern to "confirmed fine, document why" (P3).
5. **P1 — `MoveWindow` not updating logical window state** (checklist
   item). Only live call site in either game is `movie.cpp:68`/`:71`
   (planetblupi/eggbert), itself dead-reach since AVI always fails to
   open (§2.7) — **confirmed non-issue, no evidenced call site that
   matters.** Document as such (P3), no fix needed.
6. **P1 — unconditional `SDL_Log` in `ShowWindow`/`CreateWindowExA`/
   `UpdateWindow`/`SetFocus`/`timeSetEvent`/MCI paths** (checklist item,
   confirmed and expanded — see §4.29). `winuser_window.cpp` (~12 sites)
   and `LoadImageA`'s success path are the two that actually matter
   (fire on every normal run); the rest are legitimately-rare
   failure/startup-once paths. → backlog P1 items to gate the two
   confirmed offenders behind `FreeApiDiagnosticsEnabled()`.
7. **P2 — `OutputDebugStringW` being real despite ANSI-only policy**
   (checklist item, confirmed). Zero evidenced call site in either game.
   → backlog P2 item: verify no real caller, downgrade to documented
   stub or add an explicit keep-rationale note.
8. **P2 — `_chdir`/`_getcwd` no call sites** (checklist item, confirmed).
   → backlog P2 item, user-confirmed default: document+keep.
9. **P1 — `PeekMessageA` ignoring filters** (checklist item, confirmed,
   see §4.7). Neither game ever passes non-zero filters — not a bug
   today, but the ignored-arguments behavior should be documented
   explicitly as intentional (it currently reads more like an oversight
   than a decision). → backlog P2 doc item; do NOT implement real
   filtering without new evidence (explicit "do not do" per §9 of user's
   original brief).
10. **P1 — `WaitMessage` not a true blocking wait** (checklist item,
    confirmed, see §4.7). Functionally adequate for both games today.
    → backlog P2 doc item (document the actual polling-based contract
    explicitly rather than implying real blocking semantics); no
    behavior change without new evidence of it mattering.
11. **P2 — `WM_ACTIVATEAPP(0)` focus-loss suppression** (checklist item,
    confirmed, see §4.6). Real, permanent, working, but was not
    previously documented in `docs/out-of-scope.md`. → backlog P2 doc
    item.
12. **P2 — `LoadStringA` standalone-vs-target-game behavior** (checklist
    item). Re-confirmed correct and already well-documented — no new
    finding, no action needed beyond what's already in `docs/`.
13. **P2 — generated string table's configure-time `.rc` dependency**
    (checklist item). Re-confirmed working as designed (§4.24); no new
    risk found.
14. **P2 — resource stubs remaining safe after `LoadStringA` became
    real** (checklist item). Re-confirmed via §4.25 and the independent
    docs-staleness pass — `FindResourceA`'s permanent miss is exactly as
    safe today as when originally decided; no regression.
15. **P0 (confirmed non-issue) — MCI AVI intentionally unsupported**
    (checklist item). This session's dedicated feasibility pass (§5 of
    the research, folded into §1/§6 here) reconfirms: safe, no
    player-visible defect, recommend permanent decline. Not a risk
    requiring backlog action beyond the existing lock-in test.
16. **P2 — joystick backend SDL subsystem side effects** (checklist
    item, confirmed, see §4.28). Harmless in practice, never torn down.
    → backlog P3 doc item only; do not add teardown logic without
    evidence it matters (SDL subsystem re-init/shutdown churn is not
    free).
17. **P1 — test reliability under headless SDL/dummy drivers** (checklist
    item). Prior session's investigation (Wayland-only 6-test failure,
    resolved as environment artifact) still holds — this session found
    no new headless-reliability issues, but **did** find the
    `MK_SHIFT`/`MK_CONTROL` gap is *specifically* a headless-testability
    limitation (SDL's keyboard-state query needs a real backend), not a
    free-api bug. → no new backlog item beyond the existing human-playtest
    task, now sharpened with an exact keyboard path to reach live
    gameplay (§2, `Enter × 4`).
18. **P3 — Windows/MinGW compile behavior** (checklist item). Out of
    this session's scope per user's platform-restriction answer (Linux
    desktop only) — flagged as a known unverified area, not actionable
    this session.
19. **P3 — Android-specific logging/lifecycle behavior** (checklist
    item). Same — out of this session's platform scope, not actionable.
20. **P3 — four independent path-normalization implementations**
    (newly found, not on the original checklist, see §4.20). Real
    duplication risk, no proven bug. → backlog P2/P3 consolidation task.
21. **P2 — `CreateBitmap`'s 8-bit-greyscale-only gap** (newly found, see
    §4.19). Needs verification of actual severity against planetblupi's
    real minimap data before deciding fix-vs-document. → backlog P1
    investigation task.
22. **P3 — `docs/supported-apis.md` missing symbols/stale rows**
    (checklist item). Confirmed already tracked (`TASK-0001`, still
    accurate scope) plus one small addition found this session: the
    `_findfirst` leak caveat isn't surfaced in that doc's status table
    (§6). → folds into the existing `TASK-0001` follow-up plus one new
    small doc item.

---

## 6. Out-of-Scope Confirmation

| Item | Confirmed out of scope? | Evidence |
|---|---|---|
| Wine-like behavior (general Win32 emulation) | **Yes** | Both usage sweeps confirm free-api implements exactly the symbols either game calls, nothing speculative; `RegisterClassExA` absence (§2.2) is a concrete example of scope discipline in practice |
| Windows Registry | **Yes** | Zero registry API references anywhere in `include/`, `src/`, or either game's source |
| PE loader / real Windows process model | **Yes** | `FreeApiRunWinMain` is a thin `main()`→`WinMain` argument-adaptation shim only; no PE parsing, no module loading beyond the CRT's own dynamic linking |
| Full `.rc`/`.res` compiler | **Yes** | `cmake/ExtractStringTable.cmake` is explicitly, self-documentedly STRINGTABLE-only; confirmed narrow this session (§4.24) |
| General resources beyond narrow `LoadStringA` | **Yes** | `FindResourceA`/`LoadResource` family confirmed permanent, safe, working stubs (§4.25); no evidenced need for real resource loading in either game |
| Common Dialogs | **Yes** | `commdlg.h` confirmed empty stub, included by both games but zero actual API calls found in either (§2.1, §3.2) |
| OLE/COM | **Yes** | `winnt.h`'s COM-family typedefs (`IUnknown`/`GUID`/`IID`/`CLSID`) confirmed zero usage anywhere in `include/`; `wtypes.h` self-documents as proven-unused (§3.20) |
| Unicode `W` APIs (unless target games build with `UNICODE`) | **Yes** | Neither game defines `UNICODE` (confirmed, ANSI-only build both games); `winuser.h`'s `UNICODE`-branch macros deliberately reference nonexistent `W` functions so such a build would fail loudly, not silently misbehave (§3.21) |
| DirectDraw/DirectSound/DirectPlay inside free-api | **Yes** | Confirmed this session: both games `#include` `ddraw.h`/`dsound.h`/`dplay.h`, but these are implemented entirely by the sibling `../free-direct` repo — zero `DD*`/`DS*`/`DP*` symbols anywhere in free-api's `include/` or `src/` (§2.1) |
| Generic Win32 file semantics beyond what games use | **Yes** | §2.8 confirms the two games' file/path needs are narrow and divergent (free-eggbert: `_findfirst`/`_mkdir`; planetblupi: `CreateDirectoryA`, no enumeration at all) — free-api implements exactly this narrow, evidenced union, not a general filesystem API |
| Generic GDI | **Yes** | §2.6/§4.14-§4.19 confirm the GDI subset is scoped exactly to what both games' DirectDraw-era compatibility layers (`ddutil.cpp` in both) actually call |
| Generic MCI video (unless explicitly approved) | **Reconfirmed: stays declined.** User approved re-opening this session as a **feasibility audit only** (no implementation). Dedicated research (folded into §1/§5): both games already treat a missing AVI driver as a fully safe, silent, no-side-effect skip (traced end-to-end in both games' source this session); implementing real playback would add a brand-new codec/video-decode subsystem (confirmed need for both Cinepak and MS Video 1 decoding, from planetblupi's real shipped `.avi` assets) with no player-visible defect to justify it. **Recommendation: decline permanently**, unchanged. |
| Arbitrary other 1998-era Windows games | **Yes** | Every symbol in the public-surface audit (§3) traces to one of exactly two games or the narrow `free-direct` bridge exception; no speculative "might be useful for other software" additions found anywhere |

---

## Cross-references

* `plan.md` §"24-Hour Autonomous Stabilization Backlog" — the task
  backlog this audit feeds (`TASK-24H-XXXX`).
* `plan.md` `TASK-0001`–`TASK-0012` — prior session's backlog; several
  items re-confirmed accurate/still-open by this session's independent
  re-derivation (see inline references throughout §2–§6).
* `NEXT.md` — updated with this session's log at the end of the
  autonomous work pass.
* `docs/out-of-scope.md`, `docs/supported-apis.md`, `docs/headers.md`,
  `docs/target-games.md`, `docs/used-string-ids.md`, `docs/scope.md`,
  `docs/cmake-options.md` — all independently re-verified accurate this
  session (§ Part A of the docs-staleness research pass); no corrections
  needed to any of them beyond what's already tracked.
* `README.md`, `Documentation.md` — **confirmed stale** this session
  (obsolete single-file `src/winapi.cpp` claims; overstated MIDI-loop/
  `cdaudio` "TODO" framing) — corrected as part of this session's backlog.
