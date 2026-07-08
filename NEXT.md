# NEXT.md

Handoff document for resuming work on `free-api`, for either a future
Claude Code session or a human developer. Reflects the actual repository
state as of commit `d48824c` (2026-07-08, `develop` branch, 12 commits
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

**Current development phase:** two consecutive sessions this cycle. Session
1 produced a from-scratch deep audit (`docs/audit-24h-free-api.md`) and
extended `plan.md` with a 175-task backlog (`TASK-24H-0001`–`1220`), then
implemented the 2 P0 tasks plus a handful of quick P1s. Session 2
(implementation-only, no new audit/planning) worked systematically through
the backlog's remaining P1 tasks by theme (message loop, timers, cursor/
mouse input, file/path, GDI, LoadStringA hardening, MIDI/MCI, build
verification). **36 of 175 new tasks are now DONE** — all verified,
tested, and committed; **0 P0 and 6 P1 tasks remain**, the rest are P2/P3.
See section 8.

**Important architectural decisions (unchanged across both sessions):**

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
* MK_SHIFT/MK_CONTROL's OR-logic is now a shared, directly-testable helper
  (`FreeApi::Internal::ApplyKeyboardModifierFlags`,
  `src/internal/FreeApiMessageQueue.{hpp,cpp}`) instead of duplicated
  inline code.

## 2. Current status

**Build status — all confirmed working after every change this session:**
* Standalone (`-DFREE_API_USE_SYSTEM_SDL3=ON`): configures, builds,
  **22/22** tests pass (17 original + 5 new `ExtractStringTable.cmake`
  negative-path CTest tests added this session).
* As a subdirectory of `../free-eggbert` (Ninja), including the
  `free-api`+`free-direct` diamond dependency (`FREEDIRECT` backend):
  22/22.
* As a subdirectory of `../planetblupi` (Make): 22/22.
* `../free-direct` standalone: configures, builds, links `FREE_DIRECT`
  cleanly against `include/free_api_bridge.h`.

**Test status:** 22 test binaries/CTest entries, **22/22 passing** in all
three build modes, under `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy`.
This session added regression tests to: `test_winuser_regressions.cpp`
(PeekMessageA never-sleeps timing, WaitMessage unconditional-TRUE, WM_CLOSE
direct WM_DESTROY assertion, synthetic-keystate modifier-flag helper test),
`test_timer_regressions.cpp` (SetTimer auto-ID/min-interval clamp, fast-
timer-doesn't-starve-input), `test_input_pipeline.cpp` (A-Z letter VK
mapping, unmapped-scancode-produces-no-message), `test_file_regressions.cpp`
(bare `"\User"` literal stays relative to CWD, `DeleteFileA`),
`test_gdi_regressions.cpp` (bridge GDI helpers' rejection/edge cases),
`test_mci_sequences.cpp` (no-soundfont silent-success path, `NormalizeMidiPath`
case-fallback), plus 5 new CMake-level `ExtractStringTable.cmake`
negative-path tests (`CMakeLists.txt` + `cmake/test-fixtures/`).

**What does NOT work / known gaps:** unchanged except for items closed in
section 3 below. MCI digital-video remains intentionally unimplemented.
Human playtests (MIDI audio, rendering, `MK_SHIFT`/`MK_CONTROL` drag-
select/flood-fill) still needed — see section 8.

## 3. Recent changes (session 2, this pass, most recent first)

* **Documented the `free-direct` bridge build and diamond-dependency
  verification** (`TASK-24H-0005`/`0006`) — new `docs/cmake-options.md`
  section; re-confirmed both facts with real builds this session.
* **Added MIDI/MCI regression tests** (`TASK-24H-0903`/`0904`) — the
  missing-soundfont silent-success path (confirmed this environment
  naturally exercises it: no `.sf2` ships anywhere in these three repos)
  and `NormalizeMidiPath`'s uppercase case-fallback (exercised indirectly
  through `MCI_OPEN`, since the function has file-local linkage).
* **Added negative-path tests for `ExtractStringTable.cmake`'s fail-loud
  gates** (`TASK-24H-0801`/`0802`) — new fixture files
  (`cmake/test-fixtures/`) and 5 new CTest tests using `WILL_FAIL`,
  confirmed each fails at the exact expected `FATAL_ERROR` line via
  `ctest -VV`, not some unrelated error.
* **Investigated `CreateBitmap`'s 8-bit path; added bridge GDI helper
  edge-case tests** (`TASK-24H-0602`/`0603`/`0607`/`0610`) — traced
  precisely: planetblupi's minimap only takes the buggy greyscale-only
  8-bit path in fullscreen mode (not the shipped `FullScreen=0` default);
  documented rather than fixed (a real fix needs either a general Win32
  palette API or game-specific hard-coding, both out of scope). **Found
  and worked around a real segfault** while writing the new test:
  `FreeApiDestroySurfaceDC`/`AsCompatDC` dereferences a genuinely garbage
  pointer unconditionally rather than returning `FALSE` safely — no
  evidenced real caller ever does this, so it's documented as an
  out-of-scope robustness gap, not fixed with a new handle-validation
  framework.
* **Added file/path regression tests** (`TASK-24H-0708`/`0714`) — a bare
  `"\User"` literal (free-eggbert's exact call shape) proven to stay
  relative to CWD rather than escaping to the real filesystem root; new
  `DeleteFileA` coverage (a real call site with zero prior tests).
* **Extracted MK_SHIFT/MK_CONTROL helper; added input-mapping tests**
  (`TASK-24H-0404`/`0405`/`0406`/`0407`) — `ApplyKeyboardModifierFlags`
  now directly unit-testable with a synthetic keystate array; added A-Z
  letter-VK and unmapped-scancode regression tests.
* **Added message-loop and timer regression tests**
  (`TASK-24H-0203`/`0205`/`0210`/`0505`/`0508`) — `PeekMessageA` never-
  sleeps timing guard, `WaitMessage`'s unconditional-`TRUE` contract,
  `WM_CLOSE`→`WM_DESTROY` direct assertion, `SetTimer` auto-ID/min-
  interval-clamp coverage, fast-timer-doesn't-starve-input coverage.
* **(session 1)** Deep audit + 175-task backlog; fixed `StretchBlt`'s
  scaled-path Y-clamp asymmetry (the flagship P0 render bug) and the dead
  "sibling-vendored SDL3" CMake fallback; fixed the `_findfirst` session
  leak; added `include/free_api_bridge.h`; gated `winuser_window.cpp`/
  `LoadImageA`'s unconditional logging. See `git log` for full commit
  detail on any of the above.

## 4. Current blocker / main problem

**No blocker.** All build configurations work, 22/22 tests pass
everywhere. **12 commits are sitting locally on `develop`, not yet pushed
to `origin/develop`** — push only if/when the user explicitly asks.

## 5. Known bugs and limitations

* **Fixed across both sessions:** scaled `StretchBlt`'s X/Y clamp
  asymmetry, the `_findfirst` session-table leak, the
  `FreeApiCreateSurfaceDC`/`FreeApiDestroySurfaceDC`/
  `FreeApiSetWindowFullscreen` header-declaration gap, the dead
  sibling-vendored SDL3 CMake fallback, unconditional startup/asset-load
  logging in `winuser_window.cpp`/`LoadImageA`.
* **Confirmed environment artifact, not a code bug (unchanged):**
  `test_winuser_regressions` fails 6 checks under this sandbox's default
  Wayland display; passes under `SDL_VIDEODRIVER=dummy` or real X11/Xvfb.
* **New finding this session, documented not fixed (real but
  out-of-scope):** `FreeApiDestroySurfaceDC`/`AsCompatDC`
  (`src/internal/FreeApiGdi.cpp`) segfaults on a genuinely garbage,
  never-allocated pointer instead of returning `FALSE` safely — no
  evidenced `free-direct` call site ever passes such a pointer (it always
  forwards handles it received from `FreeApiCreateSurfaceDC` itself).
  Fixing this would need a general handle-validation/table framework,
  against this project's scope discipline. Documented in
  `tests/test_gdi_regressions.cpp`'s `TestBridgeGdiHelpersRejectionAndEdgeCases`.
* **Confirmed, documented not fixed:** `CreateBitmap`'s 8-bit indexed path
  is greyscale-only; confirmed this session it's only reached by
  planetblupi's minimap in fullscreen mode (not the shipped
  `FullScreen=0` default) — see `src/wingdi_bitmap.cpp`'s comment for the
  full trace.
* **By design, not a bug (reconfirmed session 1):** MCI digital-video/AVI
  is permanently declined.
* **Still needs human verification (sharpened session 1, unchanged this
  session):** `MK_SHIFT` drives planetblupi's drag-select multi-unit
  highlight; `MK_CONTROL` drives a *separate* level-editor decor
  flood-fill. Exact keyboard path to reach live planetblupi gameplay from
  cold boot: **`Enter, Enter, Enter, Enter`**. See `plan.md`
  `TASK-24H-0401`/`0402`/`0403`.
* **6 P1 tasks remain TODO** (down from 33) — see section 8.

## 6. Architecture notes

Unchanged from the prior session's version of this section except:

* `FreeApi::Internal::ApplyKeyboardModifierFlags(keys, base)`
  (`src/internal/FreeApiMessageQueue.{hpp,cpp}`) is now the single place
  MK_SHIFT/MK_CONTROL OR-logic lives — both the mouse-motion and
  mouse-button SDL event handlers call it. Test-only forward-declared into
  `tests/test_winuser_regressions.cpp` (matching the project's established
  pattern for reaching internal state from tests, e.g.
  `test_gdi_regressions.cpp`'s diagnostic-counter externs).
* `src/crt_io.cpp`'s `_findfirst` session table self-cleans on exhaustion
  inside `_findnext` now — do not revert without understanding the
  free-eggbert leak this fixes.
* `src/wingdi_blit.cpp`'s `StretchBlt` scaled path clamps out-of-range
  source coordinates identically on both axes — do not reintroduce an
  asymmetry.
* `include/free_api_bridge.h` is the single authoritative declaration site
  for the three free-direct-bridge functions.
* `cmake/test-fixtures/` holds small, deliberately-broken fixtures used
  only by `CMakeLists.txt`'s `extract_string_table_*` negative-path
  CTest tests — not real game resources, don't confuse with
  `cmake/used-string-ids/`.

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

# Run just the new ExtractStringTable.cmake negative-path tests
cd build && ctest -R extract_string_table -VV
```

See `docs/cmake-options.md` for the full, canonical build-mode reference.

No lint/formatter is configured in this repository.

## 8. Next smallest tasks

**0 P0, 6 P1 tasks remain TODO** in `plan.md`'s new backlog (grep
`Priority: P1` + `Status: TODO` together). Concrete starting points:

1. **Resolve or document the `MCI_OPEN_PARMS`/`MCI_PLAY_PARMS`
   include-order-fragile shape aliasing** (`TASK-24H-0107`) — verify no
   code path relies on the `MCI_DGV_*`-shaped alias winning, then add an
   explanatory comment at both guard sites.
2. **A sanitizer-verified regression test for the `timeKillEvent`/
   callback-in-flight race** (`TASK-24H-0506`) — adds an opt-in
   ThreadSanitizer/AddressSanitizer CMake build option; higher effort
   than the other remaining P1s, tackle when there's a clear block of
   time for it.
3. **Human playtest: planetblupi `MK_SHIFT` drag-select and `MK_CONTROL`
   flood-fill** (`TASK-24H-0401`/`0402`/`0403`) — use the `Enter × 4`
   sequence to reach live gameplay; `MK_CONTROL`'s level-editor flood-fill
   needs its in-game menu path mapped out first (`0402`).
4. **Document `free-direct`'s reach into internal
   `FreeApi::Platform::ReadRssKB`** (`TASK-24H-0102`) — a real,
   currently-undocumented cross-repo coupling to an internal (non-bridge)
   symbol found in session 1's audit.
5. **After the P1s: 92 P2 and 60 P3 tasks remain** — mostly documentation/
   header-hygiene cleanup with zero behavior change. Good candidates for a
   quick session; see the "Scope and Public API", "Headers", and
   "Documentation" themes in `plan.md`.

## 9. Do not do yet

Unchanged from the prior session's list, plus:

* Do not revert `_findnext`'s auto-erase-on-exhaustion fix
  (`src/crt_io.cpp`) without understanding the leak it fixes.
* Do not revert `StretchBlt`'s Y-axis clamp (`src/wingdi_blit.cpp`) to
  the old skip-the-row behavior.
* Do not re-declare `FreeApiCreateSurfaceDC`/`FreeApiDestroySurfaceDC`/
  `FreeApiSetWindowFullscreen` anywhere other than
  `include/free_api_bridge.h`.
* Do not "fix" `FreeApiDestroySurfaceDC`'s garbage-pointer segfault with a
  general handle-validation/table framework — documented as
  out-of-scope-to-fix per this session's finding; only revisit with new
  evidence a real caller needs it.
* Do not implement a real palette lookup for `CreateBitmap`'s 8-bit path
  — documented as out-of-scope per this session's investigation
  (`TASK-24H-0602`/`0603`).
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
Verdict (§1) for full context. plan.md has 175 new TASK-24H-* tasks; 36
are DONE (grep "Status: DONE" near "TASK-24H" to see which), 0 P0 and 6
P1 remain TODO. Pick up from section 8's "Next smallest tasks" list, or
grep plan.md for "Priority: P1"/"Priority: P2" together with "Status:
TODO" to find the next-highest-value remaining task. Make one small,
verified improvement at a time. Run the exact verification commands each
task specifies -- at minimum the standalone build's ctest (22/22),
ideally also both target games' ctest. Do not touch anything listed in
section 9. After finishing, update this file and plan.md's task statuses.
```
