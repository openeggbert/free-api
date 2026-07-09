# NEXT.md

Handoff document for resuming work on `free-api`, for either a future
Claude Code session or a human developer. Reflects the actual repository
state as of commit `3a0a493` on `develop` (2026-07-09), working tree
clean, pushed to `origin/develop`.

**Before trusting any number in this file, re-verify it** — this file
has gone stale faster than expected before. Quick checks:
`grep -cE '^### TASK-(24H-)?[0-9]{4}' plan.md` (task count),
`grep -c '^Status: DONE' plan.md` (done count), `grep -c add_test
CMakeLists.txt` (test count), `git log -1 --oneline` (current commit).

See [`plan.md`](plan.md) for the full task backlog,
[`audit.md`](audit.md) for the current deep audit,
[`docs/quality-assessment.md`](docs/quality-assessment.md) for an
independent code-quality review, and `git log` for full commit history —
this file intentionally does not repeat what those already record.

## 1. Project summary

Free API is a small, SDL3-backed C++ static library that exposes a
Win32/WinAPI-like public surface (`windows.h`, `winuser.h`,
`mmsystem.h`, `digitalv.h`, etc.) so two specific legacy Windows games
can compile and run on Linux/macOS/Web/Android without Microsoft
Windows:

* **Free Eggbert** (`../free-eggbert`, top-level CMake project
  `SPEEDY_BLUPI_WINDOWS`)
* **Planet Blupi** (`../planetblupi`, top-level CMake project
  `PLANET_BLUPI_WINDOWS`)

**Main goal:** source-level compatibility for exactly these two games —
not Wine, not a general WinAPI reimplementation, not a platform for
arbitrary 1998-era Windows software. Every public API must cite a real
usage site (`file:line`) in one of the two games' own source
(`docs/scope.md`), with one documented exception (the `free-direct`
bridge functions, declared in `include/free_api_bridge.h`) — mechanically
enforced by `cmake/CheckPublicSurfaceBaseline.cmake` (see §6).

**Current development phase: maintenance / stabilization.** The entire
AI-doable backlog (`plan.md`, both task-ID namespaces) is closed — see
§2 for the exact numbers. Recent work has shifted from bug-fixing to
verification-in-depth: a `gcov` coverage sweep, a `cppcheck`+`clang-tidy`
static-analysis pass, a 3-way maintainability/architecture review, and an
independent code-quality assessment. Remaining open items are either
human-only (real-hardware playtests) or contingent on a future audit
finding something new.

**Important architectural decisions:**

* Free API is a **static library**, normally built as a sibling
  `add_subdirectory()` of one of the two target games (which also
  provide SDL3). It also builds standalone via
  `-DFREE_API_USE_SYSTEM_SDL3=ON`.
* SDL3 is an **internal backend detail only** — public headers in
  `include/` must never expose SDL types.
* `CMakeLists.txt` keys off `CMAKE_PROJECT_NAME` to detect which game (if
  any) is driving the build, or an explicit `-DFREE_API_TARGET_GAME=...`
  override (`docs/cmake-options.md`).
* The two target games use **two different, mutually exclusive live
  timer mechanisms**: Free Eggbert uses `timeSetEvent`/`timeKillEvent`;
  Planet Blupi uses `SetTimer`/`KillTimer`/`WM_TIMER`. Both must keep
  working.
* The `free-direct` bridge exception has one single-source-of-truth
  header (`include/free_api_bridge.h`) — do not re-declare
  `FreeApiCreateSurfaceDC`/`FreeApiDestroySurfaceDC`/
  `FreeApiSetWindowFullscreen` anywhere else.
* `FREE_API_SANITIZE` CMake option (`thread`/`address`, off by default)
  runs the test suite under ThreadSanitizer/AddressSanitizer with no
  manual `LD_PRELOAD` needed — see `docs/cmake-options.md`.

## 2. Current status

**Build status** (last fully verified at commit `1d0e3f4`; commit
`3a0a493` only added a markdown doc, no source changed, so this still
holds):
* Standalone (`-DFREE_API_USE_SYSTEM_SDL3=ON`): configures, builds,
  **29/29** tests pass.
* As a subdirectory of `../free-eggbert` (Ninja), including the
  `free-api`+`free-direct` diamond dependency: 29/29.
* As a subdirectory of `../planetblupi` (Make): 29/29.
* `../free-direct` standalone: configures, builds, links cleanly against
  `include/free_api_bridge.h`.
* Same 29-test suite passes under both `-DFREE_API_SANITIZE=thread` and
  `=address` (LeakSanitizer included, enabled by default).

**Test status:** 29 CTest entries, 29/29 passing in all three build
modes and both sanitizer builds. `test_winuser_regressions` fails 6
checks under this sandbox's default Wayland display — a confirmed
environment artifact, not a code bug; passes under
`SDL_VIDEODRIVER=dummy` or real X11/Xvfb.

**CLI/tools/apps/libraries available:** `free-api` is a library
(`libfree-api.a`), not a standalone CLI/app. The 29 test binaries
(`build/test_*`) are the closest thing to runnable demos/examples — each
exercises one subsystem in isolation (GDI, MCI/MIDI, winuser messages,
file I/O, joystick, timers, etc.) and can be run directly. The two real
end-to-end demos are the target games themselves: `SPEEDY_BLUPI_WINDOWS`
(`../free-eggbert`) and `PLANET_BLUPI_WINDOWS` (`../planetblupi`), both
build and link cleanly; actual on-screen/audio behavior is only
human-verifiable (see §5/§8).

**Recently implemented (this round, see §3 for detail):**
`docs/quality-assessment.md` (independent quality review),
`tests/test_midi_soundfont_rendering.cpp` (first real TinySoundFont
rendering-path test), gated GDI diagnostic counters behind
`FreeApiDiagnosticsFastEnabled()`, marker-comment-based SDL log-gating
allowlist, `tests/support/MidiFixtures.hpp` fixture consolidation.

**What does NOT work / known gaps:** MCI digital-video/AVI is
permanently out of scope (by design). `CreateBitmap`'s 8-bit indexed
path is greyscale-only. Four tasks need a human with real display/audio
hardware — see §5/§8.

## 3. Recent changes

Most recent first. Full per-task detail: `plan.md`; full commit detail:
`git log`.

* `3a0a493` — Added `docs/quality-assessment.md`: independent review of
  WinAPI semantic fidelity, code craftsmanship, test depth, and
  architecture. No source changes.
* `1d0e3f4` — Maintainability sweep: closed 5 tasks that had been
  silently dormant in the original `TASK-0001`–`0012` namespace
  (`0001` undocumented `SetRect`/`IntersectRect`/`UnionRect` — now in
  `docs/supported-apis.md`; `0004` gated GDI diagnostic counters behind
  `FreeApiDiagnosticsFastEnabled()` in `src/wingdi_dc.cpp`,
  `src/wingdi_bitmap.cpp`, `src/internal/FreeApiGdi.cpp`, deliberately
  leaving 4 test-critical counters ungated; `0005` added explanatory
  single-window-assumption comments to
  `src/internal/FreeApiWindowRegistry.hpp`/`FreeApiMessageQueue.hpp`;
  `0008` was a false-TODO, already done under a different ID; `0010`
  added a missing `LoadImageA` regression test). Rewrote
  `tests/test_sdl_log_gating.cpp`'s allowlist from a `{file, line}` set
  to inline `// sdl-log-gating: intentional` marker comments (immune to
  line-number drift). Consolidated 3 hand-duplicated copies of
  `WriteMinimalMidi` into `tests/support/MidiFixtures.hpp` (new file).
* `b890221` — Closed 3 gaps found by a `gcov` coverage sweep
  (`TASK-24H-1251`–`1253`): added
  `tests/test_midi_soundfont_rendering.cpp` (new file, first test to
  exercise the real TinySoundFont rendering path via a
  programmatically-built minimal SF2 fixture) — building it found and
  fixed a real heap-buffer-overflow in vendored `external/tsf.h`
  (missing trailing guard-samples) and reproduced/worked around an
  already-known `SDL_Quit()`-ordering SEGV; added
  `TestGlobalMemoryStatusPopulatesPlausibleValues` and
  `TestGetActiveWindowFallsBackBeforeFocusIsSet` to
  `tests/test_winuser_regressions.cpp`.
* `85ba871` — Documentation refresh after two audit rounds: corrected
  stale test counts and sanitizer-option descriptions in
  `docs/cmake-options.md`/`docs/testing.md`, cross-referenced the
  automated public-surface-baseline check in
  `docs/public-surface-audit.md`, rewrote `README.md`'s status section.
* `a8cf266` and earlier — see `git log` / `plan.md`'s "Deep Audit
  Follow-up" sections for the two full ground-up audit rounds
  (`TASK-24H-1229`–`1250`) that preceded this round: a GDI handle
  double-free fix, a MIDI static-destruction-order race fix
  (`GetMidiState()` Meyer's-singleton pattern), an automated
  public-surface-baseline CTest guard, and LeakSanitizer enabled by
  default (blanket `detect_leaks=0` removed).

## 4. Current blocker / main problem

**No blocker.** All build configurations work, 29/29 tests pass
everywhere (default and both sanitizer builds). Working tree is clean;
`develop` is pushed and matches `origin/develop` at `3a0a493`.

## 5. Known bugs and limitations

* **Incomplete (by design, evidence-backed):** `StretchBlt` only
  supports `SRCCOPY` (`src/wingdi_blit.cpp`); `mciGetDeviceIDA` ignores
  `lpszDevice` and always returns `1` (`src/winmm.cpp:368-372`, safe
  because both games only ever open one MCI device); `_lopen` ignores
  `iReadWrite` and always opens read-only (`src/winbase_file.cpp:43-69`);
  `CreateBitmap`'s 8-bit indexed path is greyscale-only (only reached by
  planetblupi's minimap in fullscreen mode); no real `WM_ACTIVATEAPP(0)`
  delivery or true blocking `WaitMessage` — all documented, evidenced,
  permanent decisions, see `docs/out-of-scope.md`.
* **Documented, not fixed (real but unreachable by evidenced call
  sites):** `FreeApiDestroySurfaceDC`/`AsCompatDC` segfaults on a
  genuinely garbage, never-allocated pointer instead of returning
  `FALSE` safely; `CompatDC::selectedBitmap`'s dangling-pointer risk —
  both traced and confirmed unreachable by either game's real code paths
  (`docs/out-of-scope.md`).
* **Documented, not fixed (explicit scope decision, left open):**
  `../free-direct`'s `Diagnostics.cpp` reaches past the documented
  bridge exception into the internal `FreeApi::Platform::ReadRssKB()`
  symbol directly — see `docs/scope.md`.
* **Confirmed environment artifact, not a code bug:**
  `test_winuser_regressions` fails 6 checks under this sandbox's default
  Wayland display; passes under `SDL_VIDEODRIVER=dummy` or real X11/Xvfb.
* **Needs human verification** (all four formally tracked, acceptance
  criteria in `docs/target-game-verification.md`): `MK_SHIFT`
  drag-select (`TASK-24H-0401`), `MK_CONTROL` level-editor flood-fill
  (`TASK-24H-0403`), MIDI audio sign-off (`TASK-24H-1221`), rendering/
  save-load sign-off (`TASK-24H-1222`) — none of these are completable
  by a Claude Code session; they need a real display/audio backend and a
  human judgment call.
* **By design, not a bug:** MCI digital-video/AVI is permanently
  declined (no evidenced call site in either game).

## 6. Architecture notes

* **Main modules:** `src/wingdi_*.cpp` (GDI — device contexts, bitmaps,
  blitting), `src/winmm.cpp`/`src/MidiMusic.cpp` (MCI/MIDI/joystick/
  timers), `src/winuser_*.cpp` (windows, messages, input),
  `src/winbase_*.cpp` (files, paths, process/env), `src/internal/*`
  (SDL3 bridging, window/message-queue registries, path normalization —
  never exposed via public headers).
* **Data flow:** target-game code calls WinAPI-shaped functions declared
  in `include/*.h` → `src/*.cpp` implementations translate to SDL3 calls
  → SDL3 handles the real OS interaction. No SDL types ever cross back
  into public headers.
* **Important invariants:**
  - GDI handles (`CompatDC`/`CompatBitmap`) use an in-struct magic-number
    tag, cleared before `delete`, to make double-free detectable rather
    than a silent use-after-free.
  - `MidiMusic.cpp`'s `g_midi` is a function-local static via
    `GetMidiState()` (Meyer's singleton) — the language guarantees
    destruction order relative to other statics; do not revert to a
    plain namespace-scope static.
  - `src/internal/FreeApiWindowRegistry.hpp`'s 5 globals and
    `FreeApiMessageQueue.hpp`'s `g_mouseButtons` are deliberately
    unsynchronized — safe only under the documented single-live-window,
    single-thread assumption (`docs/out-of-scope.md`). Do not touch from
    a new background thread without adding real synchronization first.
  - `tests/test_gdi_regressions.cpp` reads `g_diagCompatDcs`/
    `g_diagCompatDcsEver`/`g_diagCompatBitmaps`/`g_diagCompatBitmapsEver`
    directly as an always-on leak-detection primitive, independent of
    the diagnostics-enabled flag — these 4 counters must stay ungated.
* **Boundaries that should not be broken:** public headers in
  `include/`/`include_non_windows/` must never include anything from
  `src/internal/`; the `check_public_surface_baseline` CTest test fails
  the build if any new public declaration appears without a matching
  entry in `cmake/known-public-symbols.txt`.
* **API/compatibility rules:** every public API needs a cited `file:line`
  usage site in `../free-eggbert` or `../planetblupi` (`docs/scope.md`),
  except the documented `free-direct`-bridge exception
  (`include/free_api_bridge.h`).
* **Must remain stable:** `FREE_API_SANITIZE` build option semantics;
  the two games' respective timer mechanisms; `MidiState::backendInitFailed`
  latch (prevents re-logging on every `MCI_OPEN` after an audio-init
  failure); LeakSanitizer enabled by default under
  `FREE_API_SANITIZE=address` (do not reintroduce
  `ASAN_OPTIONS=detect_leaks=0` — use a scoped `LSAN_OPTIONS=suppressions=`
  if a genuine SDL3 false positive is ever found).

## 7. Useful commands

```bash
# Build via a real target game
cd ../free-eggbert/cmake-build-debug && cmake . && ninja -j"$(nproc)"
cd ../planetblupi/build && cmake . && make -j"$(nproc)"

# Standalone build (no sibling game needed)
cmake -S . -B build -DFREE_API_USE_SYSTEM_SDL3=ON -DFREE_API_BUILD_TESTS=ON
cmake --build build -j"$(nproc)"
cd build && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure

# Sanitizer builds -- plain ctest works, no manual LD_PRELOAD
cmake -B build-tsan -DFREE_API_SANITIZE=thread -DFREE_API_USE_SYSTEM_SDL3=ON
cmake --build build-tsan -j"$(nproc)"
cd build-tsan && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure

cmake -B build-asan -DFREE_API_SANITIZE=address -DFREE_API_USE_SYSTEM_SDL3=ON
cmake --build build-asan -j"$(nproc)"
cd build-asan && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure

# Reproduce the known Wayland-only test_winuser_regressions failures
cd build && ctest -R test_winuser_regressions --output-on-failure   # fails under default Wayland
cd build && SDL_VIDEODRIVER=dummy ctest -R test_winuser_regressions --output-on-failure  # passes

# Run one demo/example test binary directly
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./build/test_midi_soundfont_rendering
```

No lint/formatter is configured in this repository. See
`docs/testing.md` for the full canonical test reference and
`docs/cmake-options.md` for the full canonical build-option reference.

## 8. Next smallest tasks

The AI-doable P0–P3 backlog in `plan.md` is fully closed. These are the
next concrete, bounded, single-session tasks, in priority order:

1. **Re-run static analysis at current HEAD.** Goal: confirm `cppcheck`
   and `clang-tidy` are still clean after the 2 commits since the last
   run. Files: none expected to change unless something is found.
   Verify: `cppcheck --enable=warning,performance,portability,style
   --check-level=exhaustive src include` and a `clang-tidy` pass over
   `compile_commands.json` from a `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
   build; expect zero new findings.
2. **Re-run the `gcov` coverage sweep.** Goal: confirm the previously
   measured 72.1% weighted `src/**` coverage figure still holds after
   `test_midi_soundfont_rendering` and the other Session-7 test
   additions, and check whether any function is still at 0%. Files:
   none expected unless a real gap is found. Verify:
   `-DCMAKE_CXX_FLAGS="--coverage -O0"
   -DCMAKE_EXE_LINKER_FLAGS="--coverage"` build, `ctest`, then `gcov -o
   <objdir> <file>.cpp` per translation unit.
3. **Skeptical re-audit of the highest-churn files.** Goal:
   independently re-verify `src/wingdi_dc.cpp`, `src/wingdi_bitmap.cpp`,
   `src/internal/FreeApiGdi.cpp` (all touched by the diagnostic-counter
   gating change) against current source rather than assuming the
   `DONE` status still holds. Files: the three above plus their tests
   in `tests/test_gdi_regressions.cpp`. Verify: full `ctest` in all 5
   build configurations (standalone, both target games, `build-tsan`,
   `build-asan`) — expect 29/29 in every one.
4. **Human playtest pass (not AI-doable).** Goal: complete the 4
   remaining `TODO` tasks (`TASK-24H-0401`/`0403`/`1221`/`1222`) via the
   single checklist at `docs/target-game-verification.md`. Requires a
   human with a real display and audio backend.

If tasks 1–3 all come back clean and no human playtest is available, the
next productive step is a fresh, independent, skeptical full-codebase
re-audit (the pattern that has found real gaps every time it's been run
historically — see `git log` / `plan.md` for precedent). Do not invent
speculative new tasks just to have something to do.

## 9. Do not do yet

* No broad refactor of the GDI diagnostic-counter gating scheme just
  added — it was implemented carefully to avoid breaking
  `tests/test_gdi_regressions.cpp`'s leak-detection reads; see §6.
* No unrelated cleanup or speculative architecture changes while the
  backlog is closed — if nothing concrete is broken, prefer the
  re-verification tasks in §8 over inventing new work.
* No new public API without a cited `file:line` usage site in
  `../free-eggbert` or `../planetblupi` — except the documented
  `free-direct`-bridge exception, mechanically enforced by
  `check_public_surface_baseline`.
* No API/header changes without checking
  `cmake/known-public-symbols.txt` and `docs/scope.md` compatibility
  rules first.
* Do not revert any of these specific, evidenced fixes without a
  deliberate, re-justified decision (all have a `plan.md`/`docs/*`
  entry explaining why):
  - `GetMidiState()` (`src/MidiMusic.cpp`) back to a plain
    `static MidiState g_midi;`.
  - `ASAN_OPTIONS=detect_leaks=0` (`CMakeLists.txt`) — use a scoped
    `LSAN_OPTIONS=suppressions=<file>` instead if ever needed.
  - `ResolveJoystick`'s bounds check (`src/winmm.cpp`) to comparing
    `static_cast<int>(uJoyID) < count`.
  - `g_debugInput` (`src/internal/FreeApiMessageQueue.{hpp,cpp}`) to a
    plain `bool`.
  - `tests/test_sdl_log_gating.cpp`'s marker-comment allowlist back to a
    `{file, line}` set.
  - The 4 GDI counters (`g_diagCompatDcs(Ever)`/`g_diagCompatBitmaps(Ever)`)
    to being gated behind `FreeApiDiagnosticsFastEnabled()`.
* Do not re-copy `WriteMinimalMidi`'s fixture bytes into a new test file
  — include `tests/support/MidiFixtures.hpp` instead.
* Do not touch `joystick`, MCI digital-video, DirectDraw, DirectSound,
  DirectPlay, or `free-direct` without a specific, separately-scoped
  request — except `include/free_api_bridge.h`'s three declarations.
* When writing a new test in a file that includes `<windows.h>`,
  remember `fopen()` is globally macro-redirected to `free_api_fopen`
  (`docs/headers.md`) — use relative temp-file paths, not absolute ones.
* Do not dispatch a fork/subagent to edit files on this repo while
  making direct edits yourself in parallel — read-only parallel research
  forks are fine; concurrent *writes* are not.

## 10. Resume prompt

```
Read NEXT.md first. Inspect only the files needed for the first task in
section 8 ("Next smallest tasks") — do not read or touch unrelated parts
of the codebase. Do not refactor anything not required for that task.

Make one small, verified improvement: implement the first task, run its
listed verification command, and confirm the result matches what's
expected before moving on. Do not start a second task in the same
session unless the first is fully verified and committed.

After finishing, update NEXT.md: refresh section 2 (current status) and
section 3 (recent changes) with what actually happened, move the
completed task out of section 8, and re-verify every number in this file
against current source before trusting it (see the note at the top of
this file).
```
