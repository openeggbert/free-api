# Free API — Out-of-Scope / Permanently-Declined Behavior

This document records WinAPI/WinMM behavior that Free API deliberately does
**not** implement, with the evidence backing that decision. See
[`docs/scope.md`](scope.md) for the underlying policy and
[`plan.md`](../plan.md) section 5 for the full point-in-time audit table this
document complements (not duplicates).

## `WM_ACTIVATEAPP(0)` focus-loss suppression (TASK-24H-0206/1205)

**Problem:** real Win32 delivers `WM_ACTIVATEAPP(0)` when a window loses
focus. Free API deliberately never does — `SDL_EVENT_WINDOW_FOCUS_LOST` is
translated to nothing but a diagnostic log line, not a message
(`src/internal/FreeApiMessageQueue.cpp:283-291`). `WM_ACTIVATEAPP(1)`
(focus-**gained**) is still delivered normally
(`FreeApiMessageQueue.cpp:272-280`) — the suppression is one-sided.

**Why:** sending a real `WM_ACTIVATEAPP(0)` causes both games to set their
internal "inactive" flag (`g_bActive=FALSE`), which stops rendering and
input processing. SDL's focus-lost event fires spuriously on startup and in
certain desktop/window-manager environments; a spurious `WM_ACTIVATEAPP(0)`
would freeze an otherwise-visible, otherwise-fine game window. This is a
genuine, permanent Win32 semantic deviation, not an oversight or an
unfinished implementation.

**Decision:** keep the suppression. If a real need for deactivation
behavior emerges (e.g. alt-tab should visibly pause the game), add a
configurable delay or explicit flag then — do not restore unconditional
`WM_ACTIVATEAPP(0)` delivery without new evidence it's safe.

## MCI digital-video / AVI movie playback ("avivideo")

**Decision: permanently declined. This was the single highest-risk unresolved
item in the original audit (`plan.md` section 9, prior version) and has since been investigated and resolved
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

**mciGetDeviceIDA's fixed-return-value collision at teardown (TASK-24H-0806).**
`mciGetDeviceIDA` (`src/winmm.cpp`) is a permanent stub that always returns
the fixed value `1`, regardless of the requested device-type string. free-api's
MIDI sequencer path also numbers its own device IDs starting at 1
(`src/MidiMusic.cpp`), so this hardcoded `1` can coincide with a real,
currently-open MIDI session's actual device ID. In free-eggbert this is
reachable exactly once, at final process teardown: `FinishObjects()`
(`blupi.cpp`) deletes `g_pMovie` — triggering `termAVI()` → `mciGetDeviceIDA`
→ `MCI_CLOSE` on device `1` — *before* it calls `g_pSound->StopMusic()`'s own
explicit close. Confirmed harmless: `MidiMusicSendCommand`'s `MCI_CLOSE`
handler treats an already-closed/unknown ID as non-fatal, and it's the last
teardown step before process exit regardless. Locked in by
`tests/test_mci_sequences.cpp`'s `TestMciGetDeviceIdaClosesARealOpenSequencerSessionAtCollidingId`
(`TASK-24H-0808`).

**Independent feasibility re-confirmation (TASK-24H-0909/1207, cross-references
`docs/audit-24h-free-api.md` §1/§5/§6).** Per `docs/audit-24h-free-api.md`
§1/§5/§6 (a prior session's audit record): the user had explicitly approved
re-opening this decision for a feasibility-only audit (no implementation) in
that session. That audit confirmed the concrete codec dependency real
playback would require: Cinepak and MS Video 1, identified directly from
planetblupi's real shipped `.avi` assets. Recommendation, independently
re-confirmed rather than merely inherited unchanged: decline permanently.

## `MoveWindow`'s stale logical-size tracking (TASK-24H-0309)

**Confirmed harmless, not a TODO.** `MoveWindow` (`src/winuser_window.cpp`)
repositions/resizes the real SDL window via `SDL_SetWindowPosition`/
`SDL_SetWindowSize`, but never updates `g_freeApiWindowStates`'s tracked
logical width/height — so `GetClientRect`/`ClientToScreen`/
`ScreenToClient` would report stale dimensions after a real `MoveWindow`
resize, if anything actually called `MoveWindow` on a live window.

Nothing does: the only `MoveWindow` call sites in either target game are
inside `movie.cpp`'s AVI-playback code path, which is itself dead-reach
(see "MCI digital-video / AVI movie playback" above — the AVI probe always
fails, so `movie.cpp`'s body, including its `MoveWindow` calls, never
executes in either game). Do not add logical-state-update handling to
`MoveWindow` without new evidence a real, reachable call site needs it.

## `CreateWindowExA`'s `dwExStyle` is stored/logged only (TASK-24H-0305)

**Confirmed harmless, not a TODO.** `CreateWindowExA`'s `Uint32 flags`
computation (`src/winuser_window.cpp`) only inspects `dwStyle`
(`WS_VISIBLE`/`WS_POPUP`/`WS_CAPTION`); `dwExStyle` is used only for
diagnostics logging and to populate `CREATESTRUCTA::dwExStyle` — it never
maps to a real SDL window flag (e.g. `SDL_WINDOW_ALWAYS_ON_TOP` for
`WS_EX_TOPMOST`). Both target games pass `WS_EX_TOPMOST`
(`../free-eggbert/src/blupi.cpp:735`, `../planetblupi/src/blupi.cpp:627`)
but neither reads `dwExStyle` back or otherwise depends on real OS-level
always-on-top enforcement — both run one exclusive fullscreen/popup window
for their entire session, so there's nothing else for a topmost window to
compete with. Do not implement real `WS_EX_TOPMOST`/other extended-style
SDL behavior without a new evidenced call site that requires it.

## No `SDL_WINDOW_RESIZABLE` on any `CreateWindowExA` window (TASK-24H-0316)

**Deliberate, load-bearing — do not add without re-verifying first.**
`CreateWindowExA` (`src/winuser_window.cpp`) never sets `SDL_WINDOW_RESIZABLE`
on the SDL window it creates, regardless of `dwStyle`. This is a workaround,
not an oversight: on some Wayland/X11 compositors, a resizable popup window
immediately receives a spurious `WM_CLOSE` from the compositor itself,
killing the game on startup before any real user interaction. Planet Blupi's
own window style (`WS_POPUPWINDOW|WS_CAPTION`) is a fixed-size popup with a
title bar — it never needs to be resizable for either target game's real
usage.

Locked in by `TestCreateWindowExANeverSetsResizableFlag`
(`tests/test_winuser_regressions.cpp`), which asserts
`(SDL_GetWindowFlags(window) & SDL_WINDOW_RESIZABLE) == 0`. Do not add real
resizable-window support, or any per-game opt-in for it, without first
re-verifying on an affected compositor that this workaround is no longer
needed — there is no evidenced need for resizability in either game.

## `SetTimer`/`WM_TIMER` and `timeSetEvent`/`timeKillEvent` must stay two independent implementations (TASK-24H-0507)

**Previously-considered-and-rejected idea, not an oversight — do not
unify.** `SetTimer`/`KillTimer`/`WM_TIMER` (`src/winuser_timer.cpp`) and
`timeSetEvent`/`timeKillEvent` (`src/winmm.cpp`) are confirmed to be
planetblupi's and free-eggbert's respective sole, mutually-exclusive frame
pumps — each game depends on its own mechanism working in isolation.
Consolidating them into one shared implementation has been considered and
rejected: the two mechanisms only share one intentional coupling (the
`g_nextTimerId` ID-generator counter, `src/internal/FreeApiTimers.hpp`,
TASK-24H-0502) and must otherwise remain fully independent (separate maps,
separate mutexes, separate SDL timer backends). Do not perform or propose
any consolidation without new evidence a real caller needs it.

## `WM_NCMOUSEMOVE` is never generated (TASK-24H-0411)

**Investigated, confirmed non-issue.** planetblupi's `WndProc` has a real
`case WM_NCMOUSEMOVE:` handler (`../planetblupi/src/event.cpp:4998-5006`)
that calls `ShowCursor(TRUE)` to restore the real OS cursor when the mouse
moves over the window's non-client area (e.g. the title bar in windowed
mode, `WS_POPUPWINDOW|WS_CAPTION`). Free API's SDL event translation never
produces `WM_NCMOUSEMOVE` — grep confirms zero references anywhere in
`src/`.

This does **not** cause any observable cursor-visibility defect. Free API's
cursor hiding (`ShowCursor(FALSE)` → SDL's `SDL_HideCursor`) is implemented,
on both compositor backends both games actually run on, as a call scoped
strictly to the app's own client window/surface — never the window
manager's/compositor's own title-bar decoration, which is a wholly separate
window/surface the WM/compositor owns and manages its own cursor for:

* **X11**: `X11_ShowCursor` (`SDL/src/video/x11/SDL_x11mouse.c`) calls
  `XDefineCursor(display, data->xwindow, ...)` — `data->xwindow` is the
  app's own client window; the WM's separate frame/decoration window (which
  draws the title bar after reparenting) is never touched.
* **Wayland**: cursor changes go through `wl_pointer_set_cursor`, issued
  only in response to `wl_pointer` enter/motion events for the app's own
  `wl_surface`. The app never receives — and never attempts to override —
  pointer events for compositor-drawn (server-side/xdg-decoration) chrome.

So the window manager/compositor always shows its own default cursor over
its own title bar, completely independent of whatever cursor-visibility
state the app has set via `ShowCursor`. There is nothing for
`WM_NCMOUSEMOVE`'s `ShowCursor(TRUE)` handler to actually fix in practice —
planetblupi's handler is defensive code for a Win32-specific scenario (a
real Win32 non-client area is owned by the same process/window as the
client area, so the OS cursor really can get left hidden there) that
doesn't apply under SDL's X11/Wayland windowing model. Do not add
`WM_NCMOUSEMOVE` generation without new evidence of a real, observed cursor
defect in windowed mode.

## `CreateDirectoryA`'s already-exists return value (TASK-24H-0709)

**Confirmed harmless, not a TODO.** Real Win32 `CreateDirectoryA` returns
`FALSE` and sets `ERROR_ALREADY_EXISTS` when the target directory already
exists. `src/winbase_file.cpp`'s implementation has code for that exact
case (an `std::filesystem::exists` check that sets `ERROR_ALREADY_EXISTS`
and returns `FALSE`) — but it is unreachable in practice, because
`SDL_CreateDirectory` (called first) already reports success for an
already-existing path ("This reports success if `path` already exists as
a directory", `SDL_filesystem.h`), so the function returns `TRUE` before
ever reaching that branch.

This is a real, confirmed deviation from Win32 semantics — `CreateDirectoryA`
here is idempotent (`mkdir -p`-style), not already-exists-detecting. It's
harmless because neither game ever checks `CreateDirectoryA`'s return
value: both call it fire-and-forget for a fixed save-directory path
(`../free-eggbert/src/misc.cpp:184`, `../planetblupi/src/misc.cpp:220`).
Do not "fix" this by reordering the checks without new evidence a real
call site depends on the `FALSE`/`ERROR_ALREADY_EXISTS` distinction —
`tests/test_file_regressions.cpp`'s `TestCreateDirectoryACreatesRealDirectory`
locks in the actual (idempotent-`TRUE`) behavior.

## `WM_MOUSEMOVE`/`WM_TIMER` queue coalescing (TASK-24H-0212)

**Deliberate, permanent Win32 semantic deviation — not a bug.** `PushMessage`
(`src/internal/FreeApiMessageQueue.cpp:68-101`) coalesces two message types
rather than queuing every instance, unlike real Win32:

* `WM_MOUSEMOVE` — a pending `WM_MOUSEMOVE` for the same `hwnd` is updated
  in place (new `wParam`/`lParam`/`time`) instead of appending a duplicate.
* `WM_TIMER` — a pending `WM_TIMER` for the same `hwnd`+timer-id (`wParam`)
  is dropped entirely rather than duplicated.

Both exist because mouse-motion and timer events can fire far faster than
either game's own message pump drains the queue; without coalescing, every
real click/keypress would end up queued behind a growing backlog of stale
motion/timer messages, and the queue would grow unboundedly under sustained
input. Real Win32 does not do this. Do not remove or change this behavior,
and do not extend coalescing to any other message type without new
evidence a specific message type causes the same unbounded-growth problem.

## Single live window assumption / `DispatchMessageA`'s null-hwnd fallback (TASK-24H-0208/0304)

**Deliberate, load-bearing — do not add multi-window support without
revisiting this first.** free-api's window model assumes exactly one live
window at a time. `DispatchMessageA` (`src/winuser_message.cpp:183-192`)
relies on this directly: when a message's `hwnd` is `NULL` or not found in
`g_windowProcedures`, it falls back to dispatching to whatever single
window happens to be registered first
(`g_windowProcedures.begin()`) — genuinely reachable today (e.g. if
`SDL_EVENT_WINDOW_CLOSE_REQUESTED`'s window-ID resolution ever fails and
pushes `WM_CLOSE` with a `NULL` hwnd). This is correct and safe only
because both target games ever have at most one window open at a time; it
would silently dispatch to the wrong window if the project ever grew
multi-window support. Do not add multi-window support, or generalize this
fallback, without first revisiting it — no evidenced need exists in either
target game today.

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
| `HFONT`, `HPALETTE` | `minwindef.h` | No font- or palette-creation/manipulation API exists anywhere in Free API -- no function takes or returns either type. Vestigial opaque-handle placeholders kept for type-compatibility; downgradeable to removal only if a stronger signal emerges (TASK-24H-0111). |
| `IUnknown`, `GUID`/`LPGUID`/`LPCGUID`, `IID`/`LPIID`/`REFIID`, `CLSID`/`LPCLSID`/`REFCLSID` | `winnt.h` | Same COM-family vestige as the `wtypes.h` row above, in the header that actually defines the `GUID` struct layout and its IID/CLSID aliases. No COM/OLE behavior implemented; zero runtime use anywhere in `include/` (TASK-24H-0112). |
| `LoadCursorA`/`LoadIconA` | `winuser.h` | Non-null handle only; neither game inspects the real cursor/icon shape (TASK-0056 confirms coverage for the names both games actually request). |
| `GetStockBrush` | `windowsx.h` | Returns a valid non-null `HBRUSH`; the window is always fully covered by the game's own blit before becoming visible, so the real brush color is never seen. |
| `MessageBoxA` | `winuser.h` | Only reached on fatal init failure in either game; a real message box isn't required for that path to behave correctly (the process still reports failure). |
| `InvalidateRect` | `winuser.h` | A no-op; neither game's visible behavior depends on the repaint actually being scheduled (both redraw every frame regardless). |
| `UnlockResource` | `winbase.h` | Always returns `FALSE`, gated by the same permanent `FindResourceA` safe-miss contract as `LoadResource`/`SizeofResource`/`LockResource`/`FreeResource` above — see "Resource subsystem" (TASK-24H-0807). |
| `CloseHandle` | `handleapi.h` | **Vestigial, not test-infrastructure-only** (correcting `TASK-24H-0114`'s problem text, which listed it alongside `GetLastError`/`SetLastError`/`RemoveDirectoryA`/`SetEnvironmentVariableA` as if it were test-infrastructure-only too): proven zero call sites anywhere — not in either game, not in any `tests/*.cpp` file, and not in free-api's own `src/` beyond its own definition. Kept only for Win32 header-shape compatibility (TASK-24H-1228, found by the `TASK-24H-0115` public-surface audit). |

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

## `AdjustWindowRect`'s identity transform (TASK-24H-0310)

**Deliberate, permanent — do not add real window-chrome math.**
`AdjustWindowRect` (`src/winuser_misc.cpp`) is an identity transform:
`lpRect` is returned unchanged. This is correct, not a stub, because
`CreateWindowExA`/`GetClientRect` define "window size" as equal to client
size in this implementation — there is no separate title-bar/border size to
account for. Do not implement real Win32 non-client-area size math (title
bar/border thickness deltas) absent new evidence that window size and
client size must differ for some game.

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

**Precise failure mode (TASK-24H-0118):** the sixteen `W`-suffixed target
functions the `UNICODE`-branch macros alias to (`SetWindowTextW`,
`PostMessageW`, `MessageBoxW`, `LoadStringW`, `GetModuleHandleW`,
`LoadImageW`, `GetObjectW`, `RegisterClassW`, `CreateWindowExW`,
`CreateWindowW`, `PeekMessageW`, `GetMessageW`, `DispatchMessageW`,
`DefWindowProcW`, `LoadCursorW`, `LoadIconW`) are **not declared anywhere**
in free-api (confirmed by grep — zero matches for any of the sixteen
outside the macro definitions themselves). A `UNICODE` build therefore
**fails to compile** at the first macro use, not merely "compiles against
an alias with no real behavior" — a deliberate fail-loud design.

**`OutputDebugStringW` (`include/debugapi.h`, `src/winbase.cpp:70-76`) is the
one exception worth calling out explicitly** (TASK-24H-0109/1215, closing
`plan.md` `TASK-0007`'s open decision): unlike the alias-only `W` variants
above, it's a real, non-trivial UTF-16-to-narrow conversion, despite zero
evidenced call sites in either target game (re-confirmed this session via
`grep -rn "OutputDebugStringW" ../free-eggbert/src ../planetblupi/src` —
no matches). **Decision: keep the real implementation, do not downgrade to
a stub.** It costs nothing at runtime since it's never invoked, and keeping
it trivially in sync with `OutputDebugStringA` is cheaper than maintaining
a special-cased stub exception to the pattern above.

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
| `PeekMessageA`'s `hWnd`/`wMsgFilterMin`/`wMsgFilterMax` | Implemented, but **intentionally ignored** — do not implement real filtering | Every call is an unfiltered peek regardless of what's passed. Two independent full-source usage sweeps confirm neither game ever passes a non-zero filter or specific `hWnd` (TASK-24H-0201/1210). |
| `WaitMessage`'s blocking contract | Implemented as a **polling approximation**, not true OS-level blocking | Checks the queue twice around one SDL event pump, sleeps ~1ms if still empty, then returns `TRUE` unconditionally even if the queue is still empty. Adequate for both games' idle-loop usage; do not implement a real condition-variable-based blocking wait without new evidence it's needed (TASK-24H-0204/1211). |
| `RegisterClassExA`/`WNDCLASSEXA` | Not implemented | Confirmed absent-and-correct, not an oversight: zero call sites in either game (both use the plain `WNDCLASSA`/`RegisterClassA` form). Checked explicitly this session (TASK-24H-0116/0302). Any future request to add these should be scrutinized hard, since there is no existing partial implementation to extend from. |

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
* **`"RES_<id>"` is a debug/developer placeholder only, never acceptable in
  shipped game UI.** It means "this ID has no STRINGTABLE entry in the
  generated table" — either a standalone free-api build with no sibling
  game's `.rc` extracted, or (in a target-game build) an ID with genuinely
  no STRINGTABLE entry in that game's own `.rc`. When `CMAKE_PROJECT_NAME`
  is `SPEEDY_BLUPI_WINDOWS` or `PLANET_BLUPI_WINDOWS`,
  `cmake/ExtractStringTable.cmake` fails CMake configure loudly
  (`REQUIRE_STRINGS`) if that game's `.rc` is missing or yields zero
  strings, and separately verifies (`VERIFY_ID`/`VERIFY_TEXT`) that a known
  ID (`TX_BUTTON_QUITTER`, 106, "Quit BLUPI" — present in both games'
  `.rc` files) resolves correctly — so a successful configure in either
  target game is a real signal the table is populated, not just present.
  Beyond that single sentinel ID, `USED_IDS_FILE` (also target-game only)
  verifies *every* ID either game's own source actually passes to
  `LoadString`, per the evidence-based manifests in
  `cmake/used-string-ids/*.txt` — see
  [`used-string-ids.md`](used-string-ids.md) for how those manifests were
  derived and re-verified. Standalone free-api builds intentionally do
  **not** set any of these checks and may legitimately return `"RES_<id>"`
  for any ID — see `tests/test_loadstring_regressions.cpp` for the
  build-mode split.

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
