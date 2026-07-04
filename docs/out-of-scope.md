# Free API — Out-of-Scope / Permanently-Declined Behavior

This document records WinAPI/WinMM behavior that Free API deliberately does
**not** implement, with the evidence backing that decision. See
[`docs/scope.md`](scope.md) for the underlying policy and
[`plan.md`](../plan.md) section 5 for the full point-in-time audit table this
document complements (not duplicates).

## MCI digital-video / AVI movie playback ("avivideo")

**Decision: permanently declined. This was the single highest-risk unresolved
item in `plan.md` (section 10) and has since been investigated and resolved
as a non-issue.**

Both target games' `CMovie::initAVI()` (`movie.cpp` in each) call:

```cpp
mciSendCommand(0, MCI_OPEN, MCI_OPEN_TYPE, &mciOpen); // no MCI_OPEN_ELEMENT
```

to probe for an `"avivideo"` MCI driver. `mciSendCommandA` (`src/winmm.cpp`)
returns `MCIERR_UNSUPPORTED_FUNCTION` for this exact call shape. Tracing the
full call chain in both games' own source (identical in both — same shared
heritage code):

1. `CMovie::initAVI()` returns `FALSE` (checks `mciSendCommand(...) == 0`).
2. `CMovie::Create()` sets `m_bEnable = FALSE`.
3. `CEvent::StartMovie()` guards on `if (!m_pMovie->GetEnable()) return FALSE;`
   at its very top — no side effects (no music stop, no palette save, no
   sound cache) ever execute when movies are disabled.
4. `CEvent::MovieToStart()`, on `StartMovie()` returning `FALSE`, immediately
   calls `ChangePhase(m_phaseAfterMovie)` — **the exact same phase
   transition a real, completed movie would trigger.**

Net effect: cutscenes are silently and safely skipped end-to-end. No crash,
no hang, no visible error, no black screen — the player just proceeds
straight to whatever comes after the movie, as confirmed by static trace of
both games' actual source (not assumed). A stale `// TODO: segfault is
happening here for the game Planet Blupi` comment in `src/winmm.cpp`
predated the early-return guard that makes this safe; it has been removed
since the guard makes that code path unreachable for this call shape.

Locked in by `tests/test_mci_avivideo_regressions.cpp`.

**Decision: keep declining this permanently.** Do not implement AVI/digital-
video playback — there is no evidence either game's actual player-visible
behavior needs it, and doing so would require a video codec dependency this
project has no other reason to take on.

## Full audit

See `plan.md` section 5 for the complete, point-in-time table of every other
API/area classified as out-of-scope (compile-only stubs, unproven-but-
harmless translations, DirectX-family symbols owned by `free-direct`, etc.).
That table is not duplicated here to avoid the two documents drifting out of
sync; this file is reserved for findings substantial enough to warrant their
own write-up, like the one above.
