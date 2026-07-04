# NEXT.md

Handoff document for resuming work on `free-api`, for either a future
Claude Code session or a human developer. Reflects the actual repository
state as of commit `8e7cf0a` (2026-07-04). See [`plan.md`](plan.md) for the
full evidence-based usage audit and 124-item task backlog, and
[`docs/scope.md`](docs/scope.md) for the scope policy.

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
evidence-based audit (`plan.md`). The P0 foundation work (build
portability, core WinUser/message-loop correctness, file/path handling,
cursor visibility) is done; several real bugs found via that audit have
since been fixed; `LoadStringA` now returns real text; MCI digital-video
was investigated and resolved as an intentional non-issue; a round of
logging/diagnostics/GDI test cleanup is done.

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

**Test status:** 10 test binaries, **9 pass / 1 fails**. The failure
(`basic_test`) is a known, pre-existing, environment-specific issue (see
section 4) — every other test passes, including all tests added/modified
this session: `test_header_compile`, `test_winuser_regressions`,
`test_file_regressions`, `test_loadstring_regressions`,
`test_mci_avivideo_regressions`, `test_gdi_regressions`, `test_input_pipeline`,
`test_timeb`.

**CLI/tools/apps/libraries:** Free API produces one artifact,
`libfree-api.a`, plus its test binaries. It has no standalone CLI/app of its
own — it's a library consumed by the two games' own executables
(`SPEEDY_BLUPI_WINDOWS`, `PLANET_BLUPI_WINDOWS`), both of which build and
link successfully against the current code (verified this session).

**Recently implemented / fixed (this session, real behavior changes):**
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
* `CreateBitmap`'s 8-bit and 16-bit-to-RGBA32 conversion paths (used by
  Planet Blupi's minimap) are implemented but have **zero test coverage**.
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

**There is no blocker preventing further work.** The one open issue is a
known, environment-specific test failure that has not stopped any of the
above from being completed or verified:

* **Symptom:** `basic_test` fails with `MCI_OPEN failed (305): Internal MCI
  error` inside its `RunMidiCaseFallbackRegression` sub-test.
* **Failing command:** `ctest --output-on-failure` (or directly:
  `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./build/basic_test`).
* **Failing test:** `basic_test` (specifically the MCI-open-a-real-MIDI-file
  regression, not the `Sleep`/`GetTickCount` smoke test, which passes).
* **Affected files/modules:** `src/MidiMusic.cpp`, `src/winmm.cpp` (MCI/MIDI
  open path); `tests/basic_test.cpp` (the test itself, unmodified this
  session).
* **Suspected cause:** SDL3's audio subsystem/SDL3_mixer failing to
  initialize a real audio device/stream in this sandboxed dev environment,
  even with `SDL_AUDIODRIVER=dummy` set. This has been consistently
  reproducible across every build performed this session.
* **What's been tried / ruled out:** confirmed via `git stash` comparison
  and diff review that no change made this session touches
  `MidiMusic.cpp`/`winmm.cpp`'s actual MCI/MIDI-open logic — only an
  unrelated comment in `winmm.cpp` was edited. The failure is identical
  with and without an explicit `SDL_AUDIODRIVER` override. **Not yet
  tried:** running on a machine with real, working audio hardware/drivers;
  instrumenting SDL3_mixer's dummy-driver audio-stream-open path directly
  to find the exact failure point.

## 5. Known bugs and limitations

* **Confirmed bug / environment limitation:** `basic_test`'s MCI-open
  sub-test fails in this sandbox (section 4). Root cause not fully isolated
  (audio backend, not application logic).
* **Incomplete:** `CreateBitmap`'s 8-bit and 16-bit-to-RGBA32 conversion
  paths (`src/wingdi_bitmap.cpp`) have no test coverage. Implementation
  looks correct by inspection but is unverified.
* **Incomplete / needs verification:** the `WM_MOUSEMOVE` `MK_SHIFT`/
  `MK_CONTROL` fix (`src/internal/FreeApiMessageQueue.cpp`) cannot be
  automatically tested in this headless environment — confirmed
  empirically that `SDL_PushEvent`-injected key events do not update
  `SDL_GetKeyboardState()`. Needs a real manual playtest of Planet Blupi's
  shift-drag cell-highlight feature to fully confirm.
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
* **Suspected risk, unverified:** the `STRINGTABLE` parser in
  `ExtractStringTable.cmake` assumes no `L"..."` wide-string entries and no
  embedded escaped double-quotes in either game's `.rc`. Confirmed true for
  both files as they exist today; would silently mis-parse (or skip) an
  entry if either file changed to use that syntax, with no warning emitted.
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

1. **Add tests for `CreateBitmap`'s 8-bit and 16-bit conversion paths.**
   Goal: verify known 8-bit-indexed and RGB565 input bytes convert to the
   expected RGBA32 output (Planet Blupi's minimap rebuild path).
   Files: `tests/test_gdi_regressions.cpp` (extend), `src/wingdi_bitmap.cpp`
   (read-only reference).
   Verify: `cmake --build build --target test_gdi_regressions && ./build/test_gdi_regressions`

2. **Manually verify the `MK_SHIFT`/`MK_CONTROL` fix in a real playtest.**
   Goal: confirm Planet Blupi's shift-drag cell-highlight feature actually
   works now that `WM_MOUSEMOVE` carries live modifier state (cannot be
   automated in this environment — see section 5).
   Files: none to change; `src/internal/FreeApiMessageQueue.cpp` is the
   implementation under test.
   Verify: launch Planet Blupi, hold Shift while dragging over cells,
   confirm highlight behavior matches expectations.

3. **Add a defensive check/warning to `ExtractStringTable.cmake` for
   unsupported `STRINGTABLE` syntax.** Goal: emit a `message(WARNING ...)`
   if a `.rc` file contains `L"..."` wide-string entries or escaped
   double-quotes within a `STRINGTABLE` block, so a future `.rc` edit
   doesn't silently lose data.
   Files: `cmake/ExtractStringTable.cmake`.
   Verify: `cmake -B build -DFREE_API_USE_SYSTEM_SDL3=ON` and confirm the
   logged string counts are unchanged (257 for planetblupi / 364 for
   free-eggbert, depending on which is detected).

4. **Add `docs/supported-apis.md`** (plan.md `TASK-0005`): a hand-maintained
   reference table of every implemented public symbol and its status.
   Files: `docs/supported-apis.md` (new).
   Verify: none — documentation only.

## 9. Do not do yet

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
