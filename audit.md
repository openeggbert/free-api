# Free API — Deep Audit: Correctness, Performance, Memory, Edge Cases, Risk

**Date:** 2026-07-09
**Scope:** `src/*.cpp` + `include/*.h` (the WinAPI/Win32 reimplementation), as consumed by the two target games `../free-eggbert` and `../planetblupi`.
**Baseline:** commit `fdc38bf` on `develop` (178 of 183 `TASK-24H-*` backlog tasks closed; see `plan.md`/`NEXT.md` for that history).

This is a fresh, independent, skeptical audit — not a re-derivation of `plan.md`/`docs/out-of-scope.md`/`docs/audit-24h-free-api.md`'s existing conclusions. Where this audit's findings converge with an already-documented, deliberately-accepted decision, that is noted explicitly rather than re-litigated. Every finding below was verified against the actual current source, not assumed from prior documentation. **No files were modified as part of this audit** — it is a read-only report.

## 1. Executive Summary

The codebase is in good shape for its stated scope: a narrow, source-level WinAPI compatibility shim for exactly two legacy games, not a general Win32 reimplementation. The existing test suite (26 tests, clean under both ThreadSanitizer and AddressSanitizer) already catches a meaningful class of concurrency bugs, and this project's own history shows a track record of finding and fixing real issues (a use-after-free in the timer bridge, a data race on a debug flag, several dead-code/stale-doc corrections).

This audit found **no new critical, immediately-exploitable bugs reachable by either target game's real, current code paths**. The most significant findings are:

- **One HIGH-severity architectural risk**: an unenforced static-destruction-order dependency between the MIDI mixer thread and the message queue, currently safe only by accident of link order (§7, Finding R1).
- **One convergent, high-confidence correctness/memory finding**, independently flagged by three of the five reviews: GDI handle deletion (`DeleteDC`/`DeleteObject`/`FreeApiDestroySurfaceDC`) has no double-free protection — a freed handle's validation ("magic number") is never cleared, so a double-delete can pass validation against already-freed memory (§3 Finding C1, §5 Finding M1).
- **One convergent finding across three independent reviews**: `wsprintfA`'s fixed 1024-byte internal cap is larger than the 256-byte stack buffers all three real call sites actually declare — currently safe only because the fixed format string can't produce enough output to matter, but a latent footgun for any future call site (§3 Finding C4, §5 Finding M3, §6 Finding E1).
- Everything else is lower-severity, mostly defensive-hardening territory not reachable by either game's real, current behavior — cataloged below for completeness and future reference, not because the project is at risk today.

**No performance bottleneck was found in any real hot path.** The one actionable performance item is a small, per-frame heap allocation with an existing in-tree pattern that already solves the same problem elsewhere (§4, Finding P1).

## 2. Methodology

Five independent, parallel investigations were run against the current source, each with a distinct lens:

1. **Correctness** — does the real, live implementation match the semantics either game's actual behavior depends on; logic bugs, incorrect bit-packing, silent-failure paths, type-confusion risk.
2. **Performance** — hot-path allocation, lock contention, algorithmic complexity, real-time-safety of the audio render path.
3. **Memory safety** — use-after-free, double-free, buffer overflows, integer overflow feeding allocation sizes, resource leaks, thread-safety of global state.
4. **Edge cases / extreme situations** — adversarial/boundary inputs (zero/negative/huge dimensions, malformed files, resource exhaustion, rapid repeated calls, encoding edge cases).
5. **Architectural risk** — systemic risk beyond any one function: locking architecture, threading model, external dependency pinning, scope-policy enforcement, structural test-coverage limits.

Each review was scoped to the actual, live public surface (cross-referenced against `docs/supported-apis.md`/`docs/public-surface-audit.md`'s real usage map), not every declared symbol regardless of use. Findings are reported with `file:line` evidence, a concrete failure scenario, a severity assessment weighted by real reachability from either game's actual code, and a suggested fix direction (not implemented).

## 3. Correctness Analysis

### C1. Double-free / use-after-free risk in GDI object destructors — no dangling-handle protection
**Severity: HIGH (systemic pattern; currently only theoretically triggerable)**
**Files:** `src/wingdi_dc.cpp` (`DeleteDC`, `FreeApiDestroySurfaceDC`), `src/wingdi_bitmap.cpp` (`DeleteObject`); validation logic in `src/internal/FreeApiGdi.cpp` (`AsCompatDC`/`AsCompatBitmap`).
*Independently found by both the correctness and memory-safety reviews.*

`AsCompatDC`/`AsCompatBitmap` validate a handle by dereferencing it and checking an in-struct magic number. None of the three delete functions clear that magic number before calling `delete`. Freed heap memory is not guaranteed to be overwritten immediately — if a caller calls `DeleteObject(bmp)` (or `DeleteDC`/`FreeApiDestroySurfaceDC`) twice on the same handle before the memory is reused for something else, the second call's magic-number check can still read the stale (already-freed) tag as valid, and the function proceeds to `delete` already-freed memory a second time — genuine heap corruption, not a fail-safe rejection.

**Concrete scenario:** `HDC h = CreateCompatibleDC(nullptr); DeleteDC(h); DeleteDC(h);` — no existing test exercises double-deletion of the same handle (the existing test suite only covers "wrong-kind handle," not "already-freed handle").

**Relationship to existing documented decisions:** `docs/out-of-scope.md`/`NEXT.md` already record a deliberate decision *not* to fix `FreeApiDestroySurfaceDC`'s behavior on a **garbage** (never-valid) pointer with "a general handle-validation/table framework." This finding is a different, narrower case: a handle that *was* valid and has since been freed. The suggested fix below does not require the previously-declined general framework — it is a small, local change to the three delete functions.

**Suggested fix direction:** set the `magic` field to zero (or a distinct "tombstone" value) immediately before `delete` in all three functions. This does not require a handle table or generation-counter framework — it closes the double-free specifically, at near-zero cost, without expanding scope.

### C2. `CompatDC::selectedBitmap` can become a dangling pointer if the selected bitmap is deleted first
**Severity: MEDIUM (not confirmed reachable by either game's real call order, but not confirmed safe either)**
**Files:** `src/wingdi_bitmap.cpp` (`DeleteObject`), `src/wingdi_dc.cpp` (`SelectObject`).

`DeleteObject` never scans live `CompatDC` instances to clear a `selectedBitmap` pointer that references the bitmap being deleted. If a caller deletes a bitmap while it is still selected into a DC (rather than deselecting or deleting the DC first), that DC's `selectedBitmap` becomes dangling; a subsequent `GetPixel`/`SetPixel`/`StretchBlt`/`GetObjectA` call through that DC would read or write freed memory. Real Win32 has partial protection against exactly this case (a selected object generally cannot be deleted). This has not been confirmed as reachable by either target game's actual delete ordering — worth a targeted verification pass, not an emergency fix.

### C3. `_findfirst`'s directory iteration can throw an uncaught exception on a real, live call path
**Severity: MEDIUM (real, live call site: free-eggbert's design-mission picker)**
**File:** `src/crt_io.cpp` (the `directory_iterator` loop).

The `std::filesystem::directory_iterator(dir, ec)` constructor overload only makes the *open* non-throwing. The range-based `for` loop's implicit `operator++()` on each step still uses the throwing increment, not the `ec`-reporting form. A permission error, or a file removed/changed mid-iteration, throws `std::filesystem::filesystem_error` uncaught — with no exception handling anywhere in this codebase, this would very likely propagate to `std::terminate()`, crashing the whole game process, rather than `_findfirst` returning `-1` as its documented failure contract requires. Live call site: free-eggbert's design-mission file picker (`event.cpp:4741-4747`).

**Suggested fix direction:** wrap the loop body in try/catch, or switch to `it.increment(ec)` (the non-throwing increment form) inside a manual iteration loop.

### C4. `wsprintfA`'s internal write cap (1024 bytes) exceeds the real call sites' actual buffer sizes (256 bytes)
**Severity: MEDIUM (currently safe; latent footgun) — see also §5 Finding M3 and §6 Finding E1 (convergent across three independent reviews)**
**File:** `src/winuser_message.cpp` (`wsprintfA`).

`wsprintfA` calls `vsnprintf(lpOut, 1024, ...)`, matching real Win32 `wsprintfA`'s own historically-unsafe contract (no length parameter). All three real call sites (`free-eggbert/src/soundbass.cpp:142`, `sound.cpp:117`; `planetblupi/src/sound.cpp:99`) declare a 256-byte stack buffer — 768 bytes smaller than what `wsprintfA` will happily write into if given the chance. **Currently safe** only because the fixed format string (`"Data1 : %d, dwdata: %d, pFile: %d"`) cannot realistically exceed ~60 bytes. A future call site (or a hand-edited/modded build of either game) with a longer format string and a small buffer would get a genuine stack-buffer overflow.

Also noted: both real call sites pass **pointers** where the format string expects `int` (`%d` against `pData1`/`pFile`) — technically undefined behavior per the C standard, though harmless in practice on this project's actual target ABI (SysV x86-64, where pointer and `int` arguments share the same register class). Inherited from the original 1998-era game source, not introduced by free-api.

**Suggested fix direction:** since the real Win32 API this mirrors has no size parameter to safely bound against (matching the deliberate compatibility goal), the only honest mitigation is documentation: a permanent, prominent comment/doc note that any *future* `wsprintfA` call site must keep its expected output well under its own actual buffer size, not just under 1024 bytes — this cannot be fixed in the implementation without breaking API compatibility.

### C5. `ClientToScreen`/`ScreenToClient` have an asymmetric zero-guard on their scaling division
**Severity: LOW-MEDIUM (not currently reachable; inconsistent defensive posture)**
**File:** `src/winuser_cursor.cpp`.

`ScreenToClient` explicitly guards its coordinate-scaling division against `width`/`height` being zero. `ClientToScreen`'s mirror-image division has no equivalent guard. Currently unreachable because `CreateWindowExA` always populates a positive width/height (defaulting to 640×480 if given a non-positive value) — but the two functions doing the same class of computation should have matching defensive postures, and a latent SIGFPE risk exists if that invariant is ever broken by a future change (e.g., an incomplete `MoveWindow` logical-size update, already documented elsewhere as a confirmed-harmless gap).

### C6. `SetTimer`'s timer-ID map is keyed globally, not per-window
**Severity: LOW (architectural; confirmed harmless under the project's current single-window design)**
**File:** `src/internal/FreeApiTimers.hpp`, `src/winuser_timer.cpp`.

Real Win32 scopes a timer ID to the window that created it (the same numeric ID can be reused by different windows without conflict). Here, `g_winTimers` is a single global map keyed by ID alone — a second `SetTimer` call with the same ID from a *different* window would silently overwrite the first window's timer entry, permanently stopping its `WM_TIMER` delivery with no error. Confirmed harmless today only because this project's own established single-live-window design (documented in `docs/out-of-scope.md`) means two windows never coexist. Worth a one-line doc note alongside the existing single-window-assumption documentation, not a code change.

### C7. `StretchBlt`'s two internal paths (1:1 fast path vs. scaled path) handle an out-of-range source rect inconsistently
**Severity: LOW (defensive-only; not proven reachable by either game)**
**File:** `src/wingdi_blit.cpp`.

For the same nominal "source rect far outside the source bitmap" input, the 1:1 fast path clips to a no-op and draws nothing; the scaled path clamps per-pixel coordinates to the nearest edge and draws a stretched/duplicated edge-pixel artifact. Both games always pass in-bounds source rects derived from real bitmap dimensions, so this is not an active bug — but the two paths disagree on the contract for the same edge case, worth documenting as an accepted inconsistency (or unifying) rather than leaving unstated.

### C8. `GetObjectA` always returns `sizeof(BITMAP)` even when the caller's buffer was smaller
**Severity: INFORMATIONAL (correctness deviation, not a memory-safety issue)**
**File:** `src/wingdi_bitmap.cpp`.

The `memcpy` itself is correctly bounded to `min(c, sizeof(BITMAP))` — no overflow. But the *return value* is unconditionally `sizeof(BITMAP)` even when fewer bytes were actually copied (real Win32 returns the actual byte count written). A caller relying on the return value to know how much of the struct is valid would be misled. No evidence either game does this, but it's a real, easily-fixed deviation from the documented contract.

### C9. Process-lifetime monotonic counters (MIDI session IDs, timer IDs, file handles)
**Severity: INFORMATIONAL — not a practical risk.**

All relevant counters are wide enough (32/64-bit) that wraparound within any real play session is not reachable — noted only for completeness, no action warranted.

## 4. Performance Analysis

No real hot-path bottleneck was found. All findings below are small, low-risk, low-priority.

### P1. Per-frame heap allocation in `PeekMessageA`'s WM_TIMER-generation path
**Severity: MEDIUM (clean, near-zero-risk fix; not a measured bottleneck)**
**File:** `src/winuser_message.cpp`.

A `std::vector<MSG> pendingTimers` is freshly stack-declared on every `PeekMessageA` call where the queue was empty — essentially every frame for planetblupi, whose live frame-pump is `SetTimer`/`WM_TIMER`. When the timer has elapsed (the common case), a `push_back` triggers one small heap allocation, freed a few lines later. Not unbounded (the timer map is typically size 1), but avoidable: `src/wingdi_blit.cpp`'s `StretchBlt` already uses the right pattern (a `thread_local` reusable vector) for exactly this class of problem elsewhere in the same codebase.

**Suggested fix direction:** apply the same `thread_local` (or otherwise call-scope-persistent) reusable-vector pattern already established in `StretchBlt`.

### P2. MIDI mixer thread holds its state mutex across an unrelated `PostMessageA` call
**Severity: LOW**
**File:** `src/MidiMusic.cpp`.

The lock scope guarding session lookup + audio rendering also wraps the `PostMessageA` call on song completion, which internally acquires a *different* mutex and does unrelated bookkeeping (message-queue coalescing scan, diagnostics). Not a deadlock (different mutex, and no evidence of reverse-order acquisition elsewhere — see also §7 Finding R2), just an avoidably-widened critical section that makes `MCI_OPEN`/`MCI_CLOSE` wait slightly longer than necessary.

**Suggested fix direction:** capture the notify target/ID locally, release the MIDI mutex, then call `PostMessageA` outside the lock scope.

### P3. `g_debugInput` is read with the default (sequentially-consistent) memory order on every message
**Severity: LOW/INFORMATIONAL**
**File:** `src/internal/FreeApiMessageQueue.cpp`/`.hpp`.

A plain `bool` conversion of `std::atomic_bool g_debugInput` defaults to `memory_order_seq_cst`. On x86/x64 (the primary desktop target) this is free (compiles to a plain load); on ARM/Android (which this codebase explicitly supports) a seq_cst load requires a memory barrier, checked on every message. `memory_order_relaxed` would be strictly sufficient (it's a debug on/off flag, not synchronizing any other data) and costs nothing to change.

### P4. `StretchBlt`'s scaled path writes destination pixels byte-by-byte instead of as one 32-bit store
**Severity: LOW/INFORMATIONAL — confirmed low-impact given actual usage**
**File:** `src/wingdi_blit.cpp`.

Since alpha is unconditionally forced to 255, the three-byte-plus-one-byte write could be a single 32-bit store. Confirmed this path is only reachable by planetblupi's minimap (a small, infrequent render relative to the main viewport, which already uses a fast `memcpy` path for the common 1:1 case) — unlikely to be measurable in practice.

### Confirmed fine (no action needed)
- Diagnostics gate checks (`FreeApiDiagnosticsEnabled()`/`FreeApiGdiDebugEnabled()`) are properly cached after the first `getenv()` call — not a hot-path cost.
- All handle-keyed global containers are `std::unordered_map` (O(1) average lookup), not linear scans.
- The MIDI mixer thread's render buffer is allocated once outside its render loop — zero per-block allocation. `SDL_PutAudioStreamData` (the potentially-blocking call) happens outside the MIDI mutex's lock scope — correct design.
- `FreeApiMmTimerBridge` invokes the user's timer callback outside the timer-map lock — correct, avoids blocking timer creation/deletion for the callback's duration.
- Message-queue coalescing scans are technically O(n) but bounded by the queue's actual (small, coalesced) depth given the single-window architecture — not a real scalability concern.

## 5. Memory Safety Analysis

### M1. Double-free / use-after-free in GDI handle deletion
**See §3 Finding C1 — independently found by both the correctness and memory-safety reviews; not repeated here.**

### M2. Unchecked `int` multiplication in `CreateBitmap`'s pitch computation
**Severity: LOW-MEDIUM (theoretical only) — see also §6 Finding E3 (convergent)**
**File:** `src/wingdi_bitmap.cpp`.

`bitmap->pitch = nWidth * 4;` is a plain `int * int` with no overflow guard — inconsistent with the very next line, which correctly casts to `size_t` before multiplying to size the actual pixel buffer. If `nWidth` exceeds roughly 536 million, this specific multiplication overflows (undefined behavior, typically wraps to a negative/garbage value) while the buffer itself is still sized correctly — a corrupted `pitch` would then drive incorrect (not necessarily out-of-bounds, but definitely wrong) row-offset math in every downstream pixel access. Not reachable by either game's real, fixed-size bitmap dimensions.

**Suggested fix direction:** cast to `size_t`/`int64_t` before the multiplication, matching the pattern already used one line below for the buffer size itself.

### M3. `wsprintfA`'s buffer-size mismatch
**See §3 Finding C4 — independently found by three reviews (correctness, memory, edge-case); not repeated here.**

### M4. Unhandled `std::bad_alloc` mid-construction leaks and aborts
**Severity: LOW (theoretical — genuine OOM or an absurd caller-supplied scale target only)**
**File:** `src/internal/FreeApiGdi.cpp` (`CreateCompatBitmapFromSurface`).

If the pixel-buffer `resize()` call throws `std::bad_alloc`, the already-allocated `CompatBitmap` and the just-converted SDL surface both leak, and since this codebase has no exception handling anywhere, the exception propagates uncaught to `std::terminate()`. Only reachable under genuine memory exhaustion or a maliciously huge `LoadImageA` scale target — not reachable via either game's real, bounded asset set. No action needed unless untrusted input is ever routed through this path.

### Confirmed fine (no action needed)
- **No use-after-free found in the MIDI mixer-thread/session-vector interaction.** The mixer thread correctly holds its mutex for the entire find-session→render→notify sequence in one uninterrupted lock scope (the fix for a real, previously-existing bug, per the code's own comment) — verified this covers the current code, not just the historically-fixed case.
- **No lock-ordering deadlock found**: verified `DispatchMessageA` releases the message-queue mutex before invoking any `WndProc`, so no path re-enters with two of this codebase's mutexes held in conflicting order (see also §7 Finding R2 for the *latent*, not-yet-triggered version of this risk).
- **Window-registry globals** (`g_registeredClasses`, `g_windowProcedures`, `g_freeApiWindowStates`, `g_windowsById`, `g_focusWindow`) have no mutex protection, but every read/write site traced is reachable only from the main thread today — the same risk category as the already-documented `TASK-24H-0707` (`_lopen`'s unsynchronized handle table): a latent risk if a future change ever calls these from a background thread, not a live bug.
- No new resource leaks, buffer overflows, or null-dereferences found beyond what's cataloged above.

## 6. Edge Case / Extreme Situation Analysis

### E1. `wsprintfA`'s buffer-size mismatch under an adversarial/future format string
**See §3 Finding C4 — independently found by three reviews; not repeated here. This was the highest-severity item flagged by the edge-case review specifically because it is a genuine stack-overflow shape, even though currently unreachable.**

### E2. `g_activeTimerIds` is unsynchronized *and* dead code (write-only, never read)
**Severity: MEDIUM (as a code-quality/latent-UB issue), effectively zero real risk today**
**File:** `src/internal/FreeApiTimers.cpp`, `src/winmm.cpp`.

A plain `std::unordered_set<UINT>` with no mutex is inserted into by `timeSetEvent` and erased from by `timeKillEvent`. Exhaustively grepped: **it is never read anywhere** — the diagnostics snapshot's active-timer count is actually computed from the two properly-mutex-protected timer maps, not this set. Concurrent unsynchronized `insert`/`erase` on an `unordered_set` is undefined behavior if ever triggered from two threads at once (theoretically possible if a timer callback ever reentrantly called `timeSetEvent`/`timeKillEvent` — confirmed neither game's real callback does this today). Since the data is never consulted, the simplest and safest fix is deletion, not synchronization.

**Suggested fix direction:** remove `g_activeTimerIds` entirely — it is confirmed dead code carrying a latent (currently unreachable) thread-safety hazard for no benefit.

### E3. `CreateBitmap`'s pitch computation, adversarial framing
**See §5 Finding M2 — independently found by both the memory-safety and edge-case reviews; not repeated here.**

### E4. No message-queue purge on `DestroyWindow`
**Severity: LOW (defensive-only; confirmed safe under current single-window usage)**
**File:** `src/winuser_window.cpp`.

`DestroyWindow` never drains already-queued messages still carrying the destroyed window's `HWND` (which is the raw `SDL_Window*` pointer value) from the message queue. If a future second window happened to receive the same freed memory address (a classic allocator pointer-reuse hazard) and a stale queued message for the destroyed window were still pending, it could theoretically be misdelivered to the new window. **Verified safe today**: `DispatchMessageA`'s fallback only activates when the window-procedure map is non-empty; after the sole window is destroyed, the map is empty, so a stale post-destroy message is silently dropped, not misdelivered — and neither game ever creates a second window after destroying the first.

### E5. Non-coalesced message types have no queue-depth bound
**Severity: LOW (structural; not a demonstrated trigger)**
**File:** `src/internal/FreeApiMessageQueue.cpp`.

Only `WM_MOUSEMOVE` and `WM_TIMER` are coalesced (a documented, deliberate decision). Every other message type appends to the queue with no cap. No evidence either game's real call patterns (human-input-rate-bounded keyboard/mouse events) can trigger unbounded growth — noted as a structural gap without a demonstrated real trigger, not an active bug.

### Confirmed fine (no action needed)
- `CreateWindowExA` correctly clamps non-positive width/height to sane defaults; extreme-but-positive values correctly fall through to SDL's own failure path.
- `LoadStringA` correctly uses a size-bounded write (`snprintf` with the caller's actual `cchBufferMax`) — a useful positive contrast to `wsprintfA`'s structural inability to do the same (no size parameter exists in that API's real Win32-matching signature).
- `_lread` correctly uses `fread` short-read semantics — a truncated file yields a short read count, never an overrun.
- Malformed-file robustness (corrupt BMP/MIDI/SoundFont) is correctly delegated to the underlying parsers (`SDL_LoadBMP`, `tml_load_filename`, `tsf_load_filename`) with `NULL`-result checks present at every free-api call site; the parsers' own internal robustness is a third-party concern outside this audit's scope.
- `timeSetEvent`/`timeKillEvent`'s core use-after-free-avoidance design (packing the timer ID into the SDL userdata slot rather than a live map-node pointer) is sound under a fresh read.

## 7. Architectural Risk Analysis

### R1. Static destruction order is unenforced between the MIDI subsystem and the message queue
**Severity: HIGH (latent — currently safe only by accident of link order)**
**Files:** `src/MidiMusic.cpp` (`static MidiState g_midi;`), `src/internal/FreeApiMessageQueue.cpp` (`g_messageQueue`/`g_messageQueueMutex`).

These are ordinary namespace-scope statics in different translation units. C++ does not guarantee cross-translation-unit static destruction order — it depends on link order, which the standard leaves unspecified and which can silently change with a toolchain upgrade, an added source file, or a build-system refactor.

The MIDI mixer thread holds the MIDI-state mutex while calling `PostMessageA` (which internally locks the message-queue mutex and touches the message queue). `MidiState`'s destructor correctly stops and joins the mixer thread before releasing its own resources — so if `g_midi` is destroyed **before** `g_messageQueue`, teardown is safe (the thread is guaranteed fully stopped before the queue could be destroyed). But if link order ever puts the message queue's destruction **first**, the mixer thread — not yet told to stop — could call into an already-destroyed mutex/deque during process teardown.

**Concrete scenario:** a toolchain upgrade, a new source file, or a build-system reorder silently flips link order; the game hangs or crashes with heap corruption only at process exit, intermittently — a very difficult failure to reproduce or bisect, since it depends on build configuration, not runtime input.

**Suggested fix direction:** convert `g_midi` (and ideally the message-queue globals) to function-local statics (Meyer's singletons), whose destruction order C++ *does* guarantee (reverse of first-use order) — or add an explicit, ordered shutdown routine that joins the mixer thread before any other global teardown can run.

### R2. Latent lock-order dependency: MIDI mutex → message-queue mutex
**Severity: MEDIUM (currently safe; not structurally enforced)**
**Files:** `src/MidiMusic.cpp`, `src/internal/FreeApiMessageQueue.cpp`.

Beyond the destruction-order issue above, the *live* nested-lock pattern (the mixer thread holds the MIDI mutex while acquiring the message-queue mutex via `PostMessageA`) only avoids deadlock because no other code path currently acquires these two mutexes in the reverse order — verified `DispatchMessageA` releases the queue mutex before invoking any `WndProc`, so a game's message handler calling into MIDI functions never re-enters with the queue mutex still held. This holds today but is not enforced by any comment, assertion, or test; a future change that posts a message while already holding the MIDI mutex from a different call path, or that adds queue-processing logic inside a MIDI-locked region, would introduce a real deadlock with no existing test catching it.

**Suggested fix direction:** document the required lock order explicitly next to each mutex's declaration, and/or restructure `PostMessageA` calls to happen after releasing the MIDI mutex (see also §4 Finding P2, which suggests the same change for a performance reason — the fixes converge).

### R3. No automated enforcement of the project's core scope policy
**Severity: MEDIUM (by-design tradeoff, worth naming explicitly)**

`docs/scope.md`'s citation rule ("every public API must cite a real usage site") is enforced entirely by human/AI diligence when writing `plan.md` entries. There is no CI check, lint rule, or test verifying that a new public declaration in `include/*.h` has corresponding evidence — `test_header_compile.cpp` only proves headers compile, not that they're scoped. A future contributor (human or AI, possibly in a rushed session) could add a new public function "because it seemed useful," and nothing in the build or test suite would flag it.

**Suggested fix direction:** a lightweight script-mode CTest (matching the existing `check_no_hardcoded_paths` pattern) that diffs the public declaration list in `include/*.h` against a maintained baseline and fails loudly on any new, unlisted symbol.

### R4. Silent message loss if the sole live window is ever destroyed while messages are still in flight
**Severity: LOW-MEDIUM (consistent with the documented single-window design; would degrade silently if that assumption is ever broken)**

If `g_windowProcedures` becomes empty (the sole window destroyed) while the MIDI mixer thread or SDL timer thread is still posting messages, those messages are enqueued but then silently dropped by `DispatchMessageA`'s fallback — no window to route to, and nothing logs or reports this. Consistent with the project's deliberate single-window assumption (documented elsewhere), but it means any future bug that destroys the window slightly early degrades to silent inaction rather than a diagnosable error.

**Suggested fix direction:** a diagnostics-only counter for "dispatched with zero registered windows" would make this failure mode observable if it's ever needed; low priority given the confirmed single-window usage pattern.

### R5. No minimum-version pin on SDL3/SDL3_image/SDL3_mixer
**Severity: LOW-MEDIUM**

`CMakeLists.txt`'s `find_package(SDL3/SDL3_image/SDL3_mixer REQUIRED)` calls specify no minimum version — free-api links against whatever the consuming game or system provides, with no build-time compatibility guard. (The two vendored third-party libraries, TinySoundFont and TinyMidiLoader, are directly committed source and therefore effectively pinned; SDL3 itself is not.) A future SDL3 release changing an edge-case behavior this project's tests don't cover could surface only on a machine with a different SDL3 version than was tested against, with no configure-time warning.

**Suggested fix direction:** add a version floor to the `find_package` calls matching whatever SDL3 version this project's testing has actually been validated against.

### R6. Structurally untestable coverage ceiling
**Severity: LOW (already partially acknowledged via the 4 human-playtest-only tasks) — stated here as an explicit, permanent architectural fact, not a gap to close**

All 26 automated tests run headlessly against synthetic fixtures — none exercise either game's real shipped assets (not part of this repository), real GPU rendering, or real audio output. A regression in real-asset decoding, real-display color/scaling behavior, or real-audio timing could pass 26/26 and still be broken for an actual player. This is why 4 backlog tasks are explicitly human-playtest-only; this finding names it as a structural ceiling of the current test architecture, not something more automated testing could ever close.

### Confirmed fine (no action needed)
- `CMAKE_PROJECT_NAME` auto-detection uses exact string matching against two specific project names; a collision with an unrelated third project is unlikely, and even then the existing fail-loud `.rc`-file-existence check (re-verified this session) catches a real mismatch loudly.
- Video/audio subsystem init failure degrades gracefully at the free-api layer in both cases (clean `NULL` return for video; a documented "silent success" latch for audio) — no cascading failure into an unrelated subsystem.
- The threading model is small and fully enumerable (main thread, SDL's internal timer thread, one MIDI mixer thread) — not the kind of ad-hoc thread sprawl that tends to accrete bugs over time.

## 8. Cross-Validated Findings

The following were independently flagged by more than one of the five parallel reviews, which is a meaningfully stronger signal than a single review's finding:

| Finding | Reviews that found it | Severity |
|---|---|---|
| GDI handle deletion has no double-free protection (magic number never cleared) | Correctness, Memory | HIGH |
| `wsprintfA`'s 1024-byte cap exceeds real call sites' 256-byte buffers | Correctness, Memory, Edge-case | MEDIUM (currently safe) |
| `CreateBitmap`'s pitch computation is an unchecked `int` multiplication | Memory, Edge-case | LOW-MEDIUM (theoretical) |
| `PostMessageA` called while holding the MIDI mutex is both a latent lock-order risk and an avoidable performance cost | Performance, Risk | MEDIUM/LOW |

## 9. Proposed Tasks

Ordered roughly by severity × ease of fix. None of these have been implemented — this is a proposal list only, matching every item's evidence and suggested direction from the sections above.

1. **Clear the GDI handle "magic number" before `delete` in `DeleteDC`, `DeleteObject`, and `FreeApiDestroySurfaceDC`.** Closes a genuine double-free path at near-zero cost and without the general handle-validation framework this project has already, correctly, declined to build for the unrelated "garbage pointer" case. *(§3 C1 / §5 M1 — HIGH, cross-validated)*
2. **Document `wsprintfA`'s buffer-size hazard prominently** (doc comment + `docs/out-of-scope.md`/`docs/supported-apis.md` note) so any future call site is written with awareness that the function will happily write up to 1024 bytes regardless of the caller's actual buffer size. No implementation fix is possible without breaking Win32 API compatibility (the real function has the same hazard). *(§3 C4 / §5 M3 / §6 E1 — MEDIUM, cross-validated by three reviews)*
3. **Convert `CreateBitmap`'s `pitch` computation to use a `size_t`/`int64_t` cast before multiplying**, matching the pattern already used one line below for the pixel-buffer size itself. Small, mechanical, no behavior change for any real input. *(§5 M2 / §6 E3 — LOW-MEDIUM, cross-validated)*
4. **Investigate and fix (or explicitly document as accepted) the static-destruction-order dependency between `g_midi` and the message-queue globals** — convert to function-local statics, or add an explicit ordered-shutdown hook. This is the single highest-severity finding in this audit precisely because its failure mode (intermittent crash/hang only at process exit, triggered by build configuration rather than runtime input) would be exceptionally hard to diagnose if it ever manifests. *(§7 R1 — HIGH)*
5. **Move the `PostMessageA` call in the MIDI mixer thread's notify path outside the MIDI mutex's lock scope.** Closes both the latent lock-order risk and the avoidable critical-section widening in one change. *(§4 P2 / §7 R2 — MEDIUM/LOW, cross-validated)*
6. **Remove the dead, unsynchronized `g_activeTimerIds` set.** It is never read anywhere; deleting it removes a latent (if currently unreachable) undefined-behavior risk for zero functional cost. *(§6 E2 — MEDIUM as a code-quality issue, effectively zero real risk)*
7. **Wrap `_findfirst`'s directory-iteration loop to handle (or explicitly convert away from) the throwing `directory_iterator::operator++`.** This is the one finding in this audit reachable via a real, live call site (free-eggbert's design-mission picker) under a realistic failure condition (permission error, concurrent file removal). *(§3 C3 — MEDIUM)*
8. **Apply `StretchBlt`'s existing `thread_local` reusable-vector pattern to `PeekMessageA`'s WM_TIMER-generation path**, eliminating a small, avoidable per-frame heap allocation. *(§4 P1 — MEDIUM priority for a low-risk, low-effort fix; not a measured bottleneck today)*
9. **Add a version floor to the `find_package(SDL3/SDL3_image/SDL3_mixer ...)` calls** in `CMakeLists.txt`, matching whatever version this project has actually validated against. *(§7 R5 — LOW-MEDIUM)*
10. **Add a lightweight, script-mode CTest that fails loudly on any new, unlisted public declaration in `include/*.h`**, giving the project's core scope policy the same kind of automated backstop `check_no_hardcoded_paths` already gives the "no machine-specific absolute paths" policy. *(§7 R3 — MEDIUM, process/maintenance risk rather than a code bug)*
11. **Fix `GetObjectA` to return the actual number of bytes written, not always `sizeof(BITMAP)`.** Small, mechanical correctness fix; no evidence either game depends on the current (wrong) behavior, so low urgency. *(§3 C8 — INFORMATIONAL/LOW)*
12. **Add a symmetric zero-guard to `ClientToScreen`'s scaling division**, matching `ScreenToClient`'s existing guard, for defensive consistency even though not currently reachable. *(§3 C5 — LOW-MEDIUM)*
13. **Document `SetTimer`'s globally-keyed (not per-window) timer-ID map** as a confirmed-harmless simplification, alongside the existing single-window-assumption documentation. *(§3 C6 — LOW, documentation only)*
14. **Document (or unify) `StretchBlt`'s inconsistent out-of-range-source-rect handling** between its 1:1 and scaled code paths. *(§3 C7 — LOW, documentation or unification, contributor's choice)*
15. **(Lower priority, explicitly not urgent)** Consider `g_debugInput`'s memory order (`relaxed` would suffice), `int`-vs-`%d` pointer-argument UB inherited from the original game source at the `wsprintfA` call sites (harmless on this project's actual target ABI), and `CompatDC::selectedBitmap` dangling-pointer verification against both games' real delete ordering. None of these have a demonstrated real-world trigger; revisit only if a related bug is ever actually observed.

---

*This audit found no evidence that free-api's real, live behavior for either target game is currently broken. Every actionable finding above is either a defensive-hardening improvement for an input neither game produces today, or a latent risk whose trigger condition (a toolchain/link-order change, a future call site, a build-configuration change) has not yet occurred. The highest-priority item (Task 1, GDI double-free protection) and the architectural item (Task 4, static destruction order) are the two recommended for near-term attention; the rest can be picked up opportunistically.*
