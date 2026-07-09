# Free API — Deep Audit (2026-07-09, re-audit)

## Executive Summary

This is a full re-audit of free-api's current source, performed after the
previous deep audit's entire findings list (16 formalized tasks,
`TASK-24H-1229` through `TASK-24H-1244`, plus the pre-existing
`TASK-24H-0706`) was implemented, verified across all build trees and both
sanitizers, and pushed. The previous `audit.md` was deleted and this is a
ground-up re-analysis, not an incremental update — every finding below was
independently re-derived by reading the current source, not carried over
from memory of the prior audit.

**Headline result: the codebase is materially healthier than the last
audit.** Five independent parallel reviews (correctness, performance,
memory safety, edge cases, architectural risk) found **no new HIGH-severity,
live-game-reachable defects**. Two issues stand out as worth fixing:

1. The `TASK-24H-1233` fix (move `PostMessageA` outside the MIDI mutex's
   lock scope) was scoped to exactly one of the two call sites that have
   this pattern — the second, in `MCI_PLAY`'s no-SoundFont branch, still has
   it. **Independently found by two of the five reviews** (correctness and
   architectural risk).
2. The `TASK-24H-1231` pitch-overflow fix (cast before multiplying) was
   scoped to exactly one of the three call sites that compute the same
   quantity the same way — two siblings in `src/internal/FreeApiGdi.cpp`
   still have the unguarded `int * int` multiplication. **Independently
   found by two of the five reviews** (correctness and edge cases).

Both are the same lesson repeated: when a fix closes one instance of a
pattern, grep for sibling instances of the same pattern before calling the
task done. Everything else found is either a new, narrow, unreachable-today
finding worth a small defensive fix, or a real gap in the verification
workflow itself (AddressSanitizer's leak detection is effectively disabled
for the whole `ctest` suite, not just for SDL3's own allocations).

No currently-live game behavior is broken. Every finding below was checked
for reachability against `../free-eggbert` and `../planetblupi`'s actual
source, not assumed.

## Methodology

Five independent parallel AI reviews, each given the same directive
(re-derive findings fresh from current source; do not reuse the prior
audit's conclusions — everything in it is already fixed) and a distinct
analytical lens: correctness, performance, memory safety, edge
cases/extreme situations, and architectural risk. Each was explicitly
pointed at `docs/out-of-scope.md`/`docs/supported-apis.md` first, to avoid
re-flagging an already-documented, deliberate decision. All five ran
read-only, with no shared context beyond this conversation's history, so
agreement between independent reviews is treated as a stronger confidence
signal than any single review's finding alone (see "Cross-Validated
Findings" below).

## Correctness Analysis

### C1 (MEDIUM): Pitch-overflow fix wasn't applied to two sibling call sites

`src/internal/FreeApiGdi.cpp:52` (`CreateCompatBitmapFromSurface`, the
backing function for `LoadImageA`) and `:99` (`ScaleCompatBitmap`, called
from `LoadImageA` when `cx`/`cy` are supplied) both compute
`bitmap->pitch = width * 4` via unguarded `int * int` multiplication — the
identical signed-integer-overflow pattern `TASK-24H-1231` already fixed in
`src/wingdi_bitmap.cpp`'s `CreateBitmap` (now `static_cast<int>(static_cast<int64_t>(nWidth) * 4)`).
That fix's scope only covered `CreateBitmap`; nothing grepped for other
occurrences of the same pattern at the time.

Reachability: `LoadImageA` → `CreateCompatBitmapFromSurface`'s width comes
from `SDL_ConvertSurface`'s output on a loaded, real, shipped `.bmp` asset;
`LoadImageA`'s `cx`/`cy` → `ScaleCompatBitmap` trace back to both games'
`DDLoadBitmap(pdd, szBitmap, dx, dy)` call sites, which always pass small,
fixed sprite dimensions (most calls pass `dx=dy=0`, skipping
`ScaleCompatBitmap` entirely). **Not reachable by either game's real
assets** — same profile as the original finding: defensive-hardening
consistency gap, not an active bug.

### C2 (MEDIUM): `PostMessageA`-under-mutex pattern `TASK-24H-1233` fixed still exists at a second, real, reachable call site

`src/MidiMusic.cpp:696` — `MidiMusicSendCommand`'s `MCI_PLAY` handler calls
`PostMessageA(s.notifyHwnd, MM_MCINOTIFY, ...)` while still holding
`GetMidiState().mtx` (acquired at line 667, in scope for the whole
`MCI_PLAY` block). This is the exact widened-critical-section/latent-lock-
order pattern `TASK-24H-1233` fixed in `MixerThread` — but that task's own
"Out of scope" note explicitly left this second site untouched
("Do not restructure `MidiMusicSendCommand`'s other locking beyond this one
call site").

Reachability: fires whenever `MCI_PLAY` is called with no SoundFont loaded
(the "silent success" branch) — a real, exercised path
(`test_mci_sequences.cpp` has a dedicated no-SoundFont test covering exactly
this). Currently safe (no other code path acquires the message-queue mutex
then `GetMidiState().mtx` in the reverse order), but the identical latent
risk `TASK-24H-1233`'s own problem statement described remains live here.
`grep` confirms these are the only two `PostMessageA` call sites in
`MidiMusic.cpp`.

### Confirmed fine (re-checked, not re-flagged)

`StretchBlt`'s 1:1 fast-path clipping (worked through the four-step clip
algorithm symbolically — the bounds invariant holds for arbitrary integer
inputs, not just lucky game-specific ones); `GetPixel`/`SetPixel`'s bounds
checks; `DispatchMessageA`'s window lookup; `DestroyWindow`'s teardown
order across all four registries; `MCI_CLOSE`'s session-erase (no
post-erase iterator use); `_lopen`/`_lclose` pairing in both games;
`GetSystemPaletteEntries`'s 256-entry contract; `FreeApiDiagSnapshot`'s
28-specifier format string (manually counted against its 28 arguments).

## Performance Analysis

### P1 (LOW): `PushMessage`'s coalescing scans are O(queue-depth) under the lock, on the hottest path in the codebase

`src/internal/FreeApiMessageQueue.cpp:81-109` — every `WM_MOUSEMOVE` and
`WM_TIMER` message triggers a reverse linear scan of `g_messageQueue` while
`g_messageQueueMutex` is held, to find an existing same-hwnd entry to
coalesce into. Mouse-move fires at OS event rate. Self-limiting in
practice — coalescing keeps at most one `WM_MOUSEMOVE` entry per hwnd, so
the scan usually terminates within a few hops — but it's still a linear
scan under a lock on the single hottest call path in the message system,
worst-case O(n) if many non-coalescible messages are queued ahead of it. No
evidence of an actual observed slowdown (`g_diagQueueHighWater` gives no
indication the queue ever grows large in practice). Not urgent.

### P2 (LOW): `g_nextTimerId.fetch_add(1)` uses the default seq_cst instead of relaxed

`src/winuser_timer.cpp:16` — every other counter/diagnostic atomic in the
codebase explicitly uses `std::memory_order_relaxed` (confirmed via a full
grep of every `.fetch_add`/`.fetch_sub` call site) except this one.
`SetTimer` is not a per-frame hot path (called once to establish
planetblupi's frame-pump timer, not repeatedly), so the real-world cost is
negligible — a consistency gap, not a performance problem worth
prioritizing on its own merits.

### Confirmed fine

`PeekMessageA`'s `pendingTimers` (already `thread_local` since
`TASK-24H-1236`); `MixerThread`'s notify path (already outside the lock
since `TASK-24H-1233`); `StretchBlt` (memcpy fast path + precomputed
`thread_local` lookup table for the scaled path — no allocations);
`GetPixel`/`SetPixel` (O(1)); diagnostics-only paths (gated behind an
opt-in flag, throttled to once per 5s).

## Memory Safety Analysis

**No new findings.** All findings from the prior audit (GDI double-free,
`CreateBitmap`'s pitch overflow, `CompatDC::selectedBitmap`'s
dangling-pointer risk, a test-file resource leak) are confirmed fixed.

Specifically re-verified and confirmed sound this pass: `StretchBlt`'s 1:1
fast-path bounds invariant (proved symbolically, not just tested against
typical inputs); `GetPixel`/`SetPixel`'s bounds checks on both DC kinds;
`DestroyWindow`'s teardown leaves no stale/dangling registry entries;
`MidiMusic.cpp`'s session-erase has no post-erase iterator use;
`FreeApiMmTimerBridge` packs the timer ID into userdata rather than a real
pointer into a map (no dereference-after-erase risk); `_lopen`/`_lclose`
pairing in both games; `GetSystemPaletteEntries`'s 256-entry contract
against both games' matching `PALETTEENTRY[256]` buffers.

One LOW-severity, effectively-unreachable observation (also independently
raised by the edge-case review — see E3 below): `_getcwd`
(`src/crt_direct.cpp:23-26`) casts a caller-supplied `maxlen` to `size_t`
with no guard against a negative value. Zero call sites in either target
game — not worth a task.

## Edge Case / Extreme Situation Analysis

### E1: Same finding as C1 (pitch-overflow fix incomplete) — see Cross-Validated Findings below.

### E2 (MEDIUM): `ResolveJoystick`'s bounds check validates a different value than the one used to index

`src/winmm.cpp:53-54`:
```cpp
if (static_cast<int>(uJoyID) < count) {
    joystick = SDL_OpenJoystick(ids[uJoyID]);
}
```
The bounds check casts `uJoyID` (a `UINT`) to `int` and compares against
`count`, but the array index uses the original, uncast `uJoyID`. Any
`uJoyID` with the high bit set (≥ `0x80000000`) casts to a negative `int`,
which is `< count` for any `count > 0` — the check passes, then
`ids[uJoyID]` indexes far out of bounds into the `SDL_JoystickID` array
`SDL_GetJoysticks` returned, reading garbage memory and passing it to
`SDL_OpenJoystick`.

Reachability: free-eggbert's only call site (`event.cpp:2071`) passes
`m_joyID`, a small device-index member never plausibly set to an extreme
value — **not reachable by real gameplay**, but a genuine,
previously-undocumented out-of-bounds-read hazard in the input-validation
logic itself, not just a theoretical style concern: the check and the
indexed value are simply different expressions.

### E3 (LOW, not actionable): `_getcwd` doesn't validate `maxlen` before an unsafe cast

Same finding as the Memory Safety section's observation above — zero call
sites in either target game (`_getcwd` is kept only for
header-completeness alongside the genuinely-used `_mkdir`). Not proposing a
task.

### Confirmed fine

`StretchBlt`'s 1:1-path integer arithmetic under extreme negative
coordinates (no evidenced game ever passes non-small blit coordinates);
`MidiState::sessions`'s unbounded growth under repeated `MCI_OPEN` without
`MCI_CLOSE` (both games always pair open/close per track);
`GetSystemPaletteEntries`/`_lread` trusting caller-declared sizes (matches
real Win32's own contract; real call sites always pass matching
buffer/size pairs); `GetTickCount`'s 32-bit wraparound after ~49.7 days
(correct, faithful Win32 emulation); window-registry globals (unsynchronized
but only ever touched from the single main/game thread, consistent with
the documented single-window architecture); `ShowCursor`'s unbounded
signed counter and the MIDI/timer-ID counters (would require billions of
calls in one process to matter).

## Architectural Risk Analysis

### R1: Same finding as C2 (`PostMessageA`-under-mutex at a second site) — see Cross-Validated Findings below.

### R2 (MEDIUM-HIGH): AddressSanitizer's leak detection is blanket-disabled for the entire `ctest` suite, not just for SDL3's own allocations

`CMakeLists.txt:651-656`, documented at `docs/cmake-options.md:96-108`. The
rationale (SDL3's own long-lived global allocations read as false-positive
"leaks" under LeakSanitizer) is real and the trade-off is deliberately
documented — but the mechanism is a *blanket*
`ASAN_OPTIONS=detect_leaks=0` applied via CTest's `ENVIRONMENT` property to
**every** test, not a scoped suppression of SDL3's specific known
allocations (e.g. an `LSAN_OPTIONS=suppressions=<file>` list naming SDL3's
allocation stacks specifically).

This means running the documented, standard workflow (`ctest` in a
`FREE_API_SANITIZE=address` build tree) can **never** catch a real leak in
free-api's own code, only in SDL3's — and this isn't hypothetical: earlier
in this same session's work, a genuine resource leak in a test
(`TestGetSetPixelRoundTripOnMemoryDcWithSelectedBitmap`, missing
`DeleteDC`/`DeleteObject` cleanup) went undetected through every prior
`ctest`-based ASan verification pass across multiple sessions and was only
found by manually running `./test_gdi_regressions` directly (bypassing
ctest's `ENVIRONMENT` property, which is not part of any documented
workflow). A scoped LSAN suppressions file for SDL3's specific allocation
stacks would restore real leak-detection coverage for free-api's own code
while still avoiding the SDL3 false positives.

### Confirmed fine

The MIDI/message-queue static-destruction-order race (fixed,
`GetMidiState()`); `NormalizeMidiPath`'s prefix-duplication (fully gone
post-`TASK-24H-0706`); `FreeApiDiagSnapshot`'s and `PeekMessageA`'s
multi-mutex sequences (both sequential, never nested — no lock-order
risk); `FreeApiMmTimerBridge`'s already-correct capture-under-lock/act-
after-release pattern; no other `static` namespace-scope object in the
codebase has a non-trivial destructor with a cross-TU dependency (only one
`std::thread` exists in the whole codebase, and it's the already-fixed
`GetMidiState()` one).

## Cross-Validated Findings

The following were independently found by more than one of the five
parallel reviews, with no shared context beyond this conversation's
history — treated as materially stronger evidence than a single review's
finding alone:

| Finding | Found independently by | Severity |
|---|---|---|
| Pitch-overflow fix (`TASK-24H-1231`) was scoped too narrowly — two sibling call sites in `src/internal/FreeApiGdi.cpp` still have the unguarded `int * int` multiplication | Correctness (C1), Edge Cases (E1) | MEDIUM |
| `PostMessageA`-under-mutex fix (`TASK-24H-1233`) was scoped too narrowly — `MidiMusicSendCommand`'s `MCI_PLAY` no-SoundFont branch still calls it while holding `GetMidiState().mtx` | Correctness (C2), Architectural Risk (R1) | MEDIUM |
| `_getcwd` doesn't validate `maxlen`, but has zero call sites in either target game | Memory Safety, Edge Cases (E3) | LOW (not actionable) |

## Proposed Tasks

Prioritized by the audit's own severity assessment, translated to this
project's P0–P3 scale (P2 reserved for a real fix to a pattern already
proven to matter enough to fix once; P3 for lower-likelihood or purely
defensive items):

1. **(P2)** Apply the `TASK-24H-1233` fix pattern (capture notify data
   under the lock, post after releasing it) to `MCI_PLAY`'s no-SoundFont
   branch in `MidiMusicSendCommand` — the second, real, test-exercised
   `PostMessageA`-under-mutex call site (C2/R1).
2. **(P2)** Fix `ResolveJoystick`'s bounds check to compare the same value
   it uses to index, closing a genuine (if currently unreachable)
   out-of-bounds-read hazard (E2).
3. **(P2)** Add a scoped LeakSanitizer suppressions file for SDL3's known
   allocations, replacing the blanket `detect_leaks=0`, to restore real
   leak-detection coverage for free-api's own code in the standard `ctest`
   workflow (R2).
4. **(P3)** Apply the `TASK-24H-1231` pitch-overflow-cast fix to its two
   sibling call sites in `src/internal/FreeApiGdi.cpp` (C1/E1).
5. **(P3)** Consider narrowing `PushMessage`'s coalescing-scan lock scope
   or documenting the self-limiting assumption explicitly (P1).
6. **(P3)** Change `g_nextTimerId.fetch_add(1)` to `memory_order_relaxed`,
   matching every other counter atomic in the codebase (P2 performance
   finding).

*No currently-live game behavior is broken by any finding above. Items 1
and 2 are the two most worth doing soon: item 1 because it's the exact
pattern this project already decided was worth fixing once, at a second
site with real test coverage proving it's reachable; item 2 because it's a
genuine defect in validation logic, not just a hardening gap, even though
today's real call sites don't trigger it.*
