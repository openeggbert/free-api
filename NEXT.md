# NEXT.md

Handoff document for resuming work on `free-api`, for either a future
Claude Code session or a human developer. Reflects the actual repository
state as of commit `911b580` (2026-07-06, `develop` branch, pushed to
`origin/develop`; working tree clean). See [`plan.md`](plan.md) for the full evidence-based
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
(`plan.md`, 127 tasks) is complete — 126 `DONE`, 1 `OBSOLETE` (superseded),
plus a follow-on `TASK-0128` closing out a second LoadStringA-hardening
pass. Work has moved from "implement the missing behavior" to "harden
what's already implemented" — the two most recent passes made
`LoadStringA`'s real-text backing fail loudly at CMake configure time
(rather than only being visible at runtime) both when the whole table is
broken/missing (`TASK-0127`) and when any *specific* ID a game actually
uses is missing from it (`TASK-0128`, per-used-ID manifests in
`cmake/used-string-ids/`). This session additionally confirmed (via a real
Xvfb/X11 session) that `test_winuser_regressions`'s 6 default-Wayland
failures are purely an environment artifact, not a hidden bug — see
section 4.

**Important architectural decisions:**

* Free API is a **static library**, normally built as a sibling
  `add_subdirectory()` of one of the two target games (which also provide
  SDL3). It can also build standalone via `-DFREE_API_USE_SYSTEM_SDL3=ON`
  — confirmed working end-to-end in this sandbox this session (17/17
  tests pass) now that `SDL3_image`/`SDL3_mixer` are available under
  `/usr/local` (see section 4).
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
  deliberately not a general `.rc`/`.res` compiler. Target-game builds
  (`SPEEDY_BLUPI_WINDOWS`/`PLANET_BLUPI_WINDOWS`, or an explicit
  `-DFREE_API_TARGET_GAME=free-eggbert`/`planetblupi` override — see
  `docs/cmake-options.md`) **fail CMake configure loudly** if that game's
  `.rc` is missing, yields zero extracted strings, a known sentinel string
  ID (`TX_BUTTON_QUITTER`, 106, `"Quit BLUPI"` in both games) doesn't
  resolve to its expected text, **or (as of `TASK-0128`) any ID either
  game's own source actually passes to `LoadString`** (per the
  evidence-based manifests in `cmake/used-string-ids/*.txt` (each ID listed
  exactly once — a duplicate entry is itself a configure-time
  `FATAL_ERROR`), cross-checked against a live extraction: 308 unique used
  string IDs of 364 extracted for free-eggbert, an exact 257 of 257 for
  planetblupi — see `docs/used-string-ids.md`) is missing from the
  extracted table. Standalone builds (including an explicit
  `-DFREE_API_TARGET_GAME=standalone`) are unaffected and keep the
  placeholder-fallback behavior.
* The two target games use **two different, mutually-exclusive live timer
  mechanisms** as their frame pump: Free Eggbert uses
  `timeSetEvent`/`timeKillEvent` (WinMM multimedia timer); Planet Blupi
  uses `SetTimer`/`KillTimer`/`WM_TIMER`. Both must keep working.

## 2. Current status

**Build status:**
* As a subdirectory of `../free-eggbert` (Ninja generator,
  `cmake-build-debug/`): configures and builds cleanly, including
  configure-time verification (`known-ID verification passed for
  'free-eggbert': ID 106 -> "Quit BLUPI"` and `all 308 unique used string
  ID(s) for 'free-eggbert' verified present`).
* As a subdirectory of `../planetblupi` (Unix Makefiles generator,
  `build/`): same, `known-ID verification passed for 'planetblupi'` and
  `all 257 unique used string ID(s) for 'planetblupi' verified present`.
* Standalone (`-DFREE_API_USE_SYSTEM_SDL3=ON`, no sibling game): **now
  configures, builds, and passes 17/17 tests cleanly in this sandbox**
  (confirmed this session — see section 4). Via the sibling-lookup
  convenience path it found `../free-eggbert`'s `.rc` and populated a real
  364-string table rather than an empty one, since free-eggbert happens to
  be checked out next to `free-api` here.

**Test status:** 17 test binaries registered per target-game build.
**17/17 pass in both free-eggbert and planetblupi builds** under
`SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy`, **and (confirmed this
session) also 17/17 under a real X11 session (Xvfb)** — see section 4: the
6 failures seen under this sandbox's *default* Wayland display are a
confirmed environment artifact, not a code bug or a `dummy`-driver-specific
pass. `test_loadstring_regressions` hard-asserts, in target-game mode,
that 10 known game string IDs per game (not just one) never fall back to
the `"RES_<id>"` placeholder.

**CLI/tools/apps/libraries:** Free API produces one artifact,
`libfree-api.a`, plus its test binaries. It has no standalone CLI/app of its
own — it's a library consumed by the two games' own executables
(`SPEEDY_BLUPI_WINDOWS`, `PLANET_BLUPI_WINDOWS`), both of which build and
link successfully against the current code (verified this session).
`examples/` also exists (five standalone WinAPI demos, gated behind
`-DFREE_API_BUILD_EXAMPLES=ON`, default `OFF`) from a prior session —
not re-verified this session, no changes made to it.

**Recently implemented features (this session):**
* `TASK-0128` (commit `8272622`): per-used-ID configure-time verification
  (`USED_IDS_FILE`) and the `FREE_API_TARGET_GAME` override, described in
  full in section 3.
* Confirmed via Xvfb/X11 that the Wayland-only test failures (section 4,
  previously only suspected as an environment issue) are purely an
  environment artifact — no code change, investigation only.
* Confirmed the standalone-build environment gap (missing
  `SDL3_image`/`SDL3_mixer`) has resolved itself — those packages are now
  present under `/usr/local` in this sandbox (not something this session
  installed; already there when checked) — no code change, verification
  only.

**What does NOT work / known gaps:**
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

* **(this session)** — "Clean up LoadStringA/STRINGTABLE hardening work":
  a documentation/manifest-only pass, no `LoadStringA` runtime behavior
  changed.
  * `cmake/used-string-ids/free-eggbert.txt`: deduplicated — IDs 194
    (`TX_CONTENT`) and 288 (`TX_GAMESAVED`) were each listed twice (once
    under their symbolic name, once under a numeric-literal call site
    sharing the same value); merged into one entry each citing both call
    sites. Unique-ID count unaffected (still 308 — the set was already
    308 even with the duplicate lines, since `USED_IDS_FILE`'s missing-ID
    check operated on a list, not a set, so duplicates only inflated the
    reported "checked" count, not correctness). planetblupi's manifest had
    no duplicates.
  * `cmake/ExtractStringTable.cmake`: `USED_IDS_FILE` now tracks each ID's
    first occurrence and fails configure (`FATAL_ERROR`, listing every
    duplicate) if any ID repeats — a repeated bare ID or one already
    covered by an earlier `A-B` range — checked before the missing-ID
    check. The `STATUS` message on success now says "N **unique** used
    string ID(s) verified present" to make clear the count is
    post-deduplication. No `STRINGTABLE`-parsing behavior changed.
  * `docs/used-string-ids.md`: added a "Duplicate-ID validation" section;
    normalized wording to "free-eggbert: 308 unique used string IDs" /
    "planetblupi: 257 unique used string IDs" throughout.
  * `plan.md`: updated the two top-level audit tables' `LoadStringA` rows
    (previously stale `STUB` ⚠ despite `TASK-0074/0075/0127/0128` being
    `DONE`) to "Minimally implemented — ... generated `STRINGTABLE`
    extraction"; updated §10's final risk summary to no longer claim
    `LoadStringA`/`AdjustWindowRect`/`ShowCursor`/`SetCursor` are
    "currently `STUB`" (all four were resolved by tasks already marked
    `DONE`/`OBSOLETE` — the summary just hadn't been updated to say so).
    `TASK-0074`/`TASK-0075`/`TASK-0127`/`TASK-0128` themselves left
    untouched, still `DONE`, historical status text describing what was
    literally true when each was completed (including `TASK-0128`'s "310
    used-ID checks," accurate before this pass's dedup).
  * Verified: reconfigured through free-eggbert, planetblupi, and a forced
    `-DFREE_API_TARGET_GAME=standalone` build; all used-ID checks pass
    with the corrected unique counts (308 / 257); a deliberately-duplicated
    test manifest reproduced the new `FATAL_ERROR` exactly as designed;
    full CTest 17/17 in both target-game builds.
* **(this session, no commit — playtest attempt)** Made a partial,
  Xvfb+xdotool-driven attempt at the manual playtests from section 5/8:
  launched both real game binaries under a real (non-`dummy`) X11 session,
  drove them with `xdotool`, and visually inspected screenshots myself
  (title screens, free-eggbert player-select + an actual tutorial mission,
  planetblupi's attract-mode demo across two scenes) — all rendered
  cleanly, no corruption or crash, good evidence for the
  `GetDeviceCaps(SIZEPALETTE)`/`BITMAPFILEHEADER` fixes. Also captured
  real gameplay audio via `SDL_AUDIODRIVER=disk` and confirmed it's
  non-silent, non-clipped, and structured (spectrogram), i.e. the MIDI
  path produces real output and doesn't crash — but musical correctness
  itself is unjudgeable without hearing. Could not reach planetblupi's
  `MK_SHIFT`/`MK_CONTROL` drag-highlight (any click during its demo mode
  returns to the title screen; didn't have time to map out its
  undocumented menu navigation to start a real session). No files
  changed. See section 5 for the honest per-item breakdown and section 8
  for the three narrower follow-ups this leaves.
* **(this session, no commit — verification only)** Confirmed the
  standalone free-api build now configures, builds, and passes 17/17
  tests: `cmake -S . -B <dir> -DFREE_API_USE_SYSTEM_SDL3=ON
  -DFREE_API_BUILD_TESTS=ON && cmake --build <dir> -j"$(nproc)"` completed
  cleanly, and `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest` in that
  build dir showed 17/17 passing (including `check_no_hardcoded_paths` and
  `test_loadstring_regressions`, the latter correctly in its ungated
  standalone mode — logged `INFO: standalone build found a sibling table
  -- id 106 resolved to "Quit BLUPI"`). Root cause of the prior gap
  (`SDL3_image`/`SDL3_mixer` dev packages missing) is no longer present in
  this sandbox — `pkg-config`/CMake config files and libraries for both
  now exist under `/usr/local`. Not something this session installed;
  already there when checked, so nothing to attribute to any specific fix.
  No files changed; this closes out the "next smallest task" that asked to
  investigate this gap.
* **(this session, no commit — investigation only)** Confirmed via a real
  Xvfb/X11 session that `test_winuser_regressions`'s 6 failures under this
  sandbox's default Wayland display (section 4) are purely an
  environment artifact: `Xvfb :99 -screen 0 1024x768x24` +
  `DISPLAY=:99 SDL_VIDEODRIVER=x11` reproduces **ALL TESTS PASSED**,
  including the exact `ClientToScreen`(×4)/`ScreenToClient`/`GetCursorPos`
  checks that fail under Wayland — in both the free-eggbert and
  planetblupi build trees, and for the full 17/17 CTest suite, not just
  the one binary. This rules out a real position/warp bug hiding behind
  the Wayland explanation. No files changed; `NEXT.md` §4/§5/§8/§9 updated
  to reflect the closed-out investigation.
* **`8272622`** (this session) — "Harden LoadStringA/STRINGTABLE:
  per-used-ID verification, explicit target-game override (`TASK-0128`)":
  * `cmake/used-string-ids/{free-eggbert,planetblupi}.txt` (new): hand-
    maintained, evidence-based manifests of every numeric `STRINGTABLE` ID
    each game's own source actually passes to `LoadString`, each entry
    cited to a `file:line` or to the concrete array/loop/enum a computed
    range derives from (see `docs/used-string-ids.md` for the full
    methodology). 308 IDs for free-eggbert (of 364 extracted — the rest
    are defined-but-unused strings), an exact 257/257 for planetblupi (it
    uses every string it defines).
  * `cmake/ExtractStringTable.cmake`: new optional `USED_IDS_FILE` — for
    target-game builds, every ID (or `A-B` range) listed in the manifest
    must be present in that run's extracted table, or configure fails
    loudly listing every missing ID. Catches a *specific* used ID going
    missing, which the pre-existing `REQUIRE_STRINGS`/`VERIFY_ID` (which
    only ever checks id 106) would not.
  * `CMakeLists.txt`: new `FREE_API_TARGET_GAME` cache variable
    (`auto`/`free-eggbert`/`planetblupi`/`standalone`), overriding the
    `CMAKE_PROJECT_NAME` auto-detection explicitly; when forced to a game
    we are not actually that game's own subdirectory of, falls back to the
    `../<game>` sibling path (same layout the standalone convenience path
    already used) instead. Default (`auto`) behavior is unchanged.
  * `tests/test_loadstring_regressions.cpp`: added a 10-case
    `kKnownIdSamples` table per game (direct symbols, computed offsets,
    button-tooltip-table IDs), each hard-asserted for exact text and
    never `RES_<id>` — not just the single id-106 sentinel as before.
  * `docs/used-string-ids.md` (new): full audit methodology and re-
    verification instructions. `docs/cmake-options.md`/`docs/out-of-
    scope.md`: documented `FREE_API_TARGET_GAME`/`USED_IDS_FILE`.
    `plan.md`: added `TASK-0128`, status `DONE`.
  * Verified: reconfigured+rebuilt through both games (`all 310 used
    string ID(s) ... verified present` / `all 257 ...`, string counts
    unchanged), full CTest 17/17 in both (`SDL_VIDEODRIVER=dummy`),
    deliberately-broken `USED_IDS_FILE` cases (missing ID, missing file,
    out-of-range ID via a bogus range) each independently reproduced the
    expected `FATAL_ERROR`, `FREE_API_TARGET_GAME` override verified in
    both directions (forcing `standalone` inside a game's own tree skips
    gating; forcing one game while inside the other's tree falls back to
    the sibling path and gates on the forced game).
* **`88484a3`** — "Harden LoadStringA/STRINGTABLE: fail-loud
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

**No blocker at all right now.** Both LoadStringA hardening passes are
complete and verified, the Wayland test-failure question is fully closed
out, and the standalone-build environment gap has also resolved itself —
see below. There is no open blocker item in this section as of this
session; the "next smallest tasks" (section 8) are non-blocking follow-ups.

**Resolved this session — running `ctest` without `SDL_VIDEODRIVER=dummy`
in this sandbox's default display produces 6 spurious test failures that
are CONFIRMED NOT a code bug (previously only suspected).**
* **Exact symptom:** `ctest --output-on-failure` (no env vars set) reports
  `test_winuser_regressions` failing with 6 `FAIL` lines: `ClientToScreen`
  (×4, origin/far-corner mapping before and after `MoveWindow`),
  `ScreenToClient` (the round-trip check), and `GetCursorPos reflects the
  position set by SetCursorPos`.
* **Root cause (confirmed by direct reproduction, twice over):** this
  sandbox's default display (`DISPLAY=:0`, `XDG_SESSION_TYPE=wayland`) is a
  real Wayland session. Wayland's protocol does not let clients set an
  absolute window position, and (evidently) does not support the exact
  global mouse warp `SetCursorPos` needs either — so
  `SDL_SetWindowPosition`/mouse-warp silently don't do what the test
  expects, and every position-exactness assertion fails.
  * A prior session confirmed `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy`
    (SDL's fully-software, headless driver) makes all 6 failures disappear.
  * **This session additionally confirmed a REAL, non-headless X11 session
    also passes cleanly** — not just the software `dummy` driver papering
    over the assertions. Started `Xvfb :99 -screen 0 1024x768x24`, ran
    `DISPLAY=:99 SDL_VIDEODRIVER=x11 ./bin/test_winuser_regressions` (and
    the full CTest suite) in both the free-eggbert and planetblupi build
    trees: **ALL TESTS PASSED / 17/17**, including the exact 6 checks that
    fail under this sandbox's default Wayland session. This rules out the
    remaining open question ("is `dummy` just skipping the real
    functionality rather than proving it works?") — a real X11 compositor
    genuinely honors `SDL_SetWindowPosition`/`SetCursorPos` the way the
    test expects; Wayland genuinely does not.
* **Affected files:** none need changing — this is a test-execution
  environment issue, not a defect in `src/winuser_window.cpp`,
  `src/winuser_misc.cpp`, or `src/internal/FreeApiSdlVideo.cpp`. No code
  changes were made or are warranted.
* **Action for future sessions:** always run this repo's tests with
  `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy` set (see section 7) unless
  specifically doing GUI/visual verification work — this is now a fully
  closed investigation, not an open question. There is no further action
  item here; do not re-investigate this absent a *new* symptom.

**Resolved this session: a fully standalone free-api build (no sibling
game, using `-DFREE_API_USE_SYSTEM_SDL3=ON`) now configures, builds, and
passes 17/17 tests in this sandbox.**
* **Prior symptom (no longer reproducible):** `-DFREE_API_USE_SYSTEM_SDL3=ON`
  used to fail at `find_package(SDL3_image REQUIRED)`/
  `find_package(SDL3_mixer REQUIRED)` because those packages' CMake config
  files weren't installed, even though `pkg-config sdl3` found `3.4.0`.
  Without `-DFREE_API_USE_SYSTEM_SDL3=ON` at all, configure still fails
  the same way it always has (see next bullet) — that part is unchanged
  and expected.
* **What changed:** `SDL3_image`/`SDL3_mixer` (headers, shared libs,
  `.pc` files, and CMake `Config.cmake`/`Targets.cmake` files) are now
  present under `/usr/local` in this sandbox, alongside the
  already-present SDL3 3.4.0. This was **not** installed by this or any
  recent `free-api` session — it was simply already there when checked
  this session (likely a side effect of other work in a sibling repo in
  this same machine, e.g. one of the `cna_*` projects' `.sdl-prebuilt`
  vendoring, or a manual install — not investigated further, out of scope
  for this repo).
* **Verified:** `cmake -S . -B <dir> -DFREE_API_USE_SYSTEM_SDL3=ON
  -DFREE_API_BUILD_TESTS=ON && cmake --build <dir> -j"$(nproc)"` completes
  with no errors; `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest` in
  that directory shows 17/17 passing, including `check_no_hardcoded_paths`
  and `test_loadstring_regressions` (correctly ungated, logging
  `INFO: standalone build found a sibling table` since `../free-eggbert`
  is present as a sibling here).
* **Still true, unchanged:** the *other* standalone path — no
  `-DFREE_API_USE_SYSTEM_SDL3=ON` at all, relying purely on a sibling
  game's `cmake/ThirdPartySDL.cmake` vendoring script — still requires
  `third_party/SDL` inside **free-api's own** directory (which doesn't
  exist, by design; see `CMakeLists.txt`'s top-of-file comment) and will
  still fail with `CMake Error ... Missing vendored dependency 'SDL'` if
  attempted. This was never the recommended standalone path (system SDL3
  always was); nothing about it changed or needs to.

## 5. Known bugs and limitations

* **Confirmed environment issue, not a code bug (section 4):** Wayland
  session breaks exact window-position/cursor-warp test assertions;
  resolved by `SDL_VIDEODRIVER=dummy`, and independently confirmed correct
  via a real X11/Xvfb session this session. Fully closed out — no further
  investigation needed absent a new symptom.
* **Partially verified this session via an Xvfb-driven playtest (screenshots
  I visually inspected myself, plus a captured-audio non-silence check) —
  still needs a real human pass for final sign-off, but no longer
  "untested":**
  * `GetDeviceCaps(SIZEPALETTE)`/`BITMAPFILEHEADER`/`BITMAPINFOHEADER`
    rendering — launched both real game binaries under
    `DISPLAY=:98 SDL_VIDEODRIVER=x11` (Xvfb), drove them with `xdotool`
    (free-eggbert: title screen -> player-select -> an actual tutorial
    mission, WM_PHASE_PLAY; planetblupi: title screen -> its attract-mode
    "Demo" showing two different real terrain/building scenes), and
    captured screenshots with `import`. All render cleanly across many
    distinct sprites/bitmaps/terrain types — no color corruption, no
    garbled/black rendering, no crash. This is real evidence the two
    rendering fixes are visually correct, though I can't rule out subtle
    palette-shade differences from the "correct" 1997 look without a
    reference image — a human who's played the original should still
    glance at it.
  * MIDI music playback — confirmed it does NOT crash during real gameplay
    (matching the earlier session's dangling-pointer/`MixerThread` SIGSEGV
    fix) and does NOT silently produce dead/all-zero audio: captured
    output via `SDL_AUDIODRIVER=disk` while in actual gameplay showed real,
    non-clipped signal (max amplitude ~31% of full scale) and a spectrogram
    showed structured periodic content, not random noise. **I cannot judge
    whether it sounds musically correct** (I have no audio input/hearing) —
    the bursty, broadband-not-tonal shape of what I captured could be sound
    effects rather than continuous background music, which may be entirely
    expected for that moment, or could be a real problem — genuinely
    inconclusive without human ears. Still needs an audible playtest.
  * `WM_MOUSEMOVE`'s `MK_SHIFT`/`MK_CONTROL` fix (planetblupi's
    `BlupiHiliDown`/`Move`/`Up` multi-select drag-highlight,
    `event.cpp:3440-3504`) — **not reached**: any mouse click/drag during
    planetblupi's attract-mode "Demo" immediately interrupts it back to the
    title screen (by design), and I don't know the menu navigation to
    start a real playable session (French-labeled icons, no docs found)
    to get into build mode and test a real shift/ctrl drag. Still fully
    untested; still needs a human (or a session with more time to map out
    the menu flow).
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
* **Resolved this session (section 4):** the standalone free-api build gap
  (missing `SDL3_image`/`SDL3_mixer` dev packages) is gone — those
  packages are now present in this sandbox and
  `-DFREE_API_USE_SYSTEM_SDL3=ON` builds+tests cleanly (17/17). The
  no-`-DFREE_API_USE_SYSTEM_SDL3` sibling-vendoring path still requires a
  `third_party/SDL` free-api doesn't have, by design — unchanged, not a
  bug.
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
  + the fail-loud/known-ID-verification gate (`REQUIRE_STRINGS`/
  `VERIFY_ID`/`VERIFY_TEXT`, `TASK-0127`) + the per-used-ID verification
  gate (`USED_IDS_FILE`, `TASK-0128`) for target-game builds.
* `cmake/used-string-ids/{free-eggbert,planetblupi}.txt` — evidence-based
  manifests of every ID each game's own source actually passes to
  `LoadString` (see `docs/used-string-ids.md`); consumed by
  `USED_IDS_FILE` above.

**Data flow:** SDL3 events -> `FreeApiMessageQueue::PumpSdlEvents` ->
internal message queue -> `PeekMessageA`/`GetMessageA` -> `DispatchMessageA`
-> the game's registered `WndProc` (looked up via `FreeApiWindowRegistry`).
**`WM_CREATE` and `WM_DESTROY` are delivered synchronously** (direct
`WndProc` call from `CreateWindowExA`/`DestroyWindow`), never via the queue.

`LoadStringA` (`src/winuser_misc.cpp`) calls
`FreeApi::Internal::FindGeneratedString(uID)`; if it returns non-null, that
exact text is copied out; otherwise `"RES_%u"` is formatted instead. What's
changed across the two hardening passes (`TASK-0127`/`TASK-0128`) is how
confidently a target-game build can trust that lookup will hit — the
lookup function itself is unchanged.

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
  `CMAKE_PROJECT_NAME` at configure time (or the `FREE_API_TARGET_GAME`
  override), and is fail-loud-verified for both target games at two
  levels — do not weaken `REQUIRE_STRINGS`/`VERIFY_ID` (`TASK-0127`) or
  `USED_IDS_FILE` (`TASK-0128`) without understanding why they were added.
* `cmake/used-string-ids/*.txt` manifests must stay evidence-based (every
  entry cited to a `file:line` or a concrete computed-range derivation,
  per `docs/used-string-ids.md`) — do not add an ID "to be safe" without
  a real `LoadString` call site behind it, and do not remove/shrink an
  entry to silence a `USED_IDS_FILE` failure without first confirming the
  game genuinely no longer uses that ID.
* `cmake/ExtractStringTable.cmake` must stay a narrow `STRINGTABLE`-only
  parser — confirmed that neither game's `.rc`/header files actually use
  hex values, inline comments, or expressions in a way the parser doesn't
  already handle; do not build this into a general `.rc` compiler absent
  new evidence of an actual unsupported real-world case.
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
# Build via a real target game (still the primary way most work gets
# verified against real .rc/behavior, though a standalone build now also
# works in this sandbox -- see the next block and §4)
cd ../free-eggbert/cmake-build-debug && cmake . && ninja -j"$(nproc)"
cd ../planetblupi/build && cmake . && make -j"$(nproc)"

# Standalone build (no sibling game needed) -- confirmed working this
# session now that SDL3_image/SDL3_mixer are available in this sandbox
cmake -S . -B build -DFREE_API_USE_SYSTEM_SDL3=ON -DFREE_API_BUILD_TESTS=ON
cmake --build build -j"$(nproc)"
cd build && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure

# Run the full test suite -- SDL_VIDEODRIVER=dummy is NOT optional in this
# sandbox's default (Wayland) display; without it, 6 window-position tests
# spuriously fail (see §4)
cd ../free-eggbert/cmake-build-debug/FREE_API && \
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure
cd ../planetblupi/build/FREE_API && \
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure

# Run one specific test binary directly (from the game's own build dir,
# binaries land in bin/ at that build dir's top level, not under FREE_API/)
cd ../free-eggbert/cmake-build-debug && SDL_VIDEODRIVER=dummy ./bin/test_loadstring_regressions

# Reproduce the current CMake configure-time known-ID + used-ID
# verification directly (useful for testing REQUIRE_STRINGS/VERIFY_ID/
# USED_IDS_FILE changes in isolation, without a full project reconfigure)
cmake -DOUTPUT_CPP=/tmp/out.cpp \
  -DRC_FILE=../planetblupi/resource/blupi-e.rc -DRC_ENCODING=UTF-8 \
  "-DRESOURCE_HEADERS=../planetblupi/include/resource.h;../planetblupi/include/resrc1.h" \
  -DREQUIRE_STRINGS=ON -DTARGET_GAME_NAME=planetblupi \
  -DVERIFY_ID=106 "-DVERIFY_TEXT=Quit BLUPI" \
  -DUSED_IDS_FILE=cmake/used-string-ids/planetblupi.txt \
  -P cmake/ExtractStringTable.cmake

# Force a specific target game's gating without needing that game's full
# build tree (falls back to the ../<game> sibling path) -- see
# docs/cmake-options.md
cmake -B build -DFREE_API_TARGET_GAME=free-eggbert
```

No lint/formatter is configured in this repository.

## 8. Next smallest tasks

1. **Human sign-off on the two rendering fixes** (`GetDeviceCaps(SIZEPALETTE)`,
   `BITMAPFILEHEADER`/`BITMAPINFOHEADER` packing) — this session's
   Xvfb+xdotool screenshots (section 5) already show clean rendering across
   several real scenes in both games; a human who's played the original
   should give these a quick glance to catch anything subtler than gross
   corruption.
   * Goal: final confirmation that palette-driven rendering looks right,
     not just "doesn't crash/isn't corrupted."
   * Files: none — observation only, unless a defect is found.
   * Verification: run each game normally (not headless) and eyeball it;
     or review the screenshots this session captured (not committed
     anywhere — regenerate via `Xvfb :98 -screen 0 1024x768x24 &
     DISPLAY=:98 SDL_VIDEODRIVER=x11 <binary>` + `DISPLAY=:98 import
     -window root <file>.png` if needed).

2. **Human audible playtest of MIDI music** — this session confirmed via a
   captured-audio non-silence check (section 5) that real, non-clipped,
   structured (not obviously random) audio is produced during gameplay
   with no crash, but could not judge musical correctness (no hearing).
   * Goal: confirm the music/sound actually sounds right, not garbled/
     wrong-instrument/wrong-tempo.
   * Files: none — observation only, unless a defect is found.
   * Verification: run either game with real audio output and listen.

3. **Human playtest of `MK_SHIFT`/`MK_CONTROL` drag-highlight in
   planetblupi** (`event.cpp:3440-3504`, `BlupiHiliDown`/`Move`/`Up`) —
   this session could not reach it: planetblupi's attract-mode "Demo"
   exits back to the title screen on any click/drag, and the main menu's
   icon navigation (French-labeled, undocumented) wasn't mapped out in the
   time available. A future session with more time could try
   systematically clicking/hovering every main-menu icon (screenshot after
   each) to find the "start a real game" path, then test a real shift/ctrl
   drag in build mode — or just hand this to a human directly.
   * Goal: confirm the multi-select drag-highlight renders correctly.
   * Files: none expected — observation only, unless a defect is found.
   * Verification: get into a real (non-demo) planetblupi session, drag-
     select multiple Blupis with Shift held, and check the highlight.

_(Both earlier items — "confirm X11 vs. Wayland" and "get a standalone build
working" — are done; see sections 3/4/5.)_

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
  first (section 4). This is now a **closed investigation** (confirmed via
  a real X11/Xvfb session, not just the `dummy` driver) — do not re-open
  it absent a genuinely new symptom.
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
