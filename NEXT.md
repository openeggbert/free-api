# NEXT.md

Handoff document for resuming work on `free-api`, for either a future
Claude Code session or a human developer. Reflects the actual repository
state as of commit `8875a42` (2026-07-08, `develop` branch, 19 commits
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

**Current development phase:** three consecutive sessions this cycle.
Session 1 produced a from-scratch deep audit (`docs/audit-24h-free-api.md`)
and extended `plan.md` with a 175-task backlog (`TASK-24H-0001`–`1220`),
then implemented the 2 P0 tasks plus a handful of quick P1s. Session 2
(implementation-only) worked through most of the backlog's P1 tasks by
theme. Session 3 (this pass, also implementation-only, triggered by an
explicit "don't stop while safe work remains" correction — see §4) closed
the remaining 3 AI-doable P1 tasks and made a first pass through P2
cleanup/documentation tasks. **56 of 175 new tasks are now DONE** — all
verified, tested, and committed; **0 P0, 0 AI-doable P1 tasks remain**
(1 P1 left, `TASK-24H-0401`, is a human-only playtest). 68 P2 and 50 P3
tasks remain — see section 8.

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
  `FreeApiSetWindowFullscreen` anywhere else. `docs/scope.md` also now
  documents one further, real, currently-*unfixed* coupling beyond that
  list: `../free-direct`'s `Diagnostics.cpp` reaches directly into the
  internal `FreeApi::Platform::ReadRssKB()` symbol (not part of the bridge
  exception) — see `docs/scope.md`'s "actual bridge-exception surface"
  subsection; the fix-or-accept decision is intentionally left open.
* **New this session: `FREE_API_SANITIZE` CMake option** (`thread`/
  `address`, off by default, additive) for running the test suite under
  ThreadSanitizer/AddressSanitizer — see `docs/cmake-options.md`'s
  "Sanitizer-instrumented test builds" section and §5/§6 below.

## 2. Current status

**Build status — all confirmed working after every change this session:**
* Standalone (`-DFREE_API_USE_SYSTEM_SDL3=ON`): configures, builds,
  **22/22** tests pass.
* As a subdirectory of `../free-eggbert` (Ninja), including the
  `free-api`+`free-direct` diamond dependency (`FREEDIRECT` backend):
  22/22.
* As a subdirectory of `../planetblupi` (Make): 22/22.
* `../free-direct` standalone: configures, builds, links `FREE_DIRECT`
  cleanly against `include/free_api_bridge.h`.
* **New this session:** the same 22-test suite also passes cleanly
  (0 sanitizer reports) under both `-DFREE_API_SANITIZE=thread` and
  `=address` (see §6's exact `LD_PRELOAD` invocation).

**Test status:** 22 test binaries/CTest entries, **22/22 passing** in all
three build modes, under `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy`, and
under both sanitizer builds. This session added
`TestTimeSetEventKillRaceHasNoUseAfterFree` (`test_timer_regressions.cpp`)
— a 2000-iteration `timeSetEvent`/`timeKillEvent` race stress test,
specifically written to be run under a sanitizer (see §5).

**What does NOT work / known gaps:** unchanged except for items closed in
section 3 below. MCI digital-video remains intentionally unimplemented.
Human playtests (MIDI audio, rendering, `MK_SHIFT`/`MK_CONTROL` drag-
select/flood-fill) still needed — see section 8. The `MK_CONTROL` playtest
now has a fully-traced, documented key/menu path (`docs/target-games.md`),
closing the "not concretely actionable" blocker that previously existed.

## 3. Recent changes (session 3, this pass, most recent first)

* **Batch-closed 16 P2 documentation/decision tasks**
  (`TASK-24H-0002`/`0008`/`0109`/`0110`/`0111`/`0112`/`0113`/`0201`/`0204`/
  `0206`/`1205`/`1206`/`1210`/`1211`/`1215`/`1216`) — six of these were
  exact-duplicate task IDs the backlog itself flagged ("implement once,
  close both"). Covers: correcting the `FREE_API_TARGET_GAME=standalone`
  docstring's inaccurate claim; keep-rationale decisions for
  `OutputDebugStringW` and `_chdir`/`_getcwd` (closing the original,
  previously-open `TASK-0007`/`TASK-0006`); vestigial-typedef documentation
  for `HFONT`/`HPALETTE` and `winnt.h`'s COM-family typedefs; the `fopen`
  macro-override rationale; `PeekMessageA`'s filter-ignoring, `WaitMessage`'s
  polling-not-blocking contract, and `WM_ACTIVATEAPP(0)` focus-loss
  suppression, each now documented in `docs/out-of-scope.md` plus the
  relevant header doc comments.
* **Traced and documented the key/menu path to planetblupi's level-editor
  decor-tool-active state** (`TASK-24H-0402`) — source-traced (not run
  against a live build) via `ChangePhase`/`CreateButtons`/`VK_RETURN`
  handling in `../planetblupi/src/event.cpp`. Finding: the existing
  keyboard-only `Enter × 4` sequence (documented for `MK_SHIFT`) never
  reaches the editor, since `Enter` at the title screen skips straight past
  the private-mode toggle. The real path needs two mouse clicks ("Privé"
  then "Build") — written up in `docs/target-games.md`'s new "Manual
  playtest key/menu sequences" section, unblocking `TASK-24H-0403`'s human
  playtest.
* **Added a sanitizer-verified timer race test; fixed 3 real bugs it found**
  (`TASK-24H-0506`) — added the opt-in `FREE_API_SANITIZE` CMake option and
  a 2000-iteration `timeSetEvent`/`timeKillEvent` race test. Running it
  surfaced and led to fixing: (1) a genuine use-after-free in
  `FreeApiMmTimerBridge`'s "verify still alive" re-lookup (it dereferenced a
  raw pointer into `g_mmTimers`' map node *to find the ID to re-look-up*,
  before the lookup itself could confirm the node was still alive — fixed
  by packing the timer ID directly into the SDL userdata slot instead of a
  map-node pointer); (2) a real ThreadSanitizer-caught data race on
  `FreeApi::Internal::g_debugInput` (now `std::atomic_bool`); (3) an SDL
  subsystem refcount overflow in `timeSetEvent` (unconditional
  `SDL_InitSubSystem(SDL_INIT_EVENTS)` on every call, no matching
  `SDL_QuitSubSystem` — fixed with an `SDL_WasInit` guard, only surfaced by
  the new race test's 2000 iterations against an assertions-enabled SDL3
  build).
* **Documented `MCI_OPEN_PARMS`/`MCI_PLAY_PARMS`'s include-order-determined
  shape resolution** (`TASK-24H-0107`) — verified via grep against both
  games' `movie.cpp` that the `MCI_DGV_*`-aliased shape in `digitalv.h`
  never actually wins (the real, sequencer-call shape in `mmsystem.h`
  always does, due to include order); added explanatory comments at both
  guard sites.
* **Documented `free-direct`'s reach into internal
  `FreeApi::Platform::ReadRssKB`** (`TASK-24H-0102`/`1219`) — added a new
  `docs/scope.md` subsection naming this real, currently-unfixed coupling
  explicitly; left the fix-or-accept decision open for a future task.

## 4. Current blocker / main problem

**No blocker.** All build configurations work, 22/22 tests pass everywhere
(default and both sanitizer builds). **19 commits are sitting locally on
`develop`, not yet pushed to `origin/develop`** — push only if/when the
user explicitly asks.

**Process note carried forward from session 2→3:** mid-session-2, stopping
to write a final report while 6 P1 tasks were still open was an explicit
mistake, corrected by the user ("pracuj autonomně, proč ses zasekl?" —
"work autonomously, why did you get stuck?"). The governing rule for any
continuation of this work: **do not stop/summarize/report while safe,
AI-doable P0/P1 work remains** — only the human-only playtests
(`TASK-24H-0401`/`0403`) are legitimate exceptions. That condition is now
met (0 AI-doable P0/P1 remain), so P2 cleanup is now the correct next tier
per the original session's priority rules — continuing to work through it
is expected, not optional busywork.

## 5. Known bugs and limitations

* **Fixed across all three sessions:** scaled `StretchBlt`'s X/Y clamp
  asymmetry, the `_findfirst` session-table leak, the
  `FreeApiCreateSurfaceDC`/`FreeApiDestroySurfaceDC`/
  `FreeApiSetWindowFullscreen` header-declaration gap, the dead
  sibling-vendored SDL3 CMake fallback, unconditional startup/asset-load
  logging, **and (session 3) a real use-after-free in
  `FreeApiMmTimerBridge`, a real data race on `g_debugInput`, and an SDL
  subsystem-refcount overflow in `timeSetEvent`** (see §3).
* **Confirmed environment artifact, not a code bug (unchanged):**
  `test_winuser_regressions` fails 6 checks under this sandbox's default
  Wayland display; passes under `SDL_VIDEODRIVER=dummy` or real X11/Xvfb.
* **Documented, not fixed (real but out-of-scope):**
  `FreeApiDestroySurfaceDC`/`AsCompatDC` segfaults on a genuinely garbage,
  never-allocated pointer instead of returning `FALSE` safely — no
  evidenced `free-direct` call site ever passes such a pointer. Fixing this
  would need a general handle-validation/table framework, against this
  project's scope discipline.
* **Documented, not fixed:** `CreateBitmap`'s 8-bit indexed path is
  greyscale-only; only reached by planetblupi's minimap in fullscreen mode
  (not the shipped `FullScreen=0` default).
* **Documented, not fixed (session 3, new):** `../free-direct`'s
  `Diagnostics.cpp` reaches past the documented bridge exception into the
  internal `FreeApi::Platform::ReadRssKB()` symbol directly, bypassing the
  existing (unused by `free-direct`) `FreeApiReadRssKB()` wrapper. Whether
  to formalize this as a fourth bridge-header declaration or accept the
  informal coupling (diagnostics-only, no gameplay stakes) is an open
  decision for a future task — see `docs/scope.md`.
* **By design, not a bug (reconfirmed every session):** MCI digital-video/
  AVI is permanently declined.
* **Still needs human verification — now fully actionable for both
  features (session 3 traced the previously-missing `MK_CONTROL` path):**
  `MK_SHIFT` drives planetblupi's drag-select multi-unit highlight
  (`Enter × 4` from cold boot); `MK_CONTROL` drives a *separate*
  level-editor decor flood-fill (`Enter × 2`, then click "Privé", then
  click "Build" — see `docs/target-games.md`). See `plan.md`
  `TASK-24H-0401`/`0403`.
* **1 P1 task remains, human-only** (`TASK-24H-0401`) — see section 8.
  `TASK-24H-0403` (the `MK_CONTROL` playtest) is P2 and also human-only.

## 6. Architecture notes

Unchanged from prior sessions' notes except:

* `src/winmm.cpp`'s `FreeApiMmTimerBridge` now receives the timer ID packed
  directly into the SDL userdata pointer slot (`reinterpret_cast<void*>`),
  **not** a pointer into `g_mmTimers`. Do not revert to passing a map-node
  pointer — see the function's doc comment for the exact use-after-free
  this avoids.
* `FreeApi::Internal::g_debugInput` (`src/internal/FreeApiMessageQueue.{hpp,cpp}`)
  is now `std::atomic_bool`, not `bool` — do not revert; it is written from
  the main thread and read from the SDL timer thread.
* `FREE_API_SANITIZE` (CMake cache option, `""`/`thread`/`address`) adds
  `PUBLIC` compile/link sanitizer flags to the `free-api` target. To run it:
  ```bash
  cmake -B build-tsan -DFREE_API_SANITIZE=thread -DFREE_API_USE_SYSTEM_SDL3=ON
  cmake --build build-tsan --target test_timer_regressions
  LD_PRELOAD="$(readlink -f "$(gcc -print-file-name=libtsan.so)")" \
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./build-tsan/test_timer_regressions
  ```
  Swap `thread`/`libtsan.so` for `address`/`libasan.so` (add
  `ASAN_OPTIONS=detect_leaks=0`) for AddressSanitizer. See
  `docs/cmake-options.md` for the full writeup.
* `docs/target-games.md` now has a "Manual playtest key/menu sequences"
  section with both the `MK_SHIFT` and `MK_CONTROL` paths — check there
  before re-deriving either from source.
* Everything from prior sessions (`ApplyKeyboardModifierFlags`,
  `_findfirst` self-cleaning, `StretchBlt`'s symmetric clamp,
  `include/free_api_bridge.h`, `cmake/test-fixtures/`) is unchanged.

## 7. Useful commands

```bash
# Build via a real target game
cd ../free-eggbert/cmake-build-debug && cmake . && ninja -j"$(nproc)"
cd ../planetblupi/build && cmake . && make -j"$(nproc)"

# Standalone build (no sibling game needed)
cmake -S . -B build -DFREE_API_USE_SYSTEM_SDL3=ON -DFREE_API_BUILD_TESTS=ON
cmake --build build -j"$(nproc)"
cd build && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure

# ../free-direct standalone (depends on include/free_api_bridge.h)
cd ../free-direct && cmake -S . -B build -DFREE_API_USE_SYSTEM_SDL3=ON
cmake --build build -j"$(nproc)"

# Run the full test suite -- SDL_VIDEODRIVER=dummy is NOT optional under
# this sandbox's default (Wayland) display
cd ../free-eggbert/cmake-build-debug/FREE_API && \
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure
cd ../planetblupi/build/FREE_API && \
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure

# Sanitizer build (new this session) -- see §6 above for the full LD_PRELOAD dance
cmake -B build-tsan -DFREE_API_SANITIZE=thread -DFREE_API_USE_SYSTEM_SDL3=ON
cmake --build build-tsan -j"$(nproc)"
```

See `docs/cmake-options.md` for the full, canonical build-mode reference.

No lint/formatter is configured in this repository.

## 8. Next smallest tasks

**0 P0, 0 AI-doable P1 tasks remain TODO.** 1 P1 (`TASK-24H-0401`, human
playtest) and 68 P2 + 50 P3 tasks remain. Per the original session's
priority rules, P2 cleanup is now correctly in scope. Concrete starting
points, roughly in order of value:

1. **Human playtests** (`TASK-24H-0401`/`0403`) — both now have fully
   documented, concrete key/menu sequences (`docs/target-games.md`); need
   an actual human with a real display/input backend.
2. **P2 test-coverage tasks** — several remaining P2s ask for a specific
   new regression test with clear acceptance criteria (e.g.
   `TASK-24H-0202` PeekMessageA filter-ignoring positive test,
   `TASK-24H-0209`/`0213`/`0214` DispatchMessageA/coalescing tests,
   `TASK-24H-0301` RegisterClassA field-discarding test,
   `TASK-24H-0308`/`0311` GetSystemMetrics/ShowCursor edge-case tests,
   `TASK-24H-0609`/`0613` GDI leak/rejection tests). Grep `plan.md` for
   `Priority: P2` + `Type: Test` + `Status: TODO` together.
3. **P2 refactor/consolidation tasks with explicit "test parity first"
   gating** — the four-path-normalization-implementation cluster
   (`TASK-24H-0703`–`0706`) explicitly requires characterization tests
   *before* any consolidation; do not skip that ordering. Similarly
   `TASK-24H-0103`/`0104`/`0105`/`0905` (duplicate declaration
   consolidation) and `TASK-24H-0014` (CXX_STANDARD block consolidation).
4. **Remaining P2 documentation tasks** — many follow the same pattern as
   this session's batch (grep `Priority: P2` + `Type: Documentation` +
   `Status: TODO`); several explicitly reference "duplicates TASK-24H-XXXX
   — implement once, close both," so check for a paired ID before starting
   one to avoid redundant work.
5. **After P2: 50 P3 tasks remain** — lowest priority per the original
   session's rules; mostly further documentation/hygiene polish.

## 9. Do not do yet

Unchanged from prior sessions' list, plus:

* Do not revert `FreeApiMmTimerBridge` to passing a map-node pointer as SDL
  userdata (`src/winmm.cpp`) — this reintroduces a real use-after-free; see
  its doc comment.
* Do not revert `g_debugInput` to plain `bool`
  (`src/internal/FreeApiMessageQueue.{hpp,cpp}`) — this reintroduces a real
  ThreadSanitizer-caught data race.
* Do not remove `timeSetEvent`'s `SDL_WasInit(SDL_INIT_EVENTS)` guard
  (`src/winmm.cpp`) — without it, enough `timeSetEvent` calls in one
  process overflow SDL's byte-sized subsystem refcount and abort under an
  assertions-enabled SDL3 build.
* Do not "fix" `free-direct`'s `ReadRssKB` reach without a deliberate
  decision (formalize as a 4th bridge function, or accept as informal
  coupling) — this session intentionally left it open, see `docs/scope.md`.
* Do not implement real `WM_ACTIVATEAPP(0)` delivery, real `PeekMessageA`
  filtering, or a true blocking `WaitMessage` — all three are now
  explicitly documented, evidence-backed permanent decisions
  (`docs/out-of-scope.md`).
* Do not remove `OutputDebugStringW`, `_chdir`/`_getcwd`, `HFONT`/
  `HPALETTE`, or `winnt.h`'s COM-family typedefs — all now have explicit
  keep-and-document decisions (`docs/out-of-scope.md`).
* Do not revert `_findnext`'s auto-erase-on-exhaustion fix
  (`src/crt_io.cpp`) without understanding the leak it fixes.
* Do not revert `StretchBlt`'s Y-axis clamp (`src/wingdi_blit.cpp`) to
  the old skip-the-row behavior.
* Do not re-declare `FreeApiCreateSurfaceDC`/`FreeApiDestroySurfaceDC`/
  `FreeApiSetWindowFullscreen` anywhere other than
  `include/free_api_bridge.h`.
* Do not "fix" `FreeApiDestroySurfaceDC`'s garbage-pointer segfault with a
  general handle-validation/table framework — documented as
  out-of-scope-to-fix; only revisit with new evidence a real caller needs
  it.
* Do not implement a real palette lookup for `CreateBitmap`'s 8-bit path
  — documented as out-of-scope per investigation (`TASK-24H-0602`/`0603`).
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
Verdict (§1) for full context. plan.md has 175 new TASK-24H-* tasks; 56
are DONE (grep "Status: DONE" near "TASK-24H" to see which), 0 P0 and 0
AI-doable P1 remain TODO (1 P1 left is human-only). Work through P2 tasks
per section 8's "Next smallest tasks" list -- many explicitly duplicate
another task ID ("implement once, close both"), check plan.md for that
note before starting. Make small, verified improvements; batch closely-
related documentation tasks together like this session did (16 closed in
one commit). Run the exact verification commands each task specifies --
at minimum the standalone build's ctest (22/22), ideally also both target
games' ctest, and for anything touching src/winmm.cpp or cross-thread code
also the sanitizer builds (§6). Do not touch anything listed in section 9.
Do NOT stop/summarize while safe P0/P1/P2 work remains -- continue
autonomously (see §4's process note). After finishing, update this file
and plan.md's task statuses.
```
