# NEXT.md

Handoff document for resuming work on `free-api`, for either a future
Claude Code session or a human developer. Reflects the actual repository
state as of commit `8e7cf0a` (2026-07-04) plus substantial uncommitted
changes made this session — a systematic pass through `plan.md`'s P0
backlog, including a critical MIDI-playback bug fix (see section 2/3/4).
See [`plan.md`](plan.md) for the full evidence-based usage audit and
126-item task backlog (every task now carries a `Status:` line reconciled
against actual repository state), and [`docs/scope.md`](docs/scope.md) for
the scope policy.

## 1. Project summary

Free API is a small, SDL3-backed C++ library that exposes a Win32/WinAPI-
like public surface (`windows.h`, `winuser.h`, `mmsystem.h`, `digitalv.h`,
etc.) so two specific legacy Windows games can compile and run on Linux/
macOS/Web/Android without Microsoft Windows:

* **Free Eggbert** (`../free-eggbert`, top-level CMake project
  `SPEEDY_BLUPI_WINDOWS`)
* **Planet Blupi** (`../planetblupi`, top-level CMake project
  `PLANET_BLUPI_WINDOWS`)

**Main goal:** source-level compatibility for exactly these two games — not
Wine, not a general WinAPI reimplementation, not a platform for arbitrary
1998-era Windows software. Every public API must cite a real usage site
(`file:line`) in one of the two games' own source (`docs/scope.md`).

**Current development phase:** incremental hardening after an initial
evidence-based audit (`plan.md`), now well past the P0 tier — as of this
session, every `plan.md` P0 task is `Status: DONE`. Several real bugs found
via test-writing have since been fixed, most significantly a MIDI-playback
kill-switch + its underlying race condition (section 2/4), a
`GetDeviceCaps(SIZEPALETTE)` contract bug affecting both games' rendering
path selection, and a struct-packing bug affecting real BMP palette
decoding. `LoadStringA` returns real text; MCI digital-video was
investigated and resolved as an intentional non-issue.

**Important architectural decisions:**

* Free API is a **static library**, normally built as a sibling
  `add_subdirectory()` of one of the two target games (which also provide
  SDL3). It can also build standalone via `-DFREE_API_USE_SYSTEM_SDL3=ON`.
* SDL3 is an **internal backend detail only** — public headers in
  `include/` must never expose SDL types.
* Both target games are always siblings of each other and of `free-api` on
  a normal dev machine — **sibling-directory existence alone cannot tell
  you which game is driving a given build.** `CMakeLists.txt` keys off
  `CMAKE_PROJECT_NAME` (the actual top-level project) to detect this,
  falling back to sibling-existence only for a genuinely standalone build.
* Real UI text for `LoadStringA` is extracted at **CMake configure time**
  from whichever game's own `resource/*.rc` is driving the build, via a
  narrow, `STRINGTABLE`-only parser (`cmake/ExtractStringTable.cmake`) —
  deliberately not a general `.rc`/`.res` compiler.
* The two target games use **two different, mutually-exclusive live timer
  mechanisms** as their frame pump: Free Eggbert uses
  `timeSetEvent`/`timeKillEvent` (WinMM multimedia timer); Planet Blupi
  uses `SetTimer`/`KillTimer`/`WM_TIMER`. Both must keep working.

## 2. Current status

**Build status:** builds cleanly in all three configurations tested this
session:
* Standalone with system SDL3 (`-DFREE_API_USE_SYSTEM_SDL3=ON`).
* As a subdirectory of `../free-eggbert` (real game build,
  `SPEEDY_BLUPI_WINDOWS` links and runs).
* As a subdirectory of `../planetblupi` (real game build,
  `PLANET_BLUPI_WINDOWS` links and runs).

**Test status:** 14 test binaries, **14/14 pass** (verified stable across
5+ repeated runs, plus a full AddressSanitizer+UBSan pass with zero
errors). `basic_test`'s long-standing MCI_OPEN failure — previously
documented here as an unresolved "environment-specific audio backend
issue" — is **fixed** (see section 4; it was misdiagnosed). New test
binaries added this session: `test_timer_regressions`,
`test_planetblupi_loop`, `test_eggbert_loop`, `test_mci_sequences`.

**CLI/tools/apps/libraries:** Free API produces one artifact,
`libfree-api.a`, plus its test binaries. It has no standalone CLI/app of its
own — it's a library consumed by the two games' own executables
(`SPEEDY_BLUPI_WINDOWS`, `PLANET_BLUPI_WINDOWS`), both of which build and
link successfully against the current code (verified this session).

**Recently completed (uncommitted, this session — a systematic pass through
`plan.md`'s P0 backlog, working task-by-task from the top):**

Real behavior/bug fixes (not just tests):
* **MIDI playback was completely disabled and is now fixed.**
  `MidiMusicSendCommand`'s `MCI_OPEN` handler (`src/MidiMusic.cpp`) had a
  hardcoded `return MCIERR_INTERNAL` behind a `//todo fix sigsegv` comment
  (commit `a35f476c`, "MIDI was disabled") — neither game could play any
  music, and `basic_test`'s failure (previously documented in this file as
  an "environment-specific audio backend issue") was actually this
  kill-switch, misdiagnosed by a prior session. Root-caused the real
  segfault: `MixerThread` captured a `MidiSession*` pointer under one
  `lock_guard` scope, released the lock, then dereferenced it after
  re-acquiring a *second* `lock_guard` scope; a concurrent `MCI_CLOSE`
  (erases from `g_midi.sessions`) or `MCI_OPEN` (`push_back`, can
  reallocate the vector) during that gap left the pointer dangling — the
  exact race free-eggbert's own `MM_MCINOTIFY` handler triggers
  (`SuspendMusic()`/`MCI_CLOSE` then immediately `RestartMusic()`/
  `MCI_OPEN`+`MCI_PLAY`, `blupi.cpp:562-580`). Fixed by folding
  find-and-render into one uninterrupted lock acquisition; removed the
  kill-switch. Verified via AddressSanitizer (zero errors) and a 30-cycle
  stress test reproducing the exact real-game close-then-reopen pattern.
  **Needs a manual playtest** to confirm audibly — cannot be verified by
  ear in this headless sandbox.
* `GetDeviceCaps(SIZEPALETTE)` (`src/wingdi_misc.cpp`) previously always
  returned 256 (a hardcoded palette-display placeholder); real Win32 only
  returns nonzero for an actual <=8bpp hardware-palette device — a modern
  TrueColor host reports 0. Both games' TrueColor-vs-palette branching
  logic only agrees when the value is exactly 0; the old 256 forced
  free-eggbert's true-color decor off and forced planetblupi's minimap
  onto its untested 8-bit-indexed `CreateBitmap` path instead of the
  true-color 16-bit path. Fixed to return 0. **Needs a manual visual
  playtest** of planetblupi's minimap and free-eggbert's true-color
  rendering.
* `BITMAPFILEHEADER`/`BITMAPINFOHEADER` (`include/wingdi.h`) lacked
  `#pragma pack`, so `BITMAPFILEHEADER` was 16 bytes instead of the real,
  on-disk 14-byte BMP file header size. Both games' `_lopen`/`_lread`
  palette-fallback path (`ddutil.cpp`, live since `FindResourceA` always
  misses) reads every real `.bmp` asset's header directly into these
  structs — every such read was misaligned by 2 bytes, corrupting the
  palette data read after it. Fixed with `#pragma pack(push, 2)`/`pop`
  (matching real Win32's own `<wingdi.h>`). **Needs a manual visual
  playtest** of both games' palette-driven rendering.

Test-only additions (`tests/`), one per completed `plan.md` P0 task —
see `plan.md` for the full per-task `Status:` lines:
* `TestCreateBitmap8BitIndexedExpandsToGreyscaleRgba`/`...Rgb565...`
  (`test_gdi_regressions.cpp`, TASK-0125) — pixel-value correctness for
  `CreateBitmap`'s conversion paths (previously only object-validity
  checked).
* `TestRegisterClassAWithFullFieldSet`,
  `TestCreateWindowExAFullscreenPath`,
  `TestClientToScreenTracksWindowPositionNotStale`,
  `TestSetCursorPosAndGetCursorPosRoundTrip` (`test_winuser_regressions.cpp`,
  TASK-0027/0028/0055).
* `tests/test_timer_regressions.cpp` (new file, TASK-0039/0040/0041/
  0042/0043/0044) — `SetTimer`/`KillTimer`/`WM_TIMER` and `timeSetEvent`/
  `timeKillEvent`, a cross-thread `PostMessageA` stress test, and the
  `WM_DESTROY`→kill-timer→`PostQuitMessage` sequence for both timer
  mechanisms.
* `tests/test_planetblupi_loop.cpp`, `tests/test_eggbert_loop.cpp` (new
  files, TASK-0037/0110/0111) — end-to-end reproductions of both games'
  exact `PeekMessage(PM_NOREMOVE)`→`GetMessage`→`Dispatch` loop idiom.
* `TestCreateCompatibleDcRepeatedLifecycleDoesNotLeak`,
  `TestGetDeviceCapsSizePaletteReportsTrueColorHost`,
  `TestGetSystemPaletteEntriesFills256WellFormedEntries`,
  `TestLoadImageADecodesNonBmpExtensionAndGetObjectAReportsCorrectDimensions`,
  `TestSelectObjectDeleteObjectBitmapIntoDcLifecycle`
  (`test_gdi_regressions.cpp`, TASK-0059/0060/0061/0062/0063/0064).
* `TestFindResourceAMissesThenLopenLreadLcloseDecodesRealBmpHeader`
  (`test_file_regressions.cpp`, TASK-0073/0084).
* `tests/test_mci_sequences.cpp` (new file, TASK-0089/0090/0091/0094/0098)
  — sequencer open/play/notify/close, the notify-triggered
  close-then-reopen stress test, cdaudio decline.

Documentation/build-hardening (test-adjacent, no behavior change):
* `cmake/ExtractStringTable.cmake` now emits a `message(WARNING ...)` for
  an unsupported `L"..."` wide-string `STRINGTABLE` entry or an embedded
  escaped double-quote, instead of silently mis-parsing (TASK-0126).

`plan.md` itself was reconciled against actual repository state: every one
of its 126 tasks now carries a `Status:` line (DONE/PARTIAL/TODO/
NOT-APPLICABLE, with evidence), verified against real code/tests rather
than trusting the plan's own prior text.

**Recently implemented / fixed (prior session, real behavior changes):**
* `LoadStringA` returns real `STRINGTABLE` text for both games instead of a
  `"RES_<id>"` placeholder.
* `DestroyWindow` now synchronously dispatches `WM_DESTROY` to the window's
  own `WndProc` before tearing it down (previously never happened at all).
* `ShowCursor`/`SetCursor` are real SDL-backed implementations (previously
  no-op stubs).
* `_mkdir`/`CreateDirectoryA` correctly normalize Windows-style backslash
  paths (e.g. `"\User"`) instead of potentially resolving to the real
  filesystem root.
* `WM_MOUSEMOVE`'s `wParam` now carries live `MK_SHIFT`/`MK_CONTROL` state
  (previously only button-down/up messages did).
* A hardcoded, machine-specific absolute path in `CMakeLists.txt` was
  removed; standalone builds now work via `-DFREE_API_USE_SYSTEM_SDL3=ON`.
* A diagnostic-counter gating bug in `PeekMessageA` was fixed.

**What does NOT work / is not implemented:**
* MCI digital-video (AVI movie codec playback) is **not implemented** —
  confirmed intentional; both games already gracefully skip movies when
  this is declined (see section 4/5).
* `LoadStringA`'s real-text table contains **only one game's strings per
  build** (whichever game's `.rc` was extracted for that specific build) —
  by design, not a bug.
* Joystick support (`joyGetPosEx`/`joyGetNumDevs`) is a safe stub (reports
  0 devices) — Free Eggbert degrades to keyboard/mouse gracefully; not a
  real implementation.
* `_findfirst`/`_findnext` (Free Eggbert's design-file picker) remain a
  stub that always fails — a minor, non-startup-blocking feature gap.

## 3. Recent changes

In chronological order, most recent last (commit hashes on `develop`):

* `4cd7dbd` — Added `plan.md`: full evidence-based usage audit of both
  target games + 124-item task backlog.
* `6ce82dd` — Removed hardcoded absolute path from `CMakeLists.txt`; added
  `FREE_API_USE_SYSTEM_SDL3` option; added `docs/scope.md`,
  `docs/cmake-options.md`; added `tests/test_header_compile.cpp`,
  `tests/test_winuser_regressions.cpp`, `tests/test_file_regressions.cpp`;
  fixed the `WM_MOUSEMOVE` `MK_SHIFT`/`MK_CONTROL` bug.
* `fa6616a` — Fixed `_mkdir` path normalization (promoted a shared
  `NormalizeFilesystemPath` helper); implemented real `ShowCursor`/
  `SetCursor`; gated the `FREE_DIRECT_INPUT` first-20-events log.
* `3a796de` — `DestroyWindow` now dispatches `WM_DESTROY` synchronously;
  removed a now-redundant second `PostQuitMessage` in `DefWindowProcA`'s
  `WM_CLOSE` handling.
* `b8b6667` — Implemented real `LoadStringA` text via
  `cmake/ExtractStringTable.cmake` (new) +
  `src/internal/FreeApiGeneratedStrings.hpp`/`FreeApiStringTable.cpp`
  (new); investigated MCI digital-video/AVI and documented it as an
  intentional, evidenced non-issue (`docs/out-of-scope.md`, new; a stale
  "TODO: segfault" comment removed from `src/winmm.cpp`); added
  `tests/test_loadstring_regressions.cpp`,
  `tests/test_mci_avivideo_regressions.cpp`.
* `8e7cf0a` (current HEAD) — Fixed the `PeekMessageA`
  diagnostic-counter gating bug; gated `SetTimer`/`KillTimer` logs; added
  `tests/test_gdi_regressions.cpp` (locks in already-correct `StretchBlt`/
  `GetPixel`/`SetPixel` behavior, which had no prior test coverage); added
  `docs/target-games.md`.

## 4. Current blocker / main problem

**There is no blocker preventing further work, and the previously-documented
one has been resolved (see below) — it was misdiagnosed, not actually an
environment limitation.**

* **Previously documented here as:** `basic_test` failing with `MCI_OPEN
  failed (305): Internal MCI error`, attributed to "SDL3's audio subsystem
  failing to initialize a real audio device/stream in this sandboxed dev
  environment."
* **Actual root cause (found this session):** `src/MidiMusic.cpp`'s
  `MCI_OPEN` handler had a hardcoded `return MCIERR_INTERNAL` behind a
  `//todo fix sigsegv` comment — a deliberate kill-switch from commit
  `a35f476c` ("MIDI was disabled"), unrelated to the sandbox's audio
  hardware. The real bug it was papering over was a dangling-pointer race
  in `MixerThread` (see section 2's "Recently completed" for the full
  explanation) — now fixed, and the kill-switch removed.
* **Current state:** `basic_test` passes; MIDI music (`MCI_OPEN`/
  `MCI_PLAY`/`MM_MCINOTIFY`/`MCI_CLOSE`) works end-to-end and is covered by
  `tests/test_mci_sequences.cpp`, including a stress test reproducing the
  exact race. Verified stable across 5+ repeated runs and a full
  AddressSanitizer+UBSan pass (zero errors).
* **Still needed:** a manual playtest of both games' actual background
  music, since audio correctness/quality cannot be judged in this headless
  sandbox (SoundFont is also not present here — "No SoundFont found" is
  expected in this environment and does not indicate a bug; see
  `src/MidiMusic.cpp`'s file-level doc comment for the lookup order).

## 5. Known bugs and limitations

* **Needs manual verification (real behavior changes made this session,
  cannot be judged in this headless sandbox):**
  * MIDI music playback (section 4) — needs an audible playtest.
  * `GetDeviceCaps(SIZEPALETTE)` fix (`src/wingdi_misc.cpp`, now returns 0
    instead of 256) — changes which rendering path both games take
    (planetblupi's minimap should now use its true-color path instead of
    the untested 8-bit-indexed placeholder; free-eggbert's true-color
    decor should no longer be force-disabled). Needs a visual playtest.
  * `BITMAPFILEHEADER`/`BITMAPINFOHEADER` packing fix (`include/wingdi.h`,
    now `#pragma pack(push, 2)`) — both games' `_lopen`/`_lread` real-BMP
    palette-fallback path was reading these structs misaligned by 2 bytes
    before this fix. Needs a visual playtest of palette-driven rendering.
* **Incomplete / needs verification:** the `WM_MOUSEMOVE` `MK_SHIFT`/
  `MK_CONTROL` fix (`src/internal/FreeApiMessageQueue.cpp`, from a prior
  session) cannot be automatically tested in this headless environment —
  confirmed empirically that `SDL_PushEvent`-injected key events do not
  update `SDL_GetKeyboardState()`. Needs a real manual playtest of Planet
  Blupi's shift-drag cell-highlight feature to fully confirm.
* **By design, not a bug:** `LoadStringA`'s generated table holds only one
  game's strings per compiled build (see section 1/2).
* **By design, not a bug:** MCI digital-video/AVI is permanently declined;
  both games already skip movies gracefully as a result (see
  `docs/out-of-scope.md`).
* **Needs verification / gotcha:** `cmake/ExtractStringTable.cmake` runs at
  CMake **configure** time via `execute_process`, not as a build-time
  custom command. Editing a game's `.rc`/`resource.h` file and re-running
  `cmake --build` alone will **not** pick up the change — a full
  `cmake -B <dir>` reconfigure is required. Not currently documented
  anywhere except here.
* **Resolved this session:** the `STRINGTABLE` parser in
  `ExtractStringTable.cmake` now emits a `message(WARNING ...)` for an
  unsupported `L"..."` wide-string entry or an embedded escaped
  double-quote, instead of silently mis-parsing (`TASK-0126`).
* **Unknown:** whether real joystick support is ever actually wanted for
  Free Eggbert — no evidence of user demand either way; current safe stub
  is sufficient for correctness.

## 6. Architecture notes

**Main modules:**
* `src/winuser_*.cpp` — window lifecycle, message queue, input, cursor,
  timers (`SetTimer`/`KillTimer`).
* `src/wingdi_*.cpp` — GDI bitmap/DC/blit subset (`StretchBlt`, `GetPixel`/
  `SetPixel`, `CreateBitmap`, `CreateCompatibleDC`).
* `src/winmm.cpp` + `src/MidiMusic.cpp` — WinMM/MCI/MIDI (TinySoundFont +
  TinyMidiLoader backend).
* `src/winbase*.cpp`, `src/crt_*.cpp` — file/path/CRT helpers.
* `src/internal/*` — shared internal state and helpers: `FreeApiMessageQueue`
  (the message queue itself + SDL event translation), `FreeApiWindowRegistry`
  (`HWND` -> `WNDPROC`/state maps), `FreeApiGdi` (`CompatDC`/`CompatBitmap`),
  `FreeApiPath` (path normalization), `FreeApiDiagnostics`, `FreeApiStringTable`
  (generated-`LoadStringA`-table lookup), `FreeApiTimers`, `FreeApiSdlVideo`.
* `src/winmain_bridge.cpp` — `WinMain` -> `main` entry-point bridge.
* `cmake/ExtractStringTable.cmake` — build-time `STRINGTABLE` extractor.

**Data flow:** SDL3 events -> `FreeApiMessageQueue::PumpSdlEvents` ->
internal message queue -> `PeekMessageA`/`GetMessageA` -> `DispatchMessageA`
-> the game's registered `WndProc` (looked up via `FreeApiWindowRegistry`).
**`WM_CREATE` and `WM_DESTROY` are delivered synchronously** (direct
`WndProc` call from `CreateWindowExA`/`DestroyWindow`), never via the queue.

**Important invariants — do not break these without re-verifying against
both target games:**
* Public headers (`include/`) must never expose SDL types.
* Every public API needs a real, cited usage site in `../free-eggbert` or
  `../planetblupi` (`docs/scope.md`).
* Both timer mechanisms (`timeSetEvent`/`timeKillEvent` and `SetTimer`/
  `KillTimer`/`WM_TIMER`) must keep working — each is the sole frame pump
  for one of the two games.
* `WM_MOUSEMOVE`'s `lParam` packing must stay bit-exact (`LOWORD`=x,
  `HIWORD`=y) — both games persist raw message triples for demo-file
  replay; any change breaks demo compatibility.
* Free API's "window size" **is** the client-area size (`CreateWindowExA`
  passes `nWidth`/`nHeight` straight to `SDL_CreateWindow`; `GetClientRect`
  reads it back unchanged) — unlike real Win32. `AdjustWindowRect` is
  deliberately a no-op identity transform because of this; do not "fix" it
  to add real Win32 chrome math without changing `CreateWindowExA`/
  `GetClientRect` in lockstep.
* `LoadStringA`'s real-text backing is per-build, per-game, determined by
  `CMAKE_PROJECT_NAME` at configure time (section 1).
* `free-direct` (sibling project) depends on the non-`WINAPI`, non-static C
  entry points `FreeApiCreateSurfaceDC`/`FreeApiDestroySurfaceDC`
  (`src/wingdi_dc.cpp`) to wrap a DirectDraw surface's pixel buffer as a
  GDI-compatible `Surface`-kind `HDC`. Their exact signatures must not
  change without updating `free-direct` too.
* DirectDraw/DirectSound/DirectPlay are explicitly **out of scope** for
  Free API — that's `free-direct`'s responsibility.
* ANSI-only (`A`-suffixed) functions are implemented; no real Unicode/`W`
  behavior (neither game builds with `UNICODE` defined).

## 7. Useful commands

```bash
# Standalone configure + build (system SDL3 required)
cmake -B build -DFREE_API_USE_SYSTEM_SDL3=ON -DFREE_API_BUILD_TESTS=ON
cmake --build build -j"$(nproc)"

# Run the full test suite
cd build && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure

# Reproduce the current known basic_test failure (section 4)
cd build && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./basic_test

# Run one specific test binary directly
cd build && SDL_VIDEODRIVER=dummy ./test_gdi_regressions
cd build && SDL_VIDEODRIVER=dummy ./test_loadstring_regressions

# Build via a real target game (integration test; also builds SDL3 from
# source the first time, which is slow)
cd ../planetblupi && cmake -B build && cmake --build build -j"$(nproc)"
cd ../free-eggbert && cmake -B build && cmake --build build -j"$(nproc)"
```

No lint/formatter is configured in this repository.

## 8. Next smallest tasks

**`plan.md`'s 126-item backlog is now 123 DONE / 1 OBSOLETE / 1 MANUAL / 1
deliberately-deferred** (every task carries a `Status:` line — see the plan
file itself for the authoritative per-task record). Essentially nothing
code- or doc-shaped remains to *implement*; what's left is verification that
requires a human or real hardware, which cannot be done in this sandbox:

1. **Manually playtest this session's three production behavior changes.**
   None of these can be verified visually/audibly in this headless sandbox
   — all are backed by passing automated tests (including ASan) proving
   the *mechanism* is correct, but only a real playtest confirms the
   *player-visible result* is right:
   * MIDI music now plays (was completely disabled by a kill-switch behind
     a real, now-fixed race condition — see section 4). Launch either game
     and listen for background music (needs a `.sf2` SoundFont present;
     see README's SoundFont section).
   * `GetDeviceCaps(SIZEPALETTE)` now returns 0 instead of 256 — Planet
     Blupi's minimap should render in real color (16-bit path) instead of
     greyscale (8-bit placeholder path); free-eggbert's true-color
     decor/rendering should no longer be force-disabled.
   * `BITMAPFILEHEADER`/`BITMAPINFOHEADER` packing fix — both games'
     palette-driven rendering (via the `_lopen`/`_lread` fallback) should
     look correct; previously every such read was misaligned by 2 bytes.
   Files: none to change unless a playtest finds a real regression.

2. **Manually verify the `MK_SHIFT`/`MK_CONTROL` fix (TASK-0048/0112) in a
   real playtest.** Confirm Planet Blupi's shift-drag cell-highlight
   feature works — cannot be automated here (`SDL_PushEvent`-injected key
   events don't update `SDL_GetKeyboardState()`).

3. **TASK-0102 (MANUAL):** confirm the joystick stub is an acceptable
   fallback by launching free-eggbert's options/setup screen with a real
   or virtual gamepad attached.

4. **TASK-0103 (deliberately deferred, optional):** real
   `joyGetPosEx`/`joyGetNumDevs` via SDL Gamepad/Joystick — only worth
   doing if a concrete need for real joystick input emerges; not a
   correctness requirement today.

## 9. Do not do yet

* Do not reintroduce a MIDI "kill switch" (a hardcoded early-return in
  `MidiMusicSendCommand`'s `MCI_OPEN` handler) if a crash is ever seen again
  in this area — the real bug was a dangling-pointer race in `MixerThread`
  (fixed this session by folding find-and-render into one lock
  acquisition), not something inherent to `MCI_OPEN` itself. Root-cause any
  future crash the same way (AddressSanitizer + read the exact lock scopes)
  rather than disabling the feature.
* Do not revert the `GetDeviceCaps(SIZEPALETTE)` value (now 0) back to a
  nonzero placeholder, or the `BITMAPFILEHEADER`/`BITMAPINFOHEADER`
  `#pragma pack(push, 2)`, without re-reading the analysis in `plan.md`
  TASK-0060/TASK-0084 first — both were real, evidenced bugs affecting both
  games' actual rendering paths, not stylistic changes.
* No general `.rc`/`.res` resource compiler — `ExtractStringTable.cmake`
  must stay narrowly `STRINGTABLE`-only.
* No real Unicode/`W` API implementations.
* No DirectDraw/DirectSound/DirectPlay work — that belongs to `free-direct`.
* No consolidating the two timer mechanisms (`timeSetEvent` vs. `SetTimer`)
  into one "unified" implementation.
* No "fixing" `AdjustWindowRect` to add real Win32 window-chrome math.
* No new public API without a cited `file:line` usage site in
  `../free-eggbert` or `../planetblupi`.
* No broad refactor of the GDI blit code in `src/wingdi_blit.cpp` — it is
  correct and now tested; leave it alone absent a specific new bug report.
* No rewriting `CMakeLists.txt`'s per-game string-table detection logic
  without re-verifying by building through both `../free-eggbert` and
  `../planetblupi` directly afterward (this exact class of bug — "looks
  right, silently picks the wrong game" — has already happened once).

## 10. Resume prompt

```
Read NEXT.md first. Inspect only the files needed for the first task in
section 8 ("Next smallest tasks") — do not read or refactor unrelated
code. Make one small, verified improvement (the first unchecked task from
that list, unless told otherwise). Run the exact verification command
listed for that task. Do not touch anything listed in section 9 ("Do not
do yet"). After finishing, update NEXT.md: move the completed item into a
"recently completed" note, refresh the test-status and blocker sections if
they changed, and leave the next-smallest-tasks list accurate for whoever
picks this up next.
```
