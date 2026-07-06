# NEXT.md

Handoff document for resuming work on `free-api`, for either a future
Claude Code session or a human developer. Reflects the actual repository
state as of commit `30e0c54` (2026-07-06, `develop` branch, pushed to
`origin/develop`; working tree clean). See [`plan.md`](plan.md) for the
full evidence-based scope audit and task backlog (fully rewritten this
session — see section 3) and [`docs/scope.md`](docs/scope.md) for the
scope policy it enforces.

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
functions, see section 6).

**Current development phase:** the original evidence-based audit backlog
(127 tasks + `TASK-0125`–`0128`) reached full completion — everything
`DONE`/`OBSOLETE`. This session **fully re-derived that audit from
scratch** (not trusting the old one) to confirm free-api is still a tight,
evidenced Win32 subset: five parallel research passes (full header
inventory; independent usage cross-check against each game's own source;
verification of all 143 inline `@note Status:` header annotations against
real `src/` behavior; re-verification of the two long-lived `todo/*.md`
backlogs). Conclusion: the codebase holds up well — most of the
performance/correctness backlog was already resolved without being marked
as such, zero header annotations were found stale, and no real scope
violations were found. `plan.md` now holds a fresh, much smaller
`TASK-0001`–`TASK-0012` backlog, none of it implemented yet.

**Important architectural decisions:**

* Free API is a **static library**, normally built as a sibling
  `add_subdirectory()` of one of the two target games (which also provide
  SDL3). It also builds standalone via `-DFREE_API_USE_SYSTEM_SDL3=ON`
  (confirmed working, 17/17 tests, once `SDL3_image`/`SDL3_mixer` are
  installed).
* SDL3 is an **internal backend detail only** — public headers in
  `include/` must never expose SDL types.
* Both target games are always siblings of each other and of `free-api`
  on a normal dev machine — **sibling-directory existence alone cannot
  tell you which game is driving a given build.** `CMakeLists.txt` keys
  off `CMAKE_PROJECT_NAME` (the actual top-level project) to detect this,
  or an explicit `-DFREE_API_TARGET_GAME=free-eggbert`/`planetblupi`/
  `standalone` override (`docs/cmake-options.md`).
* Real UI text for `LoadStringA` is extracted at **CMake configure time**
  from whichever game's own `resource/*.rc` is driving the build
  (`cmake/ExtractStringTable.cmake`, a narrow `STRINGTABLE`-only parser —
  deliberately not a general `.rc`/`.res` compiler). Target-game builds
  fail CMake configure loudly if the `.rc` is missing/empty, a known
  sentinel string doesn't resolve, or (per evidence-based manifests in
  `cmake/used-string-ids/*.txt`) any ID the game actually uses is missing
  from the extracted table. Standalone builds keep the placeholder-
  fallback behavior instead.
* The two target games use **two different, mutually-exclusive live timer
  mechanisms** as their frame pump: Free Eggbert uses
  `timeSetEvent`/`timeKillEvent`; Planet Blupi uses `SetTimer`/`KillTimer`/
  `WM_TIMER`. Both must keep working.

## 2. Current status

**Build status:**
* As a subdirectory of `../free-eggbert` (Ninja, `cmake-build-debug/`):
  configures and builds cleanly, including known-ID and used-ID
  configure-time verification (308 unique used string IDs verified
  present).
* As a subdirectory of `../planetblupi` (Unix Makefiles, `build/`): same,
  257 unique used string IDs verified present (exact match against all
  extracted strings — planetblupi uses every string it defines).
* Standalone (`-DFREE_API_USE_SYSTEM_SDL3=ON`, no sibling game required):
  configures, builds, and passes 17/17 tests, now that `SDL3_image`/
  `SDL3_mixer` are available in this sandbox under `/usr/local`.

**Test status:** 17 test binaries registered per target-game build,
**17/17 passing** in all three build modes above, under
`SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy` — confirmed also passing
under a real (non-headless) X11/Xvfb session, ruling out "dummy driver
papering over a real bug" as an explanation for anything.

**CLI/tools/apps/libraries:** Free API produces one artifact,
`libfree-api.a`, plus its test binaries — no standalone CLI/app of its
own. It's a library consumed by the two games' own executables
(`SPEEDY_BLUPI_WINDOWS`, `PLANET_BLUPI_WINDOWS`), both of which build and
link successfully against the current code. `examples/` also exists
(five standalone WinAPI demos, `-DFREE_API_BUILD_EXAMPLES=ON`, default
`OFF`) — not touched or re-verified recently.

**Recently implemented features:** see section 3 — this session's work
was analysis and planning-document rewrites, not new features.

**What does NOT work / known gaps:**
* MCI digital-video (AVI movie playback) is **not implemented** —
  confirmed intentional; both games gracefully skip movies when this is
  declined (`docs/out-of-scope.md`).
* `LoadStringA`'s real-text table contains only one game's strings per
  build (whichever game's `.rc` was extracted) — by design, not a bug.
* MIDI audio, palette/BMP rendering, and the `MK_SHIFT`/`MK_CONTROL`
  drag-highlight fix have not had a full human playtest (partially
  verified via automated/Xvfb-driven checks — see section 5).
* `plan.md`'s new `TASK-0001`–`TASK-0012` backlog is entirely unimplemented
  (see section 8).

## 3. Recent changes

Most recent first (see `git log` for full detail on any entry below):

* **`30e0c54`** — Rewrote `plan.md` from scratch: deleted the old, fully-
  closed 127-task audit and independently re-derived the entire scope
  audit (five parallel research passes over free-api's headers and both
  games' source). Result: 9 of ~15 `todo/free-api-performance-todo.md`
  items were already resolved without being marked so; zero stale header
  `@note Status:` annotations found; `docs/supported-apis.md` found
  missing 28 symbol rows; two real bounded bugs found (scaled `StretchBlt`
  axis-clamp asymmetry, `_findfirst` session-table leak); one header-
  hygiene gap found (`FreeApiCreateSurfaceDC`/`FreeApiDestroySurfaceDC`
  have no shared header declaration with `free-direct`). New backlog:
  `TASK-0001`–`TASK-0012` (see section 8). Also fixed the `plan.md`
  section-number cross-references this broke in `docs/headers.md`,
  `docs/out-of-scope.md`, `docs/supported-apis.md`. No production code
  changed.
* **`911b580`** — Deduplicated `cmake/used-string-ids/free-eggbert.txt`
  (IDs 194/288 were each listed twice); added duplicate-ID detection to
  `cmake/ExtractStringTable.cmake`'s `USED_IDS_FILE` (fails configure
  loudly on any repeated ID); normalized "308/257 unique used string IDs"
  wording across docs; fixed stale `STUB` claims for `LoadStringA` in the
  (now-deleted) old `plan.md`.
* **(uncommitted investigation, same session as `911b580`)** — Xvfb+xdotool
  playtest attempt: launched both games under real X11, visually
  inspected screenshots (clean rendering, no corruption/crash) and
  captured gameplay audio via `SDL_AUDIODRIVER=disk` (real, non-silent,
  non-clipped, structured signal — but musical correctness unjudgeable
  without hearing). Could not reach planetblupi's shift/ctrl drag-
  highlight (its demo mode exits on any click; menu navigation not mapped
  out). Confirmed the standalone-build gap (missing `SDL3_image`/
  `SDL3_mixer`) had already resolved itself in this sandbox. Confirmed via
  a real X11 session that `test_winuser_regressions`'s 6 Wayland-only
  failures are purely an environment artifact.
* **`8272622`** (`TASK-0128`) — Per-used-ID configure-time verification
  (`USED_IDS_FILE` in `cmake/ExtractStringTable.cmake`) and the
  `FREE_API_TARGET_GAME` override (`auto`/`free-eggbert`/`planetblupi`/
  `standalone`) in `CMakeLists.txt`. New `cmake/used-string-ids/*.txt`
  manifests (308/257 used IDs). `tests/test_loadstring_regressions.cpp`
  extended to check 10 known IDs per game.
* **`88484a3`** (`TASK-0127`) — `cmake/ExtractStringTable.cmake` gained
  `REQUIRE_STRINGS`/`VERIFY_ID`/`VERIFY_TEXT`: target-game configure now
  fails loudly (instead of silently degrading to placeholder text) if a
  game's `.rc` is missing/empty or a known sentinel ID doesn't resolve.
* **`c8d44cb`** (`TASK-0103`) and earlier — real joystick support via SDL,
  standalone WinAPI examples, a MIDI-playback SIGSEGV fix, a
  `GetDeviceCaps(SIZEPALETTE)` fix, and a `BITMAPFILEHEADER` packing fix.
  All predate this session; see `git log` for details.

## 4. Current blocker / main problem

**No blocker.** All CMake configure paths (per-game and standalone) work,
17/17 tests pass in every build mode, and the previously-open environment
questions (Wayland test failures, standalone-build SDL3_image/mixer gap)
are both closed out and confirmed non-issues. `plan.md`'s new backlog
(section 8) is non-blocking follow-up work, not a blocker.

## 5. Known bugs and limitations

* **Confirmed environment artifact, not a code bug:** `test_winuser_regressions`
  fails 6 checks under this sandbox's default Wayland display
  (`ClientToScreen`/`ScreenToClient`/`GetCursorPos` exactness assertions);
  passes cleanly under `SDL_VIDEODRIVER=dummy` or a real X11/Xvfb session.
  Fully closed out — do not re-investigate absent a new symptom.
* **Confirmed bug, not yet fixed:** scaled `StretchBlt` clamps out-of-range
  source X to the nearest edge pixel but skips (leaves untouched) out-of-
  range source Y — an inconsistency between the two axes of the same blit
  (`src/wingdi_blit.cpp:115-126`; `plan.md` `TASK-0003`).
* **Confirmed bug, low severity, not yet fixed:** `_findfirst`'s session
  table (`src/crt_io.cpp`, `g_findSessions`) leaks one entry per visit to
  free-eggbert's design-mission file picker, since that game's one call
  site never calls `_findclose` (`plan.md` `TASK-0009`).
* **Confirmed hygiene gap, not yet fixed:** `FreeApiCreateSurfaceDC`/
  `FreeApiDestroySurfaceDC` (`src/wingdi_dc.cpp`) have no shared header
  declaration — `free-direct` hand-declares its own copy, an unenforced
  cross-repo signature sync (`plan.md` `TASK-0002`).
* **Needs verification (human):** `GetDeviceCaps(SIZEPALETTE)`/
  `BITMAPFILEHEADER` packing rendering, MIDI audio, and the
  `MK_SHIFT`/`MK_CONTROL` drag-highlight — all partially verified this
  session via automated/Xvfb-driven checks (clean rendering, non-silent/
  non-crashing audio) but still need a real human pass; the drag-highlight
  specifically was never reached at all (see section 8, items 5-7).
* **Incomplete (documentation only, `plan.md` `TASK-0001`/`0005`–`0008`/
  `0010`–`0011`):** `docs/supported-apis.md` is missing 28 symbol rows; a
  handful of symbols used only by free-api's own tests (`CloseHandle`,
  `GetLastError`/`SetLastError`, `RemoveDirectoryA`,
  `SetEnvironmentVariableA`, `access`/`_access`) aren't yet documented
  alongside the already-accepted `Sleep`/`GetTickCount` pattern;
  `_chdir`/`_getcwd`/`OutputDebugStringW` have zero call sites anywhere
  and need a keep-or-remove decision; a stale "MIDI looping not
  supported" comment in `src/MidiMusic.cpp` should be corrected (both
  games already handle looping themselves via `MM_MCINOTIFY`).
* **By design, not a bug:** `LoadStringA`'s generated table holds only one
  game's strings per compiled build; MCI digital-video/AVI is permanently
  declined; `"RES_<id>"` is a debug-only placeholder that now fails
  configure if it would appear for a known used ID.
* **Unknown:** whether real joystick support is ever actually wanted for
  Free Eggbert — the backend is real (`joyGetPosEx`/`joyGetNumDevs`), but
  the game's own `m_somethingJoystick` flag is never set to anything but
  0, so it never actually polls the joystick regardless.

## 6. Architecture notes

**Main modules:**
* `src/winuser_*.cpp` — window lifecycle, message queue, input, cursor,
  timers.
* `src/wingdi_*.cpp` — GDI bitmap/DC/blit subset.
* `src/winmm.cpp` + `src/MidiMusic.cpp` — WinMM/MCI/MIDI (TinySoundFont +
  TinyMidiLoader); real joystick backend also in `src/winmm.cpp`.
* `src/winbase*.cpp`, `src/crt_*.cpp` — file/path/CRT helpers.
* `src/internal/*` — shared internal state: `FreeApiMessageQueue`,
  `FreeApiWindowRegistry`, `FreeApiGdi`, `FreeApiPath`,
  `FreeApiDiagnostics`, `FreeApiStringTable`, `FreeApiTimers`,
  `FreeApiSdlVideo`.
* `cmake/ExtractStringTable.cmake` — configure-time `STRINGTABLE`
  extractor + fail-loud known-ID/used-ID verification gates.
* `cmake/used-string-ids/{free-eggbert,planetblupi}.txt` — evidence-based
  manifests of every ID each game actually passes to `LoadString`.

**Data flow:** SDL3 events → `FreeApiMessageQueue::PumpSdlEvents` →
internal message queue → `PeekMessageA`/`GetMessageA` →
`DispatchMessageA` → the game's registered `WndProc`. `WM_CREATE`/
`WM_DESTROY` are delivered synchronously (direct call), never via the
queue.

**Important invariants — do not break without re-verifying against both
target games:**
* Public headers (`include/`) must never expose SDL types.
* Every public API needs a real, cited usage site in `../free-eggbert` or
  `../planetblupi` — except the documented `free-direct`-bridge exception
  (`FreeApiRunWinMain`, `FreeApiCreateSurfaceDC`/`FreeApiDestroySurfaceDC`;
  `plan.md` `TASK-0011`). Do not read that exception more broadly.
* Both timer mechanisms must keep working — each is the sole frame pump
  for one of the two games.
* `WM_MOUSEMOVE`'s `lParam` packing must stay bit-exact (demo-file replay
  compatibility).
* Free API's "window size" **is** the client-area size — `AdjustWindowRect`
  is deliberately a no-op identity transform; do not "fix" it without
  changing `CreateWindowExA`/`GetClientRect` in lockstep.
* Do not weaken `REQUIRE_STRINGS`/`VERIFY_ID`/`USED_IDS_FILE` in
  `cmake/ExtractStringTable.cmake` without understanding why they exist.
* `cmake/used-string-ids/*.txt` manifests must stay evidence-based (every
  entry cited to a `file:line` or a computed-range derivation) and
  duplicate-free (enforced by `USED_IDS_FILE` itself).
* `cmake/ExtractStringTable.cmake` must stay a narrow `STRINGTABLE`-only
  parser, not a general `.rc` compiler.
* `free-direct` depends on `FreeApiCreateSurfaceDC`/
  `FreeApiDestroySurfaceDC` (`src/wingdi_dc.cpp`) — signatures must not
  change without updating `free-direct` too.
* DirectDraw/DirectSound/DirectPlay are out of scope for Free API — that's
  `free-direct`'s responsibility.
* ANSI-only (`A`-suffixed) functions are implemented; no real Unicode/`W`
  behavior (neither game defines `UNICODE`).

## 7. Useful commands

```bash
# Build via a real target game
cd ../free-eggbert/cmake-build-debug && cmake . && ninja -j"$(nproc)"
cd ../planetblupi/build && cmake . && make -j"$(nproc)"

# Standalone build (no sibling game needed)
cmake -S . -B build -DFREE_API_USE_SYSTEM_SDL3=ON -DFREE_API_BUILD_TESTS=ON
cmake --build build -j"$(nproc)"
cd build && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure

# Run the full test suite -- SDL_VIDEODRIVER=dummy is NOT optional under
# this sandbox's default (Wayland) display, or 6 window-position tests
# spuriously fail (confirmed environment-only, see section 5)
cd ../free-eggbert/cmake-build-debug/FREE_API && \
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure
cd ../planetblupi/build/FREE_API && \
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure

# Run one specific test binary directly (binaries land in bin/ at the
# game's own build-dir top level, not under FREE_API/)
cd ../free-eggbert/cmake-build-debug && SDL_VIDEODRIVER=dummy ./bin/test_loadstring_regressions

# Reproduce CMake configure-time known-ID + used-ID verification directly
cmake -DOUTPUT_CPP=/tmp/out.cpp \
  -DRC_FILE=../planetblupi/resource/blupi-e.rc -DRC_ENCODING=UTF-8 \
  "-DRESOURCE_HEADERS=../planetblupi/include/resource.h;../planetblupi/include/resrc1.h" \
  -DREQUIRE_STRINGS=ON -DTARGET_GAME_NAME=planetblupi \
  -DVERIFY_ID=106 "-DVERIFY_TEXT=Quit BLUPI" \
  -DUSED_IDS_FILE=cmake/used-string-ids/planetblupi.txt \
  -P cmake/ExtractStringTable.cmake

# Force a specific target game's gating without needing its full build tree
cmake -B build -DFREE_API_TARGET_GAME=free-eggbert
```

No lint/formatter is configured in this repository.

## 8. Next smallest tasks

`plan.md`'s fresh `TASK-0001`–`TASK-0012` backlog is entirely unimplemented.
The three below are the real, bounded correctness/hygiene gaps (highest
value); everything else in `plan.md` is small documentation/classification
cleanup with zero behavior change.

1. **Fix scaled `StretchBlt`'s asymmetric out-of-range source-clipping**
   (`plan.md` `TASK-0003`).
   * Goal: X clamps out-of-range source coordinates to the nearest edge
     pixel; Y instead skips (leaves destination untouched) — pick one
     consistent policy (clamp-to-edge on both axes recommended) and add a
     regression test.
   * Files: `src/wingdi_blit.cpp:115-126`, `tests/test_gdi_regressions.cpp`.
   * Verification: new test passes; `ctest -R test_gdi_regressions`
     (via either game's build dir, `SDL_VIDEODRIVER=dummy`) still passes.

2. **Add a shared header declaration for `FreeApiCreateSurfaceDC`/
   `FreeApiDestroySurfaceDC`** (`plan.md` `TASK-0002`).
   * Goal: these two functions currently have no declaration in any
     free-api header; `free-direct` hand-declares its own local
     `extern "C"` copy to call them. Add one authoritative declaration
     (likely `include/wingdi.h`) and update `free-direct` to use it
     instead of its own copy — this needs coordination with that sibling
     repo.
   * Files: `include/wingdi.h` (or similar), `src/wingdi_dc.cpp`;
     `../free-direct/src/directdraw/DirectDraw.cpp`.
   * Verification: both games still build and link as `free-direct`
     siblings; `ctest` still 17/17 in both.

3. **Investigate/fix `_findfirst`'s unbounded session-table leak**
   (`plan.md` `TASK-0009`).
   * Goal: free-eggbert's design-mission file picker
     (`../free-eggbert/src/event.cpp:4736-4747`) calls `_findfirst`/
     `_findnext` in a fully-draining loop but never calls `_findclose`,
     leaking one `g_findSessions` map entry per screen visit. Either make
     the session self-clean once a caller's loop fully drains it, or
     document as a known low-severity limitation — see `plan.md` for both
     candidate approaches.
   * Files: `src/crt_io.cpp`, `tests/test_file_paths.cpp`.
   * Verification: if fixed, add a regression test proving repeated
     drain-without-close cycles don't grow the table; existing
     `test_file_paths` still passes either way.

4. **`plan.md`'s remaining tasks are documentation/classification-only**
   (`TASK-0001`, `TASK-0005`–`TASK-0008`, `TASK-0010`–`TASK-0011`) — small,
   independent, no cross-repo coordination needed. Good candidates for a
   quick session.

5. **Human sign-off on rendering** (`GetDeviceCaps(SIZEPALETTE)`/
   `BITMAPFILEHEADER` packing) — automated Xvfb screenshots already look
   clean; needs a human glance for anything subtler than gross corruption.
   * Verification: run either game normally and eyeball it.

6. **Human audible playtest of MIDI music** — confirmed non-silent/non-
   crashing via captured audio, but musical correctness is unjudgeable
   without hearing.
   * Verification: run either game with real audio output and listen.

7. **Human playtest of `MK_SHIFT`/`MK_CONTROL` drag-highlight in
   planetblupi** (`event.cpp:3440-3504`) — never reached; its demo mode
   exits to the title screen on any click, and the menu navigation to
   start a real session wasn't mapped out.
   * Verification: get into a real (non-demo) planetblupi session,
     drag-select multiple Blupis with Shift held, check the highlight.

## 9. Do not do yet

* Do not weaken or remove `REQUIRE_STRINGS`/`VERIFY_ID`/`USED_IDS_FILE` in
  `cmake/ExtractStringTable.cmake`/`CMakeLists.txt` to "fix" a build
  failure — a `FATAL_ERROR` there means the `.rc`/known-ID data is
  genuinely broken; fix the underlying data, don't silence the check.
* Do not build a general `.rc`/`.res` compiler —
  `ExtractStringTable.cmake` must stay narrowly `STRINGTABLE`-only.
* Do not touch joystick, MCI digital-video, DirectDraw, DirectSound,
  DirectPlay, or `free-direct` without a specific, separately-scoped
  request.
* Do not assume `test_winuser_regressions` failures under the default
  display mean a real regression — this is a closed investigation
  (confirmed environment-only); do not re-open absent a genuinely new
  symptom.
* Do not revert the `GetDeviceCaps(SIZEPALETTE)` value (now 0) or the
  `BITMAPFILEHEADER`/`BITMAPINFOHEADER` `#pragma pack(push, 2)` without
  understanding why first (see `git log` for the original evidence).
* No real Unicode/`W` API implementations.
* No consolidating the two timer mechanisms into one "unified"
  implementation.
* No "fixing" `AdjustWindowRect` to add real Win32 window-chrome math.
* No new public API without a cited `file:line` usage site in
  `../free-eggbert` or `../planetblupi` — except the documented
  `free-direct`-bridge exception (`plan.md` `TASK-0011`); do not read it
  more broadly than that.
* No broad refactor of any currently-passing subsystem absent a specific,
  evidenced bug report.
* Do not implement any of `plan.md`'s `TASK-0001`–`TASK-0012` beyond what
  each task's own "Required work"/"Out of scope" sections state — several
  are deliberately "pick one, document the choice" decisions
  (`TASK-0006`, `TASK-0007`, `TASK-0009`), not "implement the obvious fix."

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
