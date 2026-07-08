# Free API — Target-Game Verification / Playtest Checklist

A single, runnable checklist for manually verifying both target games
against the current `free-api` tree (`TASK-24H-1209`). This is the doc a
human tester should actually work through — it does not re-derive *why*
each check matters (that reasoning lives in `docs/out-of-scope.md`,
`docs/target-games.md`, and the individual `TASK-24H-*` entries this file
cross-references) or duplicate build/test commands (`docs/testing.md`,
`docs/cmake-options.md` are canonical for those).

**None of this checklist can be run headlessly.** It needs a real display
backend, a real audio device, and a human observer — that is the entire
reason it isn't automated already (see `docs/audit-24h-free-api.md` and
`NEXT.md` for why the automated test suite can't cover this).

## 1. Launch instructions

Build each game first — see [`docs/testing.md`](testing.md) for the full
build reference; the short version:

```bash
# free-eggbert
cd ../free-eggbert/cmake-build-debug && cmake . && ninja -j"$(nproc)"
./bin/SPEEDY_BLUPI_WINDOWS

# planetblupi
cd ../planetblupi/build && cmake . && make -j"$(nproc)"
./bin/PLANET_BLUPI_WINDOWS
```

Run both **without** `SDL_VIDEODRIVER=dummy`/`SDL_AUDIODRIVER=dummy` — those
are test-only environment variables that suppress the real display/audio
this checklist needs.

**Checkpoint:** both processes start, a real window appears, and neither
crashes or hangs before reaching its title/intro screen. If either fails
here, stop and file a bug — nothing below can be checked without this.

## 2. MIDI audio sign-off (`TASK-24H-1221`)

Ensure a `.sf2` SoundFont is available first — see the README's "SoundFont
requirement" section for the lookup order, or set `FREE_API_SOUNDFONT`.

For **each** game, separately:

- [ ] Navigate to a screen/state where background music normally plays
  (title screen, in-mission gameplay).
- [ ] Confirm music is **audible** and **not corrupted** (no garbling,
  stuttering, or silence-by-error). If no SoundFont is present, confirm it
  degrades to **silence**, not a crash or error — that is documented,
  correct behavior, not a bug.
- [ ] Navigate through several level/track loads (start a mission, return
  to menu, start another; or the equivalent for free-eggbert) while
  watching console output (run with `FREE_API_DEBUG_MIDI=1` for detail).
  Confirm no `[midi] SDL_InitSubSystem(AUDIO) failed`/
  `SDL_OpenAudioDeviceStream failed` message repeats more than **once**
  per process (`TASK-24H-1109`'s fix — this branch only triggers on a
  machine with an unavailable audio device; if your machine's audio works
  fine, note that this specific scenario wasn't exercised and move on).

## 3. Rendering / blitting / image-loading sign-off (`TASK-24H-1222`)

For **each** game, separately:

- [ ] Menus and in-level graphics render without corruption, wrong colors,
  missing tiles, or obviously misplaced content.
- [ ] (planetblupi only, optional but useful) Check the level-editor/build
  screen too — this is the one place `CreateBitmap`'s 8-bit greyscale-only
  path is reachable (fullscreen mode only); a visibly wrong minimap there
  is a **known, already-documented** limitation, not a new bug — see
  `docs/out-of-scope.md`.

## 4. `LoadStringA` / UI-text check

- [ ] While navigating menus/buttons/tooltips in both games, confirm no UI
  text ever displays as a literal `"RES_<id>"` placeholder (e.g.
  `"RES_106"`). Every real UI string should show real, readable text — see
  `docs/out-of-scope.md`'s "RES_<id>" note for why this would indicate a
  real regression, not a cosmetic issue.

## 5. Save/load and path check

- [ ] Exercise a real save and load cycle (or each game's closest
  equivalent — e.g. planetblupi's mission/private-level save slots) and
  confirm the game resumes in the expected state, not a crash or
  corrupted/blank state.

## 6. planetblupi gameplay-access sequences

Both sequences below are traced from `../planetblupi/src/event.cpp`
source (not previously observed live) — see
[`docs/target-games.md`](target-games.md)'s "Manual playtest key/menu
sequences" section for the full derivation and exact `file:line` evidence
if either doesn't work as described.

**To reach live `WM_PHASE_PLAY` gameplay** (needed for step 7's `MK_SHIFT`
check, `TASK-24H-0401`):

> Press **Enter** four times from the cold-boot title/intro screens
> (`WM_PHASE_INTRO1`→`INTRO2`→`INIT`→`INFO`→`PLAY`). Dismiss the 30s
> attract-mode auto-demo with any keypress first if it triggers.

**To reach the level-editor's decor-tool-active `WM_PHASE_BUILD` state**
(needed for step 7's `MK_CONTROL` check, `TASK-24H-0403`, depends on
`TASK-24H-0402`):

> Press **Enter twice** (reaches the title screen) → click **"Privé"**
> (do not press Enter here) → click **"Build"** (now visible). A decor
> tool is auto-selected the instant this screen appears — no further step
> needed before testing `MK_CONTROL`.

## 7. `MK_SHIFT`/`MK_CONTROL` human playtest (`TASK-24H-0401`/`0403`)

- [ ] **`MK_SHIFT`** (`TASK-24H-0401`): from live `WM_PHASE_PLAY`
  gameplay (step 6's first sequence), hold Shift and drag the mouse over
  multiple friendly units. Confirm all units under the drag rectangle
  become highlighted/selected together (additive multi-unit highlight).
- [ ] **`MK_CONTROL`** (`TASK-24H-0403`): from the decor-tool-active
  `WM_PHASE_BUILD` state (step 6's second sequence), hold Ctrl and
  click/drag over the decor-placement area. Confirm the flood-fill
  (`ArrangeFill`) behavior fills contiguous matching cells.

## Recording results

Record the outcome (pass/fail, with specifics) for each numbered section
above, **per game where applicable**, in `NEXT.md`. If any check fails to
match its expected behavior, file a new follow-up bug task citing the
exact observed vs. expected behavior — do not silently note it here and
move on.
