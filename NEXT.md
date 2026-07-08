# NEXT.md

Handoff document for resuming work on `free-api`, for either a future
Claude Code session or a human developer. Reflects the actual repository
state as of commit `b5e611c` (2026-07-08, `develop` branch, 26 commits
ahead of `origin/develop`, **not yet pushed**; working tree clean). See
[`plan.md`](plan.md) for the full task backlog (12 original tasks +
175-task `TASK-24H-0001`–`1220` backlog) and
[`docs/audit-24h-free-api.md`](docs/audit-24h-free-api.md) for the full
24-hour deep audit that backlog was derived from.

## 1. Project summary

Free API is a small, SDL3-backed C++ library that exposes a Win32/WinAPI-
like public surface (`windows.h`, `winuser.h`, `mmsystem.h`, `digitalv.h`,
etc.) so two specific legacy Windows games can compile and run on Linux/
macOS/Web/Android without Microsoft Windows:

* **Free Eggbert** (`../free-eggbert`, top-level CMake project
  `SPEEDY_BLUPI_WINDOWS`)
* **Planet Blupi** (`../planetblupi`, top-level CMake project
  `PLANET_BLUPI_WINDOWS`)

**Main goal:** source-level compatibility for exactly these two games —
not Wine, not a general WinAPI reimplementation, not a platform for
arbitrary 1998-era Windows software. Every public API must cite a real
usage site (`file:line`) in one of the two games' own source
(`docs/scope.md`), with one documented exception (the `free-direct`-bridge
functions, declared in `include/free_api_bridge.h`).

**Current development phase:** four sessions this cycle. Sessions 1-3
produced the audit, the 175-task backlog, and closed all P0/P1 work.
**Session 4 (this pass)** began with an independent, skeptical audit fork
(user-requested, "verify whether 'all P0/P1 closed' is actually true") that
re-confirmed the P0/P1 claim but found 5 concrete follow-up gaps; this
session implemented all 5, then continued through 8 more P2/P3 tasks.
**71 of 175 new tasks are now DONE** — all verified, tested, and committed;
**0 P0, 0 AI-doable P1 tasks remain** (1 P1 left, `TASK-24H-0401`, is a
human-only playtest). 55 P2 and 48 P3 tasks remain — see section 8.

**Important architectural decisions (unchanged across all sessions):**

* Free API is a **static library**, normally built as a sibling
  `add_subdirectory()` of one of the two target games (which also provide
  SDL3). It also builds standalone via `-DFREE_API_USE_SYSTEM_SDL3=ON`.
* SDL3 is an **internal backend detail only** — public headers in
  `include/` must never expose SDL types.
* `CMakeLists.txt` keys off `CMAKE_PROJECT_NAME` to detect which game (if
  any) is driving the build, or an explicit `-DFREE_API_TARGET_GAME=...`
  override (`docs/cmake-options.md`).
* The two target games use **two different, mutually-exclusive live timer
  mechanisms**: Free Eggbert uses `timeSetEvent`/`timeKillEvent`; Planet
  Blupi uses `SetTimer`/`KillTimer`/`WM_TIMER`. Both must keep working.
* The free-direct-bridge exception has a real, single-source-of-truth
  header (`include/free_api_bridge.h`) — do not re-declare
  `FreeApiCreateSurfaceDC`/`FreeApiDestroySurfaceDC`/
  `FreeApiSetWindowFullscreen` anywhere else. `docs/scope.md`'s "The only
  exceptions are" list now explicitly cross-references this (`TASK-0011`,
  closed session 4).
* **`FREE_API_SANITIZE` CMake option** (`thread`/`address`, off by default)
  for running the test suite under ThreadSanitizer/AddressSanitizer — as of
  session 4, a plain `ctest` inside a sanitizer-configured build tree just
  works (no manual `LD_PRELOAD`) — see `docs/cmake-options.md`.
* **`docs/testing.md`** (new, session 4) is now the canonical "how to run
  the tests" reference; `docs/cmake-options.md` remains canonical for build
  *options*.

## 2. Current status

**Build status — all confirmed working after every change this session:**
* Standalone (`-DFREE_API_USE_SYSTEM_SDL3=ON`): configures, builds,
  **23/23** tests pass.
* As a subdirectory of `../free-eggbert` (Ninja), including the
  `free-api`+`free-direct` diamond dependency (`FREEDIRECT` backend):
  23/23.
* As a subdirectory of `../planetblupi` (Make): 23/23.
* `../free-direct` standalone: configures, builds, links `FREE_DIRECT`
  cleanly against `include/free_api_bridge.h`.
* The same 23-test suite also passes cleanly (0 sanitizer reports) under
  both `-DFREE_API_SANITIZE=thread` and `=address`, via plain `ctest`
  (no manual env vars beyond `SDL_VIDEODRIVER`/`SDL_AUDIODRIVER`).

**Test status:** 23 test binaries/CTest entries (up from 22), 23/23 passing
in all three build modes and both sanitizer builds. Session 4 added:
`test_midi_backend_failure` (new binary), plus new tests in
`test_timer_regressions.cpp` (`TestGDebugInputSurvivesRaceUnderSanitizer`),
`test_gdi_regressions.cpp` (`TestLoadImageAWithLeadingBackslashRootedPathStaysRelativeToCwd`),
and `test_winuser_regressions.cpp`
(`TestDispatchMessageARoutesNullHwndToSoleRegisteredWindow`,
`TestWmMouseMoveCoalescingKeepsOnlyLatestPosition`,
`TestWmTimerCoalescingKeepsOnlyOneQueuedMessagePerHwndAndId`).

**What does NOT work / known gaps:** unchanged except for items closed in
section 3 below. MCI digital-video remains intentionally unimplemented.
Human playtests still needed — MIDI audio sign-off (`TASK-24H-1221`),
rendering sign-off (`TASK-24H-1222`), and `MK_SHIFT`/`MK_CONTROL` drag-
select/flood-fill (`TASK-24H-0401`/`0403`) each now have a real, tracked
task with concrete acceptance criteria, and one consolidated, runnable
checklist covering all of them:
[`docs/target-game-verification.md`](docs/target-game-verification.md)
(`TASK-24H-1209`). See section 8.

## 3. Recent changes (session 4, this pass, most recent first)

* **Removed now-dead `NormalizePath`** (`TASK-24H-0615`, closes duplicate
  `TASK-24H-1220`) — its only call site migrated to
  `NormalizeFilesystemPath` earlier this session; re-grepped to confirm
  zero remaining callers before removing.
* **Created `docs/testing.md`; linked from README/Documentation.md**
  (`TASK-24H-1208`/`0009`) — consolidates the "how to run tests"
  instructions. Fixed a stale `ctest` command in `Documentation.md` missing
  the `SDL_VIDEODRIVER`/`SDL_AUDIODRIVER` vars.
* **Added 4 P2 tasks**: `TASK-24H-1105` (documented
  `FreeApiDiagnosticsFastEnabled()` as an intentional alias, not a
  behavior change); `TASK-24H-0209` (`DispatchMessageA` null-hwnd fallback
  test); `TASK-24H-0213`/`0214` (dedicated `WM_TIMER`/`WM_MOUSEMOVE`
  coalescing tests — each had only incidental coverage before).
* **Implemented the 5 concrete gaps found by session 4's own audit fork**
  (user explicitly requested a skeptical, independent re-verification of
  the "0 AI-doable P0/P1 remain" claim before continuing):
  1. `TASK-24H-1109`: `EnsureMidiBackend()` now latches on its first
     failure so a persistent audio-backend-init failure logs once per
     process, not once per `MCI_OPEN` call. New isolated-process test
     binary `test_midi_backend_failure` (the latch is permanent for the
     process lifetime, so it can't share a binary with passing MCI tests).
  2. `TASK-24H-0506` follow-up: the `g_debugInput` atomic fix was
     previously only *incidentally* covered by an unrelated test's timing.
     Added `TestGDebugInputSurvivesRaceUnderSanitizer`, a dedicated,
     deterministic race test — verified with a genuine negative control
     (temporarily reverted the fix, confirmed this exact test independently
     catches it under TSan in isolation, then reverted the revert).
  3. `TASK-24H-0605`: `LoadImageA` now uses `NormalizeFilesystemPath`
     instead of the weaker `NormalizePath`, matching every other
     file-opening entry point.
  4. `TASK-0011`: reconciled — the free-direct-bridge exception was
     already documented in practice but the task status was never flipped
     and it wasn't cross-referenced from the rule's own exceptions list;
     fixed both.
  5. **Sanitizer CMake/CTest support**: `FREE_API_SANITIZE`-configured
     build trees now auto-resolve and `LD_PRELOAD` the matching sanitizer
     runtime (plus `ASAN_OPTIONS=detect_leaks=0` for `address`) as each
     test's `ENVIRONMENT` property — a plain `ctest` just works now.

## 4. Current blocker / main problem

**No blocker.** All build configurations work, 23/23 tests pass everywhere
(default and both sanitizer builds). **26 commits are sitting locally on
`develop`, not yet pushed to `origin/develop`** — push only if/when the
user explicitly asks.

**Process notes carried forward (still governing):**
* Do not stop/summarize/report while safe, AI-doable P0/P1/P2 work remains
  — only human-only playtests (`TASK-24H-0401`/`0403`) are legitimate
  exceptions (session 2→3 correction).
* Before trusting a "done" claim from a prior session, an independent,
  skeptical re-verification is valuable and was explicitly requested once
  already (session 4) — it found real, fixable gaps even though the
  headline claim held up. Consider the same posture before extending this
  file's claims further without re-checking them.

## 5. Known bugs and limitations

* **Fixed across all four sessions:** scaled `StretchBlt`'s X/Y clamp
  asymmetry, the `_findfirst` session-table leak, the
  `FreeApiCreateSurfaceDC`/`FreeApiDestroySurfaceDC`/
  `FreeApiSetWindowFullscreen` header-declaration gap, the dead
  sibling-vendored SDL3 CMake fallback, unconditional startup/asset-load
  logging, a real use-after-free in `FreeApiMmTimerBridge`, a real data
  race on `g_debugInput`, an SDL subsystem-refcount overflow in
  `timeSetEvent`, **and (session 4) MIDI backend-failure log spam and
  `LoadImageA`'s weak path normalization**.
* **Confirmed environment artifact, not a code bug (unchanged):**
  `test_winuser_regressions` fails 6 checks under this sandbox's default
  Wayland display; passes under `SDL_VIDEODRIVER=dummy` or real X11/Xvfb.
* **Documented, not fixed (real but out-of-scope):**
  `FreeApiDestroySurfaceDC`/`AsCompatDC` segfaults on a genuinely garbage,
  never-allocated pointer instead of returning `FALSE` safely — no
  evidenced `free-direct` call site ever passes such a pointer.
* **Documented, not fixed:** `CreateBitmap`'s 8-bit indexed path is
  greyscale-only; only reached by planetblupi's minimap in fullscreen mode.
* **Documented, not fixed:** `../free-direct`'s `Diagnostics.cpp` reaches
  past the documented bridge exception into the internal
  `FreeApi::Platform::ReadRssKB()` symbol directly. Fix-or-accept decision
  intentionally left open — see `docs/scope.md`.
* **By design, not a bug (reconfirmed every session):** MCI digital-video/
  AVI is permanently declined.
* **Still needs human verification, fully actionable, all four now
  formally tracked with acceptance criteria:** `MK_SHIFT` drag-select
  (`TASK-24H-0401`), `MK_CONTROL` level-editor flood-fill
  (`TASK-24H-0403`), MIDI audio sign-off (`TASK-24H-1221`), rendering
  sign-off (`TASK-24H-1222`) — run all four via the single checklist at
  [`docs/target-game-verification.md`](docs/target-game-verification.md).

## 6. Architecture notes

Unchanged from prior sessions' notes except:

* `src/internal/FreeApiPath.hpp`/`.cpp` no longer has `NormalizePath` —
  only `NormalizeFilesystemPath` and `BuildCommandLine` remain. Every
  path-normalizing call site in the codebase now uses the stronger
  function.
* `FREE_API_SANITIZE` sanitizer runs no longer need manual `LD_PRELOAD`:
  ```bash
  cmake -B build-tsan -DFREE_API_SANITIZE=thread -DFREE_API_USE_SYSTEM_SDL3=ON
  cmake --build build-tsan -j"$(nproc)"
  cd build-tsan && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure
  ```
  Swap `thread` for `address` for AddressSanitizer. See
  `docs/cmake-options.md` for the mechanism and the manual-fallback form.
* `MidiState` (`src/MidiMusic.cpp`) has a new `backendInitFailed` latch —
  do not remove it; without it, a persistent audio-init failure re-logs on
  every `MCI_OPEN` (one per song/track load).
* `tests/test_midi_backend_failure.cpp` is a deliberately separate binary
  from `test_mci_sequences.cpp` — see its doc comment for why (the failure
  latch above is permanent for the process, so it can't share a binary
  with tests that need MCI_OPEN to keep succeeding).
* Everything from prior sessions (`ApplyKeyboardModifierFlags`,
  `_findfirst` self-cleaning, `StretchBlt`'s symmetric clamp,
  `include/free_api_bridge.h`, `cmake/test-fixtures/`,
  `FreeApiMmTimerBridge`'s packed-ID userdata, `g_debugInput` as
  `std::atomic_bool`) is unchanged.

## 7. Useful commands

See [`docs/testing.md`](docs/testing.md) for the full, canonical
test-running reference (this section is a quick-reference convenience, not
a duplicate source of truth):

```bash
# Build via a real target game
cd ../free-eggbert/cmake-build-debug && cmake . && ninja -j"$(nproc)"
cd ../planetblupi/build && cmake . && make -j"$(nproc)"

# Standalone build (no sibling game needed)
cmake -S . -B build -DFREE_API_USE_SYSTEM_SDL3=ON -DFREE_API_BUILD_TESTS=ON
cmake --build build -j"$(nproc)"
cd build && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure

# Sanitizer build -- plain ctest works now, no manual LD_PRELOAD
cmake -B build-tsan -DFREE_API_SANITIZE=thread -DFREE_API_USE_SYSTEM_SDL3=ON
cmake --build build-tsan -j"$(nproc)"
cd build-tsan && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure
```

See `docs/cmake-options.md` for the full, canonical build-mode reference.

No lint/formatter is configured in this repository.

## 8. Next smallest tasks

**0 P0, 0 AI-doable P1 tasks remain TODO.** 3 P1 (`TASK-24H-0401`,
`1221`, `1222` — all human-only) and 55 P2 + 48 P3 tasks remain. Concrete
starting points:

1. **Human playtests** — all four now formally tracked with acceptance
   criteria and one consolidated checklist:
   [`docs/target-game-verification.md`](docs/target-game-verification.md)
   covers `MK_SHIFT`/`MK_CONTROL` (`TASK-24H-0401`/`0403`), MIDI audio
   sign-off (`TASK-24H-1221`), and rendering sign-off (`TASK-24H-1222`).
   Needs an actual human with a real display/audio backend.
2. **P2 refactor/consolidation tasks with explicit "test parity first"
   gating** — the four-path-normalization-implementation cluster
   (`TASK-24H-0703`–`0706`) requires characterization tests *before* any
   consolidation. Similarly `TASK-24H-0103`/`0104`/`0105`/`0905`
   (duplicate declaration consolidation) and `TASK-24H-0014`.
3. **Remaining P2 test-coverage tasks** — grep `plan.md` for
   `Priority: P2` + `Type: Test` + `Status: TODO`.
4. **Remaining P2/P3 documentation tasks** — many reference "duplicates
   TASK-24H-XXXX — implement once, close both"; check for a paired ID
   before starting one.

## 9. Do not do yet

Unchanged from prior sessions' list, plus:

* Do not reintroduce `NormalizePath` (`src/internal/FreeApiPath.{hpp,cpp}`)
  — it was removed as genuinely dead code; if a new caller ever needs
  backslash-only normalization, that's new evidence requiring a fresh task,
  not a revert.
* Do not remove `MidiState::backendInitFailed` (`src/MidiMusic.cpp`) — a
  persistent audio-init failure would re-log on every `MCI_OPEN` without it.
* Do not revert `FreeApiMmTimerBridge` to passing a map-node pointer as SDL
  userdata (`src/winmm.cpp`) — real use-after-free; see its doc comment.
* Do not revert `g_debugInput` to plain `bool`
  (`src/internal/FreeApiMessageQueue.{hpp,cpp}`) — real ThreadSanitizer-
  caught data race; now has dedicated test coverage
  (`TestGDebugInputSurvivesRaceUnderSanitizer`), not just incidental.
* Do not remove `timeSetEvent`'s `SDL_WasInit(SDL_INIT_EVENTS)` guard
  (`src/winmm.cpp`) — SDL subsystem refcount overflow otherwise.
* Do not "fix" `free-direct`'s `ReadRssKB` reach without a deliberate
  decision — intentionally left open, see `docs/scope.md`.
* Do not implement real `WM_ACTIVATEAPP(0)` delivery, real `PeekMessageA`
  filtering, or a true blocking `WaitMessage` — documented, evidence-backed
  permanent decisions (`docs/out-of-scope.md`).
* Do not remove `OutputDebugStringW`, `_chdir`/`_getcwd`, `HFONT`/
  `HPALETTE`, or `winnt.h`'s COM-family typedefs — explicit keep-and-
  document decisions (`docs/out-of-scope.md`).
* Do not revert `_findnext`'s auto-erase-on-exhaustion fix
  (`src/crt_io.cpp`).
* Do not revert `StretchBlt`'s Y-axis clamp (`src/wingdi_blit.cpp`) to the
  old skip-the-row behavior.
* Do not re-declare `FreeApiCreateSurfaceDC`/`FreeApiDestroySurfaceDC`/
  `FreeApiSetWindowFullscreen` anywhere other than
  `include/free_api_bridge.h`.
* Do not "fix" `FreeApiDestroySurfaceDC`'s garbage-pointer segfault with a
  general handle-validation/table framework.
* Do not implement a real palette lookup for `CreateBitmap`'s 8-bit path.
* Do not touch `joystick`, MCI digital-video, DirectDraw, DirectSound,
  DirectPlay, or `free-direct` without a specific, separately-scoped
  request — except `include/free_api_bridge.h`'s three declarations.
* No new public API without a cited `file:line` usage site in
  `../free-eggbert` or `../planetblupi` — except the documented
  `free-direct`-bridge exception.
* Do not implement any `TASK-24H-*` task beyond what its own "Required
  work"/"Out of scope" sections state.

## 10. Resume prompt

```
Read NEXT.md first, then skim docs/audit-24h-free-api.md's Executive
Verdict (§1) for full context. plan.md has 175 new TASK-24H-* tasks; 71
are DONE (grep "Status: DONE" near "TASK-24H" to see which), 0 P0 and 0
AI-doable P1 remain TODO (1 P1 left is human-only). Work through P2/P3
tasks per section 8's "Next smallest tasks" list -- many explicitly
duplicate another task ID ("implement once, close both"), check plan.md
for that note before starting. Make small, verified improvements; batch
closely-related tasks together. Run the exact verification commands each
task specifies -- at minimum the standalone build's ctest (23/23), ideally
also both target games' ctest, and for anything touching src/winmm.cpp,
src/MidiMusic.cpp, or cross-thread code also the sanitizer builds (§6/§7
-- now just a plain `ctest`, no manual LD_PRELOAD). Do not touch anything
listed in section 9. Do NOT stop/summarize while safe P0/P1/P2/P3 work
remains -- continue autonomously (see §4's process notes). Consider an
independent skeptical re-check of prior "done" claims before extending
them further, the way session 4 did. After finishing, update this file
and plan.md's task statuses.
```
