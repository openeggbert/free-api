# NEXT.md

Handoff document for resuming work on `free-api`, for either a future
Claude Code session or a human developer. Reflects the actual repository
state as of commit `323de70` (2026-07-09, `develop` branch, 30 commits
ahead of `origin/develop`, **not yet pushed**; working tree clean; the
prior `6a523da` and earlier commits were pushed earlier this session — see
git log for the exact boundary). See [`plan.md`](plan.md) for the full
task backlog (12 original tasks + 183-task `TASK-24H-0001`–`1228` backlog)
and [`docs/audit-24h-free-api.md`](docs/audit-24h-free-api.md) for the full
24-hour deep audit that backlog was derived from.

**MILESTONE: the entire AI-doable P0–P3 backlog is now closed.** 178 of
183 `TASK-24H-*` tasks are `DONE`, 1 is `OBSOLETE`. The only 5 remaining
`TODO` tasks are 4 human-playtest-only items (`TASK-24H-0401`/`0403`/
`1221`/`1222`, all P1/P2, all with acceptance criteria and one consolidated
checklist — [`docs/target-game-verification.md`](docs/target-game-verification.md))
and 1 deliberately-deferred refactor (`TASK-24H-0706`). There is no more
safe, AI-doable backlog work to pick up without either a human completing
a playtest, or a fresh audit/re-scope finding new gaps.

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
**Session 4 (this pass)** ran in several waves: (1) an independent,
skeptical audit fork re-confirmed the P0/P1 claim but found 5 concrete
follow-up gaps, all implemented; (2) 8 more P2/P3 cleanup tasks; (3) a
genuine MIDI-audio/rendering human-sign-off tracking gap closed with
`TASK-24H-1221`/`1222` (formal P1 human-playtest tasks) and
`docs/target-game-verification.md` (`TASK-24H-1209`, one consolidated
checklist); (4) 7 more P2 cleanup tasks (path-normalization consolidation,
duplicate-declaration cleanup, documentation consistency); (5) **a second,
independent strict test-coverage audit fork** (user-requested: "prove or
disprove whether free-api is sufficiently tested for the actual WinAPI
subset used by both games") that produced a full symbol-by-symbol
coverage table and found **4 real, previously-untracked test-coverage
gaps** — `FreeApiRunWinMain` (the actual process-bootstrap bridge both
games launch through, flagged as the single highest-risk untested
function), `wsprintfA`, `OutputDebugStringA`, and `SetWindowTextA` — all
four real, live, used-by-both-games functions with zero test coverage and,
for three of them, zero documentation trail at all. All 4 are now closed
(`TASK-24H-1223`–`1226`) with direct tests, each verified across all three
build trees plus repeated stress runs. Two of those new tests themselves
had real bugs found and fixed during verification (a stack-lifetime
use-after-free in the `FreeApiRunWinMain` test, and two distinct bugs — a
`fopen`-macro path-normalization surprise and a stdio-buffering-vs-`dup2`
ordering bug — in the `OutputDebugStringA` test); see §3/§6 for detail.
**Session 5 (this pass)** worked straight through the *entire* remaining
P2 and P3 backlog to completion — 91 tasks closed across 30 commits, all
re-verified against current source (not copy-pasted from stale task text)
and tested across all three build trees per commit (test count grew from
24 to 26 during the session: `test_sdl_log_gating` and
`check_no_hardcoded_paths_self_test` both new). Highlights: added direct
test coverage for previously-untested WinUser/WinMM/GDI/Resource behaviors
(`GetSystemMetrics` fallback, the non-resizable-window compositor
workaround, `WM_CHAR` ASCII-only forwarding, the 5 resource-stub
contracts, the `mciGetDeviceIDA`/`MCI_CLOSE` id-collision, an end-to-end
notify-driven MIDI-loop test, `GetPixel`/`SetPixel`'s Memory-DC path,
`CreateBitmap`'s unsupported-bit-depth fallback, `GetObjectA`'s rejection
paths, and `SetCursor`'s first-call contract); added
`tests/test_sdl_log_gating.cpp` (`TASK-24H-1113`), a source-scan CTest
guard against future ungated `SDL_Log` additions, and
`cmake/CheckNoHardcodedPathsSelfTest.cmake` (`TASK-24H-0011`), which
exercises the real checker logic against controlled fixtures instead of
only the "nothing found" branch; ran a dedicated fork to produce
`docs/public-surface-audit.md` (`TASK-24H-0115`), a consolidated
required-by-game/test-infrastructure-only/free-direct-bridge/permanent-
stub/vestigial classification of every public declaration; fixed the
incomplete `install()`/export packaging (`TASK-24H-0007`); and closed out
every remaining documentation/verification cluster (diagnostics-logging,
WinMM-timer, WinUser message-pump, joystick, MCI/MidiMusic, GDI, README/
Documentation.md staleness).
**Found and fixed 4 more real, previously-unknown gaps while verifying
task premises** (not just implementing what tasks assumed):
`CreateDirectoryA`'s already-exists branch is dead code (`SDL_CreateDirectory`
itself is idempotent) — the *test* task's original premise was wrong, not
just untested; `EnsureVideoSubsystem`'s and `timeSetEvent`'s success-path
logs were both completely unconditional despite `TASK-24H-1101/1102`
supposedly having swept this area (closed as new `TASK-24H-1227` plus a
`TASK-24H-1113`-adjacent fix); `TASK-24H-1110`'s premise that
`CreateDirectoryA`'s only call sites are dead code was only true for
free-eggbert — planetblupi's `AddUserPath` call site is genuinely live;
and `TASK-24H-0907`'s premise about `cdaudio` being dead in free-eggbert
was wrong — `sound.cpp` (not the assumed-dead `soundbass.cpp`) is the
live sound backend given this build's `_BASS`/`_LEGACY` macro defaults,
and its `cdaudio` path is genuinely reachable via a real config option.
**178 of 183 new tasks are now DONE**, 1 marked OBSOLETE — all verified,
tested, and committed; **0 P0, 0 AI-doable P1, 0 AI-doable P2, 0 AI-doable
P3 tasks remain.** Only 5 tasks are still `TODO`: 4 human-playtest-only
(`TASK-24H-0401`/`0403`/`1221`/`1222`, all with acceptance criteria and one
consolidated checklist doc) and 1 deliberately-deferred refactor
(`TASK-24H-0706`) — see section 8.

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
  **26/26** tests pass.
* As a subdirectory of `../free-eggbert` (Ninja), including the
  `free-api`+`free-direct` diamond dependency (`FREEDIRECT` backend):
  26/26.
* As a subdirectory of `../planetblupi` (Make): 26/26.
* `../free-direct` standalone: configures, builds, links `FREE_DIRECT`
  cleanly against `include/free_api_bridge.h`.
* The same 26-test suite also passes cleanly (0 sanitizer reports) under
  both `-DFREE_API_SANITIZE=thread` and `=address`, via plain `ctest`
  (no manual env vars beyond `SDL_VIDEODRIVER`/`SDL_AUDIODRIVER`).

**Test status:** 26 CTest entries (started session 4 at 22), 26/26 passing
in all three build modes and both sanitizer builds (all four re-verified
fresh at the end of session 5). Session 4 added 2 new binaries
(`test_midi_backend_failure`, `test_winmain_bridge`); session 5 added a
3rd (`test_sdl_log_gating`, `TASK-24H-1113`) plus a new script-mode CTest
entry (`check_no_hardcoded_paths_self_test`, `TASK-24H-0011`) and many new
test functions across `test_winuser_regressions.cpp`,
`test_gdi_regressions.cpp`, `test_input_pipeline.cpp`,
`test_mci_sequences.cpp`, `test_resources.cpp`, and
`test_file_regressions.cpp` — see §3 below and git log for the full list.

**What does NOT work / known gaps:** unchanged except for items closed in
section 3 below. MCI digital-video remains intentionally unimplemented.
Human playtests still needed — MIDI audio sign-off (`TASK-24H-1221`),
rendering sign-off (`TASK-24H-1222`), and `MK_SHIFT`/`MK_CONTROL` drag-
select/flood-fill (`TASK-24H-0401`/`0403`) each now have a real, tracked
task with concrete acceptance criteria, and one consolidated, runnable
checklist covering all of them:
[`docs/target-game-verification.md`](docs/target-game-verification.md)
(`TASK-24H-1209`). See section 8.

## 3. Recent changes (session 5 first, then session 4, most recent first within each)

**Session 5** closed the entire remaining P2+P3 backlog (91 tasks) across
30 commits (`git log` has full detail per-commit; summary here, not a
per-task repeat of `plan.md`). First half (P2 sweep) below; second half
(P3 sweep to zero remaining) summarized at the end of this section.
* New test coverage: `GetSystemMetrics(SM_CYCAPTION)`/fallback,
  `CreateWindowExA` never sets `SDL_WINDOW_RESIZABLE` (compositor `WM_CLOSE`
  workaround), `WM_CHAR` ASCII-only forwarding (`test_input_pipeline.cpp`),
  the 5 resource-subsystem stub contracts (`LoadResource`/`SizeofResource`/
  `LockResource`/`UnlockResource`/`FreeResource`), the `mciGetDeviceIDA`/
  `MCI_CLOSE` device-id collision against a genuinely-open session, and an
  end-to-end notify-driven MIDI-loop test (as opposed to the pre-existing
  stress test, which only proved no-crash).
* **New `tests/test_sdl_log_gating.cpp`** (`TASK-24H-1113`): a source-scan
  CTest guard recognizing this codebase's 4 SDL_Log gating shapes (same-line
  if-gate, block if-gate, early-return-if-not-gate, `#if defined(__ANDROID__)`)
  plus a short file:line allowlist for confirmed-intentional unconditional
  logs. Verified with a genuine negative control. **Caught 2 real,
  previously-uncaught ungated logs while being built**: `EnsureVideoSubsystem`'s
  and `timeSetEvent`'s success-path logs — both fixed (`TASK-24H-1227` and a
  `winmm.cpp` fix folded into `TASK-24H-1113`'s own commit).
* **Real bug found while executing `TASK-24H-0709`** (not just documented):
  `CreateDirectoryA`'s "already exists → FALSE/ERROR_ALREADY_EXISTS" branch
  is dead code — `SDL_CreateDirectory` itself reports success for an
  already-existing path, so the branch is unreachable. Test re-scoped to
  lock in the real (idempotent-`TRUE`) behavior instead; documented in
  `docs/out-of-scope.md`.
* **`TASK-24H-1110`'s own premise corrected while verifying it**: it claimed
  both games' `CreateDirectoryA` call sites (`AddUserPath`) are dead code —
  true for free-eggbert (`#if _CD || _LEGACY`, never defined) but **false**
  for planetblupi, whose `AddUserPath` has no such guard and is live,
  called from `decio.cpp`'s save/load paths.
* Closed the diagnostics-logging cluster (`TASK-24H-1106`-`1111`), the
  WinMM/WinUser timer design-decision cluster (`TASK-24H-0501`/`0502`/
  `0503`/`0507`/`0509`), the WinUser message-pump cluster (`TASK-24H-0207`/
  `0208`/`0212`/`0304`), stale joystick docs (`TASK-24H-1001`-`1003`),
  stale README/Documentation.md claims (`TASK-24H-1201`/`1202`/`1204`), and
  the VK_*/MK_* documentation tasks (`TASK-24H-0409`/`0410`) — all re-verified
  against current source, not copy-pasted from the task text.
* A dispatched investigation fork (documentation/verification batch) did
  not land any usable commits — its final report was inconclusive and
  `git log` showed nothing from it. No harm done (clean repo state
  throughout); the same ground was covered directly instead. Lesson: avoid
  running a fork concurrently with direct main-thread edits to the same
  repo — it seems to get confused seeing changes it didn't make.
* **Fixed the incomplete `install()`/export packaging** (`TASK-24H-0007`):
  removed a dangling `EXPORT free-api-targets` clause with no matching
  `install(EXPORT ...)` (so `find_package()` could never have worked, and
  no consumer needs it — both games use `add_subdirectory()`); added the
  missing `install(DIRECTORY include_non_windows/...)` call. Verified via a
  real `cmake --install` into a scratch prefix, before/after.
* **A second, better-behaved fork** produced `docs/public-surface-audit.md`
  (`TASK-24H-0115`) — a consolidated classification of every public
  declaration in `include/*.h`/`include_non_windows/*.h`. It correctly
  avoided the earlier fork's confusion (left the parent session's
  concurrently-modified files untouched) and found one real classification
  mismatch: `CloseHandle` has zero call sites anywhere (not test-
  infrastructure-only as `TASK-24H-0114` assumed) — filed and closed as
  `TASK-24H-1228`.
* **`TASK-24H-0907`'s premise was wrong**, found while re-verifying it:
  free-eggbert has two alternate sound-backend files (`sound.cpp`/
  `soundbass.cpp`, selected by `_BASS`/`_LEGACY` macros); with both
  undefined in this build, `sound.cpp` — not the assumed-dead
  `soundbass.cpp` — is live, and its `cdaudio` path is genuinely reachable
  via a real `CDAudio=` config option, not dead code.
* Closed the entire P3 backlog to zero: WinUser small-cluster docs
  (`TASK-24H-0116`-`0412`, 11 tasks), `GetSystemMetrics`/`MoveWindow`/
  `GetDeviceCaps`/`CloseHandle`/`UnlockResource` docs (7 tasks), MCI/
  MidiMusic.cpp cleanup (`TASK-24H-0806`-`0910`/`1207`, 7 tasks — includes
  removing 3 genuinely-dead `MCIERR_*` constant duplicates), joystick/file/
  timer/logging-consolidation docs (9 tasks), build-verification and
  header cleanup (`TASK-24H-0015`-`0217`, 6 tasks — includes consolidating
  a duplicate `byte` macro between `rpcndr.h`/`wtypes.h`, confirmed a
  guaranteed no-op via the real include graph), and the final GDI
  test-coverage cluster (`TASK-24H-0611`/`0612`/`0614`/`0616`).
  `TASK-24H-0614`'s verification of `examples/04_gdi_minimap.cpp` is
  explicitly partial: confirmed it builds and its GDI calls execute
  without crashing headlessly, but full visual/pixel-level confirmation
  needs a real display and isn't claimed.
* Re-verified all 26 tests pass cleanly under both `FREE_API_SANITIZE=thread`
  and `=address` at the very end of the session (0 sanitizer reports).

**Session 4:**

* **Closed all 4 gaps found by the strict test-coverage audit fork**
  (`TASK-24H-1223`-`1226`, plus companions `TASK-24H-0313`/`0309`):
  - `FreeApiRunWinMain` (`src/winmain_bridge.cpp`, the real process-
    bootstrap bridge both games launch through): new
    `tests/test_winmain_bridge.cpp` covers NULL-entryPoint rejection,
    the full argv→hInstance/hPrevInstance/lpCmdLine/nCmdShow contract,
    and exit-code passthrough. Needed a dummy `WinMain` definition to
    satisfy the linker (documented as dead code). **Found a real bug in
    the test itself**: comparing `lpCmdLine`'s content after
    `FreeApiRunWinMain` had already returned was a genuine
    use-after-free (small-string-optimized onto that function's own
    stack frame) that happened to "work" in 2 of 3 build trees by luck
    — fixed by capturing the content inside the fake `WinMain` while
    the pointer is still valid.
  - `wsprintfA` (`src/winuser_message.cpp`): new test matches both
    games' exact call shape, NULL-argument safety, and the internal
    1024-byte buffer-edge truncation behavior. Added a
    `docs/supported-apis.md` row (previously undocumented anywhere).
  - `OutputDebugStringA` (`src/winbase.cpp`): new test captures real
    stdout output via `dup2`. **Found two more real bugs, both in the
    test**: (1) the read-back `fopen()` call was silently rewritten by
    free-api's own global `fopen`→`free_api_fopen` macro, stripping
    the leading slash off an absolute `/tmp/...` path and turning it
    into an unfindable relative lookup — `mkstemp`/`open`/`access`
    (not macro-redirected) all worked fine the whole time, which made
    this a very confusing failure; (2) stdout is fully buffered off a
    TTY, so an unflushed `Check()` printf sitting in the buffer
    immediately before the `dup2` redirect got flushed into the temp
    file ahead of the real target content. Both documented as a
    test-authoring caveat in `docs/headers.md`.
  - `SetWindowTextA` (`src/winuser_window.cpp`): new test verifies the
    real SDL window title actually changes via `SDL_GetWindowTitle`,
    not just "doesn't crash". Closed `TASK-24H-0313` (added the
    missing `DestroyWindow`/`MoveWindow`/`SetWindowTextA`
    `docs/supported-apis.md` rows) and `TASK-24H-0309` (documented
    `MoveWindow`'s confirmed-harmless stale-logical-size gap) as
    natural companions.
* **Deduplicated `_lopen`/`_lread`/`_lclose` declarations** (`TASK-24H-0103`/
  `0713`) — `include/io.h` redeclared them verbatim even though its own
  `#include <windows.h>` already transitively pulls in `winbase.h`'s
  declaration; removed the redundant one.
* **Updated README's input-pipeline "Verified:" line** (`TASK-24H-1203`) to
  match current test coverage (full `VK_*` sweep, `F10` SYSKEY quirk,
  unmapped-scancode case, boundary `lParam` packing, 2000-event stress test)
  instead of the original 5 message types.
* **Path-normalization consolidation** (`TASK-24H-0703`/`0704`/`0705`/`0706`)
  — added direct characterization tests for `NormalizeFilesystemPath` and
  `free_api_fopen` (`TASK-24H-0703`); `TASK-24H-0704` marked OBSOLETE (its
  premise, unifying `NormalizePath` with `NormalizeFilesystemPath`, is moot
  now that `NormalizePath` no longer exists); **implemented**
  `TASK-24H-0705` — `free_api_fopen` (`include/windows.h`) now calls
  `NormalizeFilesystemPath` directly via a forward declaration instead of
  reimplementing the identical prefix-normalization logic inline (verified
  no new link dependency: both games' own executables already always link
  free-api). `TASK-24H-0706` (the same migration for `NormalizeMidiPath`)
  deliberately left for a dedicated future pass — its fallback logic is
  more involved and it's file-local code, not a header-only change.
* **Created `docs/target-game-verification.md`; closed `TASK-24H-1209`** —
  one consolidated, runnable checklist covering launch instructions, MIDI
  audio sign-off, rendering sign-off, the `LoadStringA`/`"RES_<id>"` check,
  the save/load check, both planetblupi gameplay-access sequences, and the
  `MK_SHIFT`/`MK_CONTROL` playtest steps — cross-referencing rather than
  duplicating `docs/target-games.md` and the relevant task IDs.
* **Added formal `TASK-24H-1221`/`1222`** (MIDI audio / rendering human
  sign-off, P1, human-playtest-only) — closes the real tracking gap this
  session's own earlier audit found (mentioned in `NEXT.md` prose across
  multiple sessions, never a real backlog task). Both have concrete
  acceptance criteria (both games start; music audible/non-corrupted or
  gracefully silent; no repeated `MCI_OPEN` log spam; rendering/blitting/
  asset-loading visually correct; no `"RES_<id>"` UI leakage; save/load
  works) and explicitly cannot be completed by an AI agent.
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

**No blocker.** All build configurations work, 26/26 tests pass everywhere
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
* **Fixed (TASK-24H-1229, deep-audit follow-up):** a GDI handle double-free
  — `DeleteDC`/`DeleteObject`/`FreeApiDestroySurfaceDC` now clear the
  handle's magic-number tag before `delete`, so a double-delete of the
  same handle is rejected instead of risking a double-free against
  already-freed memory. The regression test for this is intentionally
  skipped under `FREE_API_SANITIZE=address`/`=thread` — see the test's own
  doc comment in `tests/test_gdi_regressions.cpp` for why that's inherent
  to this handle scheme, not a gap in the fix.
* **Fixed (TASK-24H-1232, deep-audit follow-up):** the MIDI subsystem's
  cross-translation-unit static destruction order dependency on the
  message queue globals (previously safe only by incidental link order —
  audit.md's highest-severity finding, R1). `MidiMusic.cpp`'s `g_midi` is
  now a function-local static accessed via `GetMidiState()` (a Meyer's
  singleton) instead of a plain namespace-scope static, so the language
  guarantees it's constructed after, and therefore destroyed before,
  `g_messageQueue`/`g_messageQueueMutex` — closing the teardown race by
  rule, not link order. **Do not revert `GetMidiState()` back to a plain
  `static MidiState g_midi;`** without re-reading this task's writeup in
  `plan.md`/`audit.md` §7 Finding R1 first; that would silently
  reintroduce the exact race this fix closes.
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

**The entire AI-doable P0–P3 backlog is closed.** Only 5 `TASK-24H-*`
tasks remain `TODO` (1 P2, `TASK-24H-0704`, is marked `OBSOLETE` rather
than `TODO`/`DONE` — see section 3):

1. **4 human-playtest-only tasks**, all formally tracked with acceptance
   criteria and one consolidated checklist:
   [`docs/target-game-verification.md`](docs/target-game-verification.md)
   covers `MK_SHIFT`/`MK_CONTROL` (`TASK-24H-0401`/`0403`), MIDI audio
   sign-off (`TASK-24H-1221`), and rendering/save-load sign-off
   (`TASK-24H-1222`). Needs an actual human with a real display/audio
   backend — a Claude Code session cannot complete these.
2. **`TASK-24H-0706`** (migrate `NormalizeMidiPath`'s backslash-conversion
   step to call `NormalizeFilesystemPath` directly, same pattern
   `TASK-24H-0705` used for `free_api_fopen`) — left `TODO` on purpose
   since it touches file-local MIDI-subsystem code with a more involved
   fallback than `0705`'s case; verify carefully in its own dedicated pass.
   This is the one remaining AI-doable task, deliberately deferred rather
   than rushed.

**If a future session has no human playtest results and doesn't want to
touch `TASK-24H-0706` alone**, the next productive step is a fresh,
skeptical re-audit (the way session 4's and session 5's audit forks did) —
re-check prior "done" claims against current source rather than assuming
they still hold, the same pattern that found 7 real premise-was-wrong bugs
across sessions 4-5. Do not invent new P2/P3 busywork tasks just to have
something to do; if nothing is actually wrong, say so.

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
* **When writing a new test in a file that `#include`s `<windows.h>`,
  remember `fopen()` is globally macro-redirected to `free_api_fopen`**
  (see `docs/headers.md`) — an absolute path like `/tmp/...` will have its
  leading slash silently stripped, turning it into an unfindable relative
  lookup. Use relative temp-file paths in tests, not absolute ones.
* When capturing a function's stdout output via `dup2` in a test, always
  `fflush(stdout)` *immediately* before the `dup2` call (not just at some
  earlier point) — any prior unflushed buffered output (e.g. from a
  `Check()` call) will otherwise land in the captured file ahead of the
  real target content once the eventual `fflush()` runs.
* Do not dispatch a fork/subagent to do file-editing work on this repo
  while continuing to make direct edits yourself in parallel — session 5
  tried this (a fork for a documentation batch while the main thread did
  test-writing) and the fork's final report was inconclusive with nothing
  usable landed in git, seemingly confused by seeing concurrent changes it
  didn't make. Either do the work directly, or dispatch one fork and wait
  for it before making further edits yourself.
* When editing a `.cpp` file with SDL_Log calls covered by
  `tests/test_sdl_log_gating.cpp`'s allowlist, remember the allowlist is
  **file:line-precise, not content-hash-based** — any edit that adds/removes
  lines above an allowlisted `SDL_Log` site will shift its line number and
  the test will fail until the allowlist entry is updated to match (this is
  a known, accepted tradeoff, not a test bug — see `TASK-24H-1113`).

## 10. Resume prompt

```
Read NEXT.md first, then skim docs/audit-24h-free-api.md's Executive
Verdict (§1) for full context. plan.md has 183 new TASK-24H-* tasks; 178
are DONE, 1 is OBSOLETE. Only 5 remain TODO: TASK-24H-0401/0403/1221/1222
(human-playtest-only -- see docs/target-game-verification.md; a Claude
Code session cannot complete these) and TASK-24H-0706 (deliberately-
deferred NormalizeMidiPath migration -- see section 8 for detail).

The AI-doable backlog is exhausted. Do NOT invent new P2/P3 busywork tasks
to have something to do. Your options, in order of preference:
1. If TASK-24H-0706 is genuinely still open and unimplemented (check
   plan.md first -- it may have been done in an intervening session), do
   it carefully in its own pass per section 8's notes.
2. Otherwise, run a fresh, independent, skeptical re-audit of prior "done"
   claims against current source (git log may have moved since this file
   was written) -- the pattern that found 7 real premise-was-wrong bugs
   across sessions 4-5. Only act on what you actually find wrong; a clean
   re-audit with nothing found is a valid, complete outcome -- report it
   as such rather than manufacturing new work.
3. If the user has given a new, different instruction (a new feature,
   bugfix, or investigation request), that takes priority over anything
   in this file -- this backlog was for a specific 24-hour stabilization
   effort, already complete.

Do NOT stop/summarize while safe, AI-doable work genuinely remains --
continue autonomously. But do not manufacture busywork once it's actually
exhausted, either -- that also violates the spirit of the same instruction.
Run the exact verification commands each task specifies -- at minimum the
standalone build's ctest (26/26), ideally also both target games' ctest,
and for anything touching src/winmm.cpp, src/MidiMusic.cpp, or cross-thread
code also the sanitizer builds (§6/§7 -- now just a plain `ctest`, no
manual LD_PRELOAD). Do not touch anything listed in section 9. After
finishing, update this file and plan.md's task statuses.
```
