# Free API — Next Up

A living, prioritized queue of what to tackle next, kept short on purpose.
For full evidence, tables, and the complete task backlog, see [`plan.md`](plan.md)
and the scope policy in [`docs/scope.md`](docs/scope.md). Every item below must
still obey that policy: no API without a real usage site in `../free-eggbert`
or `../planetblupi`.

## Just completed

* **`DestroyWindow` now dispatches `WM_DESTROY` synchronously** to the
  window's own `WndProc` before tearing the window down (matching real Win32
  semantics), and cleans up all of its registry entries
  (`g_freeApiWindowStates`, `g_focusWindow`) on destroy. Previously
  `DefWindowProcA`'s `WM_CLOSE` handler destroyed the window and posted quit
  directly, so neither game's own `WM_DESTROY` handler (which kills their
  frame-pump timer via `KillTimer`/`timeKillEvent` and tears down game
  objects) ever ran on a normal quit — that timer could outlive the message
  loop. `DefWindowProcA`'s `WM_CLOSE` branch no longer posts a redundant
  second `WM_QUIT` now that `WM_DESTROY` handling is the canonical place for
  it. Covered by a new regression test
  (`TestDestroyWindowDispatchesWmDestroySynchronously` in
  `tests/test_winuser_regressions.cpp`).

## Queue, in priority order

### 1. `LoadStringA` — biggest known functional gap

Currently a `STUB` that returns placeholder text (`"RES_<id>"`), but **both**
games source *all* on-screen UI text through it — tooltips, button labels,
win/lose/error messages — via ~50-90+ call sites each. See plan.md
`TASK-0074` (investigate exactly what's currently shown on-screen — it may
already look broken) and `TASK-0075` (implement the narrowest possible real
string-table backing, scoped only to IDs actually reachable in normal play —
explicitly **not** a general `.rc`/`resource.h` parser). This needs a design
decision (where does the real string data come from, since neither game's
`.rc` file is compiled today) before implementation, so start with the
investigation step.

### 2. MCI digital-video / AVI movie playback — highest risk per plan.md §10

Both games' `movie.cpp` drive a full `MCI_DGV_OPEN/STATUS/PLAY/PAUSE/CLOSE`
sequence for in-game movies, including expecting a real, movable window
handle back from `MCI_STATUS`/`MCI_DGV_STATUS_HWND` — but `mciSendCommandA`
only genuinely implements the `"sequencer"` (MIDI) device type; digital-video
is `STUB`. Movies may simply not work in either game today. Start with
plan.md `TASK-0095` (run each game, trigger a movie/cutscene, observe
whether it plays, is silently skipped, or errors — planetblupi's own source
even has the AVI window-*show* call commented out, so it's unclear whether
this is already inert independent of Free API). Only after that finding is
in hand should `TASK-0096` (implement minimal support, or explicitly
document it as a permanent, evidenced limitation) proceed. This is
significant, possibly large scope — do not start implementing before the
investigation confirms it's actually needed.

### 3. Smaller cleanup batch

* Gate the rest of the hot-path `SDL_Log` calls that `todo/free-api-performance-todo.md`
  already identifies (this pass only gated the `FREE_DIRECT_INPUT`
  first-20-events block) — plan.md `TASK-0105`.
* Gate diagnostics atomic counters so they're only touched when diagnostics
  are enabled — `TASK-0107`.
* Apply the already-scoped `StretchBlt` fixes from the same TODO file (1:1
  fast path, scaled-path clipping semantics, avoid per-blit allocation) —
  `TASK-0065`/`TASK-0066`/`TASK-0068`.
* Create the two scope docs from plan.md's governance milestone that don't
  exist yet: `docs/target-games.md` and `docs/out-of-scope.md` (only
  `docs/scope.md` and `docs/cmake-options.md` exist so far) — `TASK-0003`/`TASK-0004`.

## Explicitly not queued (per scope policy)

Unicode `W` APIs, PE/`.rc`/`.res` resource compilation, `_findfirst`/`_findnext`
real implementation (eggbert-only, non-blocking feature), real joystick
support (eggbert-only, optional, current safe-stub degrades gracefully), and
anything in DirectDraw/DirectSound/DirectPlay/`free-direct` — none of these
have evidence of being blockers for either game today. Revisit only if new
evidence turns up.
