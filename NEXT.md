# NEXT.md

Handoff document for resuming work on `free-api`, for either a future
Claude Code session or a human developer. Reflects the actual repository
state as of commit `9850e07` (2026-07-08, `develop` branch, 5 commits
ahead of `origin/develop`, **not yet pushed**; working tree clean). See
[`plan.md`](plan.md) for the full task backlog (12 original tasks +
175-task `TASK-24H-0001`–`1220` backlog added this session) and
[`docs/audit-24h-free-api.md`](docs/audit-24h-free-api.md) for the full
24-hour deep audit this session's backlog was derived from.

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
functions, now formally declared in `include/free_api_bridge.h`).

**Current development phase:** a user-approved 24-hour autonomous
stabilization session ran this cycle. Phase 1 produced a from-scratch deep
audit (`docs/audit-24h-free-api.md`, six parallel independent research
passes). Phase 2 extended `plan.md` with a 175-task backlog
(`TASK-24H-0001`–`1220`) across 13 themes, entirely evidence-cited. Phase 3
worked the backlog top-down by priority: **both P0 tasks and 4 P1 task
groups (16 individual `TASK-24H-*` entries plus the 3 old-numbering tasks
they close) are DONE, verified, and committed** — the remaining ~159 tasks
are untouched, ready for a future session to continue from. See section 8.

**Important architectural decisions (unchanged this session):**

* Free API is a **static library**, normally built as a sibling
  `add_subdirectory()` of one of the two target games (which also provide
  SDL3). It also builds standalone via `-DFREE_API_USE_SYSTEM_SDL3=ON`.
* SDL3 is an **internal backend detail only** — public headers in
  `include/` must never expose SDL types.
* `CMakeLists.txt` keys off `CMAKE_PROJECT_NAME` to detect which game (if
  any) is driving the build, or an explicit `-DFREE_API_TARGET_GAME=...`
  override (`docs/cmake-options.md`).
* Real UI text for `LoadStringA` is extracted at **CMake configure time**
  from whichever game's own `resource/*.rc` is driving the build.
* The two target games use **two different, mutually-exclusive live timer
  mechanisms**: Free Eggbert uses `timeSetEvent`/`timeKillEvent`; Planet
  Blupi uses `SetTimer`/`KillTimer`/`WM_TIMER`. Both must keep working.
* **New this session:** the free-direct-bridge exception now has a real,
  single-source-of-truth header (`include/free_api_bridge.h`) instead of
  every consumer hand-declaring its own copy of the three bridge
  functions' signatures.

## 2. Current status

**Build status — all confirmed working this session, after every change:**
* Standalone (`-DFREE_API_USE_SYSTEM_SDL3=ON`): configures, builds, 17/17
  tests pass. The dead "sibling-vendored SDL3" fallback tier that used to
  produce a misattributed "missing submodule" error on a bare standalone
  configure has been removed (`TASK-24H-0001`) — a bare `cmake -S . -B
  build` with no override now fails with free-api's own clear guidance
  message, or succeeds if a working acquisition path is available.
* As a subdirectory of `../free-eggbert` (Ninja): configures and builds
  cleanly, including the `free-api`+`free-direct` diamond dependency
  (`FREEDIRECT` backend) — exactly 17 CTest tests registered (not 34),
  confirming `free-direct`'s `if(NOT TARGET free-api)` guard still works.
* As a subdirectory of `../planetblupi` (Make): same, 17/17.
* `../free-direct` standalone (`-DFREE_API_USE_SYSTEM_SDL3=ON`): configures,
  builds, links `FREE_DIRECT` cleanly against the new shared bridge header.

**Test status:** 17 test binaries per target-game build, **17/17 passing**
in all four build configurations above, under
`SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy`. Two new regression tests
added this session: `TestStretchBltScaledOutOfRangeSourceYClampsToEdgeRowLikeX`
(`tests/test_gdi_regressions.cpp`) and
`TestFindFirstFindNextRepeatedDrainWithoutCloseDoesNotLeakSession`
(`tests/test_file_paths.cpp`).

**What does NOT work / known gaps:** unchanged from before this session
except for the items closed in section 3 below — MCI digital-video is
still intentionally unimplemented (this session's feasibility audit
re-confirmed the permanent-decline decision, see
`docs/audit-24h-free-api.md` §5/§6); MIDI audio, palette/BMP rendering,
and the `MK_SHIFT`/`MK_CONTROL` drag-highlight still need a full human
playtest (see section 8, items now include the exact `Enter × 4` keyboard
sequence this session found to reach live planetblupi gameplay).

## 3. Recent changes (this session, most recent first)

* **Fix `_findfirst` session-table leak via auto-erase on exhaustion**
  (`TASK-0009`/`TASK-24H-0701`/`0702`) — `_findnext`'s exhaustion branch
  now erases its own session entry (`src/crt_io.cpp`) instead of only
  `_findclose` ever doing so. free-eggbert's design-mission picker drains
  without calling `_findclose` and used to leak one entry per screen
  visit, unbounded over a play session; now it doesn't. New regression
  test proves this across 5 drain cycles. `docs/supported-apis.md`'s
  `_findfirst` row updated.
* **Add shared header for the three free-direct-bridge functions**
  (`TASK-0002`/`TASK-24H-0101`) — new `include/free_api_bridge.h` is now
  the single authoritative declaration for `FreeApiCreateSurfaceDC`,
  `FreeApiDestroySurfaceDC`, and (newly found this session)
  `FreeApiSetWindowFullscreen`, replacing three separate hand-declared
  `extern "C"` copies (`../free-direct`, `examples/04_gdi_minimap.cpp`,
  `tests/test_gdi_regressions.cpp`). Verified across all 4 build
  configurations including the free-api+free-direct diamond dependency.
* **Gate unconditional startup/asset-load logging** (`TASK-24H-0303`/
  `0606`/`1101`/`1102`/`1103`) — `winuser_window.cpp`'s 13 unconditional
  `SDL_Log` calls (`CreateWindowExA`/`ShowWindow`/`UpdateWindow`/
  `SetFocus`) and `LoadImageA`'s success-path log now gated behind
  `FreeApiDiagnosticsEnabled()`/`FreeApiGdiDebugEnabled()`, matching the
  rest of the codebase's discipline. Confirmed quiet by default and fully
  restored under the relevant env flag.
* **Fix `StretchBlt` scaled-path source-Y clamp asymmetry**
  (`TASK-0003`/`TASK-24H-0601`) — the render-hot-path bug both this
  session's and the prior session's audits flagged as the single
  highest-value fix: out-of-range source X was clamped to the nearest
  edge pixel, but out-of-range source Y instead skipped the destination
  row entirely, leaving stale pixel data. Y now clamps identically to X.
  New regression test proves it.
* **Remove the dead-on-arrival "sibling-vendored SDL3" CMake fallback**
  (`TASK-24H-0001`) — this fallback tier could never actually succeed (it
  resolves relative to `CMAKE_SOURCE_DIR`, which is free-api's own root in
  the only situation where it's reached) and produced a confusing,
  misattributed "missing submodule" error instead of free-api's own clear
  guidance. Removed; `docs/cmake-options.md` updated.
* **Add 24-hour deep audit and 175-task backlog** — six parallel
  from-scratch research passes (free-eggbert usage, planetblupi usage,
  public header surface, `src/` implementation correctness, free-direct
  bridge + MCI AVI feasibility, docs/tests staleness) produced
  `docs/audit-24h-free-api.md`. `plan.md` extended (not replaced) with
  `TASK-24H-0001`–`1220` across 13 themes, each citing concrete evidence.
* **(prior session, `30e0c54` and earlier)** — see `git log` / the
  previous version of this file in git history for the full-scope
  from-scratch re-audit that produced the original `TASK-0001`–`0012`
  backlog.

## 4. Current blocker / main problem

**No blocker.** All build configurations work, 17/17 tests pass
everywhere, and this session's two P0 findings (the StretchBlt render bug
and the dead CMake fallback) are both fixed and verified. **5 commits are
sitting locally on `develop`, not yet pushed to `origin/develop`** — push
only if/when the user explicitly asks.

## 5. Known bugs and limitations

Unchanged from the prior session's list except where noted:

* **Fixed this session:** scaled `StretchBlt`'s X/Y clamp asymmetry
  (`TASK-0003`), the `_findfirst` session-table leak (`TASK-0009`), the
  `FreeApiCreateSurfaceDC`/`FreeApiDestroySurfaceDC` header-declaration gap
  (`TASK-0002`, now also covers `FreeApiSetWindowFullscreen`), the dead
  sibling-vendored SDL3 CMake fallback, and unconditional startup/
  asset-load logging in `winuser_window.cpp`/`LoadImageA`.
* **Confirmed environment artifact, not a code bug (unchanged):**
  `test_winuser_regressions` fails 6 checks under this sandbox's default
  Wayland display; passes under `SDL_VIDEODRIVER=dummy` or real X11/Xvfb.
* **New findings from this session's deeper audit, not yet fixed** (see
  `plan.md`'s `TASK-24H-*` backlog for the full, evidence-cited list):
  `PeekMessageA` fully ignores its filter arguments (harmless today,
  undocumented as intentional); `WaitMessage` isn't a true blocking wait
  (returns `TRUE` unconditionally after one ~1ms delay); `CreateBitmap`'s
  8-bit path is greyscale-only and its real severity against planetblupi's
  minimap is unverified (`TASK-24H-0602`/`0603`); `GetDeviceCaps` ignores
  its `index` argument for every value, not just `SIZEPALETTE`; four
  independent, non-shared path-normalization implementations exist
  (`TASK-24H-0703`–`0706`); `RegisterClassExA`/`WNDCLASSEXA` confirmed
  correctly absent.
* **By design, not a bug (reconfirmed this session):** MCI digital-video/
  AVI is permanently declined — this session ran a dedicated feasibility
  audit (user-approved, audit-only) and reconfirmed the decision: both
  games already treat a missing driver as a safe, silent skip, and real
  playback would require a brand-new Cinepak+MS Video 1 codec/decode
  subsystem with no player-visible defect to justify it.
* **Unknown, needs human verification (sharpened this session):**
  `MK_SHIFT` drives planetblupi's drag-select multi-unit highlight;
  `MK_CONTROL` drives a *separate* level-editor decor flood-fill feature
  (correcting a prior assumption these were the same feature) — neither
  has automated or human test coverage. This session found the exact
  keyboard path to reach live planetblupi gameplay from cold boot:
  **`Enter, Enter, Enter, Enter`** (dismisses the intro/title screens and
  the idle attract-mode auto-demo if it triggers, landing in
  `WM_PHASE_PLAY`). See `plan.md` `TASK-24H-0401`/`0402`/`0403`.

## 6. Architecture notes

Unchanged from the prior session except:

* **New:** `include/free_api_bridge.h` — the single authoritative
  declaration site for the three free-direct-bridge functions
  (`FreeApiCreateSurfaceDC`, `FreeApiDestroySurfaceDC`,
  `FreeApiSetWindowFullscreen`). Do not re-declare these elsewhere; update
  this header if their signatures ever need to change, and coordinate
  with `../free-direct` since it now depends on this header too.
* `src/crt_io.cpp`'s `_findfirst`/`_findnext`/`_findclose` session table
  (`g_findSessions`) now self-cleans on exhaustion inside `_findnext` —
  do not revert this to "only `_findclose` erases" without understanding
  the free-eggbert design-mission-picker leak it fixes (`TASK-0009`).
* `src/wingdi_blit.cpp`'s `StretchBlt` scaled path clamps out-of-range
  source coordinates identically on both axes now — do not reintroduce
  an asymmetry between X and Y without a specific, evidenced reason.

**Main modules, invariants, and everything else:** see the prior version
of this section (unchanged) — window lifecycle, GDI, WinMM/MIDI, file/CRT
helpers, internal shared state, and all listed invariants (public headers
never expose SDL types, both timer mechanisms must keep working,
`WM_MOUSEMOVE` lParam packing must stay bit-exact, `AdjustWindowRect` stays
an identity transform, etc.) are all still accurate and still apply.

## 7. Useful commands

```bash
# Build via a real target game
cd ../free-eggbert/cmake-build-debug && cmake . && ninja -j"$(nproc)"
cd ../planetblupi/build && cmake . && make -j"$(nproc)"

# Standalone build (no sibling game needed)
cmake -S . -B build -DFREE_API_USE_SYSTEM_SDL3=ON -DFREE_API_BUILD_TESTS=ON
cmake --build build -j"$(nproc)"
cd build && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure

# ../free-direct standalone (now depends on include/free_api_bridge.h)
cd ../free-direct && cmake -S . -B build -DFREE_API_USE_SYSTEM_SDL3=ON
cmake --build build -j"$(nproc)"

# Run the full test suite -- SDL_VIDEODRIVER=dummy is NOT optional under
# this sandbox's default (Wayland) display, or 6 window-position tests
# spuriously fail (confirmed environment-only)
cd ../free-eggbert/cmake-build-debug/FREE_API && \
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure
cd ../planetblupi/build/FREE_API && \
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure

# Force a specific target game's gating without needing its full build tree
cmake -B build -DFREE_API_TARGET_GAME=free-eggbert
```

See `docs/cmake-options.md` for the full, canonical build-mode reference
(updated this session with the `SDL_VIDEODRIVER=dummy` test note and the
corrected two-tier — not three-tier — SDL3 acquisition description).

No lint/formatter is configured in this repository.

## 8. Next smallest tasks

`plan.md` now has **175 new `TASK-24H-*` tasks**; **16 are DONE** (this
session), **~159 remain**. Pick up from the highest-priority remaining
ones — grep `plan.md` for `Priority: P1` first (33 total, 4 groups/8
individual tasks done), then `P2` (92 total), then `P3` (60 total). A few
concrete, well-scoped starting points:

1. **Investigate `CreateBitmap`'s 8-bit path against planetblupi's real
   minimap data** (`TASK-24H-0602`, then `0603` for the fix-or-document
   follow-through) — determine whether the confirmed greyscale-only
   8-bit path actually produces visibly wrong colors in practice (the
   game only overrides `m_bPalette` to `FALSE` when *not* fullscreen).
2. **Extract a shared MK_SHIFT/MK_CONTROL modifier-flag helper and add a
   synthetic-keystate unit test** (`TASK-24H-0404`/`0405`) — closes the
   automated-coverage gap for the mechanism (not the gameplay feature
   itself, which still needs the human playtest below).
3. **Human playtest: planetblupi `MK_SHIFT` drag-select and `MK_CONTROL`
   flood-fill** (`TASK-24H-0401`/`0402`/`0403`) — use the `Enter × 4`
   sequence found this session to reach live gameplay; `MK_CONTROL`'s
   level-editor flood-fill needs its in-game menu path mapped out first
   (`0402`) before it can be playtested (`0403`).
4. **Consolidate the four independent path-normalization implementations**
   (`TASK-24H-0703`–`0706`) — add parity/characterization tests first
   (`0703`), then extract a shared core (`0704`), then migrate
   `free_api_fopen` (`0705`) and `NormalizeMidiPath` (`0706`) onto it.
5. **Documentation cleanup pass** — many of the remaining P2/P3 tasks are
   small, independent, evidence-cited doc fixes (stale README/
   Documentation.md claims about `src/winapi.cpp`, missing
   `docs/supported-apis.md` rows, header `@note Status:` annotations,
   duplicate-declaration consolidations). Good candidates for a quick
   session — see the "Documentation", "Scope and Public API", and
   "Headers" themes in `plan.md`.

## 9. Do not do yet

Unchanged from the prior session's list, plus:

* Do not weaken or remove `REQUIRE_STRINGS`/`VERIFY_ID`/`USED_IDS_FILE` in
  `cmake/ExtractStringTable.cmake`/`CMakeLists.txt`.
* Do not build a general `.rc`/`.res` compiler.
* Do not touch joystick, MCI digital-video, DirectDraw, DirectSound,
  DirectPlay, or `free-direct` without a specific, separately-scoped
  request — **except** `include/free_api_bridge.h`'s three declarations,
  which are now the one narrow, already-coordinated exception.
* Do not assume `test_winuser_regressions` Wayland failures mean a real
  regression — closed investigation, confirmed environment-only.
* No real Unicode/`W` API implementations. No consolidating the two timer
  mechanisms. No "fixing" `AdjustWindowRect` to add real window-chrome math.
* No new public API without a cited `file:line` usage site in
  `../free-eggbert` or `../planetblupi` — except the documented
  `free-direct`-bridge exception, now formally declared in
  `include/free_api_bridge.h`; do not read it more broadly than that.
* No broad refactor of any currently-passing subsystem absent a specific,
  evidenced bug report.
* **Do not implement AVI/MCI digital-video playback** — this session ran a
  user-approved feasibility-only audit and reconfirmed permanent decline;
  do not treat that audit as reopening the implementation question.
* Do not implement any `TASK-24H-*` task beyond what its own "Required
  work"/"Out of scope" sections state.

## 10. Resume prompt

```
Read NEXT.md first, then skim docs/audit-24h-free-api.md's Executive
Verdict (§1) for full context. plan.md has 175 new TASK-24H-* tasks from
this session's 24-hour audit; 16 are DONE (grep "Status: DONE" near
"TASK-24H" to see which). Pick up from section 8's "Next smallest tasks"
list, or grep plan.md for "Priority: P1" and "Status: TODO" together to
find the next-highest-value remaining task. Make one small, verified
improvement at a time. Run the exact verification commands each task
specifies -- at minimum the standalone build's ctest, ideally also both
target games' ctest. Do not touch anything listed in section 9. After
finishing, update this file: move completed items into a "recently
completed" note, refresh test-status/blocker sections if they changed,
and update plan.md's task statuses to DONE with a brief verification note.
```
