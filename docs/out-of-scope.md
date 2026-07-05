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

## Resource subsystem: `FindResourceA` miss → file-based fallback

**This is the actual, currently-working behavior, not a hypothetical or a
gap.** `FindResourceA`/`LoadResource`/`SizeofResource`/`LockResource`/
`FreeResource` are `STUB` because neither target game's own CMake build
compiles its `.rc` file into a real Win32 resource section — so
`FindResourceA` always misses today. Both games were written expecting that
possibility and already correctly fall through to file-based loading:

* **Palette loading** (`DDLoadPalette`, planetblupi `ddutil.cpp:268-306`,
  free-eggbert equivalent): tries `FindResourceA(NULL, szBitmap, RT_BITMAP)`
  first; on miss, falls through to `_lopen`/`_lread`/`_lclose` reading the
  `.bmp`-shaped file directly (locked in by
  `tests/test_file_regressions.cpp`'s
  `TestFindResourceAMissesThenLopenLreadLcloseDecodesRealBmpHeader`).
* **Sprite-sheet loading** (`DDLoadBitmap`, both games' `ddutil.cpp`): tries
  `LoadImageA(GetModuleHandle(NULL), ..., LR_CREATEDIBSECTION)` (resource
  path) first; on failure, falls through to
  `LoadImageA(NULL, ..., LR_LOADFROMFILE|LR_CREATEDIBSECTION)` (file path).

**Decision: keep `FindResourceA` et al. as permanent safe-miss stubs.** The
file-based fallback is sufficient and already exercised by both games on
every real run; building a full embedded-resource registry is deferred (see
`todo/Embedded_Resources_FreeAPI.md`'s decision note) unless a concrete,
evidenced need emerges.

## Compile-only stubs

Symbols tagged `@note Status: STUB` in a header are either (a) a genuinely
safe placeholder because neither game relies on its real behavior, or (b) a
real gap that happens to compile safely. This table distinguishes the two so
neither is mistaken for the other:

| Symbol(s) | Header | Why the stub is safe |
|---|---|---|
| `FindResourceA`, `LoadResource`, `SizeofResource`, `LockResource`, `FreeResource` | `winbase.h` | Permanent safe miss/no-op — see "Resource subsystem" above. |
| `GetModuleHandleA` | `winbase.h` | Only ever used as an always-missing resource-instance handle for `LoadImageA`; its real value is never inspected. |
| Everything in `commdlg.h` | `commdlg.h` | Proven zero real calls in either game (see header's own doc comment). |
| Everything in `windowsx.h` except `GetStockBrush` | `windowsx.h` | Proven zero real calls beyond that one macro (see header's own doc comment). |
| `VARTYPE`/`SCODE`/`DATE`/`CLIPFORMAT` and friends | `wtypes.h` | Proven zero real runtime use in either game (see header's own doc comment). |
| `LoadCursorA`/`LoadIconA` | `winuser.h` | Non-null handle only; neither game inspects the real cursor/icon shape (TASK-0056 confirms coverage for the names both games actually request). |
| `GetStockBrush` | `windowsx.h` | Returns a valid non-null `HBRUSH`; the window is always fully covered by the game's own blit before becoming visible, so the real brush color is never seen. |
| `MessageBoxA` | `winuser.h` | Only reached on fatal init failure in either game; a real message box isn't required for that path to behave correctly (the process still reports failure). |
| `InvalidateRect` | `winuser.h` | A no-op; neither game's visible behavior depends on the repaint actually being scheduled (both redraw every frame regardless). |

**Resolved this session (TASK-0086):** `_findfirst`/`_findnext`/
`_findclose` were previously a real gap (always failed) affecting
free-eggbert's design-file picker (`event.cpp:4741`). Now implemented for
real via `std::filesystem`, scoped to the one wildcard shape actually used
(a directory plus a simple `*.ext` pattern) — see `docs/supported-apis.md`.

**Resolved (TASK-0103, was deferred as optional):** `joyGetPosEx`/
`joyGetNumDevs` are now real, `SDL_Joystick`-backed implementations
(`src/winmm.cpp`), populating exactly the fields free-eggbert reads
(`dwXpos`/`dwYpos` from the first 2 axes, `dwButtons` bits 0-3 from the
first 4 buttons — see `include/mmsystem.h`'s `JOYINFOEX` doc comment).
Tested via SDL's virtual-joystick API (`tests/test_joystick_regressions.cpp`)
without needing real hardware. This does not change TASK-0102's finding
that free-eggbert's own joystick-enable flag (`m_somethingJoystick`) is
never set to anything but 0 — a real backend here does not, by itself,
make the game actually poll it; that would need a further, separate change
to free-eggbert's own source, which is out of this repo's scope.

*(Historical note: `AdjustWindowRect`, `ShowCursor`, `SetCursor`, and
`LoadStringA` were flagged in this exact way early in this project's audit
as "STUB but not actually safe." All four have since been investigated and
either implemented for real (`ShowCursor`/`SetCursor`/`LoadStringA`) or
confirmed correct as designed (`AdjustWindowRect`'s identity transform) —
see `docs/supported-apis.md` for their current status.)*

## Removal/hiding policy for proven-unused symbols

Any symbol proven unused by both target games (per the methodology in
`plan.md` section 5 and this file's own findings) is a legitimate candidate
for removal, hiding behind an opt-in macro, or being left as a documented
stub — contributor's choice, justified case by case. This project should
bias toward staying small, not toward "keep everything just in case."

## Unicode / `W`-suffixed API variants

Neither target game defines `UNICODE` when building, so the `W`-suffixed
macro aliases in `winuser.h` (see that header's own doc comment above its
`#ifdef UNICODE` block) exist purely for source-compile symmetry with real
Win32 headers. **Do not implement real wide-character runtime behavior**
for any `W`-suffixed function unless a target game is proven to build with
`UNICODE` defined — there is no evidence either ever will.

## Unsupported APIs

Confirmed-absent, confirmed-unused-by-both-games symbols and behaviors.
Nothing below should be implemented absent new evidence of a real call site.

| Symbol / area | Status | Why it's out of scope |
|---|---|---|
| `GetAsyncKeyState`, `GetKeyState` | Not implemented | Confirmed zero use in either game — all input is message-driven (`WM_KEYDOWN`/`UP`, `WM_MOUSEMOVE`/etc.), never polled. |
| `waveOut*` (`waveOutOpen`, etc.), `PlaySound` | Not implemented | Confirmed zero use — both games route sound effects through DirectSound/`free-direct`, not WinMM `waveOut`/`PlaySound`. |
| `QueryPerformanceCounter`/`QueryPerformanceFrequency` | Not implemented | No evidenced call site in either game. |
| Full `.rc`/`.res` resource compilation | Not implemented | See "Resource subsystem" above — the file-based fallback both games already use is sufficient; `cmake/ExtractStringTable.cmake` deliberately stays a narrow `STRINGTABLE`-only parser, not a general compiler. |
| `ddraw.h`/`dsound.h`/`dplay.h` and all `DD*`/`DS*`/`DP*` symbols | Not implemented (by design) | Owned by the sibling `free-direct` project — see `docs/scope.md`'s free-direct boundary section. |
| `joyGetDevCapsA`/`JOYCAPS` | Not implemented | free-eggbert's only call site (`event.cpp:4842-4854`) is inside a commented-out (dead) code block; planetblupi has no joystick code at all. Do not implement absent a real, live call site. |
| `WM_CHAR`/text-input translation | Implemented, but **unproven** — do not expand | Not evidenced as required by either game's core source (§3.5). Do not add further text-input handling without a concrete usage site. |
| `WM_MBUTTONDOWN`/`WM_MBUTTONUP`, `MK_MBUTTON` | Implemented, but **unproven** — do not expand | Same rationale as `WM_CHAR`: confirmed unused by both games' core source (§3.5). |
| `GlobalMemoryStatus`'s fields beyond `dwTotalPhys` | Out of scope — keep limited to `dwTotalPhys` | free-eggbert (the only caller, twice at startup) only reads `dwTotalPhys` for its TrueColor gate. Do not implement accurate `dwMemoryLoad`/`dwAvailPhys`/etc. without new evidence. |
| `GetTickCount`, `Sleep` | Implemented — kept for **test infrastructure**, not proven game-required | Neither function is proven directly called by either game's core source; they're retained because `tests/basic_test.cpp` uses them directly. Trivial and harmless either way — this note exists so a future "is this used by the games?" audit isn't confused by their presence. |

## Resources policy (summary)

Consolidates the resource-subsystem findings above into one statement:

* Full `.rc`/`.res` compilation is out of scope (see "Unsupported APIs").
* `FindResourceA` and friends are permanent, correct safe-miss stubs — both
  games already work via their own file-based fallback (see "Resource
  subsystem" above).
* `LoadStringA` is the one resource-adjacent API that needed real backing
  data (both games' UI text flows through it, ~90+ and ~50+ call sites
  respectively) — implemented via `cmake/ExtractStringTable.cmake`.
* Everything else resource-shaped (`LoadResource`/`LockResource`/
  `FreeResource`/`SizeofResource`/`GetModuleHandleA`) is a safe, permanent
  stub per the "Compile-only stubs" table above.

**Confirmed (re-checked this session, TASK-0076/0077/0080):**
* `LoadIconA`/`LoadCursorA` remain safe, sufficient stubs after
  `LoadStringA` gained real backing data — they are functionally
  independent code paths (icon/cursor handle vs. string-table lookup); the
  `LoadStringA` change did not alter or mask their behavior.
* The `wave.cpp`-style embedded-`"WAVE"`-resource path remains dead/
  unreached in both games: free-eggbert's `LoadWave`/`WAVE_LoadResource`
  (`wave.cpp`) has zero call sites anywhere else in free-eggbert's source
  (confirmed via grep); planetblupi's `WAVE_LoadResource` (`wave.cpp:50`)
  calls `FindResource(hModule, ..., "WAVE")`, which always misses per the
  same `FindResourceA`-always-misses finding above, with no fallback path
  after that miss. If either game's behavior changes (e.g. `.rc` compilation
  is added upstream), re-evaluate.
* `LoadStringA`'s minimal backing-table scope was cross-checked against
  both games' own source comments (no dedicated `TODO.md` exists in
  either game's tree); no evidence found that any string ID is considered
  more essential than another — the extraction mechanism already includes
  every `STRINGTABLE` entry in whichever game's `.rc` is driving the build,
  so no further scope narrowing was warranted.

## Full audit

See `plan.md` section 5 for the complete, point-in-time table of every other
API/area classified as out-of-scope (compile-only stubs, unproven-but-
harmless translations, DirectX-family symbols owned by `free-direct`, etc.).
That table is not duplicated here to avoid the two documents drifting out of
sync; this file is reserved for findings substantial enough to warrant their
own write-up, like the ones above.
