# NEXT.md

Handoff document for resuming work on `free-api`, for either a future
Claude Code session or a human developer. Reflects the actual repository
state as of commit `88484a3` (2026-07-05, `develop` branch, pushed to
`origin/develop`). See [`plan.md`](plan.md) for the full evidence-based
usage audit and 127-item task backlog (every task carries a `Status:` line
reconciled against actual repository state), and
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

**Current development phase:** the original evidence-based audit backlog
(`plan.md`, 127 tasks) is complete — 126 `DONE`, 1 `OBSOLETE` (superseded).
Work has moved from "implement the missing behavior" to "harden what's
already implemented" — the most recent pass (this session) hardened
`LoadStringA`'s real-text backing so a broken/missing resource silently
degrading to placeholder text in a *shipped* build is now caught at CMake
configure time instead of only being visible at runtime.

**Important architectural decisions:**

* Free API is a **static library**, normally built as a sibling
  `add_subdirectory()` of one of the two target games (which also provide
  SDL3). It can also build standalone via `-DFREE_API_USE_SYSTEM_SDL3=ON`,
  though see section 4 for a currently-unresolved gap in that path in this
  particular sandbox.
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
  deliberately not a general `.rc`/`.res` compiler. **As of this session,
  target-game builds (`SPEEDY_BLUPI_WINDOWS`/`PLANET_BLUPI_WINDOWS`) fail
  CMake configure loudly** if that game's `.rc` is missing, yields zero
  extracted strings, or a known string ID (`TX_BUTTON_QUITTER`, 106,
  `"Quit BLUPI"` in both games) doesn't resolve to its expected text.
  Standalone builds are unaffected and keep the placeholder-fallback
  behavior.
* The two target games use **two different, mutually-exclusive live timer
  mechanisms** as their frame pump: Free Eggbert uses
  `timeSetEvent`/`timeKillEvent` (WinMM multimedia timer); Planet Blupi
  uses `SetTimer`/`KillTimer`/`WM_TIMER`. Both must keep working.

## 2. Current status

**Build status:**
* As a subdirectory of `../free-eggbert` (Ninja generator,
  `cmake-build-debug/`): configures and builds cleanly, including the new
  configure-time verification (`known-ID verification passed for
  'free-eggbert': ID 106 -> "Quit BLUPI"`).
* As a subdirectory of `../planetblupi` (Unix Makefiles generator,
  `build/`): same, `known-ID verification passed for 'planetblupi'`.
* Standalone (`free-api`'s own `cmake-build-debug/`/`build/`): **does NOT
  currently configure to completion in this sandbox** — see section 4.

**Test status:** 17 test binaries registered per target-game build.
**17/17 pass in both free-eggbert and planetblupi builds**, but *only* when
run with `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy` (see section 4 for
why this matters — it is not optional in this sandbox's default display).
`test_loadstring_regressions` now hard-asserts, in target-game mode, that a
known game string ID never falls back to the `"RES_<id>"` placeholder.

**CLI/tools/apps/libraries:** Free API produces one artifact,
`libfree-api.a`, plus its test binaries. It has no standalone CLI/app of its
own — it's a library consumed by the two games' own executables
(`SPEEDY_BLUPI_WINDOWS`, `PLANET_BLUPI_WINDOWS`), both of which build and
link successfully against the current code (verified this session).
`examples/` also exists (five standalone WinAPI demos, gated behind
`-DFREE_API_BUILD_EXAMPLES=ON`, default `OFF`) from a prior session —
not re-verified this session, no changes made to it.

**Recently implemented features (this session, committed `88484a3`):**
fail-loud CMake configure-time checks and known-ID verification for
`LoadStringA`'s STRINGTABLE extraction, described in full in section 3.

**What does NOT work / known gaps:**
* A genuinely standalone free-api build (no sibling game, no full system
  SDL3 stack) cannot complete configure in this sandbox — see section 4.
* MCI digital-video (AVI movie codec playback) is **not implemented** —
  confirmed intentional; both games already gracefully skip movies when
  this is declined (`docs/out-of-scope.md`).
* `LoadStringA`'s real-text table contains **only one game's strings per
  build** (whichever game's `.rc` was extracted for that specific build) —
  by design, not a bug.
* MIDI music playback, the `GetDeviceCaps(SIZEPALETTE)` fix, and the
  `BITMAPFILEHEADER` packing fix (all from an earlier session) still have
  not had a human-eyes/ears playtest — flagged as needing manual
  verification since a prior session, status unchanged.

## 3. Recent changes

Most recent first:

* **`88484a3`** (this session) — "Harden LoadStringA/STRINGTABLE: fail-loud
  configure + known-ID verification":
  * `cmake/ExtractStringTable.cmake`: new optional `REQUIRE_STRINGS`
    (missing `.rc` or zero extracted strings becomes `FATAL_ERROR` instead
    of `WARNING`), `TARGET_GAME_NAME` (error-message context),
    `VERIFY_ID`/`VERIFY_TEXT` (a known symbol must resolve to exact
    expected text, `FATAL_ERROR` on mismatch or not-found).
  * `CMakeLists.txt`: passes `REQUIRE_STRINGS=ON` and `VERIFY_ID=106`/
    `VERIFY_TEXT="Quit BLUPI"` (`TX_BUTTON_QUITTER`, present with identical
    text in both `Eggbert2.rc` and `blupi-e.rc`) only when
    `CMAKE_PROJECT_NAME` is `SPEEDY_BLUPI_WINDOWS` or
    `PLANET_BLUPI_WINDOWS`. Also fixed `check_no_hardcoded_paths`'s
    `add_test` command, which used `CMAKE_SOURCE_DIR` (resolves to the
    *calling game's* root when free-api is nested) instead of
    `CMAKE_CURRENT_SOURCE_DIR` (always free-api's own root) — this made the
    test fail whenever free-api was built as a subdirectory of either game.
  * `tests/test_loadstring_regressions.cpp`: rewritten to branch on new
    compile-time macros (`FREE_API_TARGET_GAME_FREE_EGGBERT`/
    `FREE_API_TARGET_GAME_PLANETBLUPI`, set by `CMakeLists.txt` from the
    same `CMAKE_PROJECT_NAME` detection) — target-game mode hard-asserts
    real text and never `RES_<id>` for a known ID; standalone mode only
    logs either outcome. Added NULL-buffer and `cchBufferMax == 0` cases
    (both already correctly returned 0 in the existing `LoadStringA`
    implementation — no production code change was needed there).
  * `docs/out-of-scope.md` and `include/winuser.h`: documented that
    `"RES_<id>"` is a debug/developer-only marker, never acceptable in
    shipped game UI, and that a successful target-game configure is now a
    real signal the table is populated and correct.
  * `plan.md`: added `TASK-0127` recording this work, status `DONE`.
  * Verified: reconfigured through both games (string counts unchanged —
    364 free-eggbert / 257 planetblupi), full CTest suite 17/17 in both
    (with `SDL_VIDEODRIVER=dummy`), deliberately-broken-input cases
    (missing `.rc`, zero strings, wrong `VERIFY_TEXT`, unknown `VERIFY_ID`)
    each independently reproduced as the expected `FATAL_ERROR` via direct
    `cmake -P` script invocations before being wired into `CMakeLists.txt`.
* `c8d44cb` — Implemented real joystick support via SDL (`TASK-0103`);
  closed out `plan.md`'s backlog to 125 DONE / 1 OBSOLETE at the time.
* `8193c6a` — Added `examples/` (five standalone WinAPI demos); confirmed
  the joystick stub is safe via a real Xvfb-driven playtest (`TASK-0102`).
* `e89d65b` — Fixed a MIDI-playback SIGSEGV (a dangling-pointer race in
  `MixerThread`), a `GetDeviceCaps(SIZEPALETTE)` contract bug, and a
  `BITMAPFILEHEADER`/`BITMAPINFOHEADER` struct-packing bug; closed out
  `plan.md`'s P0–P2 backlog. (Still needs a manual playtest — section 2.)

## 4. Current blocker / main problem

**No blocker on the actual feature work** (the LoadStringA hardening pass
is complete and verified). Two known, separate issues are worth flagging
for whoever picks this up next:

**1. Running `ctest` without `SDL_VIDEODRIVER=dummy` in this sandbox
produces 6 spurious test failures that are NOT a code bug.**
* **Exact symptom:** `ctest --output-on-failure` (no env vars set) reports
  `test_winuser_regressions` failing with 6 `FAIL` lines: `ClientToScreen`
  (×4, origin/far-corner mapping before and after `MoveWindow`),
  `ScreenToClient` (the round-trip check), and `GetCursorPos reflects the
  position set by SetCursorPos`.
* **Root cause (confirmed by direct reproduction):** this sandbox's
  default display (`DISPLAY=:0`, `XDG_SESSION_TYPE=wayland`) is a real
  Wayland session. Wayland's protocol does not let clients set an absolute
  window position, and (evidently) does not support the exact global mouse
  warp `SetCursorPos` needs either — so `SDL_SetWindowPosition`/mouse-warp
  silently don't do what the test expects, and every position-exactness
  assertion fails. Running the identical binary with
  `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy` (SDL's fully-software,
  headless driver) makes **all 6 failures disappear** — confirmed via
  direct A/B reproduction this session in both the free-eggbert and
  planetblupi build trees.
* **Affected files:** none need changing — this is a test-execution
  environment issue, not a defect in `src/winuser_window.cpp`,
  `src/winuser_misc.cpp`, or `src/internal/FreeApiSdlVideo.cpp`.
* **What's already been tried:** confirmed the dummy driver fully resolves
  it (0 failures, both games, full 17/17 suites). Did not investigate
  whether a real X11 (non-Wayland, non-XWayland) session would also pass —
  untried.
* **Action for next session:** always run this repo's tests with
  `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy` set (see section 7) unless
  specifically doing GUI/visual verification work. If a future session
  ever needs the *real* SetCursorPos/window-position round-trip verified
  end-to-end, that needs a proper X11 (not Wayland) session or Xvfb, not
  this sandbox's default display.

**2. A fully standalone free-api build (no sibling game, no full system
SDL3 stack) does not configure to completion in this sandbox.**
* **Exact symptom:** `cmake -S . -B cmake-build-debug` (in `free-api`
  itself, no `-DFREE_API_USE_SYSTEM_SDL3=ON`) fails with: `CMake Error at
  /rv/.../free-eggbert/cmake/ThirdPartySDL.cmake:16 (message): Missing
  vendored dependency 'SDL' in /rv/.../free-api/third_party. Run: git
  submodule update --init --recursive`.
* **Suspected cause:** free-api has no `.gitmodules`/vendored SDL3 of its
  own by design (per `CMakeLists.txt`'s own top-of-file comment); its
  standalone developer-convenience path reuses whichever sibling game's
  `cmake/ThirdPartySDL.cmake` it finds, but that script expects a
  `third_party/SDL` checkout inside **free-api's own** directory, which
  doesn't exist here. The alternative, `-DFREE_API_USE_SYSTEM_SDL3=ON`,
  also doesn't fully work in this sandbox: `pkg-config sdl3` finds
  `3.4.0`, but `SDL3_image`/`SDL3_mixer` dev packages are absent.
* **This is pre-existing and unrelated to this session's changes** — the
  standalone-mode CMake logic itself (which `.rc`, if any, gets used, and
  that `REQUIRE_STRINGS`/`VERIFY_ID` are correctly never set) was verified
  directly via isolated `cmake -P cmake/ExtractStringTable.cmake`
  invocations instead of a full build.
* **Not attempted:** installing `SDL3_image`/`SDL3_mixer` system packages,
  or vendoring `third_party/SDL` inside `free-api` itself. Either would let
  a real standalone build+test run complete; neither was done this session
  since it's outside this pass's LoadStringA-hardening scope.

## 5. Known bugs and limitations

* **Suspected environment issue, not a confirmed code bug:** section 4,
  item 1 (Wayland session breaks exact window-position/cursor-warp test
  assertions; resolved by `SDL_VIDEODRIVER=dummy`).
* **Incomplete / needs verification (carried over from a prior session,
  unchanged this session):**
  * MIDI music playback — real behavior change, needs an audible playtest;
    cannot be judged in this headless sandbox.
  * `GetDeviceCaps(SIZEPALETTE)` fix (now returns 0 instead of 256) —
    changes which rendering path both games take; needs a visual playtest.
  * `BITMAPFILEHEADER`/`BITMAPINFOHEADER` packing fix — needs a visual
    playtest of palette-driven rendering.
  * `WM_MOUSEMOVE`'s `MK_SHIFT`/`MK_CONTROL` fix — confirmed untestable via
    `SDL_PushEvent` injection in this headless environment; needs a real
    manual playtest.
* **By design, not a bug:**
  * `LoadStringA`'s generated table holds only one game's strings per
    compiled build.
  * MCI digital-video/AVI is permanently declined.
  * `"RES_<id>"` is a debug-only placeholder; target-game builds now fail
    CMake configure if it would ever appear for a known ID (section 1).
* **Needs verification / gotcha:** `cmake/ExtractStringTable.cmake` runs at
  CMake **configure** time via `execute_process`, not as a build-time
  custom command. Editing a game's `.rc`/`resource.h` and re-running
  `cmake --build` alone will **not** pick up the change — a full
  `cmake -B <dir>`/`cmake <build-dir>` reconfigure is required.
* **Unresolved this session (section 4, item 2):** standalone free-api
  build does not complete in this sandbox.
* **Unknown:** whether real joystick support is ever actually wanted for
  Free Eggbert — `joyGetPosEx`/`joyGetNumDevs` are real (`TASK-0103`), but
  free-eggbert's own `m_somethingJoystick` flag is never assigned anything
  but its 0-initialization, so it never actually polls the joystick
  regardless (editing that is outside this repo's scope).

## 6. Architecture notes

**Main modules:**
* `src/winuser_*.cpp` — window lifecycle, message queue, input, cursor,
  timers (`SetTimer`/`KillTimer`).
* `src/wingdi_*.cpp` — GDI bitmap/DC/blit subset (`StretchBlt`, `GetPixel`/
  `SetPixel`, `CreateBitmap`, `CreateCompatibleDC`).
* `src/winmm.cpp` + `src/MidiMusic.cpp` — WinMM/MCI/MIDI (TinySoundFont +
  TinyMidiLoader backend); real joystick backend (`joyGetPosEx`) also
  lives in `src/winmm.cpp`.
* `src/winbase*.cpp`, `src/crt_*.cpp` — file/path/CRT helpers.
* `src/internal/*` — shared internal state and helpers: `FreeApiMessageQueue`
  (the message queue itself + SDL event translation), `FreeApiWindowRegistry`
  (`HWND` -> `WNDPROC`/state maps), `FreeApiGdi` (`CompatDC`/`CompatBitmap`),
  `FreeApiPath` (path normalization), `FreeApiDiagnostics`, `FreeApiStringTable`
  (generated-`LoadStringA`-table lookup — `FindGeneratedString`, a linear
  scan over `g_generatedStringTable`), `FreeApiTimers`, `FreeApiSdlVideo`.
* `src/winmain_bridge.cpp` — `WinMain` -> `main` entry-point bridge.
* `cmake/ExtractStringTable.cmake` — configure-time `STRINGTABLE` extractor
  + (as of this session) the fail-loud/known-ID-verification gate for
  target-game builds.

**Data flow:** SDL3 events -> `FreeApiMessageQueue::PumpSdlEvents` ->
internal message queue -> `PeekMessageA`/`GetMessageA` -> `DispatchMessageA`
-> the game's registered `WndProc` (looked up via `FreeApiWindowRegistry`).
**`WM_CREATE` and `WM_DESTROY` are delivered synchronously** (direct
`WndProc` call from `CreateWindowExA`/`DestroyWindow`), never via the queue.

`LoadStringA` (`src/winuser_misc.cpp`) calls
`FreeApi::Internal::FindGeneratedString(uID)`; if it returns non-null, that
exact text is copied out; otherwise `"RES_%u"` is formatted instead. The
*only* thing that changed this session is how confidently a target-game
build can trust that lookup will hit — the lookup function itself is
unchanged.

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
* Free API's "window size" **is** the client-area size — `AdjustWindowRect`
  is deliberately a no-op identity transform because of this; do not "fix"
  it without changing `CreateWindowExA`/`GetClientRect` in lockstep.
* `LoadStringA`'s real-text backing is per-build, per-game, determined by
  `CMAKE_PROJECT_NAME` at configure time, and (as of this session) is
  fail-loud-verified for both target games — do not weaken
  `REQUIRE_STRINGS`/`VERIFY_ID` without understanding why they were added
  (`plan.md` `TASK-0127`).
* `cmake/ExtractStringTable.cmake` must stay a narrow `STRINGTABLE`-only
  parser — confirmed (this session) that neither game's `.rc`/header files
  actually use hex values, inline comments, or expressions in a way the
  parser doesn't already handle; do not build this into a general `.rc`
  compiler absent new evidence of an actual unsupported real-world case.
* `free-direct` (sibling project) depends on the non-`WINAPI`, non-static C
  entry points `FreeApiCreateSurfaceDC`/`FreeApiDestroySurfaceDC`
  (`src/wingdi_dc.cpp`). Their exact signatures must not change without
  updating `free-direct` too.
* DirectDraw/DirectSound/DirectPlay are explicitly **out of scope** for
  Free API — that's `free-direct`'s responsibility.
* ANSI-only (`A`-suffixed) functions are implemented; no real Unicode/`W`
  behavior (neither game builds with `UNICODE` defined).

## 7. Useful commands

```bash
# Build via a real target game (recommended way to verify anything —
# free-api standalone does not currently configure in this sandbox, see §4)
cd ../free-eggbert/cmake-build-debug && cmake . && ninja -j"$(nproc)"
cd ../planetblupi/build && cmake . && make -j"$(nproc)"

# Run the full test suite -- SDL_VIDEODRIVER=dummy is NOT optional in this
# sandbox's default (Wayland) display; without it, 6 window-position tests
# spuriously fail (see §4, item 1)
cd ../free-eggbert/cmake-build-debug/FREE_API && \
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure
cd ../planetblupi/build/FREE_API && \
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure

# Run one specific test binary directly (from the game's own build dir,
# binaries land in bin/ at that build dir's top level, not under FREE_API/)
cd ../free-eggbert/cmake-build-debug && SDL_VIDEODRIVER=dummy ./bin/test_loadstring_regressions

# Reproduce the current CMake configure-time known-ID verification directly
# (useful for testing REQUIRE_STRINGS/VERIFY_ID changes in isolation,
# without a full project reconfigure)
cmake -DOUTPUT_CPP=/tmp/out.cpp \
  -DRC_FILE=../planetblupi/resource/blupi-e.rc -DRC_ENCODING=UTF-8 \
  "-DRESOURCE_HEADERS=../planetblupi/include/resource.h;../planetblupi/include/resrc1.h" \
  -DREQUIRE_STRINGS=ON -DTARGET_GAME_NAME=planetblupi \
  -DVERIFY_ID=106 "-DVERIFY_TEXT=Quit BLUPI" \
  -P cmake/ExtractStringTable.cmake
```

No lint/formatter is configured in this repository.

## 8. Next smallest tasks

1. **Confirm whether a real (non-Wayland) X11 session also passes
   `test_winuser_regressions` cleanly, or genuinely needs `dummy`.**
   Goal: rule out a real position/warp bug hiding behind the Wayland
   explanation. Files: `tests/test_winuser_regressions.cpp`,
   `src/internal/FreeApiSdlVideo.cpp`. Verification: run the same binary
   under Xvfb (`Xvfb :99 -screen 0 1024x768x24 & DISPLAY=:99
   SDL_VIDEODRIVER=x11 ./bin/test_winuser_regressions`) and compare.

2. **Get a genuinely standalone free-api build+test working in this
   sandbox** (currently blocked, section 4 item 2). Smallest viable step:
   try installing `libsdl3-image-dev`/`libsdl3-mixer-dev` (or equivalent)
   alongside the already-present system `sdl3` 3.4.0, then
   `cmake -B build -DFREE_API_USE_SYSTEM_SDL3=ON -DFREE_API_BUILD_TESTS=ON`.
   Files: none expected to change — this is an environment/packaging gap,
   not a code fix. Verification: `cmake -B build
   -DFREE_API_USE_SYSTEM_SDL3=ON && cmake --build build -j"$(nproc)"`
   completes and `SDL_VIDEODRIVER=dummy ctest --output-on-failure` in
   `build/` shows all tests passing with an empty (0-entry) generated
   string table.

3. **Manual playtests still outstanding from a prior session** (MIDI
   audio, `GetDeviceCaps(SIZEPALETTE)` visual rendering, BMP palette
   rendering, `MK_SHIFT`/`MK_CONTROL` drag-highlight) — see section 5.
   These need a human (or an Xvfb+xdotool-driven session with a human
   reviewing screenshots/audio) rather than further code changes.

## 9. Do not do yet

* Do not weaken or remove `REQUIRE_STRINGS`/`VERIFY_ID` in
  `cmake/ExtractStringTable.cmake`/`CMakeLists.txt` to "fix" a build
  failure — a `FATAL_ERROR` there means the `.rc`/known-ID data is
  genuinely broken for that target game; fix the underlying data/parsing,
  don't silence the check (`plan.md` `TASK-0127`).
* Do not build a general `.rc`/`.res` compiler — `ExtractStringTable.cmake`
  must stay narrowly `STRINGTABLE`-only, confirmed sufficient for both
  games' actual files this session.
* Do not touch joystick, MCI digital-video, DirectDraw, DirectSound,
  DirectPlay, or `free-direct` without a specific, separately-scoped
  request — the most recent two sessions were deliberately scoped away
  from these areas.
* Do not assume `test_winuser_regressions` failures under the default
  display mean a real regression — always try `SDL_VIDEODRIVER=dummy`
  first (section 4, item 1) before investigating further.
* Do not revert the `GetDeviceCaps(SIZEPALETTE)` value (now 0) or the
  `BITMAPFILEHEADER`/`BITMAPINFOHEADER` `#pragma pack(push, 2)` without
  re-reading `plan.md` TASK-0060/TASK-0084 first.
* No real Unicode/`W` API implementations.
* No consolidating the two timer mechanisms (`timeSetEvent` vs. `SetTimer`)
  into one "unified" implementation.
* No "fixing" `AdjustWindowRect` to add real Win32 window-chrome math.
* No new public API without a cited `file:line` usage site in
  `../free-eggbert` or `../planetblupi`.
* No broad refactor of any currently-passing subsystem absent a specific,
  evidenced bug report.

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
