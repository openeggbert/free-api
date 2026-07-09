# NEXT.md

Handoff document for resuming work on `free-api`, for either a future
Claude Code session or a human developer. Reflects the actual repository
state as of commit `a8cf266` (2026-07-09, `develop` branch, **fully pushed
to `origin/develop`**; working tree clean). See [`plan.md`](plan.md) for
the full task backlog (205 `TASK-24H-*` tasks total) and
[`docs/audit-24h-free-api.md`](docs/audit-24h-free-api.md) for the original
24-hour deep audit that backlog was derived from. [`audit.md`](audit.md) at
the repo root is the current, most recent deep audit (see "Session 6"
below) — read that instead of trusting any older audit summary in this file.

**MILESTONE: the entire AI-doable P0–P3 backlog is closed, twice over.**
200 of 205 `TASK-24H-*` tasks are `DONE`, 1 is `OBSOLETE`. The only 4
remaining `TODO` tasks are human-playtest-only
(`TASK-24H-0401`/`0403`/`1221`/`1222` — see
[`docs/target-game-verification.md`](docs/target-game-verification.md)).
There is no more safe, AI-doable backlog work to pick up without either a
human completing a playtest, or a fresh audit finding new gaps.

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
functions, declared in `include/free_api_bridge.h`) — now also
mechanically enforced by `cmake/CheckPublicSurfaceBaseline.cmake` (see §6).

**Development history, condensed (see git log / `plan.md` for full detail
on any of these):**

* **Sessions 1–3:** produced the original 24-hour audit
  (`docs/audit-24h-free-api.md`), the 175-task backlog, and closed all
  P0/P1 work.
* **Session 4:** an independent skeptical re-audit fork found 5 concrete
  follow-up gaps (all fixed — sanitizer CTest auto-`LD_PRELOAD` support,
  `g_debugInput`'s race got dedicated test coverage, `LoadImageA` migrated
  to the stronger path-normalization function, the free-direct-bridge
  exception got properly cross-referenced, a MIDI-backend-failure log-spam
  fix). A second, independent strict test-coverage audit fork found 4 real
  functions with zero test coverage despite being live, used-by-both-games
  code (`FreeApiRunWinMain`, `wsprintfA`, `OutputDebugStringA`,
  `SetWindowTextA`) — all closed, and writing their tests surfaced 3 more
  real bugs (a use-after-free in the `FreeApiRunWinMain` test itself, and
  two distinct bugs in the `OutputDebugStringA` test — see §9's test-
  authoring caveats, both still relevant to any future test-writing).
  Created `docs/target-game-verification.md` and formal human-playtest
  tasks `TASK-24H-1221`/`1222`, closing a tracking gap that had lived only
  in prose across multiple sessions.
* **Session 5:** worked through the *entire* remaining P2+P3 backlog to
  completion — 91 tasks across 30 commits, each re-verified against
  current source (several task premises turned out to be stale or wrong —
  `CreateDirectoryA`'s already-exists branch turned out to be dead code,
  planetblupi's `AddUserPath` turned out to be live where a task assumed
  it was dead, free-eggbert's live sound backend turned out to be
  `sound.cpp` not the assumed-dead `soundbass.cpp`). Added
  `tests/test_sdl_log_gating.cpp`, a source-scan CTest guard against future
  ungated `SDL_Log` additions — it caught 2 real ungated logs while being
  built. One dispatched fork produced nothing usable (see §9's fork-
  concurrency lesson); a second, dispatched sequentially instead of
  concurrently, succeeded and produced `docs/public-surface-audit.md`.
* **Session 6 (this pass): two full ground-up audit rounds, 22 new tasks
  created and all 22 closed.** Round 1: 5 parallel fork reviews
  (correctness/performance/memory/edge-cases/risk) against the
  post-session-5 source found 16 new issues, formalized as
  `TASK-24H-1229`–`1244` and all implemented — highlights: a real GDI
  handle double-free closed (magic-tag-clear-before-`delete`), the MIDI
  subsystem's static-destruction-order race closed by converting `g_midi`
  to a function-local static (`GetMidiState()`, a Meyer's singleton —
  language-guaranteed destruction order, not link-order-dependent), a real
  uncaught-exception crash in `_findfirst` closed (proven via a
  deterministic ELOOP regression test that crashes the pre-fix code), and
  a new automated guard against undocumented public-API growth
  (`cmake/CheckPublicSurfaceBaseline.cmake`, §6). The pre-existing
  deliberately-deferred `TASK-24H-0706` (migrate `NormalizeMidiPath` to the
  shared path-normalization core) was also finally closed in this round.
  Round 2: `audit.md` was **deleted and rewritten from scratch** — a
  second, independent 5-fork re-audit against the round-1-fixed source,
  explicitly told not to reuse round 1's conclusions. Found 6 more issues
  (`TASK-24H-1245`–`1250`, all implemented), two of them **independently
  found by two of the five reviews each**: the `TASK-24H-1233`
  `PostMessageA`-outside-mutex fix only covered one of two call sites (the
  second, in `MCI_PLAY`'s no-SoundFont branch, is fixed now too), and the
  `TASK-24H-1231` pitch-overflow-cast fix only covered `CreateBitmap` (two
  sibling functions needed the identical fix). Also found and fixed a real
  gap in the verification workflow itself: AddressSanitizer's leak
  detection had been blanket-disabled for the *entire* `ctest` suite, not
  scoped to SDL3's own allocations — re-verified the premise (ran all 16
  real test binaries with leak detection on, found zero leaks anywhere)
  and simply removed the override rather than adding unneeded suppression
  infrastructure; proved both directions work by temporarily
  reintroducing an already-fixed test leak and confirming `ctest` now
  catches it. **LeakSanitizer is now enabled by default in
  `FREE_API_SANITIZE=address` builds** — a real behavior change, see §6.
  Every task in both rounds was verified across all 5 relevant build
  configurations (standalone, both target games, both sanitizers where
  applicable) before being marked `DONE`.

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
  `FreeApiSetWindowFullscreen` anywhere else.
* **`FREE_API_SANITIZE` CMake option** (`thread`/`address`, off by default)
  for running the test suite under ThreadSanitizer/AddressSanitizer — a
  plain `ctest` inside a sanitizer-configured build tree just works (no
  manual `LD_PRELOAD`) — see `docs/cmake-options.md`.
* **`docs/testing.md`** is the canonical "how to run the tests" reference;
  `docs/cmake-options.md` remains canonical for build *options*.

## 2. Current status

**Build status — all confirmed working after every change this session:**
* Standalone (`-DFREE_API_USE_SYSTEM_SDL3=ON`): configures, builds,
  **28/28** tests pass.
* As a subdirectory of `../free-eggbert` (Ninja), including the
  `free-api`+`free-direct` diamond dependency (`FREEDIRECT` backend):
  28/28.
* As a subdirectory of `../planetblupi` (Make): 28/28.
* `../free-direct` standalone: configures, builds, links `FREE_DIRECT`
  cleanly against `include/free_api_bridge.h`.
* The same 28-test suite also passes cleanly under both
  `-DFREE_API_SANITIZE=thread` and `=address`, via plain `ctest` (no
  manual env vars beyond `SDL_VIDEODRIVER`/`SDL_AUDIODRIVER`) — **including
  LeakSanitizer, now enabled by default** (see §6).

**Test status:** 28 CTest entries (grew from 26 this session:
`check_public_surface_baseline`/`check_public_surface_baseline_self_test`,
`TASK-24H-1238`), 28/28 passing in all three build modes and both
sanitizer builds.

**What does NOT work / known gaps:** unchanged except for items closed in
§3/§5 below. MCI digital-video remains intentionally unimplemented. Human
playtests still needed — MIDI audio sign-off (`TASK-24H-1221`), rendering
sign-off (`TASK-24H-1222`), and `MK_SHIFT`/`MK_CONTROL` drag-select/
flood-fill (`TASK-24H-0401`/`0403`) each have a real, tracked task with
concrete acceptance criteria, and one consolidated, runnable checklist
covering all of them:
[`docs/target-game-verification.md`](docs/target-game-verification.md).
See §8.

## 3. Recent changes (session 6 first, most recent first)

**Session 6** ran two full ground-up audit-and-fix cycles — see §1's
condensed summary for the highlights. Full per-task detail is in
`plan.md`'s "Deep Audit Follow-up" and "Deep Audit Follow-up #2" sections
(`TASK-24H-1229`–`1250`); full audit reasoning is in `audit.md` (current)
and `git log` (for round 1's now-superseded `audit.md`, still visible in
history). Notable process points not already in §1:

* **A real docs-vs-code drift was found and closed while updating
  documentation after both rounds**: `docs/cmake-options.md`/
  `docs/testing.md` still described the pre-`TASK-24H-1247`
  `detect_leaks=0` behavior and a stale `23/23` test count; both fixed.
  `docs/public-surface-audit.md` (a point-in-time snapshot) got a
  cross-reference to the new automated `check_public_surface_baseline`
  check so a future reader knows which one to trust for "has the header
  set grown since this was written."
* **Verification discipline held across both rounds**: every one of the
  22 tasks was independently re-verified against current source before
  implementing (not assumed correct from the audit finding's own text),
  and every fix that touched threading/locking/memory was rebuilt and
  retested under both `build-tsan/` and `build-asan/` in addition to the
  standalone and both target-game trees. Several fixes required real
  investigation beyond the audit's own suggested approach — e.g.
  `TASK-24H-0706`'s naive substitution would have silently broken
  absolute-path MIDI loading had the actual invocation pattern not been
  traced first (it's relative in this project's documented usage, so the
  substitution is safe, but that had to be proven, not assumed); see that
  task's `plan.md` entry for the reasoning.

**Sessions 1–5:** condensed into §1 above. See `git log` and `plan.md`
(each task's own entry has a detailed `DONE` verification note) for full
per-commit/per-task detail if needed — this file intentionally no longer
carries a blow-by-blow narrative of already-closed work from before
session 6.

## 4. Current blocker / main problem

**No blocker.** All build configurations work, 28/28 tests pass everywhere
(default and both sanitizer builds). Everything is committed and pushed to
`origin/develop` as of commit `a8cf266`.

**Process notes carried forward (still governing):**
* Do not stop/summarize/report while safe, AI-doable P0/P1/P2 work remains
  — only human-only playtests are legitimate exceptions (session 2→3
  correction, reconfirmed session 6).
* Before trusting a "done" claim from a prior session, an independent,
  skeptical re-verification is valuable — sessions 4, 5, and 6 (both
  rounds) all found real, fixable gaps this way, including gaps in fixes
  from the *immediately preceding* audit round (see §1's cross-validated
  findings). Consider the same posture before extending this file's claims
  further without re-checking them.

## 5. Known bugs and limitations

* **Fixed across sessions 1–5:** scaled `StretchBlt`'s X/Y clamp asymmetry,
  the `_findfirst` session-table leak, the `FreeApiCreateSurfaceDC`/
  `FreeApiDestroySurfaceDC`/`FreeApiSetWindowFullscreen` header-declaration
  gap, the dead sibling-vendored SDL3 CMake fallback, unconditional
  startup/asset-load logging, a real use-after-free in
  `FreeApiMmTimerBridge`, a real data race on `g_debugInput`, an SDL
  subsystem-refcount overflow in `timeSetEvent`, MIDI backend-failure log
  spam, and `LoadImageA`'s weak path normalization.
* **Fixed (session 6, round 1 — `TASK-24H-1229`/`1232`, the two most
  significant fixes of that round):**
  - A GDI handle double-free — `DeleteDC`/`DeleteObject`/
    `FreeApiDestroySurfaceDC` now clear the handle's magic-number tag
    before `delete`. The regression test for this is intentionally
    skipped under `FREE_API_SANITIZE=address`/`=thread` — see the test's
    own doc comment in `tests/test_gdi_regressions.cpp` for why that's
    inherent to this handle scheme, not a gap in the fix.
  - The MIDI subsystem's cross-translation-unit static-destruction-order
    race (`audit.md`'s original highest-severity finding). `MidiMusic.cpp`'s
    `g_midi` is now a function-local static accessed via `GetMidiState()`
    (a Meyer's singleton), so the language guarantees it's destroyed
    before the message-queue globals — closing the teardown race by rule,
    not link order. **Do not revert `GetMidiState()` back to a plain
    `static MidiState g_midi;`** — see `plan.md`'s `TASK-24H-1232` entry.
* **Fixed (session 6, round 2 — `TASK-24H-1245`–`1250`):** the second
  `PostMessageA`-under-mutex call site (`MCI_PLAY`'s no-SoundFont branch);
  `ResolveJoystick`'s bounds check compared a differently-signed value
  than the one used to index (a real out-of-bounds-read hazard, not
  reachable by either game's real device indices); the two sibling
  pitch-overflow computations in `src/internal/FreeApiGdi.cpp`; and
  LeakSanitizer's blanket disable (see §6 — **do not reintroduce
  `ASAN_OPTIONS=detect_leaks=0`**; if a genuine SDL3 false-positive is
  ever observed, use a scoped `LSAN_OPTIONS=suppressions=<file>` instead).
* **Confirmed environment artifact, not a code bug (unchanged):**
  `test_winuser_regressions` fails 6 checks under this sandbox's default
  Wayland display; passes under `SDL_VIDEODRIVER=dummy` or real X11/Xvfb.
* **Documented, not fixed (real but out-of-scope):**
  `FreeApiDestroySurfaceDC`/`AsCompatDC` segfaults on a genuinely garbage,
  never-allocated pointer instead of returning `FALSE` safely — no
  evidenced `free-direct` call site ever passes such a pointer.
  `CompatDC::selectedBitmap`'s dangling-pointer risk was traced this
  session and confirmed unreachable by both games' real delete ordering
  (`docs/out-of-scope.md`).
* **Documented, not fixed:** `CreateBitmap`'s 8-bit indexed path is
  greyscale-only; only reached by planetblupi's minimap in fullscreen mode.
* **Documented, not fixed:** `../free-direct`'s `Diagnostics.cpp` reaches
  past the documented bridge exception into the internal
  `FreeApi::Platform::ReadRssKB()` symbol directly. Fix-or-accept decision
  intentionally left open — see `docs/scope.md`.
* **By design, not a bug (reconfirmed every session):** MCI digital-video/
  AVI is permanently declined.
* **Still needs human verification, fully actionable, all four formally
  tracked with acceptance criteria:** `MK_SHIFT` drag-select
  (`TASK-24H-0401`), `MK_CONTROL` level-editor flood-fill
  (`TASK-24H-0403`), MIDI audio sign-off (`TASK-24H-1221`), rendering
  sign-off (`TASK-24H-1222`) — run all four via the single checklist at
  [`docs/target-game-verification.md`](docs/target-game-verification.md).

## 6. Architecture notes

Unchanged from prior sessions' notes except:

* `MidiMusic.cpp`'s `g_midi` is a function-local static
  (`static MidiState& GetMidiState()`), not a plain namespace-scope
  static — see §5. Every in-file reference goes through `GetMidiState()`,
  not a bare `g_midi`.
* **LeakSanitizer is enabled by default** in `FREE_API_SANITIZE=address`
  builds — `ASAN_OPTIONS=detect_leaks=0` was removed (`TASK-24H-1247`). A
  real leak in free-api's own code will now be caught by a plain `ctest`
  in a `build-asan/` tree; it previously silently wouldn't be. If a
  genuine SDL3 false-positive leak is ever observed, add a scoped
  `LSAN_OPTIONS=suppressions=<file>` — do not reintroduce a blanket
  disable.
* **New automated scope-policy guard** (`TASK-24H-1238`):
  `cmake/CheckPublicSurfaceBaseline.cmake` (run via the
  `check_public_surface_baseline` CTest test) extracts every public
  declaration in `include/*.h`/`include_non_windows/*.h` and fails,
  naming the symbol, if anything new appears that isn't already listed in
  `cmake/known-public-symbols.txt`. Adding a genuinely new, evidenced
  public API requires updating that baseline file too — see
  `docs/scope.md`'s "Automated enforcement" section.
* `src/internal/FreeApiPath.hpp`/`.cpp` has exactly one normalization
  function, `NormalizeFilesystemPath` (plus `BuildCommandLine`) — every
  path-normalizing call site in the codebase, including
  `NormalizeMidiPath` (`TASK-24H-0706`, closed this session), now calls it
  directly rather than reimplementing prefix-stripping logic.
* `FREE_API_SANITIZE` sanitizer runs need no manual `LD_PRELOAD`:
  ```bash
  cmake -B build-tsan -DFREE_API_SANITIZE=thread -DFREE_API_USE_SYSTEM_SDL3=ON
  cmake --build build-tsan -j"$(nproc)"
  cd build-tsan && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure
  ```
  Swap `thread` for `address` for AddressSanitizer. See
  `docs/cmake-options.md` for the mechanism and the manual-fallback form.
* `MidiState` (`src/MidiMusic.cpp`) has a `backendInitFailed` latch — do
  not remove it; without it, a persistent audio-init failure re-logs on
  every `MCI_OPEN` (one per song/track load).
* `tests/test_midi_backend_failure.cpp` is a deliberately separate binary
  from `test_mci_sequences.cpp` — the failure latch above is permanent for
  the process, so it can't share a binary with tests that need `MCI_OPEN`
  to keep succeeding.
* Everything from prior sessions (`ApplyKeyboardModifierFlags`,
  `_findfirst` self-cleaning, `StretchBlt`'s symmetric clamp,
  `include/free_api_bridge.h`, `cmake/test-fixtures/`,
  `FreeApiMmTimerBridge`'s packed-ID userdata, `g_debugInput` as
  `std::atomic_bool` with relaxed ordering) is unchanged.

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

# Sanitizer build -- plain ctest works, no manual LD_PRELOAD
cmake -B build-tsan -DFREE_API_SANITIZE=thread -DFREE_API_USE_SYSTEM_SDL3=ON
cmake --build build-tsan -j"$(nproc)"
cd build-tsan && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure
```

See `docs/cmake-options.md` for the full, canonical build-mode reference.

No lint/formatter is configured in this repository.

## 8. Next smallest tasks

**The entire AI-doable P0–P3 backlog is closed — twice over now.** Only 4
`TASK-24H-*` tasks remain `TODO`, all human-playtest-only, all formally
tracked with acceptance criteria and one consolidated checklist:
[`docs/target-game-verification.md`](docs/target-game-verification.md)
covers `MK_SHIFT`/`MK_CONTROL` (`TASK-24H-0401`/`0403`), MIDI audio
sign-off (`TASK-24H-1221`), and rendering/save-load sign-off
(`TASK-24H-1222`). Needs an actual human with a real display/audio
backend — a Claude Code session cannot complete these.

**If a future session has no human playtest results**, the next
productive step is a fresh, skeptical re-audit — the pattern sessions 4,
5, and 6 (both rounds) all used, and which has found real, previously-
unknown gaps *every single time it's been run*, including gaps in the
immediately-preceding audit round's own fixes. Re-check prior "done"
claims against current source rather than assuming they still hold. Do
not invent new P2/P3 busywork tasks just to have something to do; if a
re-audit genuinely finds nothing, say so and stop — that is a valid,
complete outcome.

## 9. Do not do yet

Unchanged from prior sessions' list, plus (session 6):

* Do not revert `GetMidiState()` (`src/MidiMusic.cpp`) back to a plain
  `static MidiState g_midi;` — see §5/§6.
* Do not reintroduce `ASAN_OPTIONS=detect_leaks=0` (`CMakeLists.txt`) — see
  §6. If a genuine SDL3 false-positive leak is ever observed, use a scoped
  `LSAN_OPTIONS=suppressions=<file>` instead.
* Do not revert `ResolveJoystick`'s bounds check (`src/winmm.cpp`) to
  comparing `static_cast<int>(uJoyID) < count` — that reintroduces a real
  out-of-bounds-read for a `uJoyID` with the high bit set.
* Do not revert `MidiMusicSendCommand`'s `MCI_PLAY` handler
  (`src/MidiMusic.cpp`) to calling `PostMessageA` while holding
  `GetMidiState().mtx` — capture notify data under the lock, post after
  releasing it (see the lock-order comment on `MidiState::mtx`).
* Do not remove `MidiState::backendInitFailed` (`src/MidiMusic.cpp`) — a
  persistent audio-init failure would re-log on every `MCI_OPEN` without it.
* Do not revert `FreeApiMmTimerBridge` to passing a map-node pointer as SDL
  userdata (`src/winmm.cpp`) — real use-after-free; see its doc comment.
* Do not revert `g_debugInput` to plain `bool`
  (`src/internal/FreeApiMessageQueue.{hpp,cpp}`) — real ThreadSanitizer-
  caught data race; has dedicated test coverage
  (`TestGDebugInputSurvivesRaceUnderSanitizer`).
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
* Do not re-declare `FreeApiCreateSurfaceDC`/`FreeApiDestroySurfaceDC`/
  `FreeApiSetWindowFullscreen` anywhere other than
  `include/free_api_bridge.h`.
* Do not "fix" `FreeApiDestroySurfaceDC`'s garbage-pointer segfault with a
  general handle-validation/table framework, or add
  `selectedBitmap`-clearing to `DeleteObject` — both traced and confirmed
  unreachable by real call ordering (`docs/out-of-scope.md`).
* Do not implement a real palette lookup for `CreateBitmap`'s 8-bit path.
* Do not touch `joystick`, MCI digital-video, DirectDraw, DirectSound,
  DirectPlay, or `free-direct` without a specific, separately-scoped
  request — except `include/free_api_bridge.h`'s three declarations.
* No new public API without a cited `file:line` usage site in
  `../free-eggbert` or `../planetblupi` — except the documented
  `free-direct`-bridge exception, now also mechanically enforced (§6).
* Do not implement any `TASK-24H-*` task beyond what its own "Required
  work"/"Out of scope" sections state.
* **When writing a new test in a file that `#include`s `<windows.h>`,
  remember `fopen()` is globally macro-redirected to `free_api_fopen`**
  (see `docs/headers.md`) — an absolute path like `/tmp/...` will have its
  leading slash silently stripped, turning it into an unfindable relative
  lookup. Use relative temp-file paths in tests, not absolute ones.
* When capturing a function's stdout output via `dup2` in a test, always
  `fflush(stdout)` *immediately* before the `dup2` call — any prior
  unflushed buffered output will otherwise land in the captured file ahead
  of the real target content.
* Do not dispatch a fork/subagent to do file-editing work on this repo
  while continuing to make direct edits yourself in parallel — a fork tried
  this once (session 5) and produced nothing usable, seemingly confused by
  seeing concurrent changes it didn't make. Either do the work directly,
  or dispatch one fork and wait for it before making further edits
  yourself. (Read-only, parallel audit forks — as used in both session 6
  rounds — are fine; the risk is specifically concurrent *writes*.)
* When editing a `.cpp` file with SDL_Log calls covered by
  `tests/test_sdl_log_gating.cpp`'s allowlist, remember the allowlist is
  **file:line-precise, not content-hash-based** — any edit that adds/removes
  lines above an allowlisted `SDL_Log` site will shift its line number and
  the test will fail until the allowlist entry is updated to match. This
  recurred many times across session 6's 22 tasks; it's an accepted
  tradeoff, not a test bug.

## 10. Resume prompt

```
Read NEXT.md first, then skim audit.md (the current, most recent deep
audit) for full context on what's already been found and fixed. plan.md
has 205 TASK-24H-* tasks; 200 are DONE, 1 is OBSOLETE. Only 4 remain TODO:
TASK-24H-0401/0403/1221/1222 (human-playtest-only -- see
docs/target-game-verification.md; a Claude Code session cannot complete
these).

The AI-doable backlog is exhausted, twice over. Do NOT invent new P2/P3
busywork tasks to have something to do. Your options, in order of
preference:
1. If the user has given a new, different instruction (a new feature,
   bugfix, or investigation request), that takes priority over anything
   in this file.
2. Otherwise, run a fresh, independent, skeptical re-audit of prior "done"
   claims against current source -- the pattern that has found real,
   previously-unknown gaps every single time it's been run across
   sessions 4, 5, and 6 (both rounds), including gaps in the
   immediately-preceding round's own fixes. Only act on what you actually
   find wrong; a clean re-audit with nothing found is a valid, complete
   outcome -- report it as such rather than manufacturing new work.

Do NOT stop/summarize while safe, AI-doable work genuinely remains --
continue autonomously. But do not manufacture busywork once it's actually
exhausted, either. Run the exact verification commands each task
specifies -- at minimum the standalone build's ctest (28/28), ideally also
both target games' ctest, and for anything touching src/winmm.cpp,
src/MidiMusic.cpp, or cross-thread/memory-management code also the
sanitizer builds (now including real LeakSanitizer coverage under
FREE_API_SANITIZE=address -- see section 6). Do not touch anything listed
in section 9. After finishing, update this file and plan.md's task
statuses.
```
