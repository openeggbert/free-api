# NEXT.md

Handoff document for resuming work on `free-api`, for either a future
Claude Code session or a human developer. Reflects the actual repository
state as of commit `59dacc3` on `develop` (2026-07-18), plus a `gcov`
coverage sweep and two new regression tests it motivated, performed the
same day, which were pending commit as of writing — check `git log -1`
to see if they've landed yet.

**Before trusting any number in this file, re-verify it** — this file
has gone stale faster than expected before. Quick checks:
`grep -cE '^### TASK-(24H-)?[0-9]{4}' plan.md` (task count),
`grep -c '^Status: DONE' plan.md` (done count), `grep -c add_test
CMakeLists.txt` (test count), `git log -1 --oneline` (current commit).

See [`plan.md`](plan.md) for the full task backlog and
[`docs/quality-assessment.md`](docs/quality-assessment.md) for an
independent code-quality review, and `git log` for full commit history —
this file intentionally does not repeat what those already record.
**`audit.md` no longer exists** — it was a point-in-time deep-audit
document; its 6 findings were all fixed and it was deleted on 2026-07-18
once nothing in it remained open (see `git log` around that date). The
`todo/` directory (`Embedded_Resources_FreeAPI.md`,
`free-api-performance-todo.md`) was deleted the same day for the same
reason — one was a deliberately-deferred design doc with no evidenced
need, the other's one open item was profiled and shown not worth doing.
A sibling `../freeapiissues.md` file (outside this repo, in the shared
`openeggbert/` directory) existed briefly and was also fully resolved and
deleted.

## 1. Project summary

Free API is a small, SDL3-backed C++ static library that exposes a
Win32/WinAPI-like public surface (`windows.h`, `winuser.h`,
`mmsystem.h`, `digitalv.h`, etc.) so two specific legacy Windows games
can compile and run on Linux/macOS/Web/Android without Microsoft
Windows:

* **Free Eggbert** (`../free-eggbert`, top-level CMake project
  `SPEEDY_BLUPI_WINDOWS`) — has a real, working Android port (Gradle/NDK
  project under `../free-eggbert/android/`, see its `ANDROID.md`).
* **Planet Blupi** (`../planetblupi`, top-level CMake project
  `PLANET_BLUPI_WINDOWS`) — same, `../planetblupi/android/`.

**Main goal:** source-level compatibility for exactly these two games —
not Wine, not a general WinAPI reimplementation, not a platform for
arbitrary 1998-era Windows software. Every public API must cite a real
usage site (`file:line`) in one of the two games' own source
(`docs/scope.md`), with one documented exception (the `free-direct`
bridge functions, declared in `include/free_api_bridge.h`) — mechanically
enforced by `cmake/CheckPublicSurfaceBaseline.cmake` (see §6).

**Current development phase: maintenance / stabilization.** The entire
AI-doable backlog (`plan.md`, both task-ID namespaces) is closed — see
§2 for the exact numbers. Recent work has been a mix of targeted bug
fixes found by re-reading real game call sites (MIDI SoundFont lookup,
path normalization, mixer-thread pacing) and documentation cleanup
(deleting several stale audit/TODO documents whose findings had all
already been resolved). Remaining open items are either human-only (real-
hardware playtests) or contingent on a future audit finding something
new.

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
* Android support is real and already works (not speculative future
  work) — `src/winmain_bridge.cpp` has an `SDL_main` entry point and APK
  asset extraction. Web/Emscripten status is comparatively unverified —
  no `__EMSCRIPTEN__`-gated code exists in `free-api` itself, though
  `../free-eggbert` has its own `web_persistence.cpp`.

## 2. Current status

**Build status** (fully verified at commit `3bb0fd6`, 2026-07-18, all 5
configurations, all under `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy`;
29-count predates the 2 test cases §3 describes adding, which landed in
existing binaries so the total test *count* is unaffected):
* Standalone (`-DFREE_API_USE_SYSTEM_SDL3=ON`): **29/29**.
* As a subdirectory of `../free-eggbert` (Ninja): **29/29**.
* As a subdirectory of `../planetblupi` (Make): **29/29**.
* `-DFREE_API_SANITIZE=thread`: **29/29**.
* `-DFREE_API_SANITIZE=address` (LeakSanitizer included): **29/29**.
* `test_winuser_regressions` fails some checks under this sandbox's
  default Wayland display (no `SDL_VIDEODRIVER` override) — a confirmed
  environment artifact, not a code bug; the 29/29 figures above were all
  obtained under `SDL_VIDEODRIVER=dummy`.

**Test coverage** (`gcov`, weighted across `src/**.cpp` with executable
lines, re-measured 2026-07-18 in a `--coverage -O0` build): **78.53%**
(1357/1728 lines), up from a previously recorded 72.1% — see §3 for what
changed. One file remains at 0% (`src/platform/PlatformProcessInfo.cpp`,
10 lines) by design: it's only reached through `FreeApiDiagSnapshot`,
which is gated behind the opt-in `FREE_API_DIAGNOSTICS`/
`FREE_DIRECT_DIAGNOSTICS` env vars that the standard test run doesn't
set — diagnostic/observability code, not game logic, left untested
rather than adding an env-var-manipulating test for it.

**Task backlog status** (`plan.md`): 220 tasks total — **215 DONE, 1
OBSOLETE, 4 TODO**. The 4 open tasks are all human-only playtests (see
§5/§8) — nothing AI-doable remains in the formal backlog.

**CLI/tools/apps/libraries available:** `free-api` is a library
(`libfree-api.a`), not a standalone CLI/app. The 29 test binaries
(`build/test_*`) are the closest thing to runnable demos/examples — each
exercises one subsystem in isolation (GDI, MCI/MIDI, winuser messages,
file I/O, joystick, timers, etc.) and can be run directly. The two real
end-to-end demos are the target games themselves: `SPEEDY_BLUPI_WINDOWS`
(`../free-eggbert`) and `PLANET_BLUPI_WINDOWS` (`../planetblupi`).

**Recently implemented (this round, see §3 for detail):** fixed MIDI
SoundFont lookup to also try the executable's own directory; fixed
`NormalizeMidiPath` corrupting genuinely-absolute MCI element paths;
fixed `MixerThread` rendering audio far faster than real time; fixed
`OutputDebugStringW` to do a real UTF-16→UTF-8 conversion instead of
truncating each `wchar_t`; deleted `audit.md`, `todo/*.md`, and the
sibling `../freeapiissues.md` after verifying every finding in them was
either already fixed, confirmed unreachable by both games' real code
paths, or (for the one genuinely open perf question) profiled and shown
not worth doing.

**What does NOT work / known gaps:** MCI digital-video/AVI is
permanently out of scope (by design). `CreateBitmap`'s 8-bit indexed
path is greyscale-only. Four tasks need a human with real display/audio
hardware — see §5/§8. Web/Emscripten build readiness is unverified (see
§1's architectural-decisions note).

## 3. Recent changes

Most recent first. Full per-task detail: `plan.md`; full commit detail:
`git log`.

* *(pending commit)* — Re-ran the `gcov` coverage sweep (§8 task 1):
  weighted `src/**` coverage is now 78.53% (1357/1728 lines), up from a
  previously recorded 72.1%. Investigating the two lowest-coverage files
  found two real, actionable gaps and fixed both with new regression
  tests: `ScaleCompatBitmap` (`src/internal/FreeApiGdi.cpp`, exercised by
  `LoadImageA`'s `cx`/`cy` != 0 branch) had zero coverage — added
  `TestLoadImageAWithNonZeroCxCyScalesViaScaleCompatBitmap` to
  `tests/test_gdi_regressions.cpp` (`FreeApiGdi.cpp` coverage
  47%→87%); this session's own `Utf16ToUtf8`/`OutputDebugStringW` fix
  (see the `59dacc3`-and-earlier entry below) had been verified only with
  an ad hoc throwaway script, not a real test — added
  `TestOutputDebugStringWConvertsUtf16ToUtf8AndIsNullSafe` to
  `tests/test_winuser_regressions.cpp` (`winbase.cpp` coverage
  37%→87%), asserting the exact UTF-8 byte sequence for a BMP character
  (€) and a surrogate-pair-encoded astral character (🙂). Both new test
  cases landed in existing test binaries, so the total test *count*
  (29) is unchanged; re-verified 29/29 after adding them. The one
  remaining 0%-coverage file (`PlatformProcessInfo.cpp`) was left as-is
  — see §2 for why.
* `59dacc3` — Skeptical re-audit of this round's real bug fixes
  (§8 task 2): independently re-read `NormalizeMidiPath`
  (`src/MidiMusic.cpp:296-380`, the two-phase absolute-path-then-CWD-
  relative logic) and the MCI_PLAY lock-order + `MixerThread` backpressure
  fix (`src/MidiMusic.cpp:392-535`) against current source, checking edge
  cases (colliding real absolute paths, empty strings, paths with no `/`,
  lock/notify ordering, the backpressure math at
  `kMaxQueuedBlocksAhead=4`/`kBlockFrames=512`/`kThrottleSleepMs=5`) —
  found no defects. Then ran the full test suite in all 5 build
  configurations (standalone, `../free-eggbert` subdirectory,
  `../planetblupi` subdirectory, `build-tsan`, `build-asan`), all
  **29/29** under `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy` — see §2
  for the updated per-configuration figures.
* `3bb0fd6` — Re-ran static analysis (§8 task 1, first run since
  `1d0e3f4`): `cppcheck --enable=warning,performance,portability,style
  --check-level=exhaustive` on `src`/`include` (with `-I /usr/local/include`
  added for SDL3 so `SDL_PRIs64` doesn't false-positive as an unknown
  macro) came back with only cosmetic `style`-level findings in
  `free-api`'s own code (C-style casts in `%p` log format strings,
  "use an STL algorithm" suggestions) — no errors/warnings/
  performance/portability findings outside vendored `external/`
  (TinySoundFont/TinyMidiLoader, out of scope). A `clang-tidy` pass over
  a fresh `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` build (default checks,
  `-header-filter='free-api/(src|include)/'` — the default header filter
  hides warnings from any header, which is why the first pass without it
  showed 0) found 2 real, if low-severity, issues and both were fixed:
  `include/handleapi.h`'s header guard checked `FREE_API_HANDLEAPI_H` in
  `#ifndef` but defined `FREE_API_WINDOWS_HANDLEAPI_H` — a typo that made
  the guard never actually protect against double inclusion (silently
  harmless today only because the file happens to declare nothing but an
  idempotent function prototype); and a `/*`-in-comment false-trigger in
  `include/winuser.h`'s `LoadStringA` Doxygen comment (literal text
  `resource/*.rc`, reworded to avoid the false nested-comment warning, no
  meaning change). Re-verified 29/29 tests pass after both fixes.
* `471621c` — Fixed `OutputDebugStringW` (`src/winbase.cpp`) to perform a
  real UTF-16→UTF-8 conversion (surrogate-pair aware, replacement
  character for unpaired surrogates) instead of truncating each
  `wchar_t` to `char`. Currently unreachable by either target game
  (neither defines `UNICODE`, so their `OutputDebugString` calls resolve
  to the `A` variant) but was cheap and correct to fix regardless.
* `12bbd0c` — Deleted `todo/free-api-performance-todo.md`. Walked through
  every item with the user: all P0/P1 items verified fixed in current
  source, P2 test/profiling items verified present, and the one
  genuinely open item (`std::vector<uint8_t>` → `uint32_t` pixel
  buffers) was benchmarked against the real `StretchBlt` code paths —
  the dominant 1:1-blit path goes through `memcpy` (element type
  irrelevant) and the scaled path's theoretical 2x speedup only applies
  to blits that are rare and small in real gameplay. Not worth the
  byte-ordering/18-call-site risk the original TODO flagged.
* `3935a1f` — Deleted `audit.md` (all 6 proposed tasks verified
  implemented — `TASK-24H-1245`–`1250`) and
  `todo/Embedded_Resources_FreeAPI.md` (a deliberately-deferred WinAPI
  resource-emulation design doc; source-level verification against
  `free-eggbert`/`planetblupi` showed none of its 4 resource types
  (STRINGTABLE/BITMAP/CURSOR-ICON/WAVE) need it — STRINGTABLE already has
  its own narrow extractor, BITMAP resources never existed even in the
  original `.rc` files, WAVE loading is dead code, and CURSOR/ICON data
  is visually inconsequential since both games hide the OS cursor and
  draw their own sprite).
* `76c55ea` — Fixed MIDI SoundFont lookup (`LoadSoundFont()`) to also try
  the executable's own directory via `SDL_GetBasePath()`, not just the
  process's current working directory, so a bundled `default.sf2` is
  found regardless of how the game is launched (IDE run config, Steam
  "Start In" shortcut, plain `./exe` invocation).
* `3635439` — Fixed `NormalizeMidiPath` corrupting genuinely-absolute MCI
  element paths built by `CSound::PlayMusic` via
  `GetCurrentDirectory()+strcat()` when the process's real working
  directory is itself absolute (e.g. launched from an IDE) — the old
  logic unconditionally stripped a leading slash assuming a Windows
  `"\User"`-style CWD-relative path, breaking genuinely-rooted paths.
* `10d58c2` — Fixed `MixerThread` rendering PCM far faster than real time
  (no backpressure — `SDL_PutAudioStreamData` is non-blocking, so
  without a cap the mixer pegged a CPU core and `active->timeMs` (and
  therefore the `MM_MCINOTIFY` "song finished" notification) ran far
  ahead of what was actually audible).
* `94dc8a4` and earlier — see `git log` / `plan.md` for the full audit
  history (`TASK-24H-1229`–`1253` and earlier namespaces) that preceded
  this round.

## 4. Current blocker / main problem

**No blocker.** Standalone build works, 29/29 tests pass (verified under
`SDL_VIDEODRIVER=dummy`/`SDL_AUDIODRIVER=dummy`). Working tree is clean;
`develop` is pushed and matches `origin/develop` at `471621c`.

## 5. Known bugs and limitations

* **Incomplete (by design, evidence-backed):** `StretchBlt` only
  supports `SRCCOPY` (`src/wingdi_blit.cpp`); `mciGetDeviceIDA` ignores
  `lpszDevice` and always returns `1` (`src/winmm.cpp`, safe because both
  games only ever open one MCI device); `_lopen` ignores `iReadWrite` and
  always opens read-only (`src/winbase_file.cpp`); `CreateBitmap`'s
  8-bit indexed path is greyscale-only (only reached by planetblupi's
  minimap in fullscreen mode); no real `WM_ACTIVATEAPP(0)` delivery or
  true blocking `WaitMessage` — all documented, evidenced, permanent
  decisions, see `docs/out-of-scope.md`.
* **Documented, not fixed (real but unreachable by evidenced call
  sites):** `CloseHandle` (`src/winbase.cpp`) is an unconditional
  `return TRUE;` stub — confirmed zero call sites in either target game;
  the only place either game creates handles that would need it
  (`CreateMutex`/`CreateThread` in `free-eggbert/src/blupi.cpp`) is
  itself guarded by an `#if THREAD` that's never defined, so that branch
  never compiles in. `FreeApiDestroySurfaceDC`/`AsCompatDC` segfaults on
  a genuinely garbage, never-allocated pointer instead of returning
  `FALSE` safely; `CompatDC::selectedBitmap`'s dangling-pointer risk —
  both traced and confirmed unreachable by either game's real code paths
  (`docs/out-of-scope.md`).
* **Documented, not fixed (explicit scope decision, left open):**
  `../free-direct`'s `Diagnostics.cpp` reaches past the documented
  bridge exception into the internal `FreeApi::Platform::ReadRssKB()`
  symbol directly — see `docs/scope.md`.
* **Confirmed environment artifact, not a code bug:**
  `test_winuser_regressions` fails some checks under this sandbox's
  default Wayland display; passes under `SDL_VIDEODRIVER=dummy` or real
  X11/Xvfb.
* **Needs human verification** (all four formally tracked, acceptance
  criteria in `docs/target-game-verification.md`): `MK_SHIFT`
  drag-select (`TASK-24H-0401`), `MK_CONTROL` level-editor flood-fill
  (`TASK-24H-0403`), MIDI audio sign-off (`TASK-24H-1221`), rendering/
  save-load sign-off (`TASK-24H-1222`) — none of these are completable
  by a Claude Code session; they need a real display/audio backend and a
  human judgment call.
* **By design, not a bug:** MCI digital-video/AVI is permanently
  declined (no evidenced call site in either game).
* **Unverified, not confirmed broken or working:** Web/Emscripten build
  readiness — no `__EMSCRIPTEN__`-gated code found in `free-api` itself;
  would need fresh scoping if ever prioritized (see §1).

## 6. Architecture notes

* **Main modules:** `src/wingdi_*.cpp` (GDI — device contexts, bitmaps,
  blitting), `src/winmm.cpp`/`src/MidiMusic.cpp` (MCI/MIDI/joystick/
  timers), `src/winuser_*.cpp` (windows, messages, input),
  `src/winbase_*.cpp` (files, paths, process/env), `src/winmain_bridge.cpp`
  (entry point — desktop `main`/Android `SDL_main`), `src/internal/*`
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
  - `MidiMusicSendCommand`'s `MCI_PLAY` handler captures MM_MCINOTIFY
    notify data under `GetMidiState().mtx` and posts it
    (`PostMessageA`) only after releasing the lock — do not restructure
    this back to posting while the mutex is held (see the lock-order
    comment on `GetMidiState().mtx`'s declaration).
  - `MixerThread` paces rendering to the audio stream's actual drain
    rate (`kMaxQueuedBlocksAhead`) — do not remove this cap; without it
    the mixer thread renders far ahead of real time (fixed in `10d58c2`).
  - `NormalizeMidiPath`/`NormalizeFilesystemPath` try a genuinely-absolute
    path as-is before falling through to CWD-relative normalization — do
    not revert to unconditionally stripping a leading slash (fixed in
    `3635439`).
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

The AI-doable P0–P3 backlog in `plan.md` is fully closed. Every item this
file's own "next smallest tasks" list has proposed across this round
(static analysis, skeptical re-audit, gcov sweep) has now been completed
— see §3 for each. Only one task remains, and it is not AI-doable:

1. **Human playtest pass (not AI-doable).** Goal: complete the 4
   remaining `TODO` tasks (`TASK-24H-0401`/`0403`/`1221`/`1222`) via the
   single checklist at `docs/target-game-verification.md`. Requires a
   human with a real display and audio backend.

With no human playtest available, the next productive step is a fresh,
independent, skeptical full-codebase re-audit (the pattern that has
found real gaps every time it's been run historically — see `git log` /
`plan.md` for precedent, and this round's own gcov sweep, which found 2
real testing gaps). Do not invent speculative new tasks just to have
something to do — if a fresh re-audit also comes back clean, say so
plainly rather than manufacturing busywork.

**Static analysis (`cppcheck`+`clang-tidy`) was just re-run and is clean
(see §3) — when it's time to re-run it again, remember `clang-tidy`'s
default header filter hides warnings from any header, including this
project's own `include/*.h`; use `-header-filter='free-api/(src|include)/'`
or real findings (like the `handleapi.h` guard bug this pass found) will
be silently suppressed.**

## 9. Do not do yet

* No broad refactor of the GDI diagnostic-counter gating scheme — it was
  implemented carefully to avoid breaking
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
  deliberate, re-justified decision (all have a `plan.md`/`docs/*`/git
  history entry explaining why):
  - `GetMidiState()` (`src/MidiMusic.cpp`) back to a plain
    `static MidiState g_midi;`.
  - `ASAN_OPTIONS=detect_leaks=0` (`CMakeLists.txt`) — use a scoped
    `LSAN_OPTIONS=suppressions=<file>` instead if ever needed.
  - `ResolveJoystick`'s bounds check (`src/winmm.cpp`) to comparing
    `static_cast<int>(uJoyID) < count`.
  - `MidiMusicSendCommand`'s `MCI_PLAY` handler back to calling
    `PostMessageA` while `GetMidiState().mtx` is held.
  - `MixerThread`'s render-ahead cap (`kMaxQueuedBlocksAhead`).
  - `NormalizeMidiPath`/`NormalizeFilesystemPath`'s absolute-path-first
    check back to unconditionally stripping a leading slash.
  - `OutputDebugStringW`'s UTF-16→UTF-8 conversion back to a raw
    per-`wchar_t` cast.
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
* Do not re-add `audit.md`, `todo/Embedded_Resources_FreeAPI.md`,
  `todo/free-api-performance-todo.md`, or `../freeapiissues.md` as
  living documents — all were deliberately deleted on 2026-07-18 once
  every finding in them was resolved (fixed, confirmed unreachable, or
  profiled and rejected). If a genuinely new problem is found, open a
  fresh, narrowly-scoped note rather than reviving one of these.

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
