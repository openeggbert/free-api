# Free API — Next Up

A living, prioritized queue of what to tackle next, kept short on purpose.
For full evidence, tables, and the complete task backlog, see [`plan.md`](plan.md)
and the scope policy in [`docs/scope.md`](docs/scope.md). Every item below must
still obey that policy: no API without a real usage site in `../free-eggbert`
or `../planetblupi`.

## Just completed

* **MCI digital-video / AVI movie playback investigated and resolved —
  formerly the single highest-risk item in `plan.md` §10.** Static trace of
  both games' actual source (identical logic in each) shows this is already
  a permanent, deliberate, safe decline, not a gap: `CMovie::initAVI()`'s
  `MCI_OPEN(MCI_OPEN_TYPE)`-only probe correctly gets
  `MCIERR_UNSUPPORTED_FUNCTION`, `CMovie::Create()` sets `m_bEnable=FALSE`,
  and `CEvent::StartMovie()`/`MovieToStart()` then transition straight to
  the post-movie phase — exactly what a completed movie would do. No crash,
  no hang, no visible error; cutscenes are just silently skipped. Removed a
  stale "TODO: segfault" comment in `src/winmm.cpp` that predated the guard
  making that path unreachable. Documented in `docs/out-of-scope.md` (new)
  and `digitalv.h`'s doc comment, locked in by
  `tests/test_mci_avivideo_regressions.cpp`. No implementation was needed or
  added — this required investigation only, per plan.md TASK-0095/0096.

* **`LoadStringA` now returns real UI text** instead of a placeholder, for
  both games. `cmake/ExtractStringTable.cmake` is a narrow, STRINGTABLE-only
  extractor (not a general `.rc`/`.res` compiler) that pulls `ID -> text`
  pairs out of whichever target game's own `resource/*.rc` is actually
  driving the current build, and compiles them into a generated `.cpp`.
  Real evidence found real data already sitting unused in both repos:
  planetblupi's `resource/blupi-e.rc` (257 entries, ISO-8859) and
  free-eggbert's `resource/Eggbert2.rc` (364 entries, UTF-16LE — an initial
  `grep`-based estimate of "68" was itself wrong, corrupted by grep's
  binary-file heuristic misfiring on the UTF-16LE encoding). Falls back to
  the previous `"RES_<id>"` placeholder for any ID with no STRINGTABLE entry,
  or in a standalone build with neither sibling game present.
  **A real bug was found and fixed during verification**: the first version
  picked whichever sibling directory happened to exist *on disk* next to
  free-api, which is wrong — both target games are always siblings of each
  other in a normal dev checkout, so that heuristic would silently compile
  the *wrong* game's string table in whichever game *wasn't* checked first.
  Fixed by keying off `CMAKE_PROJECT_NAME` (the actual top-level project
  driving the current build) with the sibling-existence heuristic now used
  only for a genuinely standalone free-api build. Verified by building
  through both `../free-eggbert` and `../planetblupi` directly and
  confirming each pulls in its *own* data. Covered by
  `tests/test_loadstring_regressions.cpp`.

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

### 1. Smaller cleanup batch

* Gate the rest of the hot-path `SDL_Log` calls that `todo/free-api-performance-todo.md`
  already identifies (this pass only gated the `FREE_DIRECT_INPUT`
  first-20-events block) — plan.md `TASK-0105`.
* Gate diagnostics atomic counters so they're only touched when diagnostics
  are enabled — `TASK-0107`.
* Apply the already-scoped `StretchBlt` fixes from the same TODO file (1:1
  fast path, scaled-path clipping semantics, avoid per-blit allocation) —
  `TASK-0065`/`TASK-0066`/`TASK-0068`.
* Create `docs/target-games.md` from plan.md's governance milestone, the one
  remaining doc that doesn't exist yet (`docs/scope.md`,
  `docs/cmake-options.md`, and now `docs/out-of-scope.md` are all in place)
  — `TASK-0003`.

## Explicitly not queued (per scope policy)

Unicode `W` APIs, PE/`.rc`/`.res` resource compilation, `_findfirst`/`_findnext`
real implementation (eggbert-only, non-blocking feature), real joystick
support (eggbert-only, optional, current safe-stub degrades gracefully), and
anything in DirectDraw/DirectSound/DirectPlay/`free-direct` — none of these
have evidence of being blockers for either game today. Revisit only if new
evidence turns up.
