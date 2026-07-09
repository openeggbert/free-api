# Free API — Minimal Game Compatibility Plan

*Regenerated 2026-07-06. This supersedes the prior evidence-based audit (127
tasks + TASK-0125–0128, all `DONE`/`OBSOLETE`) with a fresh, full re-audit of
the current codebase — every claim below was re-derived from direct source
inspection this pass, not carried over from the previous version. The
previous audit's full history remains available in git (`git log -- plan.md`)
and is summarized in `NEXT.md`; nothing from it is silently lost.*

---

## 1. Purpose

Free API is a narrow, source-level Win32/WinAPI compatibility layer whose
**only** reason to exist is to let two specific legacy Windows games compile
and run without Microsoft Windows:

* **Free Eggbert** (`../free-eggbert`, "Speedy Eggbert 2" port)
* **Planet Blupi** (`../planetblupi`)

Both games already `add_subdirectory()` this repo and link against
`free-api` (and the sibling `free-direct`, which emulates DirectDraw/
DirectSound/DirectPlay on top of SDL3 and, in places, on top of free-api's
own GDI subset). Free API's job is **source-level compatibility for these
two games** — nothing more.

Free API is explicitly **not**:

* **Not Wine.** It does not emulate a Windows process, PE loader, registry,
  or kernel objects.
* **Not a general Win32 SDK reimplementation.** It does not aim for API
  completeness against any Windows SDK version.
* **Not a platform for arbitrary 1998-era Windows games.** Only the two
  named games define scope.
* Free API **must stay small**. Its size should track the union of what
  these two games actually call, not what Win32 offers.
* Free API **must only implement what the two target games actually use**,
  as demonstrated by direct source evidence (grep/read), not by assumption
  or by what "a game like this would probably need."
* **Any future API addition must cite a real usage site** in
  `../free-eggbert` or `../planetblupi` (file + line), or be justified as
  scope-control cleanup/tests/docs for the existing subset, or as the
  narrow "entry-point bridge consumed by `free-direct` on the games'
  behalf" exception documented in §5.

This document is built entirely from direct evidence gathered this pass
from:

* `free-api`: every header in `include/` (23 files, 2050 lines), every
  source file in `src/` and `src/internal/`, `tests/`, `todo/`,
  `docs/*.md`, `CMakeLists.txt`.
* `../free-eggbert`: `src/*.cpp`, `include/*.hpp` (excluding
  `third_party/`, `dxsdk3/`, `bass/`, `msvc5/`, `android/` vendor trees).
* `../planetblupi`: `src/*.cpp`, `include/*.h` (excluding `third_party/`,
  `android/`).

---

## 2. Hard Scope Rules

1. No API may be added just because it existed in Windows 95/98.
2. No API may be added just because it seems useful.
3. No API may be added for hypothetical future games.
4. No large compatibility framework may be introduced.
5. No generic PE/resource loader may be introduced unless the games
   require it (they don't — see §5).
6. No generic Win32 subsystem emulation (registry, COM/OLE, security
   descriptors, threading primitives beyond what's already used) may be
   added.
7. SDL3 is allowed only as an internal backend detail behind the
   WinAPI-shaped public surface; it must never leak into public headers.
8. Public headers should expose only what the two games need to compile
   and link.
9. Every unused public symbol in `include/` must be classified as one of:
   **required by target game**, **compile-only/test-only compatibility**,
   **accidental legacy leftover**, or **candidate for removal/hiding**
   (see §5).
10. Every non-trivial implementation must have a test based on behavior
    actually used by the games.

`DirectDraw`/`DirectSound`/`DirectPlay` (`ddraw.h`, `dsound.h`, `dplay.h`,
and all `DD*`/`DS*`/`DP*` symbols) are used extensively by both games but
belong to the sibling **free-direct** project, not free-api. They are
noted where they co-occur with real WinAPI/GDI/WinMM calls but are
explicitly **out of scope** for this plan. See `docs/scope.md`'s boundary
statement for the full policy.

---

## 3. Audit Methodology (this pass)

This pass re-derived the entire public surface and its usage from scratch,
rather than trusting prior documentation:

1. **Full header inventory**: every function prototype, `#define`, type,
   and message constant in `include/*.h` (`include_non_windows/` and
   `src/internal/*.hpp` excluded — private/non-Windows-only plumbing),
   with file:line citations. Result: **23 headers, 2050 lines, ~101
   function declarations, ~231 `#define`s**.
2. **Independent usage cross-check against free-eggbert's own source**
   (not trusting `docs/supported-apis.md`'s existing claims) — every
   symbol from step 1, confirmed used or not, with file:line citations.
3. **Independent usage cross-check against planetblupi's own source**,
   same method.
4. **Verification of the inline `@note Status: STUB/PARTIAL/IMPLEMENTED`
   annotations already present in every header** against the real current
   implementation in `src/` — checking for stale/overclaiming comments,
   not just trusting them. Also a full `TODO`/`FIXME`/`XXX`/`HACK` sweep
   of `src/`.
5. **Re-verification of the two long-lived non-`plan.md` backlogs**,
   `todo/free-api-performance-todo.md` (~15 items) and
   `todo/Embedded_Resources_FreeAPI.md` (1 deferred design), against
   current source.
6. **Direct spot-verification** of specific claims that came out of 1–5
   (e.g. the `RGB` macro's parenthesization, whether `_findfirst`'s
   session table can leak, whether `FreeApiCreateSurfaceDC`/
   `FreeApiDestroySurfaceDC` have a real shared header declaration, and
   whether either game actually needs native MCI looping).

**Headline result: the codebase is in materially good shape.** Of the
~15-item performance/correctness backlog, 9 items were already resolved
(not previously marked as such), 1 was never free-api's concern, and only
2 are still real, small, bounded gaps. Of 143 inline status annotations
across all headers, **zero were found stale or overclaiming** against
current `src/` — the self-documentation is trustworthy. The task list in
§6 is short precisely because this pass confirmed there is little left to
do, not because less scrutiny was applied.

---

## 4. Current State — Verified Symbol Usage by Subsystem

The full per-symbol status table is `docs/supported-apis.md` (living
document, kept current after this pass closes out §6's tasks) and the
full header-usage table is `docs/headers.md`. This section summarizes what
this pass's independent re-derivation confirmed or newly found, organized
the same way.

### 4.1 WinUser — window / message loop / input (`winuser.h`, 563 lines)

Confirmed live, real usage by **both** games of the entire core loop:
`RegisterClassA`+`WNDCLASSA`, `CreateWindowExA`/`CreateWindowA`,
`ShowWindow`/`UpdateWindow`/`SetFocus`, `GetSystemMetrics`
(`SM_CXSCREEN`/`CYSCREEN`/`CYCAPTION`), `PeekMessageA`/`GetMessageA`/
`TranslateMessage`/`DispatchMessageA`, `WaitMessage`, `PostQuitMessage`/
`PostMessageA`, `DefWindowProcA`, `GetClientRect`, `AdjustWindowRect`,
`GetCursorPos`/`ScreenToClient`/`ClientToScreen`/`SetCursorPos`,
`ShowCursor`/`SetCursor`, `LoadCursorA`/`LoadIconA`, `SetRect`/
`IntersectRect`/`UnionRect`, `MoveWindow`, `InvalidateRect`,
`SetWindowTextA`, `MessageBoxA`, the VK_*/WM_*/WS_*/CS_* families actually
exercised, and `LoadStringA` (free-eggbert ~90 sites, planetblupi ~50
sites — the exact figures reconfirmed this pass: 48 in planetblupi
alone, matching the documented "~50+").

Confirmed **single-game-only** (matches existing documentation, re-verified):
`SetTimer`/`KillTimer`/`WM_TIMER` (planetblupi live, free-eggbert dead code
path), `MK_*` flags in `WM_MOUSEMOVE` (planetblupi only — confirmed zero
hits anywhere in free-eggbert's own source), `CreateBitmap` (planetblupi
only).

**New finding — `GetObject` false-positive risk:** a raw `grep -F` for
`GetObject` in planetblupi's source returns 69 hits; only **3 are real
WinAPI calls** (`ddutil.cpp:47,104,208`, all the identical shape
`GetObject(hbm, sizeof(bm), &bm)`). The other 66 are the game's own,
unrelated `CDecor::GetObject(POINT, int&, int&)`/`MoveGetObject` methods
(name collision). No action needed — noted here so a future audit doesn't
mis-count this symbol's usage from a naive grep.

**New finding — narrow VK_*/MK_* coverage gaps in the *games*, not
free-api:** free-eggbert never references `VK_F9/F11/F12/PRIOR/NEXT/TAB/
BACK/DELETE/INSERT/MENU` — these constants exist in `winuser.h` only for
the subset either game actually presses. Not a free-api gap (nothing
*needs* implementing); just confirms the header's VK_* list should not
grow beyond what's cited.

### 4.2 GDI (`wingdi.h`, 157 lines)

Confirmed live, real usage by both games of the full documented GDI
subset: `CreateCompatibleDC`/`DeleteDC`/`SelectObject`/`DeleteObject`/
`GetObjectA`, `GetDeviceCaps`(`SIZEPALETTE`)/`GetSystemPaletteEntries`,
`LoadImageA`(`LR_LOADFROMFILE`), `StretchBlt`(`SRCCOPY`), `GetPixel`/
`SetPixel`, `CreateBitmap` (planetblupi only), plus `BITMAP`/`RGBQUAD`/
`BITMAPINFOHEADER`/`BITMAPFILEHEADER`/`PALETTEENTRY`/`COLORREF` and the
`RGB(...)` macro.

**Verified, not a bug:** the `RGB(r,g,b)` macro's parenthesization
(`include/wingdi.h:81`) looked unusual on first read (an apparently
unbalanced-looking nesting). Directly diffed against the real Win32 SDK's
`RGB` macro definition — **it is character-for-character identical**,
parens included. No task; flagged here only so it isn't re-flagged by a
future pass without checking.

**New finding — `LR_CREATEDIBSECTION` without `LR_LOADFROMFILE`:**
planetblupi's `ddutil.cpp:90` calls
`LoadImage(GetModuleHandle(NULL), szBitmap, IMAGE_BITMAP, dx, dy, LR_CREATEDIBSECTION)`
— a genuine resource-load attempt (no `LR_LOADFROMFILE`), immediately
falling back to `LR_LOADFROMFILE|LR_CREATEDIBSECTION` on failure
(`ddutil.cpp:90-95`). This is the same documented "resource lookup always
misses, falls through to file" pattern as `FindResourceA`, but it's the
first confirmed real call site where `LR_CREATEDIBSECTION` is passed
*without* `LR_LOADFROMFILE` — worth a small task (§6) to confirm
`LoadImageA`'s current implementation harmlessly ignores the unhandled
flag combination rather than mishandling it silently.

### 4.3 WinMM / MCI / MIDI / Joystick (`mmsystem.h`, `digitalv.h`, `mciapi.h`, `mmiscapi2.h`)

Confirmed live, real usage by both games: `mciSendCommandA`
(`"sequencer"`), `MM_MCINOTIFY`/`MCI_NOTIFY_SUCCESSFUL`, `midiOutGetNumDevs`/
`Open`/`SetVolume`/`Close`, `mciGetErrorStringA`, `mciGetDeviceIDA`,
`"cdaudio"` graceful decline, `mmioFOURCC` (planetblupi's WAV header
validation). Confirmed **free-eggbert-only, live**: `timeSetEvent`/
`timeKillEvent`, `joyGetPosEx`/`joyGetNumDevs`. Confirmed **still
permanently declined, correctly**: `"avivideo"`/`MCI_DGV_*` digital-video
(see `docs/out-of-scope.md`; re-confirmed this pass that neither game's
CMake build compiles a `.rc`, so the resource-lookup precondition for that
path still never fires).

**New finding, investigated and resolved as a non-issue — MIDI looping
"TODO" comment is stale.** `src/MidiMusic.cpp:15,527` carry a
`// Looping is not supported (TODO)` comment. Traced both games'
`MM_MCINOTIFY` handlers directly: free-eggbert `blupi.cpp:562-575` and
planetblupi `blupi.cpp:483-...` both handle `MM_MCINOTIFY` themselves and
explicitly re-issue playback ("`// music over, play it again`") — **native
MCI-level looping was never required**; the game-side re-trigger on
notification is the actual, working mechanism, and it's already
`IMPLEMENTED`. This is pure comment cleanup (§6), not a behavior gap.

### 4.4 File / CRT / WinBase (`winbase.h`, `io.h`, `direct.h`, `synchapi.h`, `sysinfoapi.h`, `handleapi.h`, `debugapi.h`)

Confirmed live, real usage by both games: `_lopen`/`_lread`/`_lclose`,
`CreateDirectoryA` (planetblupi live; free-eggbert dead path),
`_mkdir` (free-eggbert only), backslash/forward-slash path normalization,
`FindResourceA`(`RT_BITMAP`) miss→fallback, `FreeResource`/`LockResource`/
`UnlockResource`/`LoadResource`/`SizeofResource` (both games, proven
dead/unreachable at runtime — safe stub), `GetModuleHandleA`,
`OutputDebugStringA` (free-eggbert, 7 sites).

**New finding — a confirmed, complete list of symbols used by NEITHER
game, only by free-api's own test suite** (`tests/basic_test.cpp`):
`CloseHandle`, `GetLastError`/`SetLastError`, `RemoveDirectoryA`,
`SetEnvironmentVariableA`, `access`/`_access`. This is the *exact same*
already-accepted pattern `docs/out-of-scope.md` documents for `Sleep`/
`GetTickCount` ("kept for test infrastructure, not proven game-required")
— it just doesn't yet list these five/six symbols alongside them. Task in
§6 to extend that documented classification (no behavior change).

**New finding — `_chdir`/`_getcwd` are fully dead code:** declared
(`direct.h:21-22`) and implemented (`crt_direct.cpp:18,23`), but **zero
call sites anywhere** — not in either game, not in free-api's own tests.
Unlike the `Sleep`/`GetTickCount`-style symbols above, there is no
test-infrastructure justification either. `_mkdir` (same header, same
file) *is* live-required by free-eggbert, so `direct.h` itself must stay,
but these two specific functions have no evidenced reason to exist today.
Task in §6 to decide: remove, or explicitly document as a compile-only
"declared for `direct.h` completeness, never proven needed" stub.

**New finding — `OutputDebugStringW` is fully dead code with a
non-trivial implementation:** zero call sites anywhere (not games, not
tests), yet `src/winbase.cpp:70-76` contains a real UTF-16-to-narrow
conversion loop — more implementation effort than an unused, ANSI-policy
project (`docs/out-of-scope.md`'s Unicode section) should be carrying.
Task in §6 to simplify to match the project's own "do not implement real
`W` behavior" policy, or justify keeping it as-is.

**New finding — `_findfirst`'s session table can accumulate one entry per
game session that never gets cleaned up.** `src/crt_io.cpp`'s
`g_findSessions` (an `unordered_map<intptr_t, FindSession>`) is populated
by `_findfirst` and only erased by `_findclose`. Traced free-eggbert's one
call site (`event.cpp:4736-4747`, inside `CEvent::ChangePhase` — fires
once per transition into the design-mission save/load screen, not
per-frame): it calls `_findfirst`/`_findnext` in a fully-draining loop but
**never calls `_findclose`**. This leaks one map entry per visit to that
screen — low-impact (bounded by how many times a player opens that
specific screen in one process lifetime, not a hot path), but real and
unbounded over a long session. Task in §6 to decide a fix (candidates:
make `_findfirst`'s implementation self-clean once a caller's loop fully
drains a session via repeated `_findnext` failure, since real callers
already do that; or just document as a known, low-severity, by-design
limitation).

### 4.5 Resources / dialogs / base types

`commdlg.h` (whole file) and `windowsx.h` (everything except
`GetStockBrush`) confirmed still zero real calls in either game — safe,
harmless stubs, unchanged from prior audit. `wtypes.h`'s `VARTYPE`/
`SCODE`/`DATE`/`CLIPFORMAT` (and the related `GUID`/`IID`/`CLSID`/
`IUnknown` family in `winnt.h`) confirmed **zero runtime use in either
game** — free-eggbert includes `wtypes.h` but never references these
types at runtime; planetblupi doesn't include it at all. Unchanged
classification: harmless compile-only vestige.

**New finding — two public functions free-direct depends on have no
shared header declaration.** `FreeApiCreateSurfaceDC`/
`FreeApiDestroySurfaceDC` (`src/wingdi_dc.cpp:12,30`) are real,
`extern "C"`, non-`WINAPI`, non-static functions that
`../free-direct/src/directdraw/DirectDraw.cpp:20-21` calls — but declares
its *own* local `extern "C"` prototypes to do so, because free-api's
`include/` has no header declaring them at all. This works today but
means the two projects' function signatures are hand-synced across a repo
boundary with no compiler-enforced check that they still match. Task in
§6 to add a real shared declaration.

**New finding — `FreeApiRunWinMain` is consumed by `free-direct`, not
literally by either game's own source.** `include/windows.h:99`'s
`FreeApiRunWinMain`/`FREE_API_IMPLEMENT_WINMAIN()` has zero call sites in
free-eggbert or planetblupi's own source — its one real caller is
`../free-direct/src/Main.cpp:20` (`return FreeApiRunWinMain(&WinMain, argc, argv);`),
which is the actual executable entry point for the free-eggbert/
planetblupi binaries. This is a legitimate scope exception (the whole
reason this bridge exists is to run the games) but `docs/scope.md`'s "cite
a real usage site in `../free-eggbert` or `../planetblupi`" rule doesn't
literally cover "cited by the sibling project that runs the game on its
behalf." Task in §6 to write this exception down explicitly rather than
leave it as an implicit, undocumented gap in the policy's own wording.

### 4.6 Documentation drift found

**`docs/supported-apis.md` (the "living, current" symbol table) is
missing 28 currently-declared public symbols** it should be tracking:
`CloseHandle`, `GetTickCount`, `Sleep`, `OutputDebugStringA`,
`OutputDebugStringW`, `GetStockBrush`, `FreeResource`, `LockResource`,
`UnlockResource`, `LoadResource`, `SizeofResource`, `GetModuleHandleA`,
`RemoveDirectoryA`, `DeleteFileA`, `SetEnvironmentVariableA`,
`GetLastError`, `SetLastError`, `mciGetDeviceIDA`, `mciGetErrorStringA`,
`wsprintfA`, `InvalidateRect`, `SetWindowTextA`, `MessageBoxA`,
`FreeApiRunWinMain`, `_chdir`, `_getcwd`, `SetRect`, `IntersectRect`,
`UnionRect`. Task in §6 to close this gap.

**143 inline `@note Status:` annotations checked (78 `PARTIAL`, 32
`IMPLEMENTED`, 29 `STUB`, 4 `HEADER_ONLY`, plus 3 supplementary
`@note REVIEWED` tags) — zero found stale or overclaiming** against
current `src/` behavior (spot-checked `MessageBoxA`, `InvalidateRect`,
`GetModuleHandleA`, `FindResourceA`, `StretchBlt`, `PostQuitMessage`,
`GetTickCount` directly). Exactly one declaration has no status
annotation (`WinMain`'s forward declaration inside
`FREE_API_IMPLEMENT_WINMAIN()`), and correctly so — it's the *game's* own
entry point signature, not a free-api-implemented symbol.

---

## 5. Out-of-Scope / Special-Case Classification

Every public symbol in `include/` proven unused (by the games) falls into
one of these buckets. Nothing in this section gets an implementation
task — only the documentation tasks in §6 that formalize these findings.

| Symbol / area | Why it exists | Used by target games? | Classification |
|---|---|---|---|
| `commdlg.h` (whole file) | Both games `#include` it (vestigial) | Zero API calls in either | Harmless compile-only stub |
| `windowsx.h` beyond `GetStockBrush` | Message-cracking macro placeholder | Only `GetStockBrush` used, by both | `GetStockBrush`: required. Rest: harmless stub |
| `wtypes.h`/`winnt.h`'s COM-ish types (`VARTYPE`/`SCODE`/`DATE`/`CLIPFORMAT`/`GUID`/`IID`/`CLSID`/`IUnknown`) | free-eggbert includes `wtypes.h`; adapted-code vestige | Included (eggbert only), zero runtime use in either | Harmless compile-only stub |
| `WM_CHAR` translation | Implemented per README, "for name entry screens" | Not found used by either game's core source | Keep, but flag as unproven — do not expand |
| `WM_MBUTTONDOWN`/`UP`, `MK_MBUTTON` | Implemented per README | Not found used by either game | Keep, but flag as unproven — do not expand |
| `GlobalMemoryStatus` | free-eggbert calls it twice (startup TrueColor gate) | free-eggbert only | Required (eggbert); do not expand fields beyond `dwTotalPhys` |
| `GetTickCount`, `Sleep`, **`CloseHandle`, `GetLastError`/`SetLastError`, `RemoveDirectoryA`, `SetEnvironmentVariableA`, `access`/`_access`** (bold = confirmed this pass) | Implemented, `IMPLEMENTED`/`PARTIAL` | Not called by either game's core source — only by `tests/basic_test.cpp` | Keep: harmless, used by free-api's own test infrastructure (§6 task extends `docs/out-of-scope.md`'s existing row to name all of these, not just the first two) |
| `_chdir`, `_getcwd` | Declared+implemented for `direct.h` completeness alongside live `_mkdir` | **Zero call sites anywhere — not games, not tests** | Needs a decision (§6): remove, or document as unjustified-but-harmless |
| `OutputDebugStringW` | Real (non-trivial) implementation exists | **Zero call sites anywhere** | Needs a decision (§6): simplify to match ANSI-only policy, or justify |
| `FindResourceA`/`LoadResource`/`LockResource`/`SizeofResource`/`FreeResource`/`UnlockResource` (non-bitmap paths) | Present so `wave.cpp`-style code compiles | Called, but proven dead/unreachable at runtime in both games | Harmless compile-only stub |
| `LoadIconA`/`LoadCursorA` real resource-backed behavior | `STUB` today | Called by both, only a non-null handle required | Harmless compile-only stub |
| `joyGetDevCapsA`/`JOYCAPS` | Not implemented | Confirmed dead/commented-out in eggbert, zero use in planetblupi | Do not implement |
| `mciGetDeviceIDA` | Present for movie-teardown lookup | Called once per game at teardown, low-impact | Harmless compile-only stub |
| DirectDraw/DirectSound/DirectPlay surface | Used extensively by both games | Belongs to sibling `free-direct` | Out of scope for free-api entirely |
| Unicode (`W`-suffixed) API variants | `UNICODE`-gated macros exist | Neither game builds with `UNICODE` | Do not implement real `W` behavior (macro aliasing only) |
| `FreeApiRunWinMain`/`FREE_API_IMPLEMENT_WINMAIN` | Real entry-point bridge | Not cited in either game's own source; cited in sibling `free-direct`'s `Main.cpp` | **New this pass**: legitimate exception to the "cite free-eggbert/planetblupi" rule — document explicitly (§6) |
| `FreeApiCreateSurfaceDC`/`FreeApiDestroySurfaceDC` | Real, `free-direct`-only-consumed functions | Used by `free-direct`, not the games directly (same bridge-exception shape as above) | Legitimate, but needs a real shared header declaration (§6) — not a scope violation, a hygiene gap |

**No implementation tasks are created for any row in this table** —
only the documentation/cleanup tasks in §6 reference them.

---

## 6. Task List

Numbering restarts at `TASK-0001` for this fresh audit (the prior
`TASK-0001`–`TASK-0128` backlog is fully closed and preserved in git
history / `NEXT.md`). Every task below is small, evidenced in §4/§5 above,
and does not require re-deriving anything — the investigation is already
done; these are the concrete follow-through actions.

### TASK-0001: Close the 28-symbol documentation gap in `docs/supported-apis.md`

Status: TODO
Priority: P2
Area: Documentation
Type: Documentation
Evidence: §4.6

Problem:
`docs/supported-apis.md` describes itself as "the hand-maintained, current
reference for every public symbol Free API implements," but this pass
found 28 currently-declared public symbols with no row in that table (see
§4.6 for the full list).

Required work:
* Add a row for each of the 28 symbols listed in §4.6, using the existing
  table's columns and status conventions (cross-reference the inline
  `@note Status:` header annotations, which this pass confirmed are all
  accurate).

Acceptance criteria:
* Every function/macro/type declared in `include/*.h` has a corresponding
  row in `docs/supported-apis.md`, or is explicitly covered by
  `docs/headers.md`/`docs/out-of-scope.md` if it's a pure type/header-only
  entry not suited to that table's row shape.
* No behavior change; documentation only.

Out of scope:
* Do not add any new symbol while doing this — this task only documents
  what already exists.

---

### TASK-0002: Add a real shared header declaration for `FreeApiCreateSurfaceDC`/`FreeApiDestroySurfaceDC`

Status: DONE — fixed during the 24-hour autonomous session as `TASK-24H-0101`, which also covered a third undeclared bridge function found in this session's audit, `FreeApiSetWindowFullscreen`. New header `include/free_api_bridge.h`; `../free-direct/src/directdraw/DirectDraw.cpp`, `examples/04_gdi_minimap.cpp`, and `tests/test_gdi_regressions.cpp` all updated to use it instead of local hand-declarations. Verified: free-api standalone, both target games (including the `free-api`+`free-direct` diamond dependency), and `../free-direct` standalone all build and link cleanly; 17/17 tests pass everywhere.
Priority: P1
Area: GDI / Header hygiene
Type: Hardening
Evidence: §4.5; `src/wingdi_dc.cpp:12,30`; `../free-direct/src/directdraw/DirectDraw.cpp:20-21`

Problem:
`free-direct` depends on these two non-`WINAPI` functions but has to
declare its own local `extern "C"` prototypes to call them, since
free-api's `include/` has no header declaring them. The two projects'
function signatures are hand-synced across a repo boundary with zero
compiler-enforced consistency check — a silent signature drift in either
repo would not fail to compile, just misbehave at link/runtime.

Required work:
* Add a declaration for both functions to an appropriate free-api header
  (likely `include/wingdi.h`, or a small dedicated internal-facing header
  if keeping them out of the main GDI facade is preferred — implementer's
  call, document the choice).
* Update `../free-direct/src/directdraw/DirectDraw.cpp` to `#include` that
  header instead of hand-declaring the prototypes locally (coordinate
  with that repo; this is a cross-repo signature-hygiene fix, not a
  free-api-only change, so both sides must move together).

Acceptance criteria:
* Both functions have exactly one authoritative declaration, included by
  both `free-api`'s own implementation and `free-direct`'s caller.
* Both games still build and link successfully as siblings of
  `free-direct`.
* No behavior change — this is a signature-hygiene fix only.

Out of scope:
* Do not change either function's actual signature or behavior.

---

### TASK-0003: Fix scaled `StretchBlt`'s asymmetric out-of-range source-clipping behavior

Status: DONE — fixed during the 24-hour autonomous session as `TASK-24H-0601`: source Y now clamps to the nearest edge row (`src/wingdi_blit.cpp:123-129`), matching the X-axis edge-clamp policy exactly. New regression test `TestStretchBltScaledOutOfRangeSourceYClampsToEdgeRowLikeX` added to `tests/test_gdi_regressions.cpp`. Verified 17/17 in all three build modes (standalone, `../free-eggbert` Ninja, `../planetblupi` Make).
Priority: P1
Area: GDI
Type: Bug fix
Evidence: `todo/free-api-performance-todo.md` ("Fix Scaled StretchBlt Source Clipping Semantics"); `src/wingdi_blit.cpp:115-126`

Problem:
In the scaled (non-1:1) `StretchBlt` path, out-of-range source X is
clamped to the nearest edge pixel (`src/wingdi_blit.cpp:115-120`), but
out-of-range source Y is instead skipped entirely, leaving the
destination pixel untouched (`src/wingdi_blit.cpp:126`). This is two
different behaviors on the two axes of the same blit operation, and no
existing test exercises the scaled path with an out-of-range source rect
(`TestStretchBltScaledNearestNeighborSamplesCorrectSourcePixel`,
`tests/test_gdi_regressions.cpp:245-284`, only covers a fully in-bounds
2×2→4×4 upscale).

Required work:
* Pick one consistent policy for both axes — clamp-to-edge on both X and
  Y is recommended (matching X's current behavior), since it's already
  proven safe there.
* Apply it to the Y-axis handling in `src/wingdi_blit.cpp`.
* Add a regression test exercising the scaled path with an out-of-range
  source rect on both axes.

Acceptance criteria:
* Scaled `StretchBlt` behaves consistently (whichever single policy is
  chosen) for out-of-range source coordinates on both axes.
* New test passes; existing tests still pass.

Out of scope:
* Do not change the 1:1 fast path (already correct and tested).
* Do not add any format/rotation/mirroring support beyond what already
  exists.

---

### TASK-0004: Gate object-lifetime diagnostic counters in GDI DC/bitmap creation paths

Status: TODO
Priority: P2
Area: GDI / Diagnostics
Type: Hardening
Evidence: `todo/free-api-performance-todo.md` ("Gate Object Lifetime Diagnostics if They Become Hot"); `src/internal/FreeApiGdi.cpp:38-43,60-61`; `src/wingdi_dc.cpp:19-20,38-39`

Problem:
`CreateCompatibleDC`, `DeleteDC`, `FreeApiCreateSurfaceDC`,
`FreeApiDestroySurfaceDC`, and `CreateCompatBitmapFromSurface` increment
`g_diagCompatDcs`/`g_diagCompatBitmaps`/etc. unconditionally, unlike the
per-frame message-queue diagnostics (already gated behind
`FreeApiDiagnosticsFastEnabled()`, per the now-resolved P1 "Gate
Diagnostic Counters in Hot Paths" item). These calls are load-time, not
per-frame, so this is low urgency, but inconsistent with the rest of the
diagnostics system's gating convention.

Required work:
* Gate these specific atomic increments behind the same
  `FreeApiDiagnosticsFastEnabled()`/`FreeApiDiagnosticsEnabled()` check
  used elsewhere, for consistency.

Acceptance criteria:
* Diagnostics gating is consistent across all counters in the codebase.
* No functional/observable behavior change when diagnostics are disabled
  (the default) or enabled.

Out of scope:
* Do not touch the actual DC/bitmap creation logic, only the diagnostic
  counter increments.

---

### TASK-0005: Extend `docs/out-of-scope.md`'s test-infrastructure-only classification

Status: TODO
Priority: P2
Area: Documentation
Type: Documentation
Evidence: §4.4/§5; `tests/basic_test.cpp:81-116`

Problem:
`docs/out-of-scope.md`'s "Unsupported APIs" table already documents
`GetTickCount`/`Sleep` as "kept for test infrastructure, not proven
game-required." This pass confirmed five more symbols fit the exact same
description — `CloseHandle`, `GetLastError`/`SetLastError`,
`RemoveDirectoryA`, `SetEnvironmentVariableA`, `access`/`_access` — all
called only by `tests/basic_test.cpp`, never by either game's own source.

Required work:
* Extend the existing `GetTickCount`/`Sleep` row (or add adjacent rows)
  in `docs/out-of-scope.md` to name all of: `CloseHandle`,
  `GetLastError`, `SetLastError`, `RemoveDirectoryA`,
  `SetEnvironmentVariableA`, `access`/`_access`.

Acceptance criteria:
* `docs/out-of-scope.md` accurately and completely lists every symbol in
  this "test-infrastructure-only" category.
* No behavior change; documentation only.

Out of scope:
* Do not remove or change any of these symbols' implementations — they
  are legitimately needed by free-api's own test suite.

---

### TASK-0006: Decide the fate of `_chdir`/`_getcwd` (zero call sites anywhere)

Status: DONE — decided (b): kept, documented as intentionally-unused-but-kept per project policy (docs/headers.md's direct.h row). See TASK-24H-0110/1216.
Priority: P3
Area: File / CRT
Type: Cleanup decision
Evidence: §4.4; `include/direct.h:21-22`; `src/crt_direct.cpp:18,23`

Problem:
Unlike the test-infrastructure-justified symbols in TASK-0005, `_chdir`
and `_getcwd` have **no call site anywhere** — not in either game, not in
free-api's own tests. `direct.h` must still exist for the live `_mkdir`
declaration, but these two specific functions currently have no evidenced
reason to be implemented.

Required work (pick one, document the choice in `docs/out-of-scope.md`):
* (a) Remove both functions' implementations and declarations (keeping
  `_mkdir`), if confident nothing will need them; or
* (b) Keep them, explicitly documented as "declared for `direct.h`
  completeness, not proven needed by either game" (matching the existing
  pattern for other harmless vestiges in §5's table).

Acceptance criteria:
* Whichever choice is made, `docs/out-of-scope.md` (or
  `docs/supported-apis.md`) reflects it accurately.
* If removed: both games still build and link successfully.

Out of scope:
* Do not remove `_mkdir` or anything else in `direct.h`.

---

### TASK-0007: Decide the fate of `OutputDebugStringW` (zero call sites, non-trivial implementation)

Status: DONE — decided (b): kept as-is, documented in docs/out-of-scope.md's "Unicode / W-suffixed API variants" section. See TASK-24H-0109/1215.
Priority: P3
Area: WinBase
Type: Cleanup decision
Evidence: §4.4; `src/winbase.cpp:70-76`

Problem:
`OutputDebugStringW` has a real UTF-16-to-narrow conversion loop
implementation but zero call sites anywhere (not games, not tests) — more
implementation effort than an unused symbol needs, and arguably in
tension with the project's own "do not implement real `W`-suffixed
behavior" policy (`docs/out-of-scope.md`'s Unicode section), since this
one function actually does real wide-string work.

Required work (pick one, document the choice):
* (a) Simplify `OutputDebugStringW` to a trivial safe stub (e.g. a no-op,
  matching the project's stated Unicode policy for other `W` functions);
  or
* (b) Keep the real implementation as-is, with a comment explaining why
  (e.g. "trivial to keep in sync with `OutputDebugStringA`, low
  maintenance cost").

Acceptance criteria:
* Whichever choice is made, `docs/supported-apis.md`/
  `docs/out-of-scope.md` reflects it accurately.
* `OutputDebugStringA` (the actually-used ANSI variant) is untouched.

Out of scope:
* Do not implement real wide-string behavior anywhere else.

---

### TASK-0008: Remove the stale "MIDI looping not supported" comment

Status: TODO
Priority: P3
Area: WinMM / Documentation
Type: Cleanup
Evidence: §4.3; `src/MidiMusic.cpp:15,527`; `../free-eggbert/src/blupi.cpp:562-575`; `../planetblupi/src/blupi.cpp:483-...`

Problem:
`src/MidiMusic.cpp` carries a `// Looping is not supported (TODO)` comment
in two places. This pass traced both games' `MM_MCINOTIFY` handlers and
confirmed both games already re-issue playback themselves on song-end
notification — native MCI-level looping was never required, and nothing
is actually missing. The comment is misleading, not a real gap.

Required work:
* Update the two comments in `src/MidiMusic.cpp` (file doc-comment header
  and `MidiMusicSendCommand`'s doc comment) to state that both games
  handle looping themselves via `MM_MCINOTIFY` re-triggering, and that
  native MCI-level looping is intentionally not implemented because it's
  never needed — not a "TODO."

Acceptance criteria:
* No stale "TODO" implying missing functionality remains for this
  behavior.
* No code/behavior change — comment-only.

Out of scope:
* Do not implement native MCI-level looping — confirmed unnecessary.

---

### TASK-0009: Investigate and fix (or document) `_findfirst`'s unbounded session-table growth

Status: DONE — fixed during the 24-hour autonomous session as `TASK-24H-0701`: `_findnext`'s exhaustion branch now auto-erases its own session entry (`src/crt_io.cpp`), so a caller that drains without calling `_findclose` (free-eggbert's design-mission picker) no longer leaks. New regression test `TestFindFirstFindNextRepeatedDrainWithoutCloseDoesNotLeakSession` added to `tests/test_file_paths.cpp`. Verified 17/17 in all three build modes.
Priority: P2
Area: File / CRT
Type: Bug investigation / hardening
Evidence: §4.4; `src/crt_io.cpp` (`g_findSessions`); `../free-eggbert/src/event.cpp:4736-4747` (`CEvent::ChangePhase`)

Problem:
`_findfirst`'s implementation stores each search session in a persistent
`g_findSessions` map, only erased by `_findclose`. free-eggbert's one call
site fully drains each session via `_findnext` in a loop but **never
calls `_findclose`** — every visit to the design-mission save/load screen
leaks one map entry, for the life of the process. Low severity (bounded
by how many times a player visits that specific screen, not a hot path),
but real and unbounded over a long session.

Required work (pick one):
* (a) Make `_findfirst`'s session cleanup automatic once a caller's loop
  fully drains it (i.e. `_findnext` returning "no more files" could
  proactively erase the session, since a real caller has no further use
  for the handle at that point — verify this doesn't break any caller
  that might legitimately call `_findnext` again after exhaustion, or
  that inspects the handle after the loop); or
* (b) Leave the real Win32-matching semantics (caller must call
  `_findclose`) and just document this as a known, low-severity,
  game-side (not free-api) resource-management gap.

Acceptance criteria:
* Whichever choice is made, it's verified against free-eggbert's actual
  call pattern (`event.cpp:4736-4747`) and existing tests
  (`tests/test_file_paths.cpp`) still pass.
* If (a) is chosen, add a regression test proving repeated
  `_findfirst`/drain-without-`_findclose` cycles don't grow the session
  table unboundedly.

Out of scope:
* Do not change `_findfirst`/`_findnext`'s observable return values or
  iteration order for any correctly-behaving caller.

---

### TASK-0010: Verify `LoadImageA` harmlessly ignores `LR_CREATEDIBSECTION` without `LR_LOADFROMFILE`

Status: TODO
Priority: P3
Area: GDI
Type: Test / verification
Evidence: §4.2; `../planetblupi/src/ddutil.cpp:90-95`

Problem:
planetblupi's `ddutil.cpp:90` calls `LoadImageA` with `LR_CREATEDIBSECTION`
but *without* `LR_LOADFROMFILE` — a genuine resource-load attempt that's
expected to fail and fall through to the file-based path
(`LR_LOADFROMFILE|LR_CREATEDIBSECTION`, lines 90-95). Existing
documentation (`include/winuser.h`'s `LoadImageA` doc comment) only
discusses the `LR_LOADFROMFILE` case explicitly; this specific
flag-combination-without-`LR_LOADFROMFILE` call shape has not been
directly confirmed to behave safely (return a clean failure, not a
crash/garbage handle) rather than merely "probably fine because it falls
through today."

Required work:
* Add a small regression test calling `LoadImageA` with
  `LR_CREATEDIBSECTION` alone (no `LR_LOADFROMFILE`, matching
  planetblupi's exact call shape) against a resource name that will never
  resolve, and assert it returns a clean failure (null/zero), matching
  the existing `FindResourceA`-miss contract.

Acceptance criteria:
* New test passes, confirming this exact call shape is safe.
* No implementation change expected — this is verification, not a known
  bug — unless the test reveals a real problem, in which case fix it
  minimally and re-run.

Out of scope:
* Do not implement any real resource-backed `LoadImageA` behavior.

---

### TASK-0011: Document the `free-direct`-bridge exception to the "cite free-eggbert/planetblupi" rule

Status: DONE — this was effectively already resolved in practice (docs/scope.md's "Boundary with free-direct" section and its "The actual bridge-exception surface" subsection, added by TASK-24H-0102, both name FreeApiRunWinMain and the three free_api_bridge.h functions as the exception), but the status field was never flipped and the exception wasn't cross-referenced from "## The rule"'s own exceptions bullet list (post-session-3 audit finding). Reconciled with a minimal edit: added a 4th bullet to docs/scope.md's "The only exceptions are:" list pointing to the existing "Boundary with free-direct" section, rather than duplicating its content. No behavior change; documentation only.
Priority: P3
Area: Scope policy / Documentation
Type: Documentation
Evidence: §4.5; `include/windows.h:99` (`FreeApiRunWinMain`); `../free-direct/src/Main.cpp:20`; `src/wingdi_dc.cpp:12,30` (`FreeApiCreateSurfaceDC`/`FreeApiDestroySurfaceDC`)

Problem:
`docs/scope.md`'s rule is "every public API must cite a real usage site —
`file:line` — in `../free-eggbert` or `../planetblupi`." Two real, load-
bearing symbols (`FreeApiRunWinMain`, and `FreeApiCreateSurfaceDC`/
`FreeApiDestroySurfaceDC`) have their only real caller in the sibling
`free-direct` project (which runs/renders on the games' behalf), not
literally in either game's own source. This is a legitimate exception
that already exists in practice but isn't written into the policy's own
wording, which could cause a future audit to incorrectly flag these as
scope violations.

Required work:
* Add a short, explicit exception clause to `docs/scope.md`'s citation
  rule: a symbol whose only real caller is `free-direct`, acting as the
  bridge/rendering layer *for* one of the two target games, is an
  acceptable citation (cite the `free-direct` call site instead), since
  `free-direct` itself only exists to serve these two games.

Acceptance criteria:
* `docs/scope.md` explicitly documents this exception.
* No behavior change; documentation only.

Out of scope:
* Do not broaden this into a general "any sibling project's usage
  counts" rule — it should stay scoped to `free-direct` specifically,
  since that's the only sibling project in this relationship.

---

### TASK-0012: Fix cross-references to `plan.md` section numbers in other docs

Status: DONE — fixed immediately as part of this rewrite (mechanical, low-risk, directly caused by it): `docs/headers.md` (§3.1→§4, twice), `docs/out-of-scope.md` (section 10→9, with a "prior version" qualifier since the content it describes predates this rewrite), `docs/supported-apis.md` (§6→§4, plus a new note pointing at TASK-0001's 28-symbol gap). `docs/scope.md` and `docs/out-of-scope.md`'s other three "section 5" references already matched the new structure unchanged.
Priority: P3
Area: Documentation
Type: Cleanup
Evidence: This rewrite changed `plan.md`'s section structure.

Problem:
`docs/supported-apis.md`, `docs/out-of-scope.md`, and `docs/headers.md`
each reference specific `plan.md` section numbers (e.g. "`plan.md` §6",
"`plan.md` section 5", "`plan.md` §3.1") that were valid for the prior
version of this document. This rewrite kept §5 as "Out-of-Scope" (so
those references still resolve correctly) but changed what used to be
§3/§4/§6/§10.

Required work:
* Grep all of `docs/*.md` for `plan\.md` section references and update
  any that now point to the wrong section, given this document's new
  structure (§3 Methodology, §4 Current State, §5 Out-of-Scope, §6 Task
  List, §7 Coverage Map, §8 Anti-Bloat Policy, §9 Final Summary).

Acceptance criteria:
* Every `plan.md` section cross-reference in `docs/*.md` resolves to the
  section it actually describes.

Out of scope:
* Do not restructure `plan.md` further to chase this — fix the
  referencing docs instead.

---

## 7. Mandatory Task Themes — Coverage Map

* **Header/documentation hygiene** → TASK-0001, TASK-0002, TASK-0012.
* **GDI correctness/hardening** → TASK-0002, TASK-0003, TASK-0004,
  TASK-0010.
* **Scope-policy documentation** → TASK-0005, TASK-0006, TASK-0007,
  TASK-0011.
* **WinMM/MIDI cleanup** → TASK-0008.
* **File/CRT hardening** → TASK-0006, TASK-0009.

This pass found no gaps in WinUser's core window/message/input subset,
no gaps in Resources beyond documentation, and no gaps in Joystick — all
previously-completed work (`TASK-0001`–`0128` in the prior version of this
document) was independently re-confirmed still correct, not just assumed
so.

---

## 8. Anti-Bloat Completion Policy

A task from §6 may be marked complete only when:

* It is justified by actual usage in `../free-eggbert` or
  `../planetblupi` (per §4's evidence), or by explicit scope-control
  cleanup/documentation per §5.
* The implementation compiles.
* Existing tests still pass.
* New supported behavior has a focused test derived from real game
  usage, not a synthetic/hypothetical scenario.
* No unrelated WinAPI symbol is added as a side effect.
* No generic subsystem (resource compiler, DirectX layer, threading
  framework, Unicode runtime, etc.) is introduced.
* Unsupported behavior remains unsupported and is documented as such (in
  `docs/out-of-scope.md`), rather than being silently half-implemented.
* Free API stays smaller and more focused after the task than a
  hypothetical "implement everything Win32-shaped" alternative would have
  left it — every addition should be traceable back to one of the two
  named games (or the documented `free-direct`-bridge exception, §5/
  TASK-0011).

---

## 9. Final Requirement — Summary

This fresh, full re-audit's conclusion: **Free API's current public
surface (101 functions, 231 `#define`s, 23 headers) is already a tight,
evidenced subset of Win32 — every symbol traces to a real citation in one
of the two target games, the documented test-infrastructure exception, or
the newly-documented `free-direct`-bridge exception (TASK-0011).** No
scope violations (implemented-but-truly-unjustifiable symbols) were found
beyond the small, already-mostly-accepted vestige list in §5.

The highest-value remaining work is **not new features** — it's closing
the two small real correctness gaps found (`TASK-0003` scaled `StretchBlt`
axis asymmetry, `TASK-0009` `_findfirst` session leak) and the header-
hygiene gap (`TASK-0002`, the undeclared `free-direct`-shared functions).
Everything else in §6 is documentation/classification cleanup with zero
behavior change. This is a strong signal that the project's long-running
discipline (`docs/scope.md`'s citation rule, the anti-bloat policy in §8,
and the inline `@note Status:` self-documentation this pass verified as
trustworthy) has kept Free API exactly as small as it should be.

---

# 24-Hour Autonomous Stabilization Backlog

Produced during a user-approved 24-hour autonomous stabilization session
(see `docs/audit-24h-free-api.md` for the full evidence base this backlog
is derived from). This section is additive to the `TASK-0001`–`TASK-0012`
backlog above — it does not replace it. Every task below cites concrete
`file:line` evidence (from direct source reading this session, or from
`docs/audit-24h-free-api.md`) and follows the priority rubric:

* **P0** — build failures, target-game startup blockers, game-loop/
  message/timer/input/file/resource defects that can break running either
  game, incorrect behavior already used by either game, unsafe
  target-game resource fallback.
* **P1** — important correctness gaps in used APIs, missing tests for
  high-value used behavior, logging/diagnostics that can affect gameplay,
  integration with `free-direct` where the bridge is real.
* **P2** — documentation drift, header hygiene, low-risk implementation
  cleanup, optional but evidenced target-game behavior.
* **P3** — pure docs cleanup, compile-only stub classification,
  nice-to-have verification.

175 atomic tasks originally (177 as of session 4, which added `TASK-24H-1221`/
`1222` for the previously-untracked MIDI-audio/rendering human sign-off
gap); the per-area breakdown below is a point-in-time snapshot from that
point and has not been recomputed since — grep `plan.md` for
`^### TASK-24H-` + `Status: TODO`/`DONE` for current, accurate counts.
Session 5 closed the entire P2/P3 backlog that existed at that point (91
tasks); a subsequent deep audit (`audit.md`, 2026-07-09) added 16 more
tasks (`TASK-24H-1229`-`1244`, see "Deep Audit Follow-up" section) derived
from 5 independent correctness/performance/memory/edge-case/risk reviews.
All 16 (plus the pre-existing `TASK-24H-0706`) were implemented and pushed;
a second, ground-up re-audit against the resulting source (`audit.md`,
2026-07-09, same date but a full rewrite of the file, not an update) then
added 6 more tasks (`TASK-24H-1245`-`1250`, see "Deep Audit Follow-up #2"
section). 205 tasks total as of that addition. Original per-area breakdown (stale,
kept for historical context only): Build/Integration (17), Scope/Headers
(18), WinUser message-loop (17), Window/Cursor (16), Input (12), Timers
(9), GDI (17), Files (14), Resources (8), WinMM/MIDI/MCI (11), Joystick
(5), Diagnostics (13), Documentation (20).

## Build and Integration

### TASK-24H-0001: Fix the dead-on-arrival "sibling-vendored SDL3" CMake fallback path
Status: DONE — removed the dead fallback block (CMakeLists.txt:33-48 in the pre-fix file); standalone configure with no override now fails with free-api's own clear guidance message instead of the sibling script's misattributed "missing submodule" error. Updated docs/cmake-options.md (dropped tier 3, renumbered, added the SDL_VIDEODRIVER=dummy test note from TASK-24H-0002 while in the file). Verified 17/17 in all three build modes.
Priority: P0
Area: Build
Type: Bugfix
Evidence: CMakeLists.txt:33-58 (sibling-vendored fallback + final FATAL_ERROR guard); `../free-eggbert/cmake/ThirdPartySDL.cmake:13` (`set(_third_party_root "${CMAKE_SOURCE_DIR}/third_party")`); `../free-eggbert/CMakeLists.txt:88,100` and `../planetblupi/CMakeLists.txt:89,102` (both call `configure_vendored_sdl()` before `add_subdirectory(../free-api ...)`); direct empirical repro this session
Depends on: None

Problem:
free-api's third SDL3-acquisition tier ("sibling-vendored, developer convenience", CMakeLists.txt:33-48) reuses whichever sibling game's `cmake/ThirdPartySDL.cmake` is found and calls `configure_vendored_sdl()`. That function unconditionally resolves its vendored dependency root as `${CMAKE_SOURCE_DIR}/third_party` — the *top-level* project's source directory, not the sibling game's own directory. Both real target games call `configure_vendored_sdl()` themselves before `add_subdirectory(../free-api ...)`, so by the time free-api's own CMakeLists.txt would reach this fallback block in either game's real build, `SDL3::SDL3` already exists and the block is skipped entirely — this fallback can therefore only be *reached* when free-api itself is the top-level project (standalone), in which case `CMAKE_SOURCE_DIR` is free-api's own root, which never contains `third_party/SDL`. Verified directly this session: running `cmake -S . -B build` from free-api's own directory (both `../free-eggbert` and `../planetblupi` present as siblings, no `-DFREE_API_USE_SYSTEM_SDL3` and no override) fails with `CMake Error at .../free-eggbert/cmake/ThirdPartySDL.cmake:16 (message): Missing vendored dependency 'SDL' in .../free-api/third_party. Run: git submodule update --init --recursive` — a confusing, wrongly-attributed error, since free-api has no `third_party/` directory and no `.gitmodules` at all, by design (CMakeLists.txt:12 states free-api "never vendors SDL3 itself"). This documented, advertised build tier is structurally unable to ever succeed, and its failure actively misleads a developer into trying to `git submodule update` a repo with nothing to update.

Required work:
- Either (a) remove the dead sibling-vendored fallback block (CMakeLists.txt:33-48) entirely, since it can never succeed, and simplify to the two acquisition tiers that actually work (parent-provided, system via `-DFREE_API_USE_SYSTEM_SDL3=ON`); or (b) skip calling `configure_vendored_sdl()` unless `CMAKE_SOURCE_DIR` actually equals the sibling game's own root, so control falls through to the existing, well-worded `FATAL_ERROR` at CMakeLists.txt:50-58 instead of the sibling script's misleading one.
- Update `docs/cmake-options.md`'s "Sibling-vendored (developer convenience)" section to accurately describe the chosen behavior.

Acceptance criteria:
- Running `cmake -S . -B <fresh-dir>` from free-api's own root, with a sibling game present and no `-DFREE_API_USE_SYSTEM_SDL3`/`-DFREE_API_TARGET_GAME` override, either succeeds or fails with free-api's own clear three-option guidance message — never the sibling script's misattributed "missing submodule" error.
- `docs/cmake-options.md` matches the actual resulting behavior.
- The two real, working acquisition paths (parent-provided via either game, `-DFREE_API_USE_SYSTEM_SDL3=ON`) are unaffected; 17/17 tests still pass via `../free-eggbert` (Ninja), `../planetblupi` (Make), and standalone system-SDL3 builds.

Out of scope:
- Do not attempt to make the sibling-vendored path actually work by having free-api reach into a sibling's `third_party/SDL` directly — that would mean free-api vendoring SDL3 through a side channel, which CMakeLists.txt:10-22 explicitly says it must never do.
- Do not change `../free-eggbert/cmake/ThirdPartySDL.cmake` or `../planetblupi/cmake/ThirdPartySDL.cmake` (different repos) as part of this task.

---

### TASK-24H-0002: Document the still-required SDL_VIDEODRIVER=dummy/SDL_AUDIODRIVER=dummy test env vars
Status: DONE — already satisfied by existing content: docs/cmake-options.md's "Tests" section already prefixes the example ctest invocation with SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy and explains why (verified this session, no edit needed).
Priority: P2
Area: Build
Type: Documentation
Evidence: `docs/cmake-options.md` "## Tests" section (no mention of `SDL_VIDEODRIVER`/`SDL_AUDIODRIVER`); `NEXT.md` §7; direct empirical re-confirmation this session
Depends on: None

Problem:
`docs/cmake-options.md`'s "## Tests" section documents running tests as plain `ctest --test-dir build`, with no mention of `SDL_VIDEODRIVER=dummy`/`SDL_AUDIODRIVER=dummy`. `NEXT.md` §7 separately says these env vars are "NOT optional" under this environment's default Wayland display. Re-verified fresh this session: `ctest` without the env vars fails `test_winuser_regressions` (6 sub-assertions: `ClientToScreen`/`ScreenToClient`/`GetCursorPos` exactness checks) under the current Wayland session; with `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy`, all 17 tests pass. A developer following only `docs/cmake-options.md` — the file whose stated purpose is to be the build-options reference — would hit this exact spurious failure with no warning.

Required work:
- Add the `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy` prefix to `docs/cmake-options.md`'s example `ctest` invocation in the "## Tests" section, with a one-line note (mirroring `NEXT.md`'s existing explanation) that this avoids spurious window-position-related failures under non-dummy display backends.

Acceptance criteria:
- `docs/cmake-options.md`'s documented test command, copy-pasted verbatim, passes 17/17 in this environment.
- No behavior change; documentation only.

Out of scope:
- Do not change `test_winuser_regressions.cpp`'s assertions or add a code-level workaround for the Wayland-specific failures — this is a documented, closed environment artifact (`NEXT.md` §5/§9), not a bug to fix.

---

### TASK-24H-0003: Confirm and record FREE_API_TARGET_GAME=free-eggbert override without the full free-eggbert build tree
Status: DONE — re-ran the exact command sequence in a scratch build dir (free-eggbert's own CMake project never configured): known-ID verification passed, all 308 used string IDs verified present, clean build, 25/25 pass. Documented as a worked example in docs/cmake-options.md.
Priority: P2
Area: Build
Type: Verification
Evidence: CMakeLists.txt:76-88,131-150 (override + free-eggbert branch, sibling-root fallback at :137-141); direct empirical confirmation this session
Depends on: None

Problem:
CMakeLists.txt:131-150 lets `-DFREE_API_TARGET_GAME=free-eggbert` force free-eggbert's `REQUIRE_STRINGS`/known-ID/used-ID gating even when free-api is not being built as free-eggbert's own subdirectory, by falling back to reading `../free-eggbert/resource/Eggbert2.rc` by sibling path (CMakeLists.txt:140). This is a materially different configuration from the three build modes already confirmed this session ("via ../free-eggbert (Ninja)", "via ../planetblupi (Make)", "standalone") — it exercises the full fail-loud verification pipeline without free-eggbert's own CMake project ever being configured, and this exact scenario had not been independently re-confirmed and written down anywhere.

Required work:
- Run `cmake -S . -B build -DFREE_API_USE_SYSTEM_SDL3=ON -DFREE_API_TARGET_GAME=free-eggbert -DFREE_API_BUILD_TESTS=ON && cmake --build build -j"$(nproc)" && cd build && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest` from free-api's own directory (confirmed this session: "known-ID verification passed for 'free-eggbert'", "all 308 unique used string ID(s) ... verified present", clean build, 17/17 pass).
- Add this exact command sequence and its confirmed result to `docs/cmake-options.md`'s `FREE_API_TARGET_GAME` section, which currently only shows the bare flag with no full worked example.

Acceptance criteria:
- The documented command sequence is copy-pasteable and passes 17/17 as written.
- `docs/cmake-options.md` reflects it.

Out of scope:
- Do not change `cmake/ExtractStringTable.cmake`'s gating logic — this task is verification and documentation only.

---

### TASK-24H-0004: Confirm and record FREE_API_TARGET_GAME=planetblupi override without the full planetblupi build tree
Status: DONE — re-ran the exact command sequence in a scratch build dir (planetblupi's own CMake project never configured): known-ID verification passed, all 257 used string IDs verified present, clean build, 25/25 pass. Documented as a worked example in docs/cmake-options.md.
Priority: P2
Area: Build
Type: Verification
Evidence: CMakeLists.txt:76-88,151-171 (override + planetblupi branch, sibling-root fallback at :157-161); direct empirical confirmation this session
Depends on: None

Problem:
Same structural gap as TASK-24H-0003, for the planetblupi branch (CMakeLists.txt:151-171, sibling fallback at :157-161). Not independently re-confirmed and written down before this task.

Required work:
- Run `cmake -S . -B build -DFREE_API_USE_SYSTEM_SDL3=ON -DFREE_API_TARGET_GAME=planetblupi -DFREE_API_BUILD_TESTS=ON && cmake --build build -j"$(nproc)" && cd build && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest` from free-api's own directory (confirmed this session: "known-ID verification passed for 'planetblupi'", "all 257 unique used string ID(s) ... verified present", clean build, 17/17 pass).
- Add this exact command sequence and confirmed result to `docs/cmake-options.md`'s `FREE_API_TARGET_GAME` section, alongside TASK-24H-0003's free-eggbert example.

Acceptance criteria:
- The documented command sequence is copy-pasteable and passes 17/17 as written.
- `docs/cmake-options.md` reflects it.

Out of scope:
- Do not change `cmake/ExtractStringTable.cmake`'s gating logic — verification and documentation only.

---

### TASK-24H-0005: Verify the free-direct bridge build and document its FREE_API_BUILD_TESTS=OFF coverage gap
Status: DONE — re-verified this session (standalone ../free-direct build succeeds, links cleanly against include/free_api_bridge.h) and documented in docs/cmake-options.md's new "The ../free-direct bridge build" section, including the FREE_API_BUILD_TESTS=OFF coverage-gap caveat.
Priority: P1
Area: Integration
Type: Verification
Evidence: `../free-direct/CMakeLists.txt:11-14` (`if(NOT TARGET free-api)` guard, `set(FREE_API_BUILD_TESTS OFF)`, `add_subdirectory(../free-api FREE_API)`); `../free-direct/src/Main.cpp:1-20` (real bridge usage: `FreeApiRunWinMain`, `RegisterClassA`, `CreateWindowExA`, `DirectDrawCreate`); direct empirical confirmation this session
Depends on: None

Problem:
`../free-direct/CMakeLists.txt:11-14` builds free-api as a subdirectory but forces `FREE_API_BUILD_TESTS OFF` before doing so. Configuring and building `../free-direct` standalone this session (`cmake -S . -B build -DFREE_API_USE_SYSTEM_SDL3=ON && cmake --build build`) confirms the whole chain configures, builds, and links successfully (`libfree-api.a` → `libfree-direct.a` → the `FREE_DIRECT` demo executable, which calls `FreeApiRunWinMain`/`RegisterClassA`/`CreateWindowExA`/`DirectDrawCreate` — real bridge usage per `../free-direct/src/Main.cpp`). However, because `FREE_API_BUILD_TESTS` is forced off, free-api's own 17-test CTest suite is never built or registered in this configuration — a passing free-direct build gives zero signal on whether free-api's own regression suite still passes, and this gap is currently undocumented anywhere.

Required work:
- Record the confirmed-working `../free-direct` standalone build command and result (configure → build → link succeeds; `FREE_DIRECT` executable produced) in `docs/cmake-options.md` or a cross-reference from it.
- Explicitly document that building via `../free-direct` does not exercise free-api's own test suite (`FREE_API_BUILD_TESTS` is forced `OFF`), so a passing free-direct build proves configure/build/link only, not free-api's own regression coverage.

Acceptance criteria:
- `../free-direct`'s standalone build (as a subdirectory consumer of free-api) is confirmed to configure/build/link cleanly and the exact command is written down.
- The `FREE_API_BUILD_TESTS=OFF` test-coverage caveat is explicitly documented somewhere a reader of the free-direct integration story would see it.

Out of scope:
- Do not change `../free-direct/CMakeLists.txt` to enable free-api's tests by default — that is free-direct's own build-time choice in a different repo, not free-api's to override.
- Do not add a free-api-side mechanism to force tests on regardless of the consumer's setting.

---

### TASK-24H-0006: Verify the diamond add_subdirectory(free-api)/add_subdirectory(free-direct) case from a single game build
Status: DONE — re-verified this session via a real Ninja rebuild of ../free-eggbert's FREEDIRECT backend: exactly 17 (not 34) CTest tests registered, both SPEEDY_BLUPI_WINDOWS and FREE_DIRECT built and linked cleanly, confirming free-direct's `if(NOT TARGET free-api)` guard still works. Documented in docs/cmake-options.md's new "The ../free-direct bridge build" section.
Priority: P1
Area: Integration
Type: Verification
Evidence: `../free-eggbert/CMakeLists.txt:100,105,107` and `../planetblupi/CMakeLists.txt:102,109,111` (both games' FREEDIRECT-backend branch calls `add_subdirectory(../free-api FREE_API)` AND `add_subdirectory(../free-direct FREE_DIRECT ...)` in the same configure run); `../free-direct/CMakeLists.txt:11` (`if(NOT TARGET free-api)` guard); direct empirical confirmation this session
Depends on: TASK-24H-0005

Problem:
When either target game builds with its `FREEDIRECT` backend, its own CMakeLists.txt `add_subdirectory()`s both `../free-api` and `../free-direct` in the same configure run. `free-direct`'s own CMakeLists.txt separately tries to `add_subdirectory(../free-api ...)` itself, guarded by `if(NOT TARGET free-api)` (`../free-direct/CMakeLists.txt:11`), specifically to avoid double-registering the `free-api` target (and, transitively, its 17 CTest tests) in this diamond-dependency shape. This guard's correctness had not been independently exercised and confirmed as a named check before this session. Re-running `../free-eggbert`'s Ninja build fresh this session (confirmed via its own `-- Using FREEDIRECT backend` configure message) built exactly one `free-api` target, one `free-direct` target, both `SPEEDY_BLUPI_WINDOWS` and `FREE_DIRECT` executables, and registered exactly 17 (not 34) CTest tests, all passing.

Required work:
- Record, in `docs/cmake-options.md` or `NEXT.md`, that the `FREEDIRECT`-backend build path (both games) exercises the diamond `game → free-api` + `game → free-direct → free-api` dependency shape, that `../free-direct/CMakeLists.txt:11`'s `if(NOT TARGET free-api)` guard is what prevents double-registration, and that this was confirmed this session to produce exactly 17 CTest tests (not 34) and a clean build.

Acceptance criteria:
- The diamond-dependency scenario's confirmed-passing status is written down with its evidence (test count, both executables present).
- No code change — this is a verification/documentation task confirming an already-correct guard.

Out of scope:
- Do not modify the guard in `../free-direct/CMakeLists.txt` — it already works correctly per this session's direct verification.

---

### TASK-24H-0007: Fix or document the incomplete install()/export packaging
Status: DONE — removed the dangling EXPORT free-api-targets clause (no matching install(EXPORT...) existed, so find_package() could never have worked; no evidenced consumer needs it, both games/free-direct use add_subdirectory()). Added install(DIRECTORY include_non_windows/...) gated the same way as the compile-time include path (if(NOT WIN32)). Verified via a real cmake --install into a scratch prefix, before/after: Windows.h/WinUser.h/sys/timeb.h are now installed, no stray EXPORT artifacts. Verified 25/25 in all three build trees.
Priority: P2
Area: Build
Type: Bugfix
Evidence: CMakeLists.txt:531-540 (install rules); CMakeLists.txt:264-268 (`include_non_windows` only added to `freeapi_compat_headers` `if(NOT WIN32)`); direct empirical `cmake --install` test this session
Depends on: None

Problem:
CMakeLists.txt:531-536 installs the `free-api`/`freeapi_compat_headers` targets under an export set named `free-api-targets`, but there is no matching `install(EXPORT free-api-targets ...)` call anywhere in the file, and no generated `*Config.cmake`/`*ConfigVersion.cmake`. Running `cmake --install` this session confirms the installed tree contains only `lib/libfree-api.a` and `include/*.h` (23 files) — no `lib/cmake/free-api/*.cmake` at all, so `find_package(free-api CONFIG)` could never locate an installed copy. Separately, `install(DIRECTORY include/ ...)` (CMakeLists.txt:538-540) never installs `include_non_windows/` (`Windows.h`, `WinUser.h`, `sys/timeb.h` — 3 files), even though `freeapi_compat_headers` adds that directory to its public include path whenever `NOT WIN32` (CMakeLists.txt:264-268) — confirmed by the same `cmake --install` test, whose output contained zero `include_non_windows`-sourced headers. An installed, non-Windows copy of free-api is therefore missing headers its own build depends on.

Required work:
- Either complete the export properly (add `install(EXPORT free-api-targets NAMESPACE free-api:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/free-api)` plus a minimal generated Config file), or remove the unused `EXPORT free-api-targets` clause from `install(TARGETS ...)` if `find_package()`-based consumption is not actually a goal (neither target game nor `free-direct` uses it — both consume free-api via `add_subdirectory()`).
- Add an `install(DIRECTORY include_non_windows/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} ...)` call gated the same way as the compile-time include path (`if(NOT WIN32)`), or explicitly document that `cmake --install` is only intended for Windows targets if that is the real intent.

Acceptance criteria:
- `cmake --install <prefix>` on a non-Windows build either produces a fully working, `find_package()`-consumable installed copy (headers present, package config present), or the `EXPORT`/install surface is reduced to honestly match what it actually delivers, with the gap documented.
- Existing tests still pass; no change to how either target game or `free-direct` consumes free-api (both use `add_subdirectory()`, unaffected either way).

Out of scope:
- Do not add install-time SDL3 dependency propagation/vendoring — out of scope for this narrow packaging fix.
- Do not change how either target game or `free-direct` obtains free-api (`add_subdirectory()` stays the primary, tested consumption path).

---

### TASK-24H-0008: Correct the FREE_API_TARGET_GAME=standalone docstring's placeholder-only claim
Status: DONE — reworded CMakeLists.txt's FREE_API_TARGET_GAME CACHE STRING docstring and docs/cmake-options.md's standalone bullet to state accurately that standalone only disables fail-loud verification gating, not that it forces placeholder-only text.
Priority: P2
Area: Build
Type: Documentation
Evidence: CMakeLists.txt:76-84 (CACHE STRING docstring: "'standalone' forces the ungated placeholder-fallback path"); CMakeLists.txt:117-118,172-189 (`standalone` and unresolved-`auto` both fall into the same sibling-lookup block); direct empirical confirmation this session
Depends on: None

Problem:
CMakeLists.txt:82-83's cache-variable docstring says `standalone` "forces the ungated placeholder-fallback path even if `CMAKE_PROJECT_NAME` would otherwise select a game." In practice, `standalone` only disables the fail-loud `REQUIRE_STRINGS`/`VERIFY_ID`/`USED_IDS_FILE` gating (CMakeLists.txt:117-118 sets `_free_api_resolved_game` empty) — it does not force placeholder-only text. Confirmed this session: configuring with `-DFREE_API_TARGET_GAME=standalone` while `../free-eggbert` exists as a sibling still extracts and compiles all 364 of free-eggbert's real STRINGTABLE strings (including the real `"Quit BLUPI"` for ID 106) via the same developer-convenience sibling lookup `auto` uses (CMakeLists.txt:172-189) — it is simply never verified. A reader of the docstring alone would reasonably expect `standalone` to always yield an empty/placeholder table, which is only true when no sibling `.rc` happens to be discoverable.

Required work:
- Reword the `FREE_API_TARGET_GAME` CACHE STRING docstring (CMakeLists.txt:76-84) and the corresponding section of `docs/cmake-options.md` to state accurately that `standalone` disables fail-loud verification, not that it guarantees placeholder-only text — real text still opportunistically appears if a sibling game's `.rc` happens to be present on disk.

Acceptance criteria:
- The docstring and `docs/cmake-options.md` describe the actually-observed behavior (verified this session).
- No behavior change — documentation only.

Out of scope:
- Do not change `standalone`'s actual behavior (e.g. do not make it suppress the sibling-lookup convenience path) — that would be a scope change to an intentionally-documented developer-convenience feature, not a doc fix.

---

### TASK-24H-0009: Consolidate scattered build/test command documentation into one canonical reference
Status: DONE — docs/cmake-options.md already accumulated the dummy-driver requirement, FREE_API_TARGET_GAME override examples, and the free-direct bridge build section across earlier sessions; this session added docs/testing.md as the companion test-workflow reference (TASK-24H-1208) and linked both from Documentation.md/README.md. NOTE: did not trim NEXT.md §7's command listing to a bare pointer as this task's own text asked, because TASK-24H-1208's out-of-scope clause explicitly says "do not remove the commands from NEXT.md §7 -- that section can stay as a session-log convenience" -- the two tasks' instructions conflict; kept NEXT.md's commands intact per the more conservative instruction.
Priority: P2
Area: Build
Type: Documentation
Evidence: `docs/cmake-options.md` (no `SDL_VIDEODRIVER=dummy`, no `FREE_API_TARGET_GAME` full worked example, no `../free-direct` build mode); `NEXT.md` §7; `examples/README.md` (its own separate build snippet)
Depends on: TASK-24H-0002

Problem:
The exact, currently-passing build/test commands for free-api's supported configurations are split across three files with no single source of truth: `docs/cmake-options.md` (the file whose stated purpose is to document build modes) omits the dummy-SDL-driver test requirement and the `../free-direct` build mode entirely; `NEXT.md` §7 has more complete, accurate commands but is explicitly a rotating handoff document, not a stable reference; `examples/README.md` has its own separate build snippet. This is exactly the "scattered, possibly inconsistent" documentation state this session's brief flagged.

Required work:
- Make `docs/cmake-options.md` the single canonical reference for every confirmed-passing build/test command (standalone, both `FREE_API_TARGET_GAME` overrides, both real target-game build modes, the `../free-direct` bridge build), incorporating TASK-24H-0002's dummy-driver fix and TASK-24H-0003/0004's override examples.
- Replace the corresponding prose in `NEXT.md` §7 with a short pointer to `docs/cmake-options.md` instead of duplicating the commands, so future edits only need to happen in one place.

Acceptance criteria:
- Every command documented in `docs/cmake-options.md` is copy-pasteable and passes as written.
- `NEXT.md` §7 no longer duplicates command text that now lives in `docs/cmake-options.md`.

Out of scope:
- Do not remove `NEXT.md`'s narrative handoff content (current status, blockers, etc.) — only the duplicated command listing.

---

### TASK-24H-0010: Add CTest LABELS grouping tests by subsystem
Status: DONE — added set_tests_properties(...PROPERTIES LABELS...) calls grouping all 25 current tests (grew from 17 since this task was written) into winuser/gdi/winmm/file/resources/integration/hygiene. Verified ctest -L <label> runs a sensible non-empty subset for each label, and a plain ctest with no -L filter still runs all 25, unaffected. Verified 25/25 in all three build trees.
Priority: P2
Area: Build
Type: Implementation
Evidence: CMakeLists.txt:330-499 (17 `add_test()` calls; zero uses of `set_tests_properties(...PROPERTIES LABELS...)`, confirmed via grep this session)
Depends on: None

Problem:
free-api registers 17 CTest tests spanning distinct subsystems (WinUser: `test_winuser_regressions`, `test_input_pipeline`; GDI: `test_gdi_regressions`; WinMM/MCI: `test_mci_sequences`, `test_mci_avivideo_regressions`, `test_timer_regressions`; file/CRT: `test_file_regressions`, `test_file_paths`, `test_timeb`; resources/strings: `test_resources`, `test_loadstring_regressions`; joystick: `test_joystick_regressions`; full-loop integration: `test_planetblupi_loop`, `test_eggbert_loop`; header/build hygiene: `test_header_compile`, `check_no_hardcoded_paths`; smoke: `basic_test`) but none of the 17 `add_test()` calls set a `LABELS` property, so there is no way to run e.g. only the GDI-related subset (`ctest -L gdi`) when iterating on one subsystem.

Required work:
- Add `set_tests_properties(<test> PROPERTIES LABELS "<subsystem>")` calls grouping the 17 existing tests into a small number of sensible labels (e.g. `winuser`, `gdi`, `winmm`, `file`, `resources`, `integration`, `hygiene`).

Acceptance criteria:
- `ctest -L <label>` runs a sensible, non-empty subset of tests for each label used.
- `ctest` with no `-L` filter still runs all 17 tests, unaffected.
- Existing tests still pass.

Out of scope:
- Do not rename or restructure any existing test binary/target — labels only.
- Do not make labels a required/enforced part of CI — this is opt-in tooling for local iteration.

---

### TASK-24H-0011: Add self-test coverage for cmake/CheckNoHardcodedPaths.cmake's own detection logic
Status: DONE — refactored CheckNoHardcodedPaths.cmake's detection loop into a reusable free_api_check_no_hardcoded_paths() function (guarded so the real repo-wide check still only runs when invoked directly via cmake -P, not when include()d), added cmake/CheckNoHardcodedPathsSelfTest.cmake which includes the real script and exercises the real function against a known-bad temp fixture (asserts flagged) and a clean temp fixture (asserts not flagged), registered as check_no_hardcoded_paths_self_test. Verified the self-test genuinely catches breakage: temporarily removed "/home/" from the real pattern list, confirmed the self-test failed with a clear message, reverted, confirmed passing again. Verified 26/26 in all three build trees.
Priority: P2
Area: Build
Type: Test
Evidence: `cmake/CheckNoHardcodedPaths.cmake` (whole file, patterns at :23-28); CMakeLists.txt:342-352 (`check_no_hardcoded_paths` test only runs the checker against the real repo)
Depends on: None

Problem:
`check_no_hardcoded_paths` (CMakeLists.txt:351-352, running `cmake/CheckNoHardcodedPaths.cmake`) only proves the checker currently finds nothing wrong in `CMakeLists.txt`/`cmake/*.cmake` as they stand today — confirmed this session that its scope is already complete (a repo-wide search found no other `.cmake`/`CMakeLists.txt` files it should but doesn't cover: `git ls-files | grep -iE "\.cmake$|CMakeLists\.txt$"` returns exactly the three files already scanned). There is, however, no test proving the checker's own pattern-matching logic (`_suspicious_patterns`: `/home/`, `/rv/`, `/Users/`, `C:\\`, `cmake/CheckNoHardcodedPaths.cmake:23-28`) still actually fires a `WARNING`/`FATAL_ERROR` when one of those patterns genuinely appears — a future edit narrowing or breaking the pattern list (e.g. an accidental regex-escaping change) would silently pass this test forever, since it only ever exercises the "nothing found" branch.

Required work:
- Add a small script-mode self-test (e.g. `cmake/CheckNoHardcodedPathsSelfTest.cmake`, or a mode flag on the existing script) that writes a temporary file containing one of the known-bad patterns, runs the detection logic against it, and asserts the checker actually flags it — plus a companion assertion that a clean sample is not flagged.
- Register it as a new CTest test (e.g. `check_no_hardcoded_paths_self_test`).

Acceptance criteria:
- The new self-test fails if `cmake/CheckNoHardcodedPaths.cmake`'s pattern list or matching logic is broken (verify by temporarily breaking it locally and confirming the new test catches it, then reverting).
- Existing `check_no_hardcoded_paths` test and all other 17 tests still pass.

Out of scope:
- Do not expand `_suspicious_patterns` beyond the existing four entries — no new pattern was found missing.
- Do not turn this into a general static-analysis tool.

---

### TASK-24H-0012: Re-confirm and script the ../free-eggbert (Ninja) 17/17 build path
Status: DONE — re-ran the exact sequence fresh (cmake . && ninja, then ctest under FREE_API/): 25/25 pass (was 17/17 when this task was written; suite has grown). Documented as a canonical, copy-pasteable entry in docs/cmake-options.md.
Priority: P2
Area: Build
Type: Verification
Evidence: `NEXT.md` §7 (prose-only command); direct empirical re-confirmation this session (`cd ../free-eggbert/cmake-build-debug && cmake . && ninja -j"$(nproc)"`, then `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest` under `FREE_API/`: 17/17 passed)
Depends on: None

Problem:
The `../free-eggbert` Ninja build path is free-api's primary real-world consumption mode and was re-confirmed passing 17/17 fresh this session, but the only record of "how to check this" is prose in `NEXT.md` §7 (a rotating handoff document). There is no single, repeatable command captured in a stable reference that a future session could invoke to catch a regression in this specific path.

Required work:
- Capture the exact working sequence (`cd ../free-eggbert/cmake-build-debug && cmake . && ninja -j"$(nproc)"`, then `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure` from `FREE_API/`) as a documented, canonical entry in `docs/cmake-options.md` (per TASK-24H-0009's consolidation), noting this build path also exercises the `FREEDIRECT` backend diamond dependency (TASK-24H-0006) when that backend is selected.

Acceptance criteria:
- The documented sequence, run verbatim, passes 17/17 (already re-confirmed this session).
- Documented in the canonical build-command reference, not only in `NEXT.md`.

Out of scope:
- Do not change free-eggbert's own CMakeLists.txt (a different repo).
- Do not add a CI pipeline — this task is documentation/verification only.

---

### TASK-24H-0013: Re-confirm and script the ../planetblupi (Make) 17/17 build path
Status: DONE — re-ran the exact sequence fresh (cmake . && make, then ctest under FREE_API/): 25/25 pass (was 17/17 when this task was written; suite has grown). Documented as a canonical, copy-pasteable entry in docs/cmake-options.md.
Priority: P2
Area: Build
Type: Verification
Evidence: `NEXT.md` §7 (prose-only command); direct empirical re-confirmation this session (`cd ../planetblupi/build && cmake . && make -j"$(nproc)"`, then `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest` under `FREE_API/`: 17/17 passed)
Depends on: None

Problem:
Same gap as TASK-24H-0012, for the `../planetblupi` Make build path.

Required work:
- Capture the exact working sequence (`cd ../planetblupi/build && cmake . && make -j"$(nproc)"`, then `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure` from `FREE_API/`) as a documented, canonical entry in `docs/cmake-options.md`.

Acceptance criteria:
- The documented sequence, run verbatim, passes 17/17 (already re-confirmed this session).
- Documented in the canonical build-command reference, not only in `NEXT.md`.

Out of scope:
- Do not change planetblupi's own CMakeLists.txt (a different repo).
- Do not add a CI pipeline — this task is documentation/verification only.

---

### TASK-24H-0014: Consolidate the 18 repeated per-target CXX_STANDARD blocks into directory-scoped variables
Status: DONE — replaced all 19 (grew from 18 since the task was written; test_midi_backend_failure added its own copy) identical set_target_properties(... CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO) blocks with 3 directory-scoped set(CMAKE_CXX_STANDARD/CMAKE_CXX_STANDARD_REQUIRED/CMAKE_CXX_EXTENSIONS) calls placed right before add_library(free-api STATIC), after the SDL3-acquisition block. free-api's own set_target_properties call kept (now just VERSION/SOVERSION). Confirmed -std=c++20 still applied (flags.make) and placement doesn't affect vendored SDL3/SDL3_image/SDL3_mixer's own build settings (their configure output appears before free-api's add_subdirectory() call in both games' own CMakeLists.txt). Verified: full fresh-configure rebuild + 23-test suite passes in all three build trees; both games' own executables rebuild cleanly from scratch.
Priority: P2
Area: Build
Type: Refactor
Evidence: CMakeLists.txt (18 separate `set_target_properties(<target> PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)` blocks — `grep -c "CXX_STANDARD 20" CMakeLists.txt` = 18, `grep -c "set_target_properties" CMakeLists.txt` = 18, zero uses of `CMAKE_CXX_STANDARD`); contrast `../free-direct/CMakeLists.txt:7-9`
Depends on: None

Problem:
Every one of free-api's 18 targets (the `free-api` library itself, `basic_test`, and 16 other test/example targets) has its own separate 5-line `set_target_properties(... CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)` block, all identical. `../free-direct/CMakeLists.txt:7-9` achieves the exact same effect for its own targets with three directory-scoped `set()` calls near the top of the file. The duplication in free-api is pure repetition (~90 lines) with no behavioral difference from the directory-scoped form, and is a real maintenance burden — a future contributor adding target #19 must remember to copy the block correctly rather than getting it for free.

Required work:
- Replace the 18 repeated blocks with `set(CMAKE_CXX_STANDARD 20)` / `set(CMAKE_CXX_STANDARD_REQUIRED YES)` / `set(CMAKE_CXX_EXTENSIONS NO)`, placed after the SDL3-acquisition block (after CMakeLists.txt's current line ~241, before `add_library(free-api STATIC)`) so it does not retroactively change the C++ standard used to build any `add_subdirectory()`'d SDL3/SDL3_image/SDL3_mixer vendored subprojects (configured earlier, during SDL3 acquisition, and which must keep their own build settings unchanged).
- Remove the now-redundant per-target `set_target_properties(... CXX_STANDARD ...)` blocks for all 18 targets.

Acceptance criteria:
- `free-api` and all 16 test/example targets still compile as C++20 with extensions disabled (unchanged effective behavior).
- SDL3/SDL3_image/SDL3_mixer's own build settings are unaffected when vendored via `add_subdirectory()`.
- Existing tests still pass in all three confirmed build modes.

Out of scope:
- Do not change the actual C++ standard (stays 20) or extensions setting (stays disabled) for any target.
- Do not touch `../free-direct/CMakeLists.txt` — it already uses the directory-scoped form.

---

### TASK-24H-0015: Re-verify FREE_API_BUILD_EXAMPLES=ON builds cleanly and correct NEXT.md's stale staleness note
Status: DONE — re-verified fresh in a scratch build dir: all 5 examples + all 26 tests build cleanly, 26/26 pass. NEXT.md's stale "not touched or re-verified recently" note no longer exists in the file (already removed in an earlier session pass) -- nothing left to correct.
Priority: P3
Area: Build
Type: Verification
Evidence: CMakeLists.txt:507-529 (`FREE_API_BUILD_EXAMPLES`, default OFF); `NEXT.md` §2 ("`examples/` also exists ... not touched or re-verified recently"); `examples/README.md` (documented build command); direct empirical confirmation this session
Depends on: None

Problem:
`NEXT.md`'s "Current status" section states `examples/` (five standalone WinAPI demos) has "not [been] touched or re-verified recently." This session confirmed the claim is now stale: `cmake -S . -B build -DFREE_API_USE_SYSTEM_SDL3=ON -DFREE_API_BUILD_EXAMPLES=ON && cmake --build build -j"$(nproc)"` (the exact command in `examples/README.md`) built all five examples (`01_window_and_message_loop`, `02_timers`, `03_input_and_cursor`, `04_gdi_minimap`, `05_midi_playback`) and all 17 test binaries cleanly, with no errors, this session.

Required work:
- Update `NEXT.md`'s "examples ... not touched or re-verified recently" note to reflect this session's confirmed clean build.

Acceptance criteria:
- `NEXT.md` accurately reflects the examples build's current verified status.
- No behavior change; documentation only. Runtime execution of the examples (which need a real display/audio backend per `examples/README.md`) remains a separate, not-yet-done human-verification item — do not claim more than "builds cleanly" was confirmed.

Out of scope:
- Do not attempt to run the examples headlessly under `SDL_VIDEODRIVER=dummy` — `examples/README.md` already documents these are meant to be run with a real display, not under CI/headless conditions.

---

### TASK-24H-0016: Verify FREE_API_TARGET_GAME override still fails loudly with no sibling game directory present at all
Status: DONE — re-ran in a genuinely isolated checkout (rsync'd free-api into an otherwise-empty scratch directory, no ../free-eggbert/../planetblupi present at all) and confirmed the exact fail-loud error still fires (ExtractStringTable.cmake "required .rc file not found" -> CMakeLists.txt FATAL_ERROR). Documented the confirmed error text in docs/cmake-options.md alongside TASK-24H-0003/0004. No code change.
Priority: P3
Area: Build
Type: Verification
Evidence: `cmake/ExtractStringTable.cmake` (`required .rc file not found` fail-loud check); direct empirical confirmation this session in a fully isolated checkout (free-api copied to an empty parent directory with no `../free-eggbert`/`../planetblupi` siblings at all)
Depends on: None

Problem:
TASK-24H-0003/0004 confirm the `FREE_API_TARGET_GAME` override works when the sibling game directory exists but its own CMake build tree hasn't been configured. The stricter case — the sibling game directory not existing at all (a genuinely isolated free-api checkout) — was not previously confirmed to fail loudly and clearly rather than silently mis-degrading. Verified this session: copying free-api into an otherwise-empty directory (no `../free-eggbert`/`../planetblupi`) and running `cmake -S . -B build -DFREE_API_USE_SYSTEM_SDL3=ON -DFREE_API_TARGET_GAME=free-eggbert` fails configure with a clear, correctly-attributed error: `ExtractStringTable.cmake: required .rc file not found for target game 'free-eggbert': <path>/../free-eggbert/resource/Eggbert2.rc`, immediately followed by free-api's own `CMakeLists.txt:226` `FATAL_ERROR`. This is the correct, intended fail-loud behavior.

Required work:
- Record this confirmed-passing edge case (fully isolated checkout, forced override, no sibling present) alongside TASK-24H-0003/0004's documentation, so it's clear the fail-loud gating was verified at both the "sibling exists but unconfigured" and "sibling absent entirely" boundaries.

Acceptance criteria:
- The isolated-checkout failure mode is documented as confirmed-correct, with its exact error text.
- No code change — this is a verification task confirming already-correct behavior.

Out of scope:
- Do not weaken or change the fail-loud behavior — it is already correct.

---

### TASK-24H-0017: Remove the dead EMSCRIPTEN/non-EMSCRIPTEN branch for FREE_API_BUILD_EXAMPLES's default
Status: DONE — replaced the dead if(EMSCRIPTEN)/else()/endif() with a single option() call, identical effective behavior (always OFF by default). Re-ran TASK-24H-0015's verification (examples build cleanly under -DFREE_API_BUILD_EXAMPLES=ON). Verified 26/26 in all three build trees.
Priority: P3
Area: Build
Type: Cleanup
Evidence: CMakeLists.txt:507-511
Depends on: None

Problem:
CMakeLists.txt:507-511 sets `FREE_API_BUILD_EXAMPLES`'s default to `OFF` in both the `if(EMSCRIPTEN)` and `else()` branches — the split has no effect and appears to be a leftover copy-paste from the genuinely-differentiated `FREE_API_BUILD_TESTS` option immediately above it (CMakeLists.txt:324-328, which correctly differs: `OFF` under Emscripten, `ON` otherwise). This is harmless but confusing dead conditional logic in the build system's own source.

Required work:
- Replace the dead `if(EMSCRIPTEN)/else()/endif()` around `FREE_API_BUILD_EXAMPLES` with a single `option(FREE_API_BUILD_EXAMPLES "Build free-api examples" OFF)` call.

Acceptance criteria:
- `FREE_API_BUILD_EXAMPLES` still defaults to `OFF` under every platform, identical to today's effective behavior.
- Examples still build cleanly under `-DFREE_API_BUILD_EXAMPLES=ON` on Linux desktop (re-run TASK-24H-0015's verification).

Out of scope:
- Do not change `FREE_API_BUILD_TESTS`'s genuinely-differentiated Emscripten/non-Emscripten default (CMakeLists.txt:324-328) — that one is correct as-is.
- Do not add any actual Emscripten build verification — out of this session's Linux-desktop-only scope.

## Scope and Public API

### TASK-24H-0101: Add public header declarations for all three undeclared free-direct bridge functions
Status: DONE — new include/free_api_bridge.h is now the single authoritative declaration site for all three functions, explicitly labeled as the free-direct-bridge exception (not part of the Win32 surface). ../free-direct/src/directdraw/DirectDraw.cpp, examples/04_gdi_minimap.cpp, and tests/test_gdi_regressions.cpp all migrated. Verified across free-api standalone, both target games (diamond free-api+free-direct dependency), and ../free-direct standalone; 17/17 everywhere.
Priority: P1
Area: Integration
Type: Implementation
Evidence: src/wingdi_dc.cpp:12,30,44; ../free-direct/src/directdraw/DirectDraw.cpp:20-22,440,890,892,897,965,1149,1189,1191,1373; examples/04_gdi_minimap.cpp:44-45; tests/test_gdi_regressions.cpp:26-27; docs/audit-24h-free-api.md §3.23, §5 item 2
Depends on: None

Problem:
`FreeApiCreateSurfaceDC`, `FreeApiDestroySurfaceDC`, and `FreeApiSetWindowFullscreen` (defined `extern "C"` in `src/wingdi_dc.cpp:12,30,44`) have zero declaration anywhere in `include/`, `include_non_windows/`, or even free-api's own internal headers (`src/internal/FreeApiGdi.hpp` was checked directly this session and does not declare any of the three). Every consumer — `../free-direct/src/directdraw/DirectDraw.cpp`, `examples/04_gdi_minimap.cpp`, and `tests/test_gdi_regressions.cpp` — hand-rolls its own local `extern "C"` prototype to call them. `plan.md` `TASK-0002` already tracks this for the first two functions only; this task supersedes it by covering all three with one authoritative fix.

Required work:
- Add one authoritative declaration for all three functions to an appropriate free-api header (a small dedicated bridge-facing header is preferable to bloating `include/wingdi.h`/`include/winuser.h` with non-`WINAPI` symbols that aren't part of the ANSI Win32 surface).
- Update `../free-direct/src/directdraw/DirectDraw.cpp` to `#include` that header instead of its three local hand-declared prototypes (cross-repo change; coordinate so both repos move together, since `free-direct` is a sibling checkout, not a submodule).
- Update `examples/04_gdi_minimap.cpp` and `tests/test_gdi_regressions.cpp` to use the new shared declaration instead of their own local copies.

Acceptance criteria:
- All three functions have exactly one authoritative declaration, consumed by every in-repo and cross-repo caller.
- Both target games still build and link successfully through `free-direct` after the coordinated change.
- Existing tests still pass (`tests/test_gdi_regressions.cpp`, `tests/test_header_compile.cpp`).
- No unrelated API is added; the three functions' signatures and behavior are unchanged.

Out of scope:
- Do not rename or change the calling convention/signature of any of the three functions.
- Do not fold these into the ANSI `WINAPI` GDI surface (`wingdi.h`'s documented function family) as if they were real Win32 APIs — they are a free-api/free-direct-only bridge, and must stay labeled as such.
- Do not implement any new bridge function beyond these three.

---

### TASK-24H-0102: Document free-direct's reach into internal FreeApi::Platform::ReadRssKB
Status: DONE — added "The actual bridge-exception surface" subsection to docs/scope.md's "Boundary with free-direct" section, naming the ReadRssKB reach explicitly, noting the existing unused FreeApiReadRssKB() wrapper, and leaving the fix-or-accept decision open for a future task. No behavior change.
Priority: P1
Area: Integration
Type: Documentation
Evidence: ../free-direct/src/diagnostics/Diagnostics.cpp:18,70-72; src/platform/PlatformProcessInfo.hpp:11; src/platform/PlatformProcessInfo.cpp:8; src/internal/FreeApiDiagnostics.hpp:39; src/internal/FreeApiDiagnostics.cpp:83-85; docs/audit-24h-free-api.md §1 item 2, §5 item 2
Depends on: None

Problem:
`../free-direct/src/diagnostics/Diagnostics.cpp:18` forward-declares `FreeApi::Platform::ReadRssKB()` raw and calls it directly at line 72 — a fully internal free-api implementation detail (declared in `src/platform/PlatformProcessInfo.hpp:11`, never exposed in `include/`) that is not part of any documented bridge exception. This session additionally found that free-api already has its own internal wrapper for the exact same value, `FreeApi::Internal::FreeApiReadRssKB()` (`src/internal/FreeApiDiagnostics.hpp:39`, implemented `src/internal/FreeApiDiagnostics.cpp:83-85`), which `free-direct` does not use — it reaches past the wrapper straight into the raw platform symbol instead.

Required work:
- Document this reach explicitly in `docs/scope.md`'s `free-direct` boundary section (or `plan.md`'s out-of-scope classification) as a real, currently-undocumented cross-repo coupling to an internal symbol, not a public bridge symbol.
- Note the redundancy finding (an existing internal wrapper, `FreeApiReadRssKB()`, already does the same thing) as context for a future decision, without making that decision here.

Acceptance criteria:
- The `FreeApi::Platform::ReadRssKB` reach from `free-direct` is explicitly named and documented as an internal-symbol coupling (not silently omitted from scope docs as it is today).
- No behavior change; documentation only.
- No unrelated API is added.

Out of scope:
- Do not add a public header declaration for `ReadRssKB` or `FreeApiReadRssKB` in this task — that is a follow-on implementation decision for a future task, not a documentation task.
- Do not modify `../free-direct/src/diagnostics/Diagnostics.cpp`'s behavior.

---

### TASK-24H-0103: Consolidate duplicate _lopen/_lread/_lclose declarations between io.h and winbase.h
Status: DONE — removed the verbatim redeclaration from include/io.h (which already transitively sees winbase.h's declaration via its own #include <windows.h> -> winbase.h chain); winbase.h is now the sole textual declaration site. Verified: test_header_compile + full 23-test suite pass in all three build trees (standalone, free-eggbert, planetblupi). Closes duplicate TASK-24H-0713.
Priority: P2
Area: Headers
Type: Refactor
Evidence: include/io.h:58,60,62; include/winbase.h:81,85,89; docs/audit-24h-free-api.md §3.7, §3.14
Depends on: None

Problem:
`_lopen`, `_lread`, and `_lclose` are declared identically in both `include/io.h` (lines 58,60,62) and `include/winbase.h` (lines 81,85,89), with no `#ifndef` guard shared between the two files. Both headers are transitively included by `windows.h`, so any translation unit including both ends up with two textually-identical redeclarations — legal today (identical redeclaration is not an error), but redundant and a maintenance hazard if the two copies ever drift.

Required work:
- Pick one header as the single source of truth for these three declarations (`io.h` is the more natural home, since it already owns `_finddata_t`/`_findfirst`/`_findnext`/`_findclose`).
- Remove the duplicate declarations from the other header, replacing them with an `#include` of the canonical header if needed for standalone-inclusion compatibility.

Acceptance criteria:
- `_lopen`/`_lread`/`_lclose` are declared in exactly one header.
- Both games still build and link (both call sites: `../free-eggbert/src/ddutil.cpp:232-240`, `../planetblupi/src/ddutil.cpp:297-306`).
- `test_file_regressions.cpp` and `test_header_compile.cpp` still pass.

Out of scope:
- Do not change these functions' signatures or behavior.
- Do not consolidate any other symbol pair in this task — see the dedicated tasks for `MCIDEVICEID`, `mciSendCommandA`/MCI constants, and `byte`.

---

### TASK-24H-0104: Consolidate duplicate MCIDEVICEID typedef between mciapi.h and mmsystem.h
Status: DONE — NOTE: implemented via TASK-24H-0905's direction (mmsystem.h canonical, not mciapi.h as this task's own text originally suggested) since mciapi.h is confirmed completely unused by either game (only a commented-out reference in free-eggbert's movie.cpp) while mmsystem.h is the actually-included, foundational header. Guarded with FREE_API_MCIDEVICEID_DEFINED; mciapi.h now includes mmsystem.h and checks the same guard. Verified mciapi.h still compiles standalone (ad-hoc probe, not a committed test since neither game includes it). See TASK-24H-0905 for full verification detail.
Priority: P2
Area: Headers
Type: Refactor
Evidence: include/mciapi.h:9; include/mmsystem.h:30; docs/audit-24h-free-api.md §3.8, §3.11
Depends on: None

Problem:
`typedef UINT MCIDEVICEID;` is declared identically in both `include/mciapi.h:9` and `include/mmsystem.h:30`, with no shared guard. `mciapi.h` is a minimal, 11-line file whose only content is this one typedef.

Required work:
- Make `mciapi.h` the canonical definition site and have it guard the typedef behind a `FREE_API_MCIDEVICEID_DEFINED`-style macro (matching the project's existing pattern used for `MCI_OPEN_PARMS`/`MCI_PLAY_PARMS`).
- Have `mmsystem.h` `#include <mciapi.h>` (or check the same guard macro) instead of redefining the typedef.

Acceptance criteria:
- `MCIDEVICEID` is defined in exactly one place, reused by the other header via include or guard.
- `mciSendCommandA`'s signature (which uses `MCIDEVICEID`) is unaffected.
- Existing tests still pass, including `test_mci_sequences.cpp`.

Out of scope:
- Do not change `MCIDEVICEID`'s underlying type (`UINT`).
- Do not restructure `mciapi.h`/`mmsystem.h` beyond this one typedef's consolidation.

---

### TASK-24H-0105: Consolidate duplicate mciSendCommandA/MCI_* constant declarations between digitalv.h and mmsystem.h
Status: DONE — mmsystem.h made canonical for the 7 shared constants (MCI_OPEN/CLOSE/PLAY/NOTIFY/WAIT/OPEN_TYPE/OPEN_ELEMENT, each per-macro #ifndef-guarded) and mciSendCommandA's declaration + mciSendCommand macro (guarded behind FREE_API_MCISENDCOMMANDA_DECLARED). digitalv.h's copies now check the same guards (already transitively sees mmsystem.h via its own #include <windows.h>). MCI_STATUS/MCI_PAUSE (digitalv.h-only, not duplicated) and MCI_DGV_* (digital-video-only) left untouched. See TASK-24H-0905 for full verification.
Priority: P2
Area: Headers
Type: Refactor
Evidence: include/digitalv.h:41-43,47-48,50-51,135,145; include/mmsystem.h:153-161,251,258; docs/audit-24h-free-api.md §3.4, §3.11
Depends on: None

Problem:
`digitalv.h` and `mmsystem.h` both declare `mciSendCommandA` with an identical signature (`digitalv.h:135` / `mmsystem.h:251`), both define the `mciSendCommand`→`mciSendCommandA` macro (`digitalv.h:145` / `mmsystem.h:258`), and both `#define` the same seven `MCI_OPEN`/`MCI_CLOSE`/`MCI_PLAY`/`MCI_NOTIFY`/`MCI_WAIT`/`MCI_OPEN_TYPE`/`MCI_OPEN_ELEMENT` constants with identical values (`digitalv.h:41-51` vs `mmsystem.h:153-161`). None of these are guarded against redefinition, unlike the project's `FREE_API_*_DEFINED` pattern used elsewhere in the same two files.

Required work:
- Pick `mmsystem.h` as the canonical definition site for `mciSendCommandA`, the `mciSendCommand` macro, and the seven shared `MCI_*` constants (it is the more foundational/broadly-included of the two headers).
- Guard each with `#ifndef`/`#define FREE_API_*_DEFINED` and have `digitalv.h` check the same guards instead of unconditionally redefining them.

Acceptance criteria:
- Each of the listed symbols is textually defined in exactly one place, consumed by the other header via its existing `#include <windows.h>` chain or an explicit guard check.
- `mciSendCommandA`'s real "sequencer"-only behavior is unaffected.
- `test_mci_sequences.cpp` and `test_mci_avivideo_regressions.cpp` still pass.

Out of scope:
- Do not touch `MCI_OPEN_PARMS`/`MCI_PLAY_PARMS` in this task — that fragility is a separate, higher-priority task (see TASK-24H-0107) because those two are not simple duplicates, they are genuinely different struct shapes.
- Do not consolidate the `MCI_DGV_*` (digital-video-only) constants — those exist only in `digitalv.h` and are not duplicated.

---

### TASK-24H-0106: Consolidate duplicate byte macro guard between rpcndr.h and wtypes.h
Status: DONE — kept rpcndr.h as canonical (confirmed via windows.h's own #include <rpcndr.h>, and wtypes.h's own #include <windows.h> at its top, that byte is always already defined by the time wtypes.h's copy would run — a guaranteed no-op in every real usage). Removed wtypes.h's own copy, replaced with a doc-comment cross-reference. Verified test_header_compile.cpp and both real games (SPEEDY_BLUPI_WINDOWS/PLANET_BLUPI_WINDOWS) rebuild cleanly; 26/26 in all three build trees.
Priority: P3
Area: Headers
Type: Refactor
Evidence: include/rpcndr.h:17-18; include/wtypes.h:38-39; docs/audit-24h-free-api.md §3.12
Depends on: None

Problem:
`#ifndef byte / #define byte BYTE` is declared identically in both `include/rpcndr.h:17-18` and `include/wtypes.h:38-39`. Each is individually self-guarded against multiple inclusion of itself (via its own `#ifndef byte`), so the second header's copy is a harmless no-op today, but the definition still exists twice in source.

Required work:
- Pick one header as canonical (`wtypes.h` already carries a doc comment explaining the macro's purpose; keep it there) and have the other header `#include` it, or simply drop its own copy if the guard already makes the second define a no-op and no standalone-inclusion case needs it.

Acceptance criteria:
- The `byte` macro is textually defined in exactly one header.
- Both games still build (this macro is a compile-only compatibility shim; confirm via existing `test_header_compile.cpp`).

Out of scope:
- Do not remove the `byte` macro entirely — it is required for legacy-source compatibility per both headers' own doc comments.
- Do not touch `wtypes.h`'s other COM-adjacent typedefs (`VARTYPE`/`SCODE`/`DATE`/`CLIPFORMAT`) — already correctly classified as a documented permanent stub.

---

### TASK-24H-0107: Resolve or document the MCI_OPEN_PARMS/MCI_PLAY_PARMS include-order-fragile shape aliasing
Status: DONE — added explanatory comments at all four guard sites (include/mmsystem.h's MCI_OPEN_PARMS/MCI_PLAY_PARMS definitions, include/digitalv.h's MCI_DGV-aliased versions). Verified via grep that both games' AVI-probe code (movie.cpp, both MCI_OPEN and MCI_PLAY calls) always explicitly names MCI_DGV_OPEN_PARMS/MCI_DGV_PLAY_PARMS, never the bare alias -- confirming the include-order-determined mmsystem.h-wins resolution is safe. No behavior change. 22/22 in all three build modes; both target games still build/link.
Priority: P1
Area: Headers
Type: Bugfix
Evidence: include/digitalv.h:36,106-127; include/mmsystem.h:80-101; include/windows.h:180; ../planetblupi/src/sound.cpp:524,546-547; ../free-eggbert/src/sound.cpp:574,599-600; src/MidiMusic.cpp:537,597; docs/audit-24h-free-api.md §5 (checklist cross-ref)
Depends on: None

Problem:
`MCI_OPEN_PARMS`/`LPMCI_OPEN_PARMS` and `MCI_PLAY_PARMS`/`LPMCI_PLAY_PARMS` are each guarded by a single `FREE_API_MCI_OPEN_PARMS_DEFINED`/`FREE_API_MCI_PLAY_PARMS_DEFINED` macro in both `digitalv.h` (aliasing to the `MCI_DGV_*`-shaped structs, `digitalv.h:106-127`) and `mmsystem.h` (defining its own, differently-shaped `_MCI_OPEN_PARMSA`/`_MCI_PLAY_PARMS` structs, `mmsystem.h:80-101`). Because `digitalv.h:36` itself `#include`s `windows.h`, which includes `mmsystem.h` at `windows.h:180` before `digitalv.h` reaches its own definitions, **`mmsystem.h`'s shape always wins**, regardless of which header a caller directly includes. This happens to be currently correct — both games' real MCI_OPEN call sites for the live "sequencer" MIDI path (`../planetblupi/src/sound.cpp:546-547`, `../free-eggbert/src/sound.cpp:599-600`) use exactly the `mmsystem.h`-shaped fields (`lpstrDeviceType`, `lpstrElementName`), matching `src/MidiMusic.cpp:537`'s cast to `MCI_OPEN_PARMSA*` — but this correctness is an accident of include order, not an enforced invariant, and is not documented anywhere.

Required work:
- Add an explicit comment at both guard sites (`digitalv.h:106-127` and `mmsystem.h:80-101`) stating that `mmsystem.h`'s shape is the one actually used by both games' live MIDI/sequencer code, that it always wins because `digitalv.h` transitively includes `mmsystem.h` before defining its own alias, and that this is deliberate-by-necessity rather than accidental (since `MCI_DGV_OPEN_PARMS`'s incompatible shape is only ever used for the always-declined "avivideo" probe, never through the `MCI_OPEN_PARMS` alias).
- Verify no code path anywhere in free-api, either game, or `free-direct` relies on `MCI_OPEN_PARMS` resolving to the `MCI_DGV_*` shape (grep confirms both games' AVI probe code uses `MCI_DGV_OPEN_PARMSA` directly, not the `MCI_OPEN_PARMS` alias — confirm this holds and note it in the comment).

Acceptance criteria:
- Both guard sites carry an explicit comment explaining the include-order dependency and why the current outcome is correct, not accidental.
- No behavior change — this is a documentation-and-verification task unless the verification step finds a real live consumer of the `MCI_DGV_*`-shaped alias, in which case escalate rather than silently fix.
- `test_mci_sequences.cpp` and `test_mci_avivideo_regressions.cpp` still pass.

Out of scope:
- Do not rename or restructure `MCI_DGV_OPEN_PARMS`/`MCI_DGV_PLAY_PARMS` themselves — they are correct and required for the AVI probe shape.
- Do not change which shape `MCI_OPEN_PARMS`/`MCI_PLAY_PARMS` resolve to unless the verification step proves the current outcome is actually wrong for a live call site.

---

### TASK-24H-0108: Add missing @note Status: annotations to the 7 unannotated headers
Status: DONE — added top-of-file @note Status: HEADER_ONLY doc blocks to basestd.h/mciapi.h/minwindef.h/winerror.h (pure typedef/constant/forwarder headers, no per-declaration annotations needed), and to the 3 include_non_windows/ files (Windows.h/WinUser.h marked HEADER_ONLY as case-insensitive-filesystem forwarders; sys/timeb.h marked IMPLEMENTED since it has real ftime() behavior, not just a forward). No declarations changed. Verified 26/26 in all three build trees.
Priority: P3
Area: Headers
Type: Documentation
Evidence: include/basestd.h; include/mciapi.h; include/minwindef.h; include/winerror.h; include_non_windows/sys/timeb.h; include_non_windows/Windows.h; include_non_windows/WinUser.h; docs/audit-24h-free-api.md §3 intro
Depends on: None

Problem:
Every other public header follows a `/** @brief ... @note Status: {STUB|PARTIAL|IMPLEMENTED|HEADER_ONLY} */` convention per declaration. Seven files have zero such annotations: `basestd.h`, `mciapi.h`, `minwindef.h`, `winerror.h`, `include_non_windows/sys/timeb.h`, `include_non_windows/Windows.h`, `include_non_windows/WinUser.h` — confirmed by direct grep this session (`grep -c "@note Status:"` returns 0 for all seven).

Required work:
- Add `@note Status:` annotations to each declaration in `basestd.h`, `mciapi.h`, `minwindef.h`, and `winerror.h` (all "Keep" per the audit's §3.1/§3.8/§3.9/§3.18 classification — annotate as `IMPLEMENTED` or `HEADER_ONLY` as appropriate for foundational typedefs/constants).
- Add a short top-of-file note (not necessarily per-declaration, since these are trivial pure-`#include`/typedef shims) to the three `include_non_windows/` files explaining they are case-insensitive-filesystem/compatibility shims with no independent status of their own.

Acceptance criteria:
- All 7 files carry documentation consistent with the rest of `include/`'s convention.
- No behavior change; documentation only.
- Existing `test_header_compile.cpp` still passes.

Out of scope:
- Do not change any of the seven headers' actual declarations, only their documentation.
- Do not invent a new status category beyond the existing `STUB`/`PARTIAL`/`IMPLEMENTED`/`HEADER_ONLY` set.

---

### TASK-24H-0109: Finalize OutputDebugStringW as a documented, intentionally-kept real implementation
Status: DONE — added keep-with-rationale entry to docs/out-of-scope.md's "Unicode / W-suffixed API variants" section; cross-referenced from plan.md TASK-0007 (now also DONE). Closes TASK-24H-1215 (duplicate).
Priority: P2
Area: Scope
Type: Documentation
Evidence: include/debugapi.h:14; src/winbase.cpp:70-76; docs/audit-24h-free-api.md §3.3, §5 item 7
Depends on: None

Problem:
`OutputDebugStringW` is a real, working, non-trivial `W`-suffixed function (a genuine UTF-16-to-narrow conversion, `src/winbase.cpp:70-76`) despite the project's documented ANSI-only orientation, and this session's two independent full-source usage sweeps of both games confirm zero call sites for it anywhere. `plan.md` `TASK-0007` already frames this as an open "pick one" decision (simplify to a stub, or keep with justification). Per the user's confirmed default policy for this class of finding, this task makes the concrete "keep + document" choice rather than leaving it open.

Required work:
- Add a `docs/out-of-scope.md` entry (in the existing `W`-suffixed-variant section) explicitly naming `OutputDebugStringW` as a real, kept, zero-call-site implementation, with the rationale that it's trivial to keep in sync with `OutputDebugStringA` and costs nothing at runtime since it's never invoked.
- Cross-reference this decision from `plan.md` `TASK-0007` so a future reader sees the decision was made, not left open.

Acceptance criteria:
- `docs/out-of-scope.md` explicitly documents `OutputDebugStringW` as keep-with-rationale.
- No code change — `src/winbase.cpp:70-76` and `include/debugapi.h:14` are untouched.
- No unrelated API is added.

Out of scope:
- Do not simplify `OutputDebugStringW` to a stub — the user-confirmed default for this session is keep+document, not remove.
- Do not add any new `W`-suffixed function elsewhere based on this precedent.

---

### TASK-24H-0110: Finalize _chdir/_getcwd as documented, intentionally-kept unused symbols
Status: DONE — extended docs/headers.md's direct.h row with explicit keep-and-document decision language; cross-referenced from plan.md TASK-0006 (now also DONE). Closes TASK-24H-1216 (duplicate).
Priority: P2
Area: Scope
Type: Documentation
Evidence: include/direct.h:21-22; src/crt_direct.cpp:18,23; docs/audit-24h-free-api.md §3.5, §5 item 8
Depends on: None

Problem:
`_chdir` and `_getcwd` are declared (`include/direct.h:21-22`) and implemented (`src/crt_direct.cpp:18,23`) but have zero call sites in either target game or free-api's own test suite — confirmed again this session. `direct.h` must stay regardless (it also declares the live, free-eggbert-required `_mkdir`). `plan.md` `TASK-0006` already frames this as an open "remove or document" decision. Per the user's confirmed default policy, this task finalizes the "keep + document" branch.

Required work:
- Add a `docs/out-of-scope.md` entry naming `_chdir`/`_getcwd` as declared-for-`direct.h`-completeness-alongside-live-`_mkdir`, explicitly unproven-needed, kept rather than removed per project policy of not removing working code without stronger evidence.
- Cross-reference this decision from `plan.md` `TASK-0006`.

Acceptance criteria:
- `docs/out-of-scope.md` explicitly documents `_chdir`/`_getcwd` as keep-with-rationale.
- No code change — both functions and their declarations are untouched.
- `_mkdir`'s declaration and behavior are unaffected.

Out of scope:
- Do not remove `_chdir`/`_getcwd` — the user-confirmed default for this session is keep+document, not remove.
- Do not touch `_mkdir` or any other `direct.h` symbol.

---

### TASK-24H-0111: Document HFONT/HPALETTE as permanently vestigial-but-harmless
Status: DONE — added @note Status: VESTIGIAL doc comments to both typedefs in include/minwindef.h; added a row to docs/out-of-scope.md's "Compile-only stubs" table.
Priority: P2
Area: Scope
Type: Documentation
Evidence: include/minwindef.h:100,104; docs/audit-24h-free-api.md §3.9
Depends on: None

Problem:
`HFONT` (`include/minwindef.h:104`) and `HPALETTE` (`include/minwindef.h:100`) are declared but, confirmed this session, no function anywhere in `include/` takes or returns either type — there is no font- or palette-creation API in free-api at all. They cost nothing at runtime (opaque handle typedefs) but currently carry no `@note Status:` explanation of why they exist despite being unused.

Required work:
- Add a `@note Status:` annotation to both typedefs in `include/minwindef.h` explicitly marking them vestigial-but-harmless: kept because removing them risks an unnoticed compile break in `free-direct` or a future consumer, downgradeable to "remove" only if a stronger signal emerges.
- Add a corresponding one-line entry to `docs/out-of-scope.md`'s vestigial-typedef classification.

Acceptance criteria:
- Both typedefs carry an explicit `@note Status:` explaining their vestigial classification.
- `docs/out-of-scope.md` lists them.
- No behavior change; documentation only.

Out of scope:
- Do not remove `HFONT`/`HPALETTE`.
- Do not add any font or palette API to justify their presence — that would be new, unevidenced public API.

---

### TASK-24H-0112: Document winnt.h's COM-family typedefs as permanently vestigial-but-harmless
Status: DONE — added @note Status: VESTIGIAL doc comments to IUnknown and the GUID/IID/CLSID family in include/winnt.h; added a row to docs/out-of-scope.md's "Compile-only stubs" table alongside the existing wtypes.h row.
Priority: P2
Area: Scope
Type: Documentation
Evidence: include/winnt.h:10-11,67,79-92; docs/audit-24h-free-api.md §3.20
Depends on: None

Problem:
`winnt.h`'s COM-family typedefs — `IUnknown` (line 67), `GUID`/`LPGUID`/`LPCGUID` (lines 79-86), `IID`/`LPIID`/`REFIID` (lines 87-89), `CLSID`/`LPCLSID`/`REFCLSID` (lines 90-92) — are confirmed to have zero usage anywhere in `include/`. The header's own comment already states "no COM behavior implemented" (line 10), but there is no explicit `@note Status:` per-typedef classification tying that file-level comment to a formal keep-vs-remove decision.

Required work:
- Add `@note Status:` annotations to each of the listed typedefs in `include/winnt.h`, classifying them as permanently-vestigial (same reasoning as `wtypes.h`'s already-documented COM-adjacent types: cheap, harmless, and a documented decision beats a silent removal that might break an unknown downstream consumer).
- Add a corresponding entry to `docs/out-of-scope.md` alongside the existing `wtypes.h` COM-ish-types row, so both files' COM-family vestiges are documented in one place.

Acceptance criteria:
- All listed typedefs carry an explicit vestigial `@note Status:` annotation.
- `docs/out-of-scope.md` documents the full COM-family typedef set across both `winnt.h` and `wtypes.h` in one place.
- No behavior change; documentation only.

Out of scope:
- Do not remove any COM-family typedef.
- Do not implement any real COM/OLE behavior — this remains permanently out of scope regardless of this documentation task.

---

### TASK-24H-0113: Document the rationale for windows.h's global #define fopen free_api_fopen override
Status: DONE — added a new "Why windows.h globally redefines fopen" section to docs/headers.md; added a short cross-reference comment above the macro in include/windows.h. Closes TASK-24H-1206 (duplicate).
Priority: P2
Area: Headers
Type: Documentation
Evidence: include/windows.h:120-175; docs/audit-24h-free-api.md §3.16
Depends on: None

Problem:
`include/windows.h` contains `#undef fopen` / `#define fopen free_api_fopen` (`windows.h:174-175`), silently redefining the standard-library `fopen` symbol for every C++ translation unit that includes `<windows.h>` — a broad-reaching, global mechanism serving a narrow path-normalization need (backslash/case-fallback handling, `windows.h:120-172`). It works and is tested, but no doc currently explains why this shape (a blanket macro override) was chosen over a narrower alternative (e.g. a differently-named wrapper function that call sites opt into).

Required work:
- Add an explicit rationale note to `docs/headers.md` (or a new subsection of `docs/scope.md`) explaining: both games call plain `fopen()` pervasively via `#include <windows.h>`-transitive code with no game-side awareness of free-api, so a macro override is the only way to normalize paths without touching either game's source — the blast radius is deliberate, not accidental.
- Add a short comment directly above the `#define fopen free_api_fopen` line in `include/windows.h` cross-referencing the new doc section.

Acceptance criteria:
- The rationale is written down in `docs/headers.md` or `docs/scope.md`.
- `include/windows.h`'s macro itself is unchanged.
- Existing tests (`test_file_regressions.cpp` and any `fopen`-path test) still pass.

Out of scope:
- Do not change the macro's mechanism (e.g. do not convert it to an opt-in wrapper) — this task documents the existing, working design, it does not redesign it.
- Do not extend this override pattern to any other standard-library function.

---

### TASK-24H-0114: Establish a formal test-infrastructure-only and free-direct-bridge exception policy in docs/scope.md
Status: DONE — added a "test-infrastructure-only" named exception category to docs/scope.md's "The rule" section (criteria + GetTickCount/Sleep worked example cross-reference to docs/out-of-scope.md's Resources policy table). The free-direct-bridge category already existed (TASK-0011/0101/0102); added its "do not generalize" constraint inline for completeness. Verified 25/25 in all three build trees (docs-only). CORRECTION (TASK-24H-1228): this task's own Problem text above listed `CloseHandle` alongside the real test-infrastructure-only symbols, but the TASK-24H-0115 public-surface audit found CloseHandle has zero call sites anywhere, including tests -- it's actually vestigial-but-harmless, not test-infrastructure-only. See docs/out-of-scope.md's Compile-only stubs table for the corrected classification.
Priority: P2
Area: Scope
Type: Documentation
Evidence: docs/scope.md ("The rule" section); plan.md TASK-0005, TASK-0011; docs/audit-24h-free-api.md §3.6, §3.13, §3.14, §3.23
Depends on: TASK-24H-0101, TASK-24H-0102

Problem:
`docs/scope.md`'s citation rule already lists ad hoc exceptions ("explicit scope-control cleanup," "tests and documentation," "a minimal compile-only stub") but does not name "symbol used only by free-api's own test suite" (`CloseHandle`, `GetLastError`/`SetLastError`, `RemoveDirectoryA`, `SetEnvironmentVariableA`, `access`/`_access`, `Sleep`, `GetTickCount`) or "symbol whose only real caller is `free-direct` acting as the bridge for the two target games" (`FreeApiRunWinMain`, the three `FreeApiCreateSurfaceDC`/`FreeApiDestroySurfaceDC`/`FreeApiSetWindowFullscreen` bridge functions, and the `FreeApi::Platform::ReadRssKB` internal reach) as named, criteria-bearing exception categories. Today each instance gets documented individually (`plan.md` `TASK-0005`, `TASK-0011`), but there is no reusable policy statement a future contributor can check a new symbol against without re-deriving the reasoning from scratch.

Required work:
- Add two new named exception categories to `docs/scope.md`'s "The rule" section: "test-infrastructure-only" (criteria: called only by `tests/`, never by either game, kept because it's cheap and used by free-api's own verification) and "free-direct-bridge" (criteria: called only by `../free-direct` acting as the games' bridge/rendering layer, must have a real public or explicitly-internal-and-documented declaration per TASK-24H-0101/0102).
- Cross-reference the existing per-symbol documentation (`docs/out-of-scope.md`'s test-infrastructure rows, the bridge exception rows) as worked examples of each category.

Acceptance criteria:
- `docs/scope.md` names both categories explicitly, with reusable acceptance criteria a future task can check a new symbol against.
- No behavior change; documentation only.
- No unrelated API is added.

Out of scope:
- Do not broaden the `free-direct`-bridge category into a general "any sibling project's usage counts" rule — it stays scoped to `free-direct` specifically, per `plan.md` `TASK-0011`'s existing "do not generalize" constraint.
- Do not retroactively reclassify any currently-implemented symbol's status as part of this task — it only writes the policy, it does not re-audit every symbol against it (that is TASK-24H-0115).

---

### TASK-24H-0115: Produce a definitive cross-check of every public header declaration against target-game usage
Status: DONE — produced docs/public-surface-audit.md, a consolidated table classifying every public declaration in include/*.h and include_non_windows/*.h (grouped by header, constant families grouped into single rows per docs/supported-apis.md's own convention) into exactly one of the 5 categories (required-by-game / test-infrastructure-only / free-direct-bridge / permanent-documented-stub / vestigial-but-harmless), consolidating existing classification work from docs/scope.md, docs/out-of-scope.md, and docs/supported-apis.md rather than re-deriving it, with fresh file:line verification against ../free-eggbert/src and ../planetblupi/src for symbols not already carrying a citation. Found and filed (not fixed) one real mismatch: CloseHandle (include/handleapi.h) has zero call sites anywhere -- not either game, not any test, not free-api's own src beyond its own definition -- so its true classification is vestigial-but-harmless, not test-infrastructure-only as TASK-24H-0114's own problem text implied. Filed as TASK-24H-1228 (see below), not fixed in this pass. Verified 26/26 in the standalone build (docs-only change).
Priority: P2
Area: Scope
Type: Verification
Evidence: docs/audit-24h-free-api.md §2 (usage tables), §3 (public surface audit); docs/headers.md; docs/supported-apis.md
Depends on: TASK-24H-0101, TASK-24H-0108, TASK-24H-0114

Problem:
This session's §3 public-surface audit independently re-derived, header by header, which of the ~101 public functions and ~231 `#define`s in `include/`/`include_non_windows/` trace to a real call site in either game, the test-infrastructure exception, or the `free-direct`-bridge exception — and found the classification holds, but this cross-check was done as a one-off research pass, not as a durable, checkable artifact. Without a standing cross-reference, the next audit has to re-derive the same table from scratch instead of verifying it's still accurate.

Required work:
- Once TASK-24H-0101 (bridge headers), TASK-24H-0108 (missing status annotations), and TASK-24H-0114 (exception policy) land, produce a single consolidated table (in `docs/headers.md` or a new `docs/public-surface-audit.md`) listing every public declaration's classification: required-by-game / test-infrastructure-only / free-direct-bridge / permanent-documented-stub / vestigial-but-harmless.
- Cross-check this table against `docs/supported-apis.md` and flag (do not silently fix) any symbol present in one but not the other for a follow-up task.

Acceptance criteria:
- Every symbol in `include/*.h`/`include_non_windows/*.h` appears in the consolidated table with exactly one of the five classifications.
- Any newly-found mismatch versus `docs/supported-apis.md` is written down, not silently resolved in this task.
- No behavior change; documentation/verification only.

Out of scope:
- Do not fix any classification mismatch found — file it as a new task instead, so this verification pass stays a single atomic unit of work.
- Do not add any new public symbol as a byproduct of this cross-check.

---

### TASK-24H-0116: Add a one-line doc confirmation that RegisterClassExA/WNDCLASSEXA absence was checked
Status: DONE — added a row to docs/out-of-scope.md's "Unsupported APIs" table. Implemented together with duplicate TASK-24H-0302. Verified 26/26 in all three build trees.
Priority: P3
Area: Scope
Type: Documentation
Evidence: docs/audit-24h-free-api.md §1 item 5, §2.2, §2.3, §3.21; include/winuser.h
Depends on: None

Problem:
`RegisterClassExA`/`WNDCLASSEXA` do not exist anywhere in free-api. This session's usage sweep confirms this is correct — neither game uses them, both use the plain `WNDCLASSA`/`RegisterClassA` form — but there is no written record that this absence was deliberately checked rather than simply never noticed, which risks a future contributor treating it as an oversight to "fix."

Required work:
- Add a one-line note to `docs/out-of-scope.md`'s "Unsupported APIs" section stating `RegisterClassExA`/`WNDCLASSEXA` were checked and confirmed absent-and-correct (no evidenced call site in either game), so any future request to add them should be scrutinized hard since there is no existing partial implementation to extend.

Acceptance criteria:
- `docs/out-of-scope.md` contains this explicit confirmation.
- No code change.

Out of scope:
- Do not implement `RegisterClassExA`/`WNDCLASSEXA` — there is no evidenced need.
- Do not add any other `Ex`-suffixed WinUser function based on this precedent.

---

### TASK-24H-0117: Flag the _findfirst session-leak caveat in docs/supported-apis.md's status row
Status: DONE — leak is now fixed (TASK-24H-0701), not merely documented; docs/supported-apis.md's `_findfirst` row updated to describe the auto-erase-on-exhaustion fix. Duplicate of TASK-24H-0712/0804, closed together.
Priority: P3
Area: Scope
Type: Documentation
Evidence: docs/supported-apis.md:50; plan.md TASK-0009; docs/audit-24h-free-api.md §4.23, §5 item 22
Depends on: None

Problem:
`docs/supported-apis.md:50`'s `_findfirst`/`_findnext`/`_findclose` row states `IMPLEMENTED (real std::filesystem-backed directory listing...)` without flagging the known session-table leak that `plan.md` `TASK-0009` already tracks (free-eggbert's design-mission picker drains a search via `_findnext` to exhaustion without ever calling `_findclose`, leaking one `g_findSessions` entry per visit). A reader of `docs/supported-apis.md` alone would not know this caveat exists.

Required work:
- Add a short caveat to the `_findfirst`/`_findnext`/`_findclose` row in `docs/supported-apis.md` noting the known, bounded, low-severity session-leak on the exhaustion-without-`_findclose` path, cross-referencing `plan.md` `TASK-0009` for the tracked fix/document decision.

Acceptance criteria:
- `docs/supported-apis.md`'s `_findfirst` row accurately reflects the known caveat.
- No code change; documentation only.
- This does not duplicate or replace `plan.md` `TASK-0009`, which remains the task that decides the actual fix-or-document resolution.

Out of scope:
- Do not implement any fix to the session-leak in this task — that is `plan.md` `TASK-0009`'s job.
- Do not add rows for any other symbol — `plan.md` `TASK-0001` already owns the broader 28-symbol documentation gap.

---

### TASK-24H-0118: Sharpen the UNICODE-branch-macros documentation to state the target W functions are undeclared, not just unimplemented
Status: DONE — confirmed via grep that all 16 W-suffixed target functions are undeclared anywhere outside the macro definitions themselves. Added the precise fail-loud-at-compile-time statement to docs/out-of-scope.md's Unicode section. No code change. Verified 26/26 in all three build trees.
Priority: P3
Area: Headers
Type: Documentation
Evidence: include/winuser.h:1-29; docs/out-of-scope.md:128-135; docs/audit-24h-free-api.md §3.21
Depends on: None

Problem:
`winuser.h`'s `#ifdef UNICODE` block macro-aliases `SetWindowText`, `PostMessage`, `MessageBox`, `LoadString`, `GetModuleHandle`, `LoadImage`, `GetObject`, `RegisterClass`, `CreateWindowEx`, `CreateWindow`, `PeekMessage`, `GetMessage`, `DispatchMessage`, `DefWindowProc`, `LoadCursor`, `LoadIcon` to their `...W` forms — but none of those sixteen `W`-suffixed target functions are declared anywhere in free-api. This is confirmed deliberate (a `UNICODE` build should fail loudly at compile time, not link silently against a stub). The existing documentation (`winuser.h:6-12`'s own comment, `docs/out-of-scope.md:128-135`) already explains the macros "exist purely for source-compile symmetry" and that no real wide-character behavior is implemented, but does not state the more precise, stronger fact this session verified directly: the underlying `...W` functions are not declared at all, so the failure mode is a compile error at first use, not merely "aliases with no real behavior."

Required work:
- Tighten `docs/out-of-scope.md`'s Unicode section to state explicitly that the sixteen `W`-suffixed target functions referenced by the `UNICODE`-branch macros are undeclared anywhere in free-api, so a `UNICODE` build fails to compile at the first macro use — a deliberate fail-loud design, not an accidental gap.

Acceptance criteria:
- `docs/out-of-scope.md` states the undeclared-target-functions fact precisely, distinct from the existing "no real behavior" framing.
- No code change to `include/winuser.h`.
- Existing tests still pass (no test currently builds with `UNICODE` defined, and none should be added by this task).

Out of scope:
- Do not declare any of the sixteen `...W` functions.
- Do not implement real wide-character/Unicode runtime behavior anywhere.

## WinUser / Message Loop

### TASK-24H-0201: Document PeekMessageA's filter-ignoring as an intentional, evidence-backed decision
Status: DONE — added comment above the (void)-casts in src/winuser_message.cpp; updated include/winuser.h's PeekMessageA doc comment; updated docs/supported-apis.md's row; added a row to docs/out-of-scope.md's "Unsupported APIs" table. Closes TASK-24H-1210 (duplicate).
Priority: P2
Area: WinUser
Type: Documentation
Evidence: src/winuser_message.cpp:25-29; include/winuser.h:264-265; docs/audit-24h-free-api.md §4.7, §5 item 9
Depends on: None

Problem:
`PeekMessageA` explicitly `(void)`-casts away `hWnd`, `wMsgFilterMin`, and `wMsgFilterMax` (`src/winuser_message.cpp:27-29`) — every call behaves as an unfiltered peek regardless of what is passed. Two independent full-source usage sweeps this session confirm neither `../free-eggbert` nor `../planetblupi` ever passes non-zero filters, so this is harmless today, but nothing currently records that this is a deliberate scope decision rather than an unfinished implementation — the header doc comment (`include/winuser.h:264`, "Polls message queue. @note Status: IMPLEMENTED") gives no hint the filter arguments are ignored.

Required work:
- Add a code comment directly above the `(void)`-casts in `src/winuser_message.cpp` stating that filter arguments are intentionally ignored because no evidenced caller in either target game passes a non-zero filter, and that real filtering must not be added without new evidence.
- Update `include/winuser.h:264`'s doc comment for `PeekMessageA` to state the same caveat concisely.
- Add a one-row/one-paragraph note to `docs/out-of-scope.md`'s "Unsupported APIs" table (or a new short subsection) recording this as a permanent, evidence-backed decision, cross-referencing `docs/audit-24h-free-api.md` §4.7.
- Update `docs/supported-apis.md:28`'s `PeekMessageA`/`GetMessageA`/`TranslateMessage`/`DispatchMessageA` row so the bare "IMPLEMENTED" gains a short filter-ignoring caveat instead of implying full Win32 filter semantics.

Acceptance criteria:
- `src/winuser_message.cpp` and `include/winuser.h` both carry an explicit "filters intentionally ignored, evidence-backed" statement, not just an unexplained `(void)` cast.
- `docs/out-of-scope.md` contains a discoverable, evidence-cited entry for this decision.
- `docs/supported-apis.md:28`'s row no longer reads as unconditionally full-featured.
- Existing tests still pass (no behavior change).

Out of scope:
- Do not implement real `hWnd`/`wMsgFilterMin`/`wMsgFilterMax` filtering.
- Do not change `PeekMessageA`'s observable behavior in any way — this task is documentation-only.

---

### TASK-24H-0202: Add a positive-assertion test proving PeekMessageA safely ignores non-zero filter arguments
Status: DONE — added TestPeekMessageAIgnoresNonZeroFilterArguments (test_winuser_regressions.cpp): posts a message, peeks with a mismatched hWnd and an excluding wMsgFilterMin/Max range, asserts the message is still returned. Verified passing (22/22 suite).
Priority: P2
Area: WinUser
Type: Test
Evidence: src/winuser_message.cpp:25-29; tests/test_timer_regressions.cpp:339-344 (comment only, no assertion); docs/audit-24h-free-api.md §4.7
Depends on: None

Problem:
`tests/test_timer_regressions.cpp:339-344` already contains a comment explaining that `PeekMessageA` ignores the `hwnd` filter, but that comment exists to justify *not* testing something else — it is not itself a positive assertion that non-zero filters are safely ignored. No existing test in `tests/test_winuser_regressions.cpp` (checked: `TestPeekMessageNoRemoveAndRemove`, lines 268-309) passes a non-zero, message-excluding filter and asserts the message is still returned. This is exactly the gap the audit flags: "filter-ignoring is documented by test comment but not independently asserted as a positive claim."

Required work:
- Add a new test to `tests/test_winuser_regressions.cpp` that posts a message, then calls `PeekMessageA` with a deliberately mismatched non-zero `hWnd` and a `wMsgFilterMin`/`wMsgFilterMax` range that would exclude the posted message under real Win32 filtering semantics, and asserts the message is still returned (proving filters are ignored, not that unfiltered peeks merely work).
- Register the new test function in `main()`.

Acceptance criteria:
- New test fails if real filtering were ever accidentally introduced (i.e. it is a genuine regression guard for the documented decision in TASK-24H-0201).
- Test passes against the current implementation.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not implement real filtering to make the test "more correct" — the test asserts the current, intentional ignore-filters behavior.

---

### TASK-24H-0203: Add a timing regression test proving PeekMessageA never sleeps/blocks on an empty queue
Status: DONE — TestPeekMessageANeverSleepsOnEmptyQueue (tests/test_winuser_regressions.cpp) runs 20000 empty-queue PeekMessageA calls and asserts total elapsed time stays well under what a reintroduced per-call SDL_Delay(1) would take.
Priority: P1
Area: WinUser
Type: Test
Evidence: src/winuser_message.cpp:36-41,84-85; tests/test_timer_regressions.cpp:261-263 (comment only, no timing assertion); docs/audit-24h-free-api.md §4.7
Depends on: None

Problem:
`src/winuser_message.cpp:36-41` documents, in a code comment, that `PeekMessageA` "must NEVER sleep or block" because a prior regression that added a per-call `SDL_Delay(1)` produced a real, player-visible "rubbery / laggy feel" in both games' tight main loops. `tests/test_timer_regressions.cpp:261-263` repeats this constraint in a comment but only as justification for a stress test's loop shape — no test independently times a batch of `PeekMessageA` calls on an empty queue and asserts they complete near-instantly. This is a real historical regression with zero dedicated regression coverage protecting against its reintroduction.

Required work:
- Add a test that calls `PeekMessageA` on an empty queue N times (e.g. 10,000+) in a tight loop and asserts the total elapsed wall-clock time stays within a generous bound consistent with "no per-call sleep" (e.g. well under what N×1ms would take), distinguishing a correct zero-block implementation from a regression that reintroduces `SDL_Delay(1)` per call.
- Place the test in `tests/test_winuser_regressions.cpp` alongside the other `PeekMessageA` tests, or `tests/test_timer_regressions.cpp` next to the existing related comment.

Acceptance criteria:
- Test would fail if a `SDL_Delay(1)`-per-call regression were reintroduced into `PeekMessageA`'s empty-queue path.
- Test passes against the current implementation.
- Existing tests still pass.

Out of scope:
- Do not add any actual delay/throttling to `PeekMessageA` — this is a regression guard for the existing zero-block behavior, not a behavior change.

---

### TASK-24H-0204: Document WaitMessage's real polling-based contract instead of implying true blocking semantics
Status: DONE — added comment above WaitMessage's implementation in src/winuser_message.cpp; updated include/winuser.h's doc comment; updated docs/supported-apis.md's row; added a row to docs/out-of-scope.md's "Unsupported APIs" table. Closes TASK-24H-1211 (duplicate).
Priority: P2
Area: WinUser
Type: Documentation
Evidence: src/winuser_message.cpp:235-257; include/winuser.h:277-278; docs/supported-apis.md:29; docs/audit-24h-free-api.md §4.7, §5 item 10
Depends on: None

Problem:
`WaitMessage` (`src/winuser_message.cpp:235-257`) checks the queue twice (before and after a single `PumpSdlEvents()` call), sleeps once for ~1ms only if both checks miss, and then returns `TRUE` **unconditionally** even if the queue is still empty after that final check — it is not a true OS-level blocking wait. Neither `include/winuser.h:277-278`'s doc comment ("Waits until the message queue receives work. @note Status: PARTIAL") nor `docs/supported-apis.md:29`'s status ("IMPLEMENTED (non-busy idle wait)") mentions the unconditional-`TRUE` return or the single-poll-then-return shape — both read as if this were a real blocking wait, which it is not. `src/winuser_message.cpp` itself has zero comment explaining this design.

Required work:
- Add a code comment directly above `WaitMessage`'s implementation in `src/winuser_message.cpp` explaining the two-check-then-unconditional-`TRUE` design, why it's adequate for both games' current idle-loop usage, and that it is a documented Win32 semantic deviation.
- Update `include/winuser.h:277-278`'s doc comment to state plainly that this is a polling-based approximation, not a real OS wait/condvar.
- Correct `docs/supported-apis.md:29`'s "(non-busy idle wait)" wording, which is misleading (it correctly implies "doesn't busy-spin" but wrongly implies "genuinely blocks until a message arrives") — replace with accurate language describing the poll-twice/return-unconditionally-`TRUE` contract.

Acceptance criteria:
- A future contributor reading any of the three files above will understand `WaitMessage` may return `TRUE` with an empty queue.
- Existing tests still pass.
- No behavior change.

Out of scope:
- Do not implement a true OS-level blocking `WaitMessage` (e.g. condition variable woken by `PushMessage`) without new evidence it's needed — this is a "pick the documented behavior, don't implement the Win32-faithful version" task per project policy.

---

### TASK-24H-0205: Add a regression test proving WaitMessage returns TRUE unconditionally even when the queue is still empty
Status: DONE — TestWaitMessageDoesNotBusySpin (tests/test_winuser_regressions.cpp) now also asserts every WaitMessage() call in its confirmed-empty-queue busy-spin window returns TRUE.
Priority: P1
Area: WinUser
Type: Test
Evidence: src/winuser_message.cpp:235-257; tests/test_winuser_regressions.cpp:631-685 (TestWaitMessageDoesNotBusySpin — covers non-busy-spin and cross-thread delivery, not the unconditional-TRUE-on-still-empty contract)
Depends on: None

Problem:
`tests/test_winuser_regressions.cpp:631-685`'s `TestWaitMessageDoesNotBusySpin` already proves `WaitMessage` doesn't busy-spin (iteration count stays low) and that it observes a message posted from another thread — but it never inspects `WaitMessage`'s own return value in the case where the queue is confirmed still empty after the internal poll. The specific, real spec deviation — "returns `TRUE` unconditionally even if the queue is still empty" — has zero direct assertion anywhere in the test suite. Since both games' idle loops depend on this exact contract (not a true block), a future refactor could silently "fix" it into a real blocking wait or into returning `FALSE` on timeout, either of which could hang or busy-loop the games.

Required work:
- Add a new test (or extend `TestWaitMessageDoesNotBusySpin` with a clearly separated new check) that calls `WaitMessage` with a guaranteed-empty queue (no posts, no background thread) and asserts the return value is `TRUE`, locking in the current unconditional-`TRUE` contract.

Acceptance criteria:
- New assertion explicitly checks `WaitMessage()`'s `BOOL` return value against `TRUE` under a confirmed-empty queue.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not change `WaitMessage`'s return-value contract to make it "more correct" relative to real Win32 — this test locks in the current, intentional behavior.

---

### TASK-24H-0206: Document WM_ACTIVATEAPP(0) focus-loss suppression in docs/out-of-scope.md
Status: DONE — added a new "WM_ACTIVATEAPP(0) focus-loss suppression" section to docs/out-of-scope.md, cross-referencing the still-delivered WM_ACTIVATEAPP(1) focus-gained case. Closes TASK-24H-1205 (duplicate).
Priority: P2
Area: WinUser
Type: Documentation
Evidence: src/internal/FreeApiMessageQueue.cpp:276-291; docs/audit-24h-free-api.md §4.6, §5 item 11
Depends on: None

Problem:
`SDL_EVENT_WINDOW_FOCUS_LOST` is deliberately suppressed in `src/internal/FreeApiMessageQueue.cpp:276-291` — no `WM_ACTIVATEAPP(0)` is ever sent — specifically to avoid the game setting an internal "inactive" flag and freezing on spurious focus-loss events (the source comment at lines 277-282 explains this clearly). This is real, permanent, working behavior, but it is a genuine Win32 semantic deviation (real Win32 does deliver `WM_ACTIVATEAPP(0)`) and is not written up anywhere in `docs/out-of-scope.md`, which is the project's canonical location for exactly this kind of decision.

Required work:
- Add a new subsection to `docs/out-of-scope.md` (matching the style of the existing "MCI digital-video / AVI movie playback" section) documenting the suppression, its exact freeze-avoidance rationale, and citing `src/internal/FreeApiMessageQueue.cpp:276-291`.
- Cross-reference `WM_ACTIVATEAPP(1)` (focus-gained, which **is** still delivered, `FreeApiMessageQueue.cpp:265-274`) so the asymmetry is explicit and not mistaken for an oversight.

Acceptance criteria:
- `docs/out-of-scope.md` contains a discoverable, evidence-cited entry for this permanent suppression.
- Existing tests still pass.
- No behavior change.

Out of scope:
- Do not implement `WM_ACTIVATEAPP(0)` delivery on focus loss — this is a "pick the documented behavior, don't implement the Win32-faithful version" task per project policy.
- Do not add a configurable delay/flag for this — no evidence either game needs it.

---

### TASK-24H-0207: Document GetMessageA's poll-based (SDL_Delay(1)) implementation
Status: DONE — updated include/winuser.h's GetMessageA doc comment to state it's a PeekMessageA spin-loop with SDL_Delay(1), not a real OS wait; added a matching code comment above the definition in src/winuser_message.cpp cross-referencing PeekMessageA's "never sleep" invariant. Verified 25/25 in all three build trees.
Priority: P2
Area: WinUser
Type: Documentation
Evidence: src/winuser_message.cpp:102-149 (SDL_Delay(1) at line 146); include/winuser.h:252-253; docs/audit-24h-free-api.md §4.7
Depends on: None

Problem:
`GetMessageA` (`src/winuser_message.cpp:102-149`) is a `PeekMessageA`-based spin-loop that calls `SDL_Delay(1)` (line 146) between poll attempts — functionally blocking via 1ms-granularity polling, not an OS-level wait/condvar. `include/winuser.h:252-253`'s doc comment ("Blocks until a message is available or quit is posted. @note Status: PARTIAL") does not distinguish this from a real OS blocking primitive, which matters for anyone reasoning about CPU usage or wake latency.

Required work:
- Update `include/winuser.h:252-253`'s doc comment to state that `GetMessageA` is implemented as a `PeekMessageA` spin-loop with a 1ms delay between attempts, not a real OS wait.
- Add a brief code comment in `src/winuser_message.cpp` above `GetMessageA` cross-referencing this design and its relationship to `PeekMessageA`'s "never sleep" invariant (i.e. only `GetMessageA`/`WaitMessage` are allowed to sleep; `PeekMessageA` itself never does).

Acceptance criteria:
- Header doc comment accurately describes the polling mechanism.
- Existing tests still pass.
- No behavior change.

Out of scope:
- Do not replace the polling loop with a real OS-level blocking primitive without new evidence it's needed.

---

### TASK-24H-0208: Document DispatchMessageA's null-hwnd single-window fallback assumption
Status: DONE — updated include/winuser.h's DispatchMessageA doc comment and added a docs/out-of-scope.md entry ("Single live window assumption / DispatchMessageA's null-hwnd fallback") stating the single-window assumption and that multi-window support must not be added without revisiting this fallback. Implemented together with duplicate TASK-24H-0304. Verified 25/25 in all three build trees.
Priority: P2
Area: WinUser
Type: Documentation
Evidence: src/winuser_message.cpp:183-191; docs/audit-24h-free-api.md §4.5, §2.2
Depends on: None

Problem:
`DispatchMessageA`'s null-hwnd fallback (`src/winuser_message.cpp:183-191`) dispatches to "whatever single window happens to be registered first" when a message's `hwnd` is `NULL` and isn't found in `g_windowProcedures`. This is fine for the single-window case both games use (this fallback is genuinely reachable today, e.g. if `SDL_EVENT_WINDOW_CLOSE_REQUESTED`'s `FindWindowById` ever fails to resolve a window and pushes `WM_CLOSE` with a `NULL` hwnd), but would silently dispatch to the wrong window if the project ever grew multi-window support. This assumption is documented in an inline code comment but not in `include/winuser.h`'s doc comment or in any project-level scope document.

Required work:
- Update `include/winuser.h:260-261`'s `DispatchMessageA` doc comment to note the single-registered-window fallback assumption explicitly.
- Add an entry to `docs/out-of-scope.md` (or extend the "Unsupported APIs" area) stating that multi-window support is not implemented and that this fallback must be revisited before any multi-window work is undertaken.

Acceptance criteria:
- Both the header doc comment and `docs/out-of-scope.md` explicitly describe the single-window assumption and its fragility if extended.
- Existing tests still pass.
- No behavior change.

Out of scope:
- Do not add multi-window support or generalize the fallback — no evidenced need in either target game.

---

### TASK-24H-0209: Add a regression test exercising DispatchMessageA's null-hwnd single-window fallback path
Status: DONE — added TestDispatchMessageARoutesNullHwndToSoleRegisteredWindow (tests/test_winuser_regressions.cpp): registers one window, PostMessageA(NULL, WM_USER+77, ...), drains, asserts delivery to the sole window's own WndProc with the correct HWND. Verified passing (23/23 suite).
Priority: P2
Area: WinUser
Type: Test
Evidence: src/winuser_message.cpp:183-191; src/internal/FreeApiMessageQueue.cpp:255-264 (WM_CLOSE pushed with FindWindowById(...) result, which can be NULL)
Depends on: None

Problem:
`DispatchMessageA`'s null-hwnd single-window fallback (`src/winuser_message.cpp:183-191`) is real, reachable code (not dead code — `WM_CLOSE` is pushed with whatever `FindWindowById` returns, which can be `NULL`), but no existing test in `tests/test_winuser_regressions.cpp` or elsewhere directly constructs a `NULL`-hwnd message and verifies it dispatches to the sole registered window's `WndProc` rather than falling through to the generic `DefWindowProcA(NULL, ...)` path.

Required work:
- Add a test that registers exactly one window/WndProc, pushes a message with `hwnd = NULL` via `PostMessageA(nullptr, ...)`, drains it via `PeekMessageA`/`DispatchMessageA`, and asserts it was delivered to the single registered window's own `WndProc` (not silently dropped or misrouted).

Acceptance criteria:
- New test directly exercises the null-hwnd fallback branch in `DispatchMessageA` (`src/winuser_message.cpp:185-191`).
- Test passes against the current implementation.
- Existing tests still pass.

Out of scope:
- Do not test or add multi-window fallback disambiguation — single-window case only, matching current scope.

---

### TASK-24H-0210: Strengthen WM_CLOSE→DestroyWindow test with a direct teardown assertion
Status: DONE — TestDefWindowProcHandlesWmClose (tests/test_winuser_regressions.cpp) now uses a dedicated WmCloseTrackingWndProc and directly asserts WM_DESTROY was delivered to the correct HWND, not just that WM_QUIT eventually appeared.
Priority: P1
Area: WinUser
Type: Test
Evidence: tests/test_winuser_regressions.cpp:185-220 (TestDefWindowProcHandlesWmClose); src/winuser_message.cpp:202-213 (DefWindowProcA's WM_CLOSE handling)
Depends on: None

Problem:
`tests/test_winuser_regressions.cpp:185-220`'s `TestDefWindowProcHandlesWmClose` already exercises the real call chain (`WM_CLOSE` → `DefWindowProcA` → `DestroyWindow` → synchronous `WM_DESTROY` → `DefWindowProcA`'s `WM_DESTROY` handler → `PostQuitMessage`), but it only asserts that `WM_QUIT` eventually appears in the queue — it never directly proves `DestroyWindow` itself was invoked as a consequence of `WM_CLOSE`. The `WM_QUIT` observation is a correct but indirect proxy; a regression that broke the `WM_CLOSE`→`DestroyWindow` call specifically (while some other path still produced `WM_QUIT`) would not be caught by the current assertion shape.

Required work:
- Extend `TestDefWindowProcHandlesWmClose` (or add an adjacent test) using a `WndProc` that tracks `WM_DESTROY` receipt (reusing the existing `DestroyTrackingWndProc` pattern at `tests/test_winuser_regressions.cpp:225-232`), post `WM_CLOSE` to that window, drain the queue, and assert `WM_DESTROY` was directly received by that window's own `WndProc` as a direct result of `WM_CLOSE` processing — not just that `WM_QUIT` eventually appeared.

Acceptance criteria:
- Test directly asserts `DestroyWindow`'s effect (WM_DESTROY delivery to the destroyed window's own WndProc) as a consequence of posting `WM_CLOSE`, not only the downstream `WM_QUIT`.
- Existing `TestDefWindowProcHandlesWmClose` assertions remain and still pass.
- Existing tests still pass.

Out of scope:
- Do not duplicate `TestDestroyWindowDispatchesWmDestroySynchronously` (`tests/test_winuser_regressions.cpp:234-266`), which already covers direct `DestroyWindow()` calls — this task is specifically about the `WM_CLOSE`-triggered path through `DefWindowProcA`.

---

### TASK-24H-0211: Verify existing coverage for DestroyWindow's synchronous WM_DESTROY dispatch is adequate
Status: DONE — re-ran TestDestroyWindowDispatchesWmDestroySynchronously directly and confirmed it passes and genuinely exercises synchronous dispatch. docs/supported-apis.md's DestroyWindow row already cites the exact test name (added in an earlier session) -- acceptance criteria already satisfied, no doc change needed. No duplicate test added.
Priority: P3
Area: WinUser
Type: Verification
Evidence: tests/test_winuser_regressions.cpp:234-266 (TestDestroyWindowDispatchesWmDestroySynchronously); docs/audit-24h-free-api.md §4.5
Depends on: None

Problem:
The audit checklist calls for confirming `DestroyWindow` dispatches `WM_DESTROY` synchronously, with a test. `tests/test_winuser_regressions.cpp:234-266`'s `TestDestroyWindowDispatchesWmDestroySynchronously` already does exactly this (registers a `WM_DESTROY`-tracking `WndProc`, calls `DestroyWindow` directly, asserts `WM_DESTROY` was received synchronously with the correct `hwnd`). This backlog item exists to formally close out that checklist item without adding duplicate test code, per the explicit instruction not to duplicate existing coverage.

Required work:
- Re-run `tests/test_winuser_regressions.cpp` and confirm `TestDestroyWindowDispatchesWmDestroySynchronously` passes and genuinely exercises synchronous dispatch (not just registration).
- Record in `docs/supported-apis.md`'s `DefWindowProcA` row (`docs/supported-apis.md:31`) or a nearby note that this specific behavior (`DestroyWindow` → synchronous `WM_DESTROY`) is covered by that named test, so a future contributor doesn't re-propose it.

Acceptance criteria:
- Confirmed (by running the test) that coverage is adequate; no new test added for this specific behavior.
- `docs/supported-apis.md` cross-references the exact test name for future discoverability.
- Existing tests still pass.

Out of scope:
- Do not add a second, duplicate test for synchronous `WM_DESTROY` dispatch.

---

### TASK-24H-0212: Document WM_MOUSEMOVE/WM_TIMER queue coalescing as a deliberate, permanent Win32 semantic deviation
Status: DONE — added a docs/out-of-scope.md entry ("WM_MOUSEMOVE/WM_TIMER queue coalescing") citing src/internal/FreeApiMessageQueue.cpp:68-101 and explaining why (unbounded queue growth without it). Verified 25/25 in all three build trees.
Priority: P2
Area: WinUser
Type: Documentation
Evidence: src/internal/FreeApiMessageQueue.cpp:68-101; docs/audit-24h-free-api.md §4.6
Depends on: None

Problem:
`PushMessage` (`src/internal/FreeApiMessageQueue.cpp:68-101`) coalesces `WM_MOUSEMOVE` (in-place update, per-hwnd) and `WM_TIMER` (drop-duplicate, per-hwnd+timer-id) to prevent unbounded queue growth. This is real, permanent, working behavior and is well-commented in the source, but real Win32 does not silently drop/merge queued messages this way, and this deviation is not recorded anywhere in `docs/out-of-scope.md` — only in source comments and this session's audit.

Required work:
- Add an entry to `docs/out-of-scope.md` documenting the coalescing behavior for both `WM_MOUSEMOVE` and `WM_TIMER`, citing `src/internal/FreeApiMessageQueue.cpp:68-101`, and explaining why it's necessary (mouse motion/timer messages fire faster than the game can consume them, causing input lag behind stale motion/jittery timers without coalescing).

Acceptance criteria:
- `docs/out-of-scope.md` contains a discoverable, evidence-cited entry for this permanent deviation.
- Existing tests still pass.
- No behavior change.

Out of scope:
- Do not remove or change the coalescing behavior.
- Do not extend coalescing to any other message type without new evidence.

---

### TASK-24H-0213: Add a dedicated regression test for WM_TIMER message coalescing
Status: DONE — added TestWmTimerCoalescingKeepsOnlyOneQueuedMessagePerHwndAndId (tests/test_winuser_regressions.cpp): two PostMessageA(hwnd, WM_TIMER, id, 0) calls back-to-back, drains, asserts exactly one WM_TIMER with that id survives. Verified passing (23/23 suite).
Priority: P2
Area: WinUser
Type: Test
Evidence: src/internal/FreeApiMessageQueue.cpp:91-101; docs/audit-24h-free-api.md §4.6 ("already implemented and reasonably tested" — verified only incidental coverage exists, not a dedicated direct test)
Depends on: None

Problem:
`WM_TIMER` coalescing (drop-duplicate per hwnd+timer-id when a message of the same shape is already queued, `src/internal/FreeApiMessageQueue.cpp:91-101`) has no test that directly, minimally proves the coalescing invariant itself — existing timer tests (`tests/test_timer_regressions.cpp`) exercise `SetTimer`/`WM_TIMER` delivery broadly but don't isolate "posting the same timer id twice while one is already queued results in exactly one queued message, not two."

Required work:
- Add a focused test that pushes two `WM_TIMER` messages for the same `hwnd`+timer-id in rapid succession (before either is drained) and asserts the queue contains exactly one such message afterward (not two), directly proving the coalescing behavior documented in TASK-24H-0212.

Acceptance criteria:
- New test directly and minimally proves the `WM_TIMER` coalescing invariant.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not test or add coalescing for any message type other than `WM_TIMER` in this task (see TASK-24H-0214 for `WM_MOUSEMOVE`).

---

### TASK-24H-0214: Add a dedicated regression test for WM_MOUSEMOVE message coalescing
Status: DONE — added TestWmMouseMoveCoalescingKeepsOnlyLatestPosition (tests/test_winuser_regressions.cpp): two InjectMouseMotion calls back-to-back before draining, asserts exactly one WM_MOUSEMOVE survives and carries the second (latest) position. Verified passing (23/23 suite).
Priority: P2
Area: WinUser
Type: Test
Evidence: src/internal/FreeApiMessageQueue.cpp:73-86; docs/audit-24h-free-api.md §4.6, §4.10
Depends on: None

Problem:
`WM_MOUSEMOVE` coalescing (in-place update per hwnd when a `WM_MOUSEMOVE` is already queued, `src/internal/FreeApiMessageQueue.cpp:73-86`) has no direct, minimal test proving the invariant — existing mouse tests (`tests/test_winuser_regressions.cpp:345-454`, `TestMouseMoveLParamPackingAndModifierFlags`) verify lParam packing and modifier flags via injected single motion events, but never inject two rapid motions before draining and assert only one coalesced `WM_MOUSEMOVE` (with the latest position) remains queued.

Required work:
- Add a focused test that injects two `SDL_EVENT_MOUSE_MOTION` events for the same window in rapid succession (before draining), pumps them into the internal queue, and asserts exactly one `WM_MOUSEMOVE` is present afterward, carrying the *second* (latest) position — directly proving the coalescing behavior documented in TASK-24H-0212.

Acceptance criteria:
- New test directly and minimally proves the `WM_MOUSEMOVE` coalescing invariant, including that the surviving message reflects the latest, not the first, position.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not test or add coalescing for any message type other than `WM_MOUSEMOVE` in this task (see TASK-24H-0213 for `WM_TIMER`).

---

### TASK-24H-0215: Document PostMessageA's lack of hWnd/message validation as an accepted design choice
Status: DONE — added a note to docs/supported-apis.md's PostQuitMessage/PostMessageA row and a matching code comment above PostMessageA (src/winuser_message.cpp). No behavior change. Verified 26/26 in all three build trees.
Priority: P3
Area: WinUser
Type: Documentation
Evidence: src/winuser_message.cpp:19-23; docs/audit-24h-free-api.md §2.2 (row: "PARTIAL, mutex-protected, no validation")
Depends on: None

Problem:
`PostMessageA` (`src/winuser_message.cpp:19-23`) enqueues any `hWnd`/`Msg`/`wParam`/`lParam` combination without validating that `hWnd` refers to a real registered window or that `Msg` is a recognized message ID. This is low-risk (both games always pass a valid, live `hWnd`) but is currently undocumented as an intentional, evidence-scoped simplification rather than a gap.

Required work:
- Add a short note to `docs/supported-apis.md`'s `PostQuitMessage`/`PostMessageA` row (`docs/supported-apis.md:30`) or `docs/out-of-scope.md` stating that `PostMessageA` performs no `hWnd`/message validation, that this is safe because both target games only ever post to windows they themselves created, and that this should not be read as an oversight.

Acceptance criteria:
- Documented decision is discoverable in `docs/supported-apis.md` or `docs/out-of-scope.md`.
- Existing tests still pass.
- No behavior change.

Out of scope:
- Do not add hWnd/message validation to `PostMessageA` — no evidenced need, and would add rejection-path behavior neither game exercises.

---

### TASK-24H-0216: Add missing docs/supported-apis.md rows for pass-through, non-input WM_* message IDs
Status: DONE — added a grouped row for the 8 pass-through WM_* constants. Verified 26/26 in all three build trees (docs-only).
Priority: P3
Area: WinUser
Type: Documentation
Evidence: docs/audit-24h-free-api.md §2.5; include/winuser.h:182-215; docs/supported-apis.md (no rows found for WM_CREATE, WM_SYSCOLORCHANGE, WM_ACTIVATEAPP, WM_SETCURSOR, WM_NCMOUSEMOVE, WM_DISPLAYCHANGE, WM_QUERYNEWPALETTE, WM_PALETTECHANGED)
Depends on: None

Problem:
The audit's §2.5 "Used message IDs" confirms both target games handle `WM_CREATE`, `WM_ACTIVATEAPP`, `WM_SYSCOLORCHANGE`, `WM_QUERYNEWPALETTE`, `WM_PALETTECHANGED`, `WM_DISPLAYCHANGE`, `WM_SETCURSOR`, and (planetblupi only) `WM_NCMOUSEMOVE`. These constants exist in `include/winuser.h:182-215` under one group-level `@note Status: PARTIAL` comment, but `docs/supported-apis.md` — the project's living, per-symbol reference table — has zero individual rows for any of them (confirmed by grep: only `MM_MCINOTIFY` and the input-message rows are present). A contributor scanning `docs/supported-apis.md` for "what message IDs does free-api support" would not find these at all.

Required work:
- Add a row (or a small grouped row) to `docs/supported-apis.md` for the pass-through, non-input `WM_*` constants both games rely on, noting they are plain dispatched constants (no free-api-side special handling beyond generic `DispatchMessageA` routing, unlike `WM_MOUSEMOVE`/`WM_TIMER`/keyboard messages which have dedicated translation logic), citing `include/winuser.h:182-215` and `docs/audit-24h-free-api.md` §2.5.

Acceptance criteria:
- `docs/supported-apis.md` has a discoverable entry covering the listed message IDs.
- Existing tests still pass.
- No behavior change.

Out of scope:
- Do not add per-message dedicated translation/handling logic for any of these IDs — they are correctly generic pass-through today.

---

### TASK-24H-0217: Correct docs/supported-apis.md's test-coverage column for the message-pump row
Status: DONE — added test_winuser_regressions.cpp and test_timer_regressions.cpp to the PeekMessageA/GetMessageA row's test-coverage column. Spot-checked nearby rows (WaitMessage/PostMessageA/wsprintfA); already accurate, no other fixes needed. Verified 26/26 in all three build trees (docs-only).
Priority: P3
Area: WinUser
Type: Documentation
Evidence: docs/supported-apis.md:28; tests/test_winuser_regressions.cpp (TestPeekMessageNoRemoveAndRemove, TestGetMessageReturnsFalseOnQuit, TestWaitMessageDoesNotBusySpin); tests/test_timer_regressions.cpp
Depends on: None

Problem:
`docs/supported-apis.md:28`'s row for `PeekMessageA`/`GetMessageA`/`TranslateMessage`/`DispatchMessageA` lists only `test_input_pipeline.cpp`, `test_planetblupi_loop.cpp`, and `test_eggbert_loop.cpp` as test coverage. `tests/test_winuser_regressions.cpp` (which directly tests `PeekMessageA` remove/no-remove semantics and `GetMessageA`'s `WM_QUIT` return) and `tests/test_timer_regressions.cpp` (which stress-tests `PeekMessageA` under concurrent posting) are not listed, understating actual coverage and making it harder for a future contributor to find the relevant tests.

Required work:
- Update `docs/supported-apis.md:28`'s test-coverage column to also list `test_winuser_regressions.cpp` and `test_timer_regressions.cpp`.
- Spot-check the same row/nearby rows (`docs/supported-apis.md:29-31`) for any other missing test-file references while making this edit, correcting any found.

Acceptance criteria:
- `docs/supported-apis.md:28`'s test-coverage column accurately reflects all test files that actually exercise these symbols.
- Existing tests still pass.
- No behavior change.

Out of scope:
- Do not restructure the table's columns or format — value-only correction.

## Window and Cursor

### TASK-24H-0301: Add positive test that RegisterClassA discards non-WNDPROC WNDCLASSA fields
Status: DONE — added TestRegisterClassADiscardsNonWndprocFields (test_winuser_regressions.cpp): re-registers the same class name with a fully different field set (including a different lpfnWndProc) and confirms the second registration's WndProc is the one that actually runs. Added a cross-referencing comment at src/winuser_window.cpp:19 (RegisterClassA). Verified passing (22/22 suite).
Priority: P2
Area: WinUser
Type: Test
Evidence: src/winuser_window.cpp:13-21; tests/test_winuser_regressions.cpp:119-147; docs/audit-24h-free-api.md §4.4
Depends on: None

Problem:
`RegisterClassA` (`src/winuser_window.cpp:13-21`) stores only the `WNDPROC` keyed by class name and silently discards `hIcon`/`hCursor`/`hbrBackground`/`style`/`cbClsExtra`/`cbWndExtra`/`lpszMenuName`. This is confirmed acceptable — neither target game reads these fields back after registration — but `TestRegisterClassAWithFullFieldSet` (`tests/test_winuser_regressions.cpp:119-147`) only proves populating every field doesn't crash `RegisterClassA`/`CreateWindowA`; it never positively asserts that the non-`WNDPROC` fields are in fact not retained or exposed anywhere.

Required work:
- Add an assertion (or a new small test function) that, after `RegisterClassA` with a fully-populated `WNDCLASSA`, no free-api-visible state exposes `hIcon`/`hCursor`/`hbrBackground`/`style`/extra-bytes back to the caller (e.g., confirm there is no getter, and that a second `RegisterClassA` call for the same class name with different non-`WNDPROC` fields still succeeds identically).
- Add a one-line code comment at `src/winuser_window.cpp:19` cross-referencing this test, so the "fields intentionally dropped" behavior is discoverable from the implementation, not only from the test file.

Acceptance criteria:
- A new or extended test explicitly documents and locks in that only `lpfnWndProc` is retained by `RegisterClassA`.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not start storing `hIcon`/`hCursor`/`hbrBackground`/style/extra-bytes — no evidenced call site in either game reads them back.
- Do not implement `RegisterClassExA`/`WNDCLASSEXA` as part of this task; that remains a separate, unevidenced non-goal (see TASK-24H-0302).

---

### TASK-24H-0302: Add one-line confirming note that RegisterClassExA/WNDCLASSEXA do not exist
Status: DONE — duplicate of TASK-24H-0116; implemented once there (see that entry).
Priority: P3
Area: WinUser
Type: Documentation
Evidence: docs/audit-24h-free-api.md §1 risk#5, §2.2, §3.21, §4.4; docs/out-of-scope.md
Depends on: None

Problem:
`RegisterClassExA`/`WNDCLASSEXA` are absent from free-api's entire source and header tree. This is correct — neither target game uses them, both use the plain `WNDCLASSA`/`RegisterClassA` form — but `docs/out-of-scope.md`'s "Unsupported APIs" table has no row for this pair, so a future contributor scanning that file for "is this decided?" won't find it there.

Required work:
- Add one row to `docs/out-of-scope.md`'s "Unsupported APIs" table for `RegisterClassExA`/`WNDCLASSEXA`, stating they are confirmed absent, zero call sites in either game, and any future WinAPI surface expansion request for these should be scrutinized hard since there is no existing partial implementation to extend from.

Acceptance criteria:
- `docs/out-of-scope.md` contains an explicit, evidenced row for `RegisterClassExA`/`WNDCLASSEXA`.
- Existing tests still pass (doc-only change).
- No unrelated API is added.

Out of scope:
- Do not implement `RegisterClassExA`/`WNDCLASSEXA` — no evidenced call site in either game; this task is documentation only.
- Do not add any other new WinUser header declarations as part of this task.

---

### TASK-24H-0303: Gate winuser_window.cpp's unconditional SDL_Log calls behind FreeApiDiagnosticsEnabled()
Status: DONE — all 13 sites gated; verified quiet-by-default and restored under FREE_API_DIAGNOSTICS=1; 17/17 in all three build modes. Duplicate of TASK-24H-1101, closed together.
Priority: P1
Area: WinUser
Type: Bugfix
Evidence: src/winuser_window.cpp:42,53,59,82,89,93,98,126,200,228,262,313,315; src/internal/FreeApiDiagnostics.hpp:36; src/wingdi_blit.cpp:26,35,89,142 (existing gating pattern); docs/audit-24h-free-api.md §4.29, §5 item 6
Depends on: None

Problem:
`src/winuser_window.cpp` contains 13 confirmed unconditional `SDL_Log` calls across `CreateWindowExA`, `ShowWindow`, `UpdateWindow`, and `SetFocus` (lines 42, 53, 59, 82, 89, 93, 98, 126, 200, 228, 262, 313, 315). Every one of them fires on every normal game launch/window-lifecycle event with no opt-out short of a code change, unlike the rest of the codebase's disciplined `FreeApiGdiDebugEnabled()`/`FreeApiDiagnosticsEnabled()` gating (e.g. `src/wingdi_blit.cpp`). The file already `#include`s `internal/FreeApiDiagnostics.hpp` (line 5) but never calls `FreeApiDiagnosticsEnabled()`. This violates the user's "quiet by default" requirement for this session and is the single largest unconditional-logging source in the codebase. NOTE: this task is also tracked as TASK-24H-1101 in the Diagnostics theme — implement once, close both.

Required work:
- Wrap all 13 `SDL_Log` call sites in `src/winuser_window.cpp` in `if (FreeApiDiagnosticsEnabled()) { ... }`, matching the existing GDI gating pattern exactly.
- Verify no log call's side effect (only logging, no state mutation) is lost by gating.

Acceptance criteria:
- With diagnostics disabled (default), launching either target game produces zero log lines from `winuser_window.cpp`.
- With diagnostics enabled, all 13 log lines still appear exactly as before.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not change any non-logging behavior in `CreateWindowExA`/`ShowWindow`/`UpdateWindow`/`SetFocus`.
- Do not touch `LoadImageA`'s unconditional success-path log (`src/wingdi_bitmap.cpp:47`) — that belongs to the GDI theme (TASK-24H-0606), not window/cursor.

---

### TASK-24H-0304: Document DispatchMessageA's single-registered-window null-hwnd fallback as a documented single-window-only assumption
Status: DONE — duplicate of TASK-24H-0208; implemented once there (see that entry).
Priority: P2
Area: WinUser
Type: Documentation
Evidence: src/winuser_message.cpp:183-192; src/winuser_window.cpp (single-window creation path); docs/audit-24h-free-api.md §4.5
Depends on: None

Problem:
`DispatchMessageA`'s fallback (`src/winuser_message.cpp:183-192`) dispatches a message with a `NULL`/unmatched `hwnd` to "whatever single window happens to be registered first" (`g_windowProcedures.begin()`). The audit (§4.5) flags this as fine for the single-window case both games use, but fragile if the project were ever extended to multiple windows — and this assumption currently exists only as an inline code comment, not as a documented, discoverable project decision. NOTE: overlaps with TASK-24H-0208 (WinUser message-loop theme) — implement once, close both.

Required work:
- Add an explicit entry to `docs/out-of-scope.md` (or `docs/headers.md`, whichever already houses window-model decisions) stating that free-api's window model assumes exactly one live window at a time, that `DispatchMessageA`'s null-hwnd fallback depends on this, and that multi-window support must not be added without revisiting this fallback.

Acceptance criteria:
- The single-window assumption and its `DispatchMessageA` dependency are documented in `docs/` with a file:line citation.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not implement real multi-window routing or change the fallback's behavior.
- Do not add a second concurrent window to any test as part of this task.

---

### TASK-24H-0305: Verify and document that CreateWindowExA's dwExStyle is stored/logged only, never translated into real SDL window behavior
Status: DONE — confirmed via grep that neither game reads dwExStyle back after passing WS_EX_TOPMOST; both run one exclusive fullscreen/popup window for their whole session. Added a docs/out-of-scope.md entry. Verified 25/25 in all three build trees (docs-only, no rebuild needed).
Priority: P2
Area: WinUser
Type: Verification
Evidence: src/winuser_window.cpp:36,42-50,72-80,123; tests/test_winuser_regressions.cpp:153-181 (TestCreateWindowExAFullscreenPath, uses WS_EX_TOPMOST)
Depends on: None

Problem:
`CreateWindowExA`'s `Uint32 flags` computation (`src/winuser_window.cpp:72-80`) only inspects `dwStyle` (`WS_VISIBLE`, `WS_POPUP`, `WS_CAPTION`); `dwExStyle` is only used for logging (line 46) and to populate `CREATESTRUCTA::dwExStyle` (line 123) — it never maps to an SDL window flag such as `SDL_WINDOW_ALWAYS_ON_TOP` for `WS_EX_TOPMOST`. The existing fullscreen-path test passes `WS_EX_TOPMOST` but only asserts window creation succeeds, not that any topmost behavior is applied. This has not been previously written up as a deliberate decision anywhere in `docs/`.

Required work:
- Confirm (via the existing game-usage sweep) that neither game's actual runtime behavior depends on real OS-level always-on-top enforcement (both games run one exclusive fullscreen/popup window, so this is expected to be a non-issue).
- Add a short doc note (in `docs/out-of-scope.md` or a comment block in `src/winuser_window.cpp`) stating `dwExStyle` is compile/log/`CREATESTRUCTA`-shape-only and does not drive SDL window flags, and why that's sufficient for both games.

Acceptance criteria:
- A documented, evidenced statement exists that `dwExStyle` bits (beyond storage/logging) are intentionally not translated to SDL behavior.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not implement real `WS_EX_TOPMOST`/other extended-style SDL behavior without a new evidenced call site that requires it.
- Do not change `CreateWindowExA`'s flag-computation logic.

---

### TASK-24H-0306: Replace ShowWindow's magic-number SW_* cases with named constants or document why they're intentionally unreachable
Status: DONE — chose the documentation approach (lower risk than adding new SW_* macros that could imply broader support than exists). Added a code comment at ShowWindow's switch (src/winuser_window.cpp) confirming nCmdShow is always SW_SHOW from FreeApiRunWinMain and both games forward it unchanged, so cases 2/6/3/9 are structurally unreachable. Verified 26/26 in all three build trees.
Priority: P3
Area: WinUser
Type: Cleanup
Evidence: src/winuser_window.cpp:201-223; include/winuser.h:71-72 (only SW_HIDE/SW_SHOW defined); src/winmain_bridge.cpp:84 (nCmdShow hardcoded to SW_SHOW)
Depends on: None

Problem:
`ShowWindow`'s `switch (nCmdShow)` (`src/winuser_window.cpp:201-223`) handles `case 2: // SW_SHOWMINIMIZED`, `case 6: // SW_MINIMIZE`, `case 3: // SW_SHOWMAXIMIZED`, `case 9: // SW_RESTORE` via raw numeric literals, but `include/winuser.h` only `#define`s `SW_HIDE` (0) and `SW_SHOW` (5) — there are no named constants backing these four cases. Separately, `FreeApiRunWinMain` (`src/winmain_bridge.cpp:84`) always calls the game's entry point with `nCmdShow = SW_SHOW`, so these four cases are currently structurally unreachable from either game's real startup path.

Required work:
- Either add named `SW_SHOWMINIMIZED`/`SW_MINIMIZE`/`SW_SHOWMAXIMIZED`/`SW_RESTORE` macros to `include/winuser.h` and use them in the `switch`, or add a code comment at `src/winuser_window.cpp:201` explicitly noting these branches are dead given `nCmdShow` is hardcoded to `SW_SHOW` by the bridge, kept only for source-completeness.
- Pick one approach; do not do a larger `SW_*` constant sweep beyond this function's existing cases.

Acceptance criteria:
- The magic-number-vs-declared-constant mismatch is resolved or explicitly explained in a comment.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not add `SW_*` constants beyond the four numeric values already referenced in `ShowWindow`'s switch.
- Do not change `FreeApiRunWinMain`'s hardcoded `SW_SHOW` behavior.

---

### TASK-24H-0307: Document why GetSystemMetrics' fixed return values are correct (confirmed non-issue)
Status: DONE — extended include/winuser.h's GetSystemMetrics doc comment and docs/supported-apis.md's row with the fixed-value rationale. Implemented together with duplicate TASK-24H-1212. Verified 26/26 in all three build trees.
Priority: P3
Area: WinUser
Type: Documentation
Evidence: src/winuser_misc.cpp:57-72; docs/audit-24h-free-api.md §5 item 4; tests/test_winuser_regressions.cpp:161-164
Depends on: None

Problem:
`GetSystemMetrics` (`src/winuser_misc.cpp:57-72`) returns fixed values for `SM_CXSCREEN` (1024), `SM_CYSCREEN` (768), `SM_CYCAPTION` (24), and 0 for any other index. Both games are re-confirmed to query only these three indices, exactly once, at startup. This is a confirmed non-issue, not a bug, but `docs/supported-apis.md`'s row for this symbol doesn't explain *why* fixed values are sufficient (it just says "IMPLEMENTED").

Required work:
- Add a short explanatory note to `docs/supported-apis.md` (or `docs/out-of-scope.md`) next to `GetSystemMetrics` stating that both games only ever query these three fixed indices at startup, values are hardcoded on purpose, and this must not be made dynamic without a new evidenced call site.

Acceptance criteria:
- `docs/` explicitly documents the fixed-value rationale with a citation to the usage evidence.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not make `GetSystemMetrics` dynamic/screen-size-aware — no evidenced call site needs this; this is an explicit non-goal for this task.
- Do not add support for additional `SM_*` indices without new evidence.

---

### TASK-24H-0308: Add regression test coverage for GetSystemMetrics(SM_CYCAPTION) and the unqueried-index fallback
Status: DONE — added TestGetSystemMetricsCyCaptionAndUnqueriedIndexFallback (tests/test_winuser_regressions.cpp): asserts GetSystemMetrics(SM_CYCAPTION) == 24 and GetSystemMetrics(12345) == 0 (fallback branch, 12345 chosen as a value distinct from all three implemented constants). Verified 24/24 standalone.
Priority: P2
Area: WinUser
Type: Test
Evidence: src/winuser_misc.cpp:57-72; tests/test_winuser_regressions.cpp:153-181 (only SM_CXSCREEN/SM_CYSCREEN currently asserted)
Depends on: None

Problem:
`TestCreateWindowExAFullscreenPath` (`tests/test_winuser_regressions.cpp:153-181`) is the only test that calls `GetSystemMetrics`, and it only asserts `SM_CXSCREEN`/`SM_CYSCREEN` return "sane positive" values. `SM_CYCAPTION` — the third index both games genuinely query at startup — has zero test assertions anywhere in the suite, and the "any other index returns 0" fallback branch (`src/winuser_misc.cpp:71`) is also untested.

Required work:
- Add an assertion that `GetSystemMetrics(SM_CYCAPTION) == 24`.
- Add an assertion that `GetSystemMetrics` for an index other than the three known ones (e.g. an arbitrary unused `SM_*` value) returns 0, locking in the documented fallback contract.

Acceptance criteria:
- New assertions for `SM_CYCAPTION` and the default-fallback case exist and pass.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not add test coverage for any `SM_*` index beyond the three already implemented plus one arbitrary unimplemented index used to prove the fallback.
- Do not change `GetSystemMetrics`'s implementation.

---

### TASK-24H-0309: Document MoveWindow's confirmed dead-reach status; no logical-state-update implementation
Status: DONE — added a code comment at MoveWindow's definition (src/winuser_window.cpp) and a new docs/out-of-scope.md section stating it never updates g_freeApiWindowStates's logical width/height, and that this is safe only because the sole call sites (movie.cpp in both games) are dead-reach behind the always-failing AVI probe. No behavior change.
Priority: P3
Area: WinUser
Type: Documentation
Evidence: src/winuser_window.cpp:233-244; docs/audit-24h-free-api.md §5 item 5, §2.7; ../planetblupi and ../free-eggbert movie.cpp call sites (dead since AVI always fails to open)
Depends on: None

Problem:
`MoveWindow` (`src/winuser_window.cpp:233-244`) repositions/resizes the real SDL window but never updates `g_freeApiWindowStates`, so `GetClientRect`/`ClientToScreen`/`ScreenToClient`'s logical-size tracking would go stale after a real resize. The audit confirms this doesn't matter: the only live call sites in either game are inside `movie.cpp`, which is itself dead-reach since the AVI probe always fails (§2.7). This is a confirmed non-issue but is not documented anywhere as a deliberate "known but harmless" gap.

Required work:
- Add a code comment at `src/winuser_window.cpp:233` and a `docs/out-of-scope.md` entry stating `MoveWindow` does not update `g_freeApiWindowStates`'s logical width/height, that this is known, and that it's safe only because the sole call sites are dead-reach (`movie.cpp` in both games, gated by the always-failing AVI probe).

Acceptance criteria:
- The gap and its dead-reach justification are documented in both a code comment and `docs/`.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not implement `g_freeApiWindowStates` updates inside `MoveWindow` — no live call site needs it.
- Do not change `MoveWindow`'s current SDL-position/size behavior.

---

### TASK-24H-0310: Add explicit AdjustWindowRect non-goal statement to docs/out-of-scope.md
Status: DONE — added a dedicated section stating the identity transform is deliberate and permanent, and forbidding real window-chrome math absent new evidence. Verified 26/26 in all three build trees.
Priority: P3
Area: WinUser
Type: Documentation
Evidence: src/winuser_misc.cpp:74-84; include/winuser.h:387-396; docs/out-of-scope.md:113-118 (only a brief historical-note mention)
Depends on: None

Problem:
`AdjustWindowRect` (`src/winuser_misc.cpp:74-84`) is a deliberate identity transform, correct because `CreateWindowExA`/`GetClientRect` define "window size" as equal to client size in this implementation. `docs/out-of-scope.md` currently only mentions this in a one-sentence historical note (lines 113-118) alongside `ShowCursor`/`SetCursor`/`LoadStringA`; it has no dedicated entry stating explicitly that real Win32 window-chrome math (title bar/border thickness) must not be added.

Required work:
- Add a dedicated `AdjustWindowRect` row/paragraph to `docs/out-of-scope.md`'s unsupported/decided-behavior section, explicitly stating the identity transform is deliberate and permanent, and that implementing real non-client-area size math is out of scope absent new evidence that window size and client size must differ for some game.

Acceptance criteria:
- `docs/out-of-scope.md` contains a standalone, explicit statement forbidding real window-chrome math for `AdjustWindowRect`.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not implement real Win32 window-chrome math (title bar/border size deltas) for `AdjustWindowRect` under any circumstance in this task.
- Do not change `AdjustWindowRect`'s return value or `lpRect` handling.

---

### TASK-24H-0311: Add ShowCursor counter overflow/underflow regression test
Status: DONE — added TestShowCursorCounterAccumulatesWithoutClamping (test_winuser_regressions.cpp): 5x ShowCursor(TRUE) in a row, 7x ShowCursor(FALSE) in a row, and a single partial-recovery call, all asserted relative to the counter's baseline value (process-global, shared with the adjacent test). Verified passing (22/22 suite).
Priority: P2
Area: WinUser
Type: Test
Evidence: src/winuser_cursor.cpp:72-88; tests/test_winuser_regressions.cpp:717-729
Depends on: None

Problem:
`ShowCursor`'s signed-counter contract (`src/winuser_cursor.cpp:72-88`) is confirmed to exactly match real Win32 semantics, but the only existing test (`TestShowCursorHidesAndShowsRealCursor`, `tests/test_winuser_regressions.cpp:717-729`) exercises exactly one `FALSE` then one `TRUE` call. The edge case of many more `ShowCursor(TRUE)` calls than `ShowCursor(FALSE)` calls (or vice versa) — i.e., the counter going well above 0 or well below -1 — has no test coverage at all.

Required work:
- Add a test that calls `ShowCursor(TRUE)` several times in a row and asserts the counter keeps incrementing (stays `>= 0`, cursor stays visible via `SDL_CursorVisible()`).
- Add a test that calls `ShowCursor(FALSE)` several times in a row and asserts the counter keeps decrementing (stays `< 0`, cursor stays hidden), then verify a single `ShowCursor(TRUE)` only partially recovers the counter (still `< 0` if it started deeply negative).

Acceptance criteria:
- New test assertions cover multi-call counter accumulation in both directions.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not change `ShowCursor`'s counter arithmetic or clamp it — real Win32 `ShowCursor` has no clamp either; this task only adds test coverage.
- Do not add a new cursor-visibility query API.

---

### TASK-24H-0312: Document why UpdateWindow's raise-window mapping is safe given neither game uses WM_PAINT
Status: DONE — confirmed via grep zero WM_PAINT/BeginPaint/EndPaint/PAINTSTRUCT references in either game's source. Added a code comment at UpdateWindow (src/winuser_window.cpp) documenting the rationale. Verified 26/26 in all three build trees.
Priority: P3
Area: WinUser
Type: Documentation
Evidence: src/winuser_window.cpp:254-264; docs/audit-24h-free-api.md §2.3 (`PAINTSTRUCT` confirmed unused by both games)
Depends on: None

Problem:
`UpdateWindow` (`src/winuser_window.cpp:254-264`) is implemented as `SDL_RaiseWindow`, not real Win32's "force an immediate `WM_PAINT` if the update region is non-empty" semantics. This is already marked `PARTIAL` in the audit's usage table but the reason it's safe — neither game ever uses `WM_PAINT`/`BeginPaint`/`EndPaint`/`PAINTSTRUCT` at all (§2.3) — is not written down anywhere outside the audit document itself.

Required work:
- Add a code comment at `src/winuser_window.cpp:254` (or a `docs/out-of-scope.md` entry) explaining `UpdateWindow`'s raise-window substitution is safe specifically because `WM_PAINT` is unused by both games.

Acceptance criteria:
- The rationale is documented in a discoverable, permanent location (code comment and/or `docs/`).
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not implement real `WM_PAINT` dispatch or a `PAINTSTRUCT`-backed paint cycle — no evidenced call site in either game needs it.
- Do not change `UpdateWindow`'s current `SDL_RaiseWindow` behavior.

---

### TASK-24H-0313: Add missing docs/supported-apis.md rows for DestroyWindow, MoveWindow, and SetWindowTextA
Status: DONE — added all three rows: DestroyWindow (synchronous WM_DESTROY dispatch, tested), MoveWindow (PARTIAL, dead-reach caveat per TASK-24H-0309, no test since dead-reach), SetWindowTextA (IMPLEMENTED, now tested via TASK-24H-1226).
Priority: P2
Area: WinUser
Type: Documentation
Evidence: docs/supported-apis.md (no rows currently exist for these three symbols); src/winuser_window.cpp:161-191 (DestroyWindow), 233-244 (MoveWindow), 266-275 (SetWindowTextA)
Depends on: None

Problem:
`docs/supported-apis.md`'s table has rows for `RegisterClassA`, `CreateWindowExA`/`CreateWindowA`, `AdjustWindowRect`, `ShowWindow`/`UpdateWindow`/`SetFocus`, and `GetClientRect`, but has zero rows for `DestroyWindow`, `MoveWindow`, or `SetWindowTextA` despite all three being live, implemented, and (for `DestroyWindow`) tested symbols used by both games. This is part of the already-known `TASK-0001` "28 missing rows" gap, but these three are directly evidenced by this session's window-subsystem pass and worth closing independently.

Required work:
- Add a `DestroyWindow` row noting the confirmed-synchronous `WM_DESTROY` dispatch invariant and its test coverage (`test_winuser_regressions.cpp`'s `TestDestroyWindowDispatchesWmDestroySynchronously`).
- Add a `MoveWindow` row noting PARTIAL status and the dead-reach caveat from TASK-24H-0309.
- Add a `SetWindowTextA` row noting IMPLEMENTED status and its `WM_ACTIVATEAPP` title-change usage.

Acceptance criteria:
- All three symbols have accurate, evidenced rows in `docs/supported-apis.md`.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not attempt to close the full `TASK-0001` 28-row gap in this task — scope is limited to these three window-theme symbols.
- Do not change any implementation behavior.

---

### TASK-24H-0314: Verify and document GetClientRect's SDL-size fallback branch is defensive-only dead code
Status: DONE — confirmed CreateWindowExA has a single, unconditional g_freeApiWindowStates insertion point for every window it creates. Added a code comment at the fallback branch (src/winuser_window.cpp) documenting it as defensive-only. Verified 26/26 in all three build trees.
Priority: P3
Area: WinUser
Type: Verification
Evidence: src/winuser_window.cpp:277-304 (fallback at lines 293-297); src/winuser_window.cpp:105-109 (g_freeApiWindowStates always populated by CreateWindowExA)
Depends on: None

Problem:
`GetClientRect` (`src/winuser_window.cpp:277-304`) first looks up `g_freeApiWindowStates` for the logical size and, only if that lookup misses, falls back to `SDL_GetWindowSize`. Since `CreateWindowExA` always inserts a `g_freeApiWindowStates` entry for every window it creates (lines 105-109), the fallback branch is only reachable for an `hWnd` that was never created via `CreateWindowExA`/`CreateWindowA` — effectively unreachable from either game's real code path, and not currently proven to be exercised by any test.

Required work:
- Confirm via a targeted grep/read that no code path in either game or in free-api itself can produce an `hWnd` with no `g_freeApiWindowStates` entry.
- Add a short comment at `src/winuser_window.cpp:293` documenting the fallback as defensive-only, kept for robustness against a hypothetically foreign `HWND` rather than a real, evidenced call path.

Acceptance criteria:
- The fallback branch's reachability status is confirmed and documented in a code comment.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not remove the fallback branch — it costs nothing and guards against a foreign/invalid `HWND` even if unreached today.
- Do not add a synthetic test that manufactures a fake `HWND` just to exercise this branch; that would test an implementation detail, not evidenced game behavior.

---

### TASK-24H-0315: Assert SetCursor's very first call returns a NULL previous handle
Status: DONE — replaced the (void) discard with a real Check(previousBeforeAny == nullptr, ...) assertion (tests/test_winuser_regressions.cpp). Confirmed this is the only SetCursor call site in the whole test binary (genuinely the first-ever call in the process). Verified 26/26 in all three build trees.
Priority: P3
Area: WinUser
Type: Test
Evidence: src/winuser_cursor.cpp:63,65-70 (g_currentCursor initialized to nullptr); tests/test_winuser_regressions.cpp:731-742 (previousBeforeAny discarded via `(void)previousBeforeAny;`)
Depends on: None

Problem:
`SetCursor`'s previous-handle-return contract (`src/winuser_cursor.cpp:65-70`) is confirmed to match real Win32 semantics, including that `g_currentCursor` starts as `nullptr` (line 63). `TestSetCursorReturnsPreviousHandle` (`tests/test_winuser_regressions.cpp:731-742`) captures this exact first-call return value into `previousBeforeAny` but immediately discards it with `(void)previousBeforeAny;` instead of asserting it equals `NULL` — the one part of the contract this test was positioned to prove is the one part it doesn't check.

Required work:
- Replace the `(void)previousBeforeAny;` discard with a `Check(previousBeforeAny == nullptr, ...)` assertion, guarding against any earlier test in the suite having already called `SetCursor` (either by ordering this test first among cursor tests, or by resetting/documenting the initial-state assumption).

Acceptance criteria:
- The test explicitly asserts `SetCursor`'s first captured return value is `NULL`/`nullptr`.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not add a `GetCursor`-style query API — none exists in real Win32 either, and none is evidenced as needed.
- Do not change `SetCursor`'s implementation.

---

### TASK-24H-0316: Document and regression-lock the non-resizable-window compositor WM_CLOSE workaround
Status: DONE — added TestCreateWindowExANeverSetsResizableFlag (tests/test_winuser_regressions.cpp): asserts (SDL_GetWindowFlags(window) & SDL_WINDOW_RESIZABLE) == 0. Added docs/out-of-scope.md entry ("No SDL_WINDOW_RESIZABLE on any CreateWindowExA window") citing the source comment and the compositor WM_CLOSE quirk. Verified 24/24 standalone.
Priority: P2
Area: WinUser
Type: Test
Evidence: src/winuser_window.cpp:69-80 (deliberately never sets SDL_WINDOW_RESIZABLE); docs/audit-24h-free-api.md §4.5
Depends on: None

Problem:
`CreateWindowExA` deliberately never sets `SDL_WINDOW_RESIZABLE` (`src/winuser_window.cpp:69-71`), with a code comment explaining that on some Wayland/X11 compositors a resizable popup window immediately receives a spurious `WM_CLOSE`. This is a real, load-bearing decision — re-adding resizability would silently reintroduce a startup-killing bug on affected compositors — but it exists only as an inline code comment: it has zero test coverage (nothing asserts the created window's SDL flags exclude `SDL_WINDOW_RESIZABLE`) and is not surfaced in `docs/out-of-scope.md`'s "must not generalize" list the way other window-subsystem decisions are.

Required work:
- Add a regression test that creates a window via `CreateWindowExA` and asserts `(SDL_GetWindowFlags(window) & SDL_WINDOW_RESIZABLE) == 0`.
- Add a `docs/out-of-scope.md` entry cross-referencing the compositor `WM_CLOSE` quirk and stating resizable-window support must not be added without first re-verifying this workaround is still needed.

Acceptance criteria:
- A test fails if `SDL_WINDOW_RESIZABLE` is ever added back to `CreateWindowExA`'s flags.
- The compositor workaround is documented in `docs/out-of-scope.md` with a citation to the source comment.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not add real resizable-window support or any per-game opt-in for it — no evidenced need, and doing so risks reintroducing the documented compositor bug.
- Do not investigate or attempt to fix the underlying compositor `WM_CLOSE` quirk itself — out of free-api's scope (it's a compositor behavior, not a free-api bug).

## Input

### TASK-24H-0401: Human-playtest MK_SHIFT drag-select multi-unit highlight
Status: TODO
Priority: P1
Area: WinUser
Type: Verification
Evidence: src/internal/FreeApiMessageQueue.cpp:344-352, tests/test_winuser_regressions.cpp:27-37, ../planetblupi/src/event.cpp:3440,3472,3504, docs/audit-24h-free-api.md §4.10/§5 item 4
Depends on: None

Problem:
MK_SHIFT is computed live on every WM_MOUSEMOVE and matches planetblupi's CEvent::PlayMove requirement for shift-drag multi-unit highlight, but the underlying gameplay feature has zero automated coverage (SDL's headless keyboard-state query can't be scripted) and zero human playtest. This session found the exact keyboard path from cold boot to live WM_PHASE_PLAY gameplay: Enter, Enter, Enter, Enter (WM_PHASE_INTRO1→INTRO2→INIT→INFO→PLAY).

Required work:
- Launch planetblupi (target-game build); from the cold-boot title/intro screens press Enter four times to reach WM_PHASE_PLAY (dismiss the 30s attract-mode auto-demo with any keypress if it triggers first).
- Once in live gameplay, hold Shift and drag the mouse over multiple friendly units on the map; observe whether all units under the drag rectangle become highlighted/selected together (additive multi-unit highlight), matching BlupiHiliDown/BlupiHiliMove/BlupiHiliUp's fwKeys&MK_SHIFT branch.
- Record the exact steps and the observed pass/fail result in NEXT.md.

Acceptance criteria:
- A human (non-headless, real-backend) playtest is performed and its outcome is recorded in NEXT.md.
- If the behavior fails to match the expected additive multi-unit highlight, a new follow-up bug task is filed citing the exact observed vs. expected behavior.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not change the WM_MOUSEMOVE lParam packing or the MK_SHIFT/MK_CONTROL computation mechanism (both already confirmed correct by source tracing) unless the playtest itself reveals a genuine behavioral defect.
- Do not attempt to playtest MK_CONTROL's level-editor flood-fill in this same task — that is a separate feature covered by its own task.
- Do not add DirectInput or raw-input APIs.

---

### TASK-24H-0402: Locate the in-game UI path to planetblupi's level-editor decor flood-fill mode
Status: DONE — traced ChangePhase/CreateButtons/VK_RETURN handling in ../planetblupi/src/event.cpp and documented the exact sequence in docs/target-games.md's new "Manual playtest key/menu sequences" section: Enter x2 (to WM_PHASE_INIT) -> click "Privé" (WM_PHASE_PRIVATE, sets m_bPrivate=TRUE) -> click "Build" (WM_PHASE_BUILD, hidden unless m_bPrivate; event.cpp:3038-3043). Also found WM_DECOR1 (a decor tool) is unconditionally auto-selected the instant WM_PHASE_BUILD is entered (event.cpp:2977-2985's SetState(WM_DECOR1,1)), so no further click is needed before a human can test MK_CONTROL. Traced from source only (research-only task, no source/test files modified), not run against a live build — a human tester should confirm before relying on it for TASK-24H-0403.
Priority: P2
Area: WinUser
Type: Audit
Evidence: ../planetblupi/src/event.cpp:3843,3877,3908 (MK_CONTROL flood-fill), :4888-4897,:4939-4947 (VK_CONTROL press/release gating m_bCtrlDown/m_bFillMouse), docs/audit-24h-free-api.md §2.4/§5 item 4
Depends on: None

Problem:
MK_CONTROL drives a level-editor decor flood-fill feature (ArrangeFill calls gated on fwKeys&MK_CONTROL while placing floor/object/building decor), distinct from MK_SHIFT's drag-select highlight. Unlike the MK_SHIFT gameplay path, this session did not trace the exact in-game UI sequence needed to reach the WM_PHASE_BUILD decor-placement state from cold boot, so a human playtest of this feature is not yet concretely actionable.

Required work:
- Trace ChangePhase and the WM_PHASE_BUTTON/WM_PHASE_BUILD transition logic (e.g. the VK_RETURN case at event.cpp:4820-4826 and related menu/button handlers) to determine the exact key/menu sequence that reaches a state where GetState(WM_DECOR1/2/3)==1 (a decor tool is actively selected, ready for Ctrl+click/drag).
- Write the confirmed sequence down precisely enough that a future session can execute it without re-deriving it, matching the detail this session already achieved for the Enter×4 MK_SHIFT path.

Acceptance criteria:
- A concrete, reproducible key/menu sequence from cold boot to a decor-tool-active state is documented in NEXT.md or docs/target-games.md.
- No source or test files are modified by this task (research only).
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not perform the actual playtest in this task — that is TASK-24H-0403, which depends on this one.
- Do not change any MK_CONTROL/flood-fill source code.

---

### TASK-24H-0403: Human-playtest MK_CONTROL level-editor flood-fill
Status: TODO
Priority: P2
Area: WinUser
Type: Verification
Evidence: ../planetblupi/src/event.cpp:3843,3877,3908, docs/audit-24h-free-api.md §4.10/§5 item 4
Depends on: TASK-24H-0402

Problem:
Same underlying zero-coverage risk as MK_SHIFT (§5 item 4), but for the separate level-editor decor flood-fill feature. This is a distinct playtest from TASK-24H-0401's drag-select highlight and must not be conflated with it or considered satisfied by it.

Required work:
- Using the key/menu sequence documented by TASK-24H-0402, reach a decor-tool-active state in planetblupi.
- Hold Ctrl and click (or drag) over the decor-placement area; observe whether the flood-fill (ArrangeFill) behavior fills contiguous matching cells, matching the fwKeys&MK_CONTROL branch's expected behavior.
- Record the exact steps and observed pass/fail result in NEXT.md, explicitly noting this is the flood-fill feature, not the drag-select highlight.

Acceptance criteria:
- A human playtest of MK_CONTROL's flood-fill is performed and its outcome recorded in NEXT.md as a distinct entry from TASK-24H-0401's result.
- If behavior fails to match the expected flood-fill, a new follow-up bug task is filed citing the exact observed vs. expected behavior.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not treat this as covered by or a duplicate of TASK-24H-0401's MK_SHIFT playtest.
- Do not change the MK_SHIFT/MK_CONTROL computation mechanism unless the playtest reveals a genuine defect.

---

### TASK-24H-0404: Extract a shared MK_SHIFT/MK_CONTROL modifier-flag helper
Status: DONE — FreeApi::Internal::ApplyKeyboardModifierFlags(keys, base) extracted (src/internal/FreeApiMessageQueue.{hpp,cpp}); both mouse-motion and mouse-button handlers now call it instead of duplicating the OR-logic inline.
Priority: P1
Area: WinUser
Type: Refactor
Evidence: src/internal/FreeApiMessageQueue.cpp:349-352 (mouse-motion handler), :398-401 (mouse-button handler)
Depends on: None

Problem:
The logic that reads SDL_GetKeyboardState and ORs MK_SHIFT/MK_CONTROL into a wParam is duplicated verbatim in two places (the SDL_EVENT_MOUSE_MOTION handler and the SDL_EVENT_MOUSE_BUTTON_DOWN/UP handler). This duplication also blocks the next task's testability improvement, since there's no single seam to unit-test.

Required work:
- Extract a small internal helper (e.g. `WPARAM ApplyKeyboardModifierFlags(const bool* keys, WPARAM base)`) that takes an SDL keyboard-state array and a base wParam (button-state bits) and returns the base OR'd with MK_SHIFT/MK_CONTROL exactly as the current inline code does.
- Replace both call sites with calls to the new helper, passing `SDL_GetKeyboardState(nullptr)` as before.

Acceptance criteria:
- Both call sites use the shared helper; no duplicated OR-logic remains.
- Behavior is bit-for-bit identical to before (same wParam values for the same inputs).
- Existing tests still pass, including tests/test_winuser_regressions.cpp's MK_LBUTTON/MK_RBUTTON drag cases.
- No unrelated API is added.

Out of scope:
- Do not change when or how often the modifier state is sampled (still live, per-event, not latched).
- Do not add MK_MBUTTON or any other new modifier flag.

---

### TASK-24H-0405: Add a synthetic-keystate unit test for the modifier-flag helper
Status: DONE — TestApplyKeyboardModifierFlagsHelperWithSyntheticKeystate (tests/test_winuser_regressions.cpp) covers no-modifiers/L+R Shift/L+R Ctrl/both-together/null-keys cases via a hand-built keystate array.
Priority: P1
Area: WinUser
Type: Test
Evidence: docs/audit-24h-free-api.md §4.10/§5 item 4/§5 item 17, tests/test_winuser_regressions.cpp:27-37
Depends on: TASK-24H-0404

Problem:
MK_SHIFT/MK_CONTROL's live-computation logic cannot be exercised end-to-end in the headless test environment because SDL_PushEvent-injected key events do not update SDL_GetKeyboardState()'s real array (documented at tests/test_winuser_regressions.cpp:27-37). Once the OR-logic is extracted into a standalone helper (TASK-24H-0404), it can be unit-tested directly with a synthetic keystate array, closing the automated-coverage gap for the underlying mechanism (the end-to-end gameplay feature still needs the human playtests in TASK-24H-0401/0403).

Required work:
- Add a test that constructs a local synthetic keystate array, sets/clears the LSHIFT/RSHIFT/LCTRL/RCTRL indices in various combinations (none held, shift only, ctrl only, both held), and calls the extracted helper directly.
- Assert the returned wParam has exactly the expected MK_SHIFT/MK_CONTROL bits set (and any pre-existing button-state bits preserved) for each combination.

Acceptance criteria:
- The new test passes against current behavior and fails if the OR-logic is broken (verify by temporarily breaking the helper locally, then restoring).
- docs/supported-apis.md's MK_* row is updated to note the helper's OR-logic now has direct unit coverage, while the end-to-end gameplay feature still relies on human playtest.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not attempt to make SDL_PushEvent update SDL_GetKeyboardState() in the test harness — treat this as a known, permanent headless-environment limitation.
- This test does not replace or substitute for the human playtests in TASK-24H-0401/0403.

---

### TASK-24H-0406: Add a regression test asserting unmapped scancodes produce no message
Status: DONE — Test 11 (tests/test_input_pipeline.cpp) injects SDL_SCANCODE_CAPSLOCK down/up and asserts no WM_KEYDOWN/WM_SYSKEYDOWN/WM_KEYUP/WM_SYSKEYUP is produced.
Priority: P1
Area: WinUser
Type: Test
Evidence: src/internal/FreeApiMessageQueue.cpp:120-201 (SdlScancodeToVK default case, line 199), :298-299 (`if (vk == 0) break;`), docs/audit-24h-free-api.md §4.9
Depends on: None

Problem:
SdlScancodeToVK silently returns 0 for any scancode not in its explicit table, and the caller drops the event entirely rather than forwarding a garbage VK code — confirmed correct, safe-default behavior by this session's audit, but no existing test asserts this as a positive claim (only the mapped-key cases are tested).

Required work:
- In tests/test_input_pipeline.cpp, inject an SDL key-down and key-up event using a scancode confirmed absent from SdlScancodeToVK's switch (e.g. SDL_SCANCODE_CAPSLOCK — verify against the current switch body before picking one).
- Assert that no WM_KEYDOWN/WM_KEYUP/WM_SYSKEYDOWN/WM_SYSKEYUP message is received for that injected event.

Acceptance criteria:
- The new test passes against current behavior and would fail if an unmapped scancode were ever accidentally forwarded with a garbage VK code.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not add new scancode→VK_* mappings as part of this task.
- Do not change the drop behavior — it is confirmed correct and must remain a silent, safe no-op.

---

### TASK-24H-0407: Add a regression test for planetblupi's 'A'-'Z' cheat-code VK mapping
Status: DONE — Test 10 (tests/test_input_pipeline.cpp) covers a representative sample (A, B, M, Y, Z) of the letter-scancode-to-VK mapping.
Priority: P1
Area: WinUser
Type: Test
Evidence: src/internal/FreeApiMessageQueue.cpp:124-149 (letter scancode→VK_* mapping), ../planetblupi/src/event.cpp:4633 (`wParam >= 'A' && wParam <= 'Z'` cheat-code check), :4547-4580 (DemoRecEvent persists WM_KEYDOWN/UP wParam to demo files), tests/test_input_pipeline.cpp (Test 7 currently covers only F-keys/navigation/RETURN/SHIFT/CONTROL/PAUSE, no letters)
Depends on: None

Problem:
planetblupi's cheat-code system reads `wParam >= 'A' && wParam <= 'Z'` directly from WM_KEYDOWN, and these same WM_KEYDOWN events are persisted verbatim to demo files by DemoRecEvent — so an incorrect letter-key VK mapping would be both a live-gameplay bug and a demo-file-compatibility bug. Despite this, Test 7's VK_* coverage table tests F-keys, navigation, and a handful of control keys, but zero letter scancodes.

Required work:
- Extend Test 7's VkCase table (or add a new loop) in tests/test_input_pipeline.cpp to cover a representative sample of SDL_SCANCODE_A..Z mapping to VK codes 'A'..'Z' (either the full 26-letter range or a representative sample spanning it, e.g. A, M, Z, plus one or two more).
- Reuse the existing key-down/key-up injection and assertion pattern already used by Test 7.

Acceptance criteria:
- The new/extended test passes against current behavior and would fail if a letter scancode were mapped to the wrong VK code or dropped.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not add digit-key (0-9) test coverage in this task — digits are not evidenced as used by either game (see TASK-24H-0409).
- Do not change the letter-key mapping itself; it is already confirmed correct.

---

### TASK-24H-0408: Add a regression test locking in WM_CHAR's ASCII-only forwarding behavior
Status: DONE — added InjectTextInput helper + Test 12 (tests/test_input_pipeline.cpp): injects SDL_EVENT_TEXT_INPUT with "ab" + U+00E9 (2-byte UTF-8) + "cd", asserts WM_CHAR fires for exactly 'a','b','c','d' in order and the multi-byte sequence produces nothing and doesn't corrupt surrounding ASCII. Verified 24/24 standalone.
Priority: P2
Area: WinUser
Type: Test
Evidence: src/internal/FreeApiMessageQueue.cpp:316-329, docs/out-of-scope.md:150 ("WM_CHAR/text-input translation... unproven — do not expand"), grep confirms zero `case WM_CHAR:` in either ../free-eggbert or ../planetblupi source
Depends on: None

Problem:
WM_CHAR forwards only ASCII bytes (<128) from SDL_EVENT_TEXT_INPUT and silently drops non-ASCII UTF-8 bytes; docs/out-of-scope.md already documents this as implemented-but-unproven (neither game has a `case WM_CHAR:` handler at all — reconfirmed this session). No test anywhere in tests/ exercises SDL_EVENT_TEXT_INPUT or asserts WM_CHAR's behavior, so this correct behavior has zero regression protection.

Required work:
- Add a test that injects an SDL_EVENT_TEXT_INPUT event with a mixed string containing ASCII characters and a multi-byte UTF-8 sequence (e.g. "ab" + a 2-byte UTF-8 character + "cd").
- Assert that exactly the ASCII characters ('a','b','c','d') each produce a WM_CHAR message with the correct wParam, in order, and that no WM_CHAR is produced for the non-ASCII bytes, and that they don't corrupt or skip subsequent ASCII characters.

Acceptance criteria:
- The new test passes against current behavior and would fail if non-ASCII bytes were ever forwarded or corrupted subsequent ASCII byte processing.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not change WM_CHAR to forward non-ASCII bytes or add any UTF-16/W-API conversion.
- Do not add a `case WM_CHAR:` handler to either game — out of this repo's control and not evidenced as needed.

---

### TASK-24H-0409: Document the unevidenced-but-harmless entries in the scancode→VK_* table
Status: DONE — re-verified the exact unevidenced-entry list against src/internal/FreeApiMessageQueue.cpp directly (matches: digits 0-9, INSERT/DELETE/PAGEUP/PAGEDOWN/TAB/BACKSPACE, VK_MENU, KP_ENTER alias). Added a note to docs/supported-apis.md's VK_* row listing them as intentionally kept, referencing the existing HFONT/HPALETTE and _chdir/_getcwd "keep + document" pattern. Verified 25/25 in all three build trees.
Priority: P2
Area: WinUser
Type: Documentation
Evidence: src/internal/FreeApiMessageQueue.cpp:120-201, docs/audit-24h-free-api.md §2.4/§4.9
Depends on: None

Problem:
This session's audit confirmed the hand-maintained SDL-scancode→VK_* table covers every VK_* both games actually use, but the table also contains entries with no evidenced call site in either game's source: digit keys 0-9, INSERT, DELETE, PAGEUP, PAGEDOWN, TAB, BACKSPACE, VK_MENU (Alt), and the KP_ENTER→VK_RETURN alias. These are harmless but the "confirmed complete, no gaps" framing doesn't distinguish evidenced-required entries from unevidenced-but-kept extras, unlike the project's existing pattern for other vestigial-but-harmless surface (e.g. HFONT/HPALETTE, _chdir/_getcwd).

Required work:
- In docs/supported-apis.md (or docs/out-of-scope.md, whichever hosts the VK_*/scancode row), add a note listing which scancode→VK_* table entries are evidenced-required (per docs/audit-24h-free-api.md §2.4) versus kept-but-unevidenced extras, using the same "keep + document" decision already applied elsewhere in this codebase.

Acceptance criteria:
- The relevant doc explicitly lists the unevidenced entries and states they are kept intentionally (not scope creep), with no behavior change.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not remove any of the unevidenced table entries.
- Do not add new scancode mappings.

---

### TASK-24H-0410: Update the MK_SHIFT/MK_CONTROL documentation row with the drag-select/flood-fill distinction
Status: DONE — re-verified both call-site sets directly against event.cpp (MK_SHIFT -> BlupiHiliDown/Move/Up drag-select; MK_CONTROL -> ArrangeFill flood-fill, confirmed real flood-fill call, not just a generic modifier check). Updated docs/supported-apis.md's MK_* row to distinguish the two features with file:line evidence for each, referencing TASK-24H-0401/0403. Verified 25/25 in all three build trees.
Priority: P2
Area: WinUser
Type: Documentation
Evidence: docs/supported-apis.md:39, ../planetblupi/src/event.cpp:3440,3472,3504 (MK_SHIFT drag-select) vs :3843,3877,3908 (MK_CONTROL flood-fill), docs/audit-24h-free-api.md §2.4/§5 item 4
Depends on: None

Problem:
docs/supported-apis.md's MK_* row currently states "MK_SHIFT/MK_CONTROL implemented but not automatable in this headless environment" without distinguishing that they drive two entirely different planetblupi features — this session corrected a prior assumption that conflated drag-select highlight and level-editor flood-fill into a single feature.

Required work:
- Update docs/supported-apis.md's MK_* row (and/or docs/out-of-scope.md) to state explicitly that MK_SHIFT drives drag-select multi-unit highlight (event.cpp:3440,3472,3504) and MK_CONTROL drives a separate level-editor decor flood-fill (event.cpp:3843,3877,3908), and reference TASK-24H-0401/0403 as the tracked human-playtest verification for each.

Acceptance criteria:
- The doc row (or a new adjacent row) clearly distinguishes the two features with file:line evidence for each.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not merge this into a single generic "MK_SHIFT/MK_CONTROL modifier" description that re-conflates the two features.

---

### TASK-24H-0411: Investigate whether WM_NCMOUSEMOVE's absence in free-api's SDL translation is a real gap
Status: DONE — investigated and confirmed non-issue via direct SDL source evidence (not requiring live playtest): SDL's cursor-hiding is scoped strictly to the app's own client window/surface on both X11 (XDefineCursor on the app's own xwindow, never the WM's separate decoration/frame window) and Wayland (wl_pointer_set_cursor only in response to the app's own wl_surface's pointer events, never compositor-drawn chrome). The WM/compositor always shows its own default cursor over its own title bar independent of the app's ShowCursor state, so WM_NCMOUSEMOVE's absence causes no observable defect. Documented in docs/out-of-scope.md ("WM_NCMOUSEMOVE is never generated"). No follow-up bugfix task filed since no real defect was found.
Priority: P2
Area: WinUser
Type: Audit
Evidence: include/winuser.h:196 (WM_NCMOUSEMOVE constant), ../planetblupi/src/event.cpp:4998-5006 (case WM_NCMOUSEMOVE: real-cursor-restore handler), src/winuser_window.cpp:69-79 (WS_CAPTION windowed mode gets real OS window decorations, not SDL_WINDOW_BORDERLESS), grep confirms no SDL event anywhere in src/ is ever translated to WM_NCMOUSEMOVE
Depends on: None

Problem:
planetblupi's WndProc has a real `case WM_NCMOUSEMOVE:` handler that restores the real OS cursor (ShowCursor(TRUE)) when the mouse moves over the window's non-client area, used when the game hides the OS cursor over its own client-area sprite cursor. planetblupi's windowed mode (WS_POPUPWINDOW|WS_CAPTION|WS_VISIBLE) gets a real OS-drawn title bar via SDL, but free-api's SDL event translation never produces a WM_NCMOUSEMOVE message at all — a gap not previously called out anywhere in docs or the main audit body. It's unclear whether this is a real bug (the compositor-drawn title bar might leave the OS cursor invisible while free-api's hide-cursor state is active) or a non-issue (window-manager decoration cursor visibility is independent of the app's SDL cursor state).

Required work:
- Determine, for the target windowed-mode build, whether the OS/compositor-drawn title bar's cursor visibility is actually affected by free-api's SDL_HideCursor/ShowCursor(FALSE) call, or whether it's independently managed by the window manager.
- Document the finding (non-issue or real gap) with evidence in docs/out-of-scope.md or docs/supported-apis.md.

Acceptance criteria:
- A documented conclusion exists (with evidence) on whether WM_NCMOUSEMOVE's absence causes any observable cursor-visibility defect in windowed mode.
- If a real defect is found, a separate, scoped follow-up bugfix task is filed rather than fixed inline in this investigation task.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not implement a general non-client-area mouse-tracking system.
- Do not add WM_NCMOUSEMOVE generation speculatively without first confirming it's needed.

---

### TASK-24H-0412: Fix the misleading "Ignore repeat keydowns" comment in the SDL key-event handler
Status: DONE — rewrote the comment (src/internal/FreeApiMessageQueue.cpp) to accurately describe current behavior: no repeat filtering happens, every keydown forwards as WM_KEYDOWN with a hardcoded repeat-count of 1. No behavior change. Verified 26/26 in all three build trees.
Priority: P3
Area: WinUser
Type: Cleanup
Evidence: src/internal/FreeApiMessageQueue.cpp:294-295
Depends on: None

Problem:
The comment at FreeApiMessageQueue.cpp:294-295 reads "Ignore repeat keydowns (SDL sends repeat events; pass them through as WM_KEYDOWN repeats matching Windows behavior)" but the code does not check `event.key.repeat` or filter anything — every keydown (initial or OS autorepeat) is unconditionally forwarded as WM_KEYDOWN with a hardcoded repeat-count of 1, and no "previous key state" bit is set to distinguish an autorepeat WM_KEYDOWN from an initial press. This session confirmed neither game inspects those lParam bits for WM_KEYDOWN, so the behavior itself is not a bug — only the comment is inaccurate and could mislead a future maintainer.

Required work:
- Rewrite the comment at lines 294-295 to accurately describe current behavior: all keydown events (including OS/SDL autorepeat) are forwarded as WM_KEYDOWN with a fixed repeat-count of 1 and no previous-key-state distinction, and that neither target game depends on those specific lParam bits.

Acceptance criteria:
- The comment accurately reflects the code's actual behavior.
- No behavior change.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not add real repeat-count accumulation or previous-key-state bit tracking — not evidenced as needed by either game.
- Do not change WM_KEYUP's transition/previous-state bit handling.

## Timers

### TASK-24H-0501: Verify and document timeKillEvent's non-standard failure return values
Status: DONE — confirmed free-eggbert's sole timeKillEvent call site (blupi.cpp:628) is a bare, return-value-discarding statement; planetblupi has zero call sites. Added comments at both failure returns (src/winmm.cpp) and the doc comment (include/mmsystem.h) stating this is a known, intentional-but-informal convention. Verified 25/25 in all three build trees.
Priority: P2
Area: WinMM
Type: Verification
Evidence: src/winmm.cpp:163-185 (timeKillEvent), docs/audit-24h-free-api.md §4.13
Depends on: None

Problem:
timeKillEvent returns the literal `1` for both the trivial `uTimerID==0` case (src/winmm.cpp:166) and the not-found case (src/winmm.cpp:174), while its success path returns `MMSYSERR_NOERROR` (0, src/winmm.cpp:184) — an inconsistent, non-named-constant return value across branches of the same function. This is not currently proven to affect either game, but is undocumented and reads as an oversight to a future maintainer.

Required work:
- Grep both ../free-eggbert and ../planetblupi source for every timeKillEvent call site and confirm neither inspects the return value by exact numeric comparison (only by rough success/failure, if at all).
- If confirmed unused, add an explicit comment at both failure `return 1;` sites in src/winmm.cpp and in the include/mmsystem.h doc comment stating this is a known, intentional-but-informal non-zero-failure convention (not a real MMRESULT error code), and that changing it requires new evidence of a caller depending on the exact value.

Acceptance criteria:
- Both games confirmed (via grep+read of actual call sites) not to depend on the specific numeric return value of timeKillEvent.
- src/winmm.cpp and include/mmsystem.h carry an explicit comment explaining the return-value asymmetry is known and intentional.
- Existing tests still pass, including tests/test_timer_regressions.cpp's `timeKillEvent returns MMSYSERR_NOERROR` assertion.
- No unrelated API is added.

Out of scope:
- Do not change timeKillEvent's actual return values (e.g. to MMSYSERR_NOERROR or a named TIMERR_* constant) without new evidence a real caller needs it — that is a behavior change, not documentation.
- Do not touch SetTimer/KillTimer's BOOL return convention, which is unrelated and covered by TASK-24H-0509.

---

### TASK-24H-0502: Document the shared SetTimer/timeSetEvent ID-generator design decision
Status: DONE — added comments at g_nextTimerId's declaration (src/internal/FreeApiTimers.hpp) and definition (.cpp) explaining the deliberate sharing; added a cross-referencing sentence to docs/headers.md's mmsystem.h row. Verified 25/25 in all three build trees.
Priority: P2
Area: WinUser
Type: Documentation
Evidence: src/internal/FreeApiTimers.hpp:35, src/internal/FreeApiTimers.cpp:10, src/winuser_timer.cpp:16, src/winmm.cpp:127
Depends on: None

Problem:
SetTimer and timeSetEvent are two fully independent timer mechanisms, but both draw IDs from the same `std::atomic<UINT> g_nextTimerId` counter (declared src/internal/FreeApiTimers.hpp:35, defined src/internal/FreeApiTimers.cpp:10). This is intentional — it avoids ID collisions if a caller ever mixed both APIs — but there is currently zero comment anywhere explaining why the counter is shared, so it reads like an accidental coupling to a future maintainer.

Required work:
- Add a comment at the `g_nextTimerId` declaration (src/internal/FreeApiTimers.hpp) and definition (src/internal/FreeApiTimers.cpp) explaining it is deliberately shared across the two otherwise-independent timer mechanisms specifically to guarantee ID uniqueness if a caller mixed both APIs, and that this is the only coupling between them.
- Add one cross-referencing sentence to docs/headers.md's `mmsystem.h`/`winuser.h` rows noting this shared-counter decision.

Acceptance criteria:
- A reader of src/internal/FreeApiTimers.hpp/.cpp understands, without needing this backlog entry, why the ID counter is shared.
- Existing tests still pass unchanged (this is a comment-only change).
- No unrelated API is added.

Out of scope:
- Do not split the counter into two independent per-mechanism counters — the shared counter is confirmed intentional, not a bug.
- Do not merge any other state (maps, mutexes) between the two mechanisms — they must remain fully independent.

---

### TASK-24H-0503: Document that timeSetEvent ignores fuEvent and always behaves as periodic
Status: DONE — updated include/mmsystem.h's timeSetEvent doc comment stating fuEvent/TIME_ONESHOT is accepted but not honored, and why TIME_ONESHOT has no defined constant. Verified 25/25 in all three build trees.
Priority: P2
Area: WinMM
Type: Documentation
Evidence: src/winmm.cpp:109-121 (fuEvent cast to void, line 116), include/mmsystem.h:174-184, include/mmsystem.h:122 (only TIME_PERIODIC is defined)
Depends on: None

Problem:
timeSetEvent's implementation comment in src/winmm.cpp already notes "one-shot is treated as periodic," but `fuEvent` is unconditionally cast away (`(void)fuEvent;`, src/winmm.cpp:116) and never actually inspected — every timer always reschedules itself (the SDL3 timer bridge always returns the same interval, src/winmm.cpp:95). The public header doc comment (include/mmsystem.h:174-179) only says "Starts a WinMM-style periodic timer" and does not carry this deviation-from-real-Win32 detail where a maintainer reading the public API surface would actually see it. Separately, `TIME_ONESHOT` is not even defined in include/mmsystem.h (only `TIME_PERIODIC` at line 122), so there is no way to even name the flag a caller might otherwise pass.

Required work:
- Update the include/mmsystem.h doc comment for timeSetEvent to explicitly state that `fuEvent`/`TIME_ONESHOT` is accepted but not honored — every timer is treated as periodic (`TIME_PERIODIC`) regardless of the flag passed, matching src/winmm.cpp's actual behavior.
- Note in the same comment that `TIME_ONESHOT` has no defined constant in this header because neither target game uses it, and that adding it is out of scope without an evidenced call site.

Acceptance criteria:
- include/mmsystem.h's timeSetEvent doc comment explicitly states the periodic-only behavior, matching src/winmm.cpp's implementation comment.
- Existing tests still pass, including tests/test_timer_regressions.cpp and tests/test_eggbert_loop.cpp (both use TIME_PERIODIC already, unaffected).
- No unrelated API is added.

Out of scope:
- Do not add a `TIME_ONESHOT` constant or implement real one-shot semantics — neither target game uses it; this is documentation only.
- Do not change timeSetEvent's runtime behavior in any way.

---

### TASK-24H-0504: Document that SetTimer/WM_TIMER has no one-shot mode
Status: DONE — added the sentence to include/winuser.h's SetTimer doc comment. Comment-only. Verified 26/26 in all three build trees.
Priority: P3
Area: WinUser
Type: Documentation
Evidence: src/winuser_timer.cpp:11-37, include/winuser.h:341-348
Depends on: None

Problem:
SetTimer's implementation (src/winuser_timer.cpp:11-37) repeats indefinitely at `wt.intervalMs` until KillTimer is called — matching real Win32, where SetTimer has never had a one-shot mode at the API level. The current include/winuser.h doc comment (lines 341-348) explains ID generation, `lpTimerFunc` being ignored, and the "only fires while polled" caveat, but does not explicitly say the timer repeats indefinitely with no one-shot option.

Required work:
- Add one sentence to the SetTimer doc comment in include/winuser.h stating explicitly that the timer repeats every `uElapse` ms until `KillTimer` is called; there is no one-shot mode, matching real Win32 SetTimer semantics.

Acceptance criteria:
- include/winuser.h's SetTimer doc comment explicitly states the always-periodic, no-one-shot behavior.
- Existing tests still pass unchanged (this is a comment-only change).
- No unrelated API is added.

Out of scope:
- Do not add any one-shot variant or new flag to SetTimer — real Win32 SetTimer has none; this is documentation only.

---

### TASK-24H-0505: Add regression coverage that WM_TIMER coalescing does not starve concurrent input
Status: DONE — TestFastTimerDoesNotStarveConcurrentNonTimerMessages (tests/test_timer_regressions.cpp) interleaves posting 10 non-timer messages with a 2ms SetTimer and asserts all are received exactly once while the timer keeps firing concurrently.
Priority: P1
Area: WinUser
Type: Test
Evidence: src/internal/FreeApiMessageQueue.cpp:88-101 (WM_TIMER coalescing), tests/test_input_pipeline.cpp:363-390 (sustained-input stress test, no live timer involved), tests/test_timer_regressions.cpp:227-279 (cross-thread stress test, no non-timer input involved)
Depends on: None

Problem:
FreeApiMessageQueue.cpp coalesces WM_TIMER messages (at most one pending per hwnd+timer-id, dropping duplicates) so that stale timer messages don't pile up. Existing coverage tests this indirectly but never in combination: the sustained-input stress test injects mouse/keyboard events with no live timer running, and the timer cross-thread stress test runs a live timeSetEvent timer with no concurrent non-timer input. Neither proves a fast-firing timer cannot delay or starve real player input, which is the actual scenario both games' frame pumps create every frame.

Required work:
- Add a new regression test that runs a fast SetTimer (or timeSetEvent) alongside injected keyboard/mouse events over a real wall-clock window, then confirms via PeekMessageA/DrainMessages that every injected non-timer input message is delivered and that the WM_TIMER queue depth for that timer id never exceeds 1 pending entry (the coalescing bound).

Acceptance criteria:
- New test demonstrates all injected non-timer input messages are received exactly once during sustained concurrent timer activity.
- New test demonstrates WM_TIMER never accumulates more than one coalesced-pending entry per id during the same run.
- Existing tests still pass, including test_input_pipeline.cpp's sustained-input test and test_timer_regressions.cpp's existing stress test.
- No unrelated API is added.

Out of scope:
- Do not change FreeApiMessageQueue.cpp's coalescing logic — this task is verification-only unless the new test finds a real gap, in which case stop and report rather than silently changing coalescing behavior.
- Do not add general multi-timer stress testing beyond the single-active-timer-per-game usage pattern both target games actually exhibit.

---

### TASK-24H-0506: Add a sanitizer-verified regression test for the timeKillEvent/callback-in-flight race
Status: DONE — added FREE_API_SANITIZE (thread/address, off by default, PUBLIC on the free-api target so tests/examples inherit matching flags) and TestTimeSetEventKillRaceHasNoUseAfterFree (2000-iteration timeSetEvent/timeKillEvent race, tests/test_timer_regressions.cpp). Running it (and the full 22-test suite) under this new option found TWO real, previously-undetected bugs, both now fixed:
  (1) FreeApiMmTimerBridge's "verify still alive" re-lookup was itself a use-after-free: it dereferenced `entry->mmId` on a raw pointer straight into g_mmTimers' map node BEFORE confirming (by that very read) whether the node was still alive -- holding g_mmTimerMutex only serializes the two critical sections, it does not stop a prior timeKillEvent's erase() from already having freed that memory. Fixed by packing the timer ID directly into the SDL userdata pointer slot instead of a map-node pointer, so the bridge never dereferences a potentially-freed pointer at all (src/winmm.cpp).
  (2) ThreadSanitizer caught a real, unrelated cross-thread data race on `FreeApi::Internal::g_debugInput` (plain `bool`, written by EnsureVideoSubsystem() on the main thread on every CreateWindowExA, read by InputLog() from the SDL timer thread). Fixed by making it `std::atomic_bool` (src/internal/FreeApiMessageQueue.hpp/.cpp), matching the existing g_updateMessagePending precedent in the same file.
  Also found and fixed, via the free-eggbert build tree's assertions-enabled SDL3 (not caught by the standalone build's release SDL3): timeSetEvent's unconditional per-call SDL_InitSubSystem(SDL_INIT_EVENTS) (no matching SDL_QuitSubSystem) overflowed SDL's byte-sized subsystem refcount past 255 during the new 2000-iteration race test, aborting via SDL's own assertion. Fixed with an SDL_WasInit guard, matching EnsureJoystickSubsystem's existing pattern in the same file (src/winmm.cpp).
  Verified: full 22-test ctest suite clean (0 sanitizer reports) under both ThreadSanitizer and AddressSanitizer (LD_PRELOADing the versioned runtime .so; see docs/cmake-options.md's new "Sanitizer-instrumented test builds" section), and clean in the default non-sanitized build across all three build trees (free-api standalone, free-eggbert Ninja, planetblupi Make -- 22/22 each).
  FOLLOW-UP (post-session-3 audit finding): fix (2) above was only ever INCIDENTALLY exercised by an unrelated test's timing, not by a dedicated test -- added TestGDebugInputSurvivesRaceUnderSanitizer (tests/test_timer_regressions.cpp), which races g_debugInput directly and deterministically (200000 main-thread writes vs. a continuous background-thread InputLog() reader). Verified with a genuine negative control: temporarily reverted g_debugInput to plain `bool` (in both the real source and the test's matching forward-declaration), ran this ONE test in isolation under ThreadSanitizer, and confirmed it independently reports the exact race (stack trace points directly at this test's own write/read, not at any other test) -- then reverted the negative-control change back cleanly. Re-verified clean (0 warnings) with the real atomic_bool fix restored.
Priority: P1
Area: WinMM
Type: Test
Evidence: src/winmm.cpp:75-96 (FreeApiMmTimerBridge re-lookup-by-ID), src/winmm.cpp:163-185 (timeKillEvent erase-before-SDL_RemoveTimer ordering), docs/audit-24h-free-api.md §4.13
Depends on: None

Problem:
FreeApiMmTimerBridge re-looks-up its timer entry by ID on every fire specifically to guard against a documented, already-fixed prior use-after-free bug where timeKillEvent could erase a timer's map entry while its SDL_AddTimer callback was concurrently in flight on a separate thread. This fix has been re-confirmed correct by source-reading in two audit sessions now, but the repository has no ThreadSanitizer/AddressSanitizer-instrumented build or test that would catch a regression here mechanically — confidence currently rests entirely on manual code review of a genuinely subtle cross-thread ordering.

Required work:
- Add an opt-in CMake sanitizer build option (e.g. `-DFREE_API_SANITIZE=thread` or `=address`, off by default, additive to existing build modes) that instruments the test binaries only, not the target-game builds.
- Add a regression test in tests/test_timer_regressions.cpp that deliberately races timeKillEvent against an in-flight FreeApiMmTimerBridge callback (short interval, tight kill timing, looped enough iterations to make the race likely) and document how to run it under the new sanitizer option.
- Run the new test under the sanitizer build locally and record the clean result.

Acceptance criteria:
- New sanitizer build option exists, is off by default, and does not affect the default target-game build configuration.
- New regression test passes cleanly under ThreadSanitizer or AddressSanitizer (whichever is added) with no reported race/use-after-free.
- Existing tests still pass in the default (non-sanitized) build.
- No unrelated API is added.

Out of scope:
- Do not enable sanitizers by default or wire them into the target-game CMake configuration used by ../free-eggbert/../planetblupi builds.
- Do not change FreeApiMmTimerBridge's or timeKillEvent's logic unless the sanitizer run actually finds a real regression — if it does, stop and report rather than silently patching, since this session's evidence says the fix is confirmed still correct.

---

### TASK-24H-0507: Record the "do not unify SetTimer and timeSetEvent" decision in permanent docs
Status: DONE — added a docs/out-of-scope.md entry ("SetTimer/WM_TIMER and timeSetEvent/timeKillEvent must stay two independent implementations") recording the decision explicitly. Verified 25/25 in all three build trees.
Priority: P2
Area: WinMM
Type: Documentation
Evidence: docs/audit-24h-free-api.md §4.12/§4.13, src/winuser_timer.cpp, src/winmm.cpp:60-185
Depends on: None

Problem:
SetTimer/KillTimer/WM_TIMER and timeSetEvent/timeKillEvent are confirmed to be planetblupi's and free-eggbert's respective sole, mutually-exclusive frame pumps. A prior session's notes already call out that unifying these two mechanisms into one implementation must not be done, but that decision currently lives only in session-history-style notes, not in a permanent, easy-to-find scope document a future contributor or agent would actually consult before attempting such a "simplification."

Required work:
- Add a short, explicit entry to docs/out-of-scope.md stating that SetTimer/KillTimer/WM_TIMER and timeSetEvent/timeKillEvent must remain two fully independent implementations, because each target game depends on its own mechanism working in isolation, and consolidating them is a previously-considered-and-rejected idea, not an oversight.

Acceptance criteria:
- docs/out-of-scope.md contains an explicit, standalone statement of this decision, discoverable without needing to read session-history docs.
- Existing tests still pass unchanged (this is a doc-only change).
- No unrelated API is added.

Out of scope:
- Do not perform or begin any actual consolidation work — this task is documentation-only, recording a decision that has already been made.
- Do not propose or imply a future consolidation roadmap — the decision is permanent, not deferred.

---

### TASK-24H-0508: Add missing SetTimer edge-case coverage: auto-ID generation and minimum-interval clamp
Status: DONE — TestSetTimerAutoIdAndMinimumIntervalClamp (tests/test_timer_regressions.cpp) covers both nIDEvent=0 auto-ID generation and uElapse=0 clamping to a fast minimum interval.
Priority: P1
Area: WinUser
Type: Test
Evidence: src/winuser_timer.cpp:15-22 (nIDEvent==0 auto-ID branch, uElapse clamp to minimum 1ms), tests/test_timer_regressions.cpp (all SetTimer calls use explicit non-zero IDs and non-zero intervals)
Depends on: None

Problem:
SetTimer has two implemented edge-case behaviors with zero test coverage: passing `nIDEvent == 0` triggers auto-ID generation via the shared `g_nextTimerId` counter (src/winuser_timer.cpp:15-17), and passing `uElapse == 0` clamps the interval to a minimum of 1ms (src/winuser_timer.cpp:22). Every SetTimer call in the existing test suite and in both target games uses an explicit non-zero ID and a non-zero interval, so neither branch is currently exercised by any test.

Required work:
- Add a regression test confirming SetTimer with `nIDEvent == 0` returns a non-zero, valid timer ID and that the resulting timer actually fires WM_TIMER carrying that returned ID.
- Add a regression test confirming SetTimer with `uElapse == 0` still produces WM_TIMER delivery at a fast (clamped-to-1ms) cadence rather than never firing or misbehaving.

Acceptance criteria:
- Both new assertions pass, exercising the two previously-untested branches in src/winuser_timer.cpp:15-22.
- Existing tests still pass, including all tests currently using explicit SetTimer IDs/intervals.
- No unrelated API is added.

Out of scope:
- Do not change SetTimer's clamp or auto-ID logic — this task adds coverage for already-correct, already-implemented behavior only.
- Do not add coverage for re-registering an existing timer ID with a new interval — not evidenced as used by either target game.

---

### TASK-24H-0509: Verify and document KillTimer's unconditional TRUE return for unknown timer IDs
Status: DONE — confirmed both games' every KillTimer call site is a bare, return-value-discarding statement from their WM_DESTROY handlers. Added comments at src/winuser_timer.cpp's return TRUE and include/winuser.h's doc comment. Verified 25/25 in all three build trees.
Priority: P2
Area: WinUser
Type: Verification
Evidence: src/winuser_timer.cpp:39-53 (KillTimer, unconditional `return TRUE;` at line 52 regardless of whether `g_winTimers.erase(uIDEvent)` found anything), tests/test_timer_regressions.cpp:121-122 (only asserts TRUE for a known, live ID)
Depends on: None

Problem:
KillTimer always returns TRUE (src/winuser_timer.cpp:52) even when `uIDEvent` does not correspond to any registered timer, whereas real Win32's KillTimer returns 0 (FALSE) if the specified timer cannot be found. This mirrors, on the WinUser side, the same category of return-value looseness already tracked for timeKillEvent on the WinMM side (TASK-24H-0501) — undocumented, not currently proven to matter to either game, but worth recording explicitly.

Required work:
- Grep both ../free-eggbert and ../planetblupi source for every KillTimer call site and confirm neither inspects the return value (both games' actual usage is fire-and-forget from their WM_DESTROY handlers).
- Add a comment at src/winuser_timer.cpp's `return TRUE;` and in the include/winuser.h KillTimer doc comment stating explicitly that KillTimer always reports success, even for an unknown/already-removed timer ID, unlike real Win32.

Acceptance criteria:
- Both games confirmed not to depend on KillTimer's return value for an unknown ID.
- src/winuser_timer.cpp and include/winuser.h carry an explicit comment describing this deviation.
- Existing tests still pass, including the `KillTimer returns TRUE` assertion at tests/test_timer_regressions.cpp:122.
- No unrelated API is added.

Out of scope:
- Do not change KillTimer to return FALSE for unknown IDs without new evidence a real caller needs it — that would be a behavior change, not documentation.
- Do not touch timeKillEvent's separate, already-tracked return-value inconsistency (TASK-24H-0501) in this task.

## GDI

### TASK-24H-0601: Fix StretchBlt scaled-path source-Y clamp asymmetry and add regression coverage
Status: DONE — src/wingdi_blit.cpp:123-129 now clamps out-of-range source Y to the nearest edge row instead of skipping the destination row. New test TestStretchBltScaledOutOfRangeSourceYClampsToEdgeRowLikeX added to tests/test_gdi_regressions.cpp; include/wingdi.h's StretchBlt doc comment updated. Verified 17/17 in all three build modes.
Priority: P0
Area: GDI
Type: Bugfix
Evidence: src/wingdi_blit.cpp:115-126 (scaled path); tests/test_gdi_regressions.cpp:245-284 (existing scaled-path test, in-bounds only); docs/audit-24h-free-api.md §1 risk #1, §4.16, §5 item 1; plan.md TASK-0003 (prior tracking of the same defect, not yet fixed)
Depends on: None

Problem:
In `StretchBlt`'s scaled (non-1:1) blit path, out-of-range source-X coordinates are clamped to the nearest edge column (`src/wingdi_blit.cpp:118-119`: `if (sx < 0) sx = 0; if (sx >= srcBitmap->width) sx = srcBitmap->width - 1;`), but the analogous out-of-range source-Y coordinate instead does `if (srcY < 0 || srcY >= srcBitmap->height) continue;` (`src/wingdi_blit.cpp:126`), which skips writing the destination row entirely rather than clamping. Both situations arise from the exact same cause (scale/offset arithmetic pushing a sampled source coordinate out of the source bitmap's bounds); the two axes currently produce visibly different results — X degrades gracefully via edge-repeat, Y leaves stale/undrawn destination pixels. This is in the live per-frame render path used by both games' `ddutil.cpp` blit routines, and no existing test exercises the out-of-range-source-Y scaled case (`TestStretchBltScaledNearestNeighborSamplesCorrectSourcePixel`, `tests/test_gdi_regressions.cpp:245-284`, only covers a fully in-bounds 2x2->4x4 upscale).

Required work:
- In `src/wingdi_blit.cpp`, change the source-Y handling in the scaled path (around line 126) from `continue` to clamp-to-edge, matching the X-axis policy exactly: replace `if (srcY < 0 || srcY >= srcBitmap->height) continue;` with clamping logic equivalent to `if (srcY < 0) srcY = 0; if (srcY >= srcBitmap->height) srcY = srcBitmap->height - 1;` (adjust `srcY`'s declaration from `const` to mutable as needed).
- Update the `@brief`/`@note` doc comment on `StretchBlt` in `include/wingdi.h` (currently lines 132-139) to state the scaled path now clamps out-of-range source coordinates to the nearest edge pixel on both axes.
- Add a new regression test in `tests/test_gdi_regressions.cpp` that performs a scaled `StretchBlt` where the source rect's Y range extends past the source bitmap's height (e.g., a 2x2 source blitted with `ySrc`/`hSrc` chosen so some destination rows map to `srcY >= srcBitmap->height`), and asserts those destination rows are filled with the clamped edge-row's pixel data rather than left as sentinel/untouched values.

Acceptance criteria:
- Scaled `StretchBlt` clamps out-of-range source coordinates identically on both X and Y axes.
- New test fails against the pre-fix code (verified by inspection of the diff) and passes after the fix.
- Existing tests still pass, including `TestStretchBlt1to1OutOfRangeSourceRectClipsSafely` and `TestStretchBltScaledNearestNeighborSamplesCorrectSourcePixel`.
- No unrelated API is added.

Out of scope:
- Do not modify the 1:1 fast path (`src/wingdi_blit.cpp:50-94`) — already correct and tested.
- Do not add rotation, mirroring, or any ROP beyond `SRCCOPY`.
- Do not implement general Win32 `StretchBlt` stretch-mode (`SetStretchBltMode`) support — out of evidenced scope for both games.

---

### TASK-24H-0602: Investigate whether planetblupi's fullscreen minimap actually exercises CreateBitmap's 8-bit greyscale-only path with non-greyscale data
Status: DONE — traced precisely: CPixmap::m_bPalette stays TRUE unconditionally in fullscreen mode (InitSysPalette()'s GetDeviceCaps(SIZEPALETTE) override at pixmap.cpp:285-290 only runs `if (!m_bFullScreen)`), so the 8-bit path IS always taken in fullscreen. SearchColor()'s fullscreen branch (pixmap.cpp:487-511) searches the real m_pal[] table unconditionally, returning genuine non-greyscale palette-slot indices (decmap.cpp's m_colors[MAP_*], e.g. red/black/yellow/blue/grey), so the resulting minimap WOULD be visibly wrong (dark greyscale scramble) if reached. However, planetblupi's shipped data/config.def default is "FullScreen=0" (windowed) — confirmed via grep — which takes m_bPalette=FALSE (GetDeviceCaps now correctly returns 0) and the working 16-bit RGB565 path instead. Conclusion: reached only in a non-default (explicitly fullscreen-configured) run; NOT reached in the shipped default. See TASK-24H-0603 for the resulting decision.
Priority: P1
Area: GDI
Type: Audit
Evidence: src/wingdi_bitmap.cpp:97-157 (CreateBitmap, 8-bit branch at 116-131, `TODO: apply palette if one is set` at line 119); ../planetblupi/src/decmap.cpp:576-582 (branch selecting 8-bit vs 16-bit CreateBitmap call based on `g_bPalette`); ../planetblupi/src/pixmap.cpp:24-27 (constructor default `m_bPalette = TRUE`), :118-126 (`Create()` sets `m_bFullScreen` from caller), :285-294 (the `GetDeviceCaps(SIZEPALETTE)`-based override of `m_bPalette` runs **only** when `!m_bFullScreen`, i.e. is skipped entirely in fullscreen mode), :483-549 (`SearchColor` returns real 0-255 palette-slot indices, not greyscale intensities, when `m_bPalette` is true); docs/audit-24h-free-api.md §4.19, §5 item 21
Depends on: None

Problem:
`CreateBitmap`'s 8-bit path treats each input byte as a raw greyscale intensity (`R=G=B=index`), self-documented as an incomplete palette expansion. The only call site in either game is planetblupi's minimap (`decmap.cpp:578`, real player-visible UI drawn via `DrawIcon`/`CHMAP`). Direct reading of planetblupi's own source shows the 8-bit branch is selected whenever `CPixmap::IsPalette()` returns true, and `m_bPalette` defaults to `TRUE` in the constructor and is **only ever overridden to `FALSE` by the `GetDeviceCaps(SIZEPALETTE)` check when the game is running windowed** (`pixmap.cpp:285`: `if (!m_bFullScreen) { ... }`). In fullscreen mode this override never runs, so `m_bPalette` stays `TRUE` regardless of what `GetDeviceCaps` returns, `g_bPalette` in `decmap.cpp` is `TRUE`, and the game calls `CreateBitmap(..., 8, g_map8_bits)` with index values produced by `SearchColor()` — real palette-slot numbers for terrain colors (red frame, yellow Blupi, blue sea, green grass, etc.), not greyscale brightness values. If this reasoning holds and either game commonly runs fullscreen, the minimap would render as visually-wrong scrambled brightness rather than the intended colors. This has not been empirically confirmed (no screenshot/playtest comparison, no check of the games' shipped/default `FullScreen=` config value) — severity must be determined before deciding fix vs. document.

Required work:
- Confirm, by tracing `g_bFullScreen`'s actual runtime value for a normal launch of planetblupi under free-api (default config, or the config free-api's own build/run scripts use), whether the 8-bit `CreateBitmap` path is genuinely reached in practice.
- If reached, either capture a real screenshot of the in-game minimap (via the existing Xvfb-driven playtest approach referenced in `NEXT.md`) or write a standalone reproduction (feed `SearchColor`-equivalent index values through `CreateBitmap`'s 8-bit path and inspect the resulting RGBA) to determine whether the visual result is meaningfully wrong versus acceptable.
- Record the finding (reached-or-not, visually-broken-or-not) and a fix-or-document recommendation for TASK-24H-0603.

Acceptance criteria:
- A written determination exists (in the task's resolution / a linked doc note) stating definitively whether planetblupi's minimap exercises the 8-bit `CreateBitmap` path in practice, and if so, whether the resulting colors are visibly wrong.
- No source code behavior is changed by this task — investigation only.
- Existing tests still pass (unaffected).

Out of scope:
- Do not implement a real palette lookup in this task — that is TASK-24H-0603, contingent on this investigation's finding.
- Do not add a general `HPALETTE`-backed palette API — no evidence either game needs one beyond this single minimap call site.

---

### TASK-24H-0603: Apply the fix-or-document decision for CreateBitmap's 8-bit indexed path
Status: DONE — decision: document, do not fix. free-api's CreateBitmap has no palette parameter (matching real Win32's own CreateBitmap); the two ways to add real palette support -- a general HPALETTE/SetDIBColorTable-style API, or hard-coding planetblupi's specific m_colors table into free-api -- both violate this project's scope rules (no general Win32 palette API; no game-specific special-casing in a shared function). Given the bug is confirmed reached only in a non-default (explicit fullscreen) configuration, documenting is the correct, scope-respecting choice. Updated: src/wingdi_bitmap.cpp's comment (replaces the bare TODO with the full investigated conclusion), include/wingdi.h's CreateBitmap doc comment, docs/supported-apis.md's CreateBitmap row (also fixes TASK-24H-0610's stale "1bpp" claim in the same edit).
Priority: P1
Area: GDI
Type: Bugfix
Evidence: src/wingdi_bitmap.cpp:116-131; TASK-24H-0602's findings; docs/audit-24h-free-api.md §4.19, §5 item 21
Depends on: TASK-24H-0602

Problem:
TASK-24H-0602 determines whether planetblupi's minimap genuinely relies on `CreateBitmap`'s 8-bit path producing real (non-greyscale) colors. This task carries out whichever action that investigation recommends, so the gap doesn't remain merely documented-and-ignored if it turns out to be a real, live visual defect.

Required work:
- If TASK-24H-0602 found the 8-bit path is reached with non-greyscale data and the result is visibly wrong: implement a real palette-index lookup in `CreateBitmap`'s 8-bit branch (`src/wingdi_bitmap.cpp:116-131`). Since `CreateBitmap`'s signature has no palette parameter, this requires either (a) accepting the current greyscale limitation is unfixable without a new palette-passing mechanism and documenting that clearly, or (b) confirming a viable, narrowly-scoped fix exists (e.g., if planetblupi's index values are drawn from a small, known, fixed set — the `m_colors[MAP_*]` table) and implementing exactly that, nothing more general.
- If TASK-24H-0602 found the path is unreached in practice, or the visual result is acceptable: update the `TODO` comment at `src/wingdi_bitmap.cpp:119`, the `CreateBitmap` doc comment in `include/wingdi.h:94-95`, and the `CreateBitmap` row in `docs/supported-apis.md` to explicitly record the investigated conclusion (e.g., "confirmed acceptable because ...") instead of leaving a bare unresolved `TODO`.

Acceptance criteria:
- The `TODO: apply palette if one is set` comment at `src/wingdi_bitmap.cpp:119` is either resolved by a real fix or replaced with an explicit, evidence-backed "intentionally not fixed because ..." note — it does not remain a silent unresolved TODO.
- `TestCreateBitmap8BitIndexedExpandsToGreyscaleRgba` (`tests/test_gdi_regressions.cpp:307`) is updated to match whichever behavior is now correct, or a new test is added alongside it if real palette lookup was implemented.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not implement a general `HPALETTE`/`SetDIBColorTable`-style real Win32 palette API.
- Do not change the 16-bit or 32-bit `CreateBitmap` paths — both already confirmed correct.

---

### TASK-24H-0604: Document GetDeviceCaps's index-blind implementation as scoped to SIZEPALETTE only
Status: DONE — strengthened include/wingdi.h's @note comment to explicitly state index is ignored for every value, added a matching code comment in src/wingdi_misc.cpp, and updated docs/supported-apis.md's row (closing duplicate TASK-24H-1214). Verified 26/26 in all three build trees.
Priority: P3
Area: GDI
Type: Documentation
Evidence: src/wingdi_misc.cpp:5-15; include/wingdi.h:109-114; docs/audit-24h-free-api.md §4.18, §5 item 21
Depends on: None

Problem:
`GetDeviceCaps` (`src/wingdi_misc.cpp:5-15`) ignores its `index` parameter entirely — `(void)index;` at line 8 — and returns `0` for literally any index value, not just `SIZEPALETTE`. This is confirmed correct for the one index either game actually queries (`SIZEPALETTE`, both games' TrueColor-vs-palette branching), but the existing header doc comment (`include/wingdi.h:110-114`) only implies the scoping via a parenthetical rather than stating plainly that every other index also returns 0 as an unintended side effect of the implementation shape, not a deliberate per-index decision.

Required work:
- Update the `@note` comment on `GetDeviceCaps` in `include/wingdi.h:109-114` to state explicitly: implemented correctly for `SIZEPALETTE` only; every other index also happens to return 0, which is safe today only because neither game queries any other index, and must not be read as "0 is the correct value for any `GetDeviceCaps` index."
- Add the same clarification as a code comment in `src/wingdi_misc.cpp` near line 8.

Acceptance criteria:
- The header doc comment and implementation comment both state plainly that only `SIZEPALETTE` is a validated, correct return value, and that other indices returning 0 is unvalidated/incidental.
- No behavior change.
- Existing tests still pass.

Out of scope:
- Do not implement real per-index `GetDeviceCaps` behavior (e.g., `HORZRES`, `VERTRES`, `BITSPIXEL`) — no evidenced call site for any index other than `SIZEPALETTE`.

---

### TASK-24H-0605: Align LoadImageA's path normalization with NormalizeFilesystemPath
Status: DONE — changed src/wingdi_bitmap.cpp's LoadImageA to call NormalizeFilesystemPath instead of NormalizePath. Added TestLoadImageAWithLeadingBackslashRootedPathStaysRelativeToCwd (tests/test_gdi_regressions.cpp): writes a fixture at a relative path, loads it via a leading-backslash-rooted path, asserts it's found relative to CWD (not treated as absolute). Verified passing alongside the existing TestLoadImageADecodesNonBmpExtensionAndGetObjectAReportsCorrectDimensions (23/23 suite). Note (post-session-3 audit): this task was previously mislabeled "TASK-24H-1105" in a user instruction that paraphrased an audit finding -- TASK-24H-1105 is an unrelated diagnostics-alias-naming task; this entry is the correct one for the LoadImageA path-normalization fix.
Priority: P2
Area: GDI
Type: Bugfix
Evidence: src/wingdi_bitmap.cpp:26; src/internal/FreeApiPath.hpp:7,20; src/internal/FreeApiPath.cpp:7,18; docs/audit-24h-free-api.md §4.15, §4.20
Depends on: None

Problem:
`LoadImageA` (`src/wingdi_bitmap.cpp:26`) normalizes its input path with `FreeApi::Internal::NormalizePath`, which only converts backslashes to forward slashes. Every other file-opening entry point in the codebase (`_lopen`, `CreateDirectoryA`, `_mkdir`, `_findfirst`, ...) uses the stronger `NormalizeFilesystemPath`, which additionally strips a leading drive letter and leading slashes so a Windows-style rooted path like `\User\foo.bmp` is treated as relative rather than escaping to the real filesystem root. This is not a proven bug today — no evidenced game call site passes a rooted-looking bitmap path to `LoadImageA` — but it is a real inconsistency that would misbehave silently if such a path ever appeared.

Required work:
- Change `src/wingdi_bitmap.cpp:26` to call `NormalizeFilesystemPath(name)` instead of `NormalizePath(name)`.
- Add a regression test to `tests/test_gdi_regressions.cpp` that calls `LoadImageA` with a path exhibiting the specific difference between the two functions (e.g. a leading-backslash-rooted relative path such as `\test_gdi_fixture_rooted.blp`, mirroring the existing `MakeMinimalBmp`/fixture-file pattern at `tests/test_gdi_regressions.cpp:442-514`), asserting the file is found and loaded correctly under the stronger normalization.

Acceptance criteria:
- `LoadImageA` uses the same path-normalization function as every other file-opening API in the codebase.
- New test passes; existing `LoadImageA` tests (`TestLoadImageADecodesNonBmpExtensionAndGetObjectAReportsCorrectDimensions`) still pass unchanged.
- No unrelated API is added.

Out of scope:
- Do not perform the broader four-implementation path-normalization consolidation described in the audit's §4.20 — that spans file I/O and MIDI subsystems outside the GDI theme; this task only touches `LoadImageA`'s specific call.
- Do not change `NormalizePath`'s or `NormalizeFilesystemPath`'s own implementations.

---

### TASK-24H-0606: Gate LoadImageA's success-path log behind FreeApiGdiDebugEnabled
Status: DONE — gated behind FreeApiGdiDebugEnabled(); failure-path logs left unconditional per policy. Duplicate of TASK-24H-1102, closed together.
Priority: P1
Area: GDI
Type: Bugfix
Evidence: src/wingdi_bitmap.cpp:47; docs/audit-24h-free-api.md §4.29, §5 item 6 (identifies this as one of only two unconditional-logging sites, alongside `winuser_window.cpp`, that fire on every normal run and are worth actually gating)
Depends on: None

Problem:
`LoadImageA`'s success path (`src/wingdi_bitmap.cpp:47`) calls `SDL_Log(...)` unconditionally on every single successful bitmap load, unlike the rest of the GDI subsystem's disciplined `FreeApiGdiDebugEnabled()` gating (e.g. `StretchBlt`'s debug logs at `src/wingdi_blit.cpp:26,35,89,142` are all gated). Both games load many sprite-sheet bitmaps at startup, so this produces unconditional log spam on every normal run with no opt-out short of a code change. NOTE: this task is also tracked as TASK-24H-1102 in the Diagnostics theme — implement once, close both.

Required work:
- Wrap the `SDL_Log` call at `src/wingdi_bitmap.cpp:47` in `if (FreeApiGdiDebugEnabled()) { ... }`, matching the gating convention used elsewhere in the GDI subsystem.

Acceptance criteria:
- `LoadImageA`'s success-path log only fires when GDI debug logging is enabled.
- The failure-path logs at `src/wingdi_bitmap.cpp:22,29` are left unconditional (audit-confirmed acceptable — rare/failure-path-only).
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not touch `CreateBitmap`'s unsupported-bit-depth log (`src/wingdi_bitmap.cpp:152`) — audit-confirmed a legitimately rare failure-path site, not part of this session's P1 logging fixes.
- Do not change `winuser_window.cpp`'s unconditional logging — that belongs to the WinUser/Diagnostics themes, not GDI.

---

### TASK-24H-0607: Add regression test coverage for the free-direct bridge GDI helpers' rejection and edge-case behavior
Status: DONE — TestBridgeGdiHelpersRejectionAndEdgeCases (tests/test_gdi_regressions.cpp) covers FreeApiCreateSurfaceDC's five argument-validation rejections, FreeApiDestroySurfaceDC's NULL-handle and wrong-kind-handle rejections, and FreeApiSetWindowFullscreen(NULL,...)'s early-return guard. NEW FINDING while writing this test: FreeApiDestroySurfaceDC/AsCompatDC segfaults (does not return FALSE safely) when given a genuinely garbage, never-allocated pointer (e.g. reinterpret_cast<HDC>(0xDEADBEEF)) -- AsCompatDC/AsCompatBitmap (src/internal/FreeApiGdi.cpp) unconditionally dereference their argument to read a magic-number field with no handle-table validation first, unlike real Win32 handles. No evidenced free-direct call site ever passes such a pointer (it always forwards handles it received from FreeApiCreateSurfaceDC itself), so this is a real but out-of-scope-to-fix robustness gap (fixing it would need a general handle-validation/table framework, against this project's scope discipline) -- documented in the test's own comment rather than fixed. The final test instead uses a real, validly-allocated wrong-KIND handle (a CreateBitmap result cast to HDC), which is the realistic misuse the magic-number check is actually designed to catch, and which is confirmed handled safely.
Priority: P1
Area: GDI
Type: Test
Evidence: src/wingdi_dc.cpp:12-16 (`FreeApiCreateSurfaceDC` argument validation), :30-35 (`FreeApiDestroySurfaceDC` invalid-handle rejection), :44-65 (`FreeApiSetWindowFullscreen`); grep confirms zero references to `FreeApiSetWindowFullscreen` anywhere under `tests/` or `examples/`; docs/audit-24h-free-api.md §3.23, §4.14
Depends on: None

Problem:
Three non-`WINAPI` bridge functions consumed by `../free-direct` (`FreeApiCreateSurfaceDC`, `FreeApiDestroySurfaceDC`, `FreeApiSetWindowFullscreen`, all in `src/wingdi_dc.cpp`) have real behavioral edge cases with no test coverage. `FreeApiCreateSurfaceDC`'s argument-validation rejection (`pixels == nullptr`, `width/height/pitch <= 0`, `bitsPerPixel != 32`, all returning `NULL`) is never exercised — every existing test constructs it with valid 32bpp arguments only (`tests/test_gdi_regressions.cpp`'s `TestSurface` fixture, lines 63-72). `FreeApiSetWindowFullscreen` has literally zero references anywhere in `tests/` or `examples/` — its null-`hwnd` early return (`src/wingdi_dc.cpp:46`) and its window-state-map update (`:58-61`) are entirely unverified. This is separate from the header-declaration gap tracked elsewhere; this task is about verifying these three functions' actual runtime behavior.

Required work:
- Add a test asserting `FreeApiCreateSurfaceDC` returns `NULL` for each invalid-argument case (`nullptr` pixels, `width<=0`, `height<=0`, `pitch<=0`, `bitsPerPixel` other than 32).
- Add a test asserting `FreeApiDestroySurfaceDC` returns `FALSE` and does not crash when given a `NULL`/invalid handle.
- Add a test asserting `FreeApiSetWindowFullscreen(NULL, ...)` does not crash (exercises the early-return guard at `src/wingdi_dc.cpp:46`). A full SDL-window-backed fullscreen-toggle test is out of scope for this headless test suite if no such fixture already exists — limit new coverage to what's safely testable without a real window.

Acceptance criteria:
- All three functions' documented rejection/guard branches are exercised by at least one assertion.
- New tests pass; existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not add a public header declaration for these functions — that is tracked separately (plan.md TASK-0002 / TASK-24H-0101) and must not be duplicated here.
- Do not change any of the three functions' behavior — test-only task.

---

### TASK-24H-0608: Document SelectObject's simplified NULL-return behavior for non-bitmap objects
Status: DONE — updated both the header doc comment (include/wingdi.h) and a matching code comment (src/wingdi_dc.cpp) to state the NULL-for-non-bitmap behavior explicitly. No behavior change. Verified 26/26 in all three build trees.
Priority: P3
Area: GDI
Type: Documentation
Evidence: src/wingdi_dc.cpp:77-91; include/wingdi.h:124-126; docs/audit-24h-free-api.md §4.14
Depends on: None

Problem:
`SelectObject` (`src/wingdi_dc.cpp:77-91`) returns `NULL` whenever the passed handle isn't a valid `CompatBitmap`, rather than real Win32's "no-op, return the object's current default" semantics for other GDI object kinds (pens, brushes, fonts, regions). This is acceptable because neither game ever selects anything but a bitmap into a DC, but the current header doc comment (`include/wingdi.h:125`: "Selects an internal bitmap into a memory DC; returns previously selected object.") doesn't call out this simplification, which could mislead a future maintainer into assuming closer-to-real-Win32 behavior.

Required work:
- Update the `@brief`/`@note` comment on `SelectObject` in `include/wingdi.h:124-126` to state explicitly that only bitmap objects are supported; any other (or invalid) handle returns `NULL` rather than performing a real Win32 no-op/default-object return.
- Add a matching comment at `src/wingdi_dc.cpp:77` above the function body.

Acceptance criteria:
- The header and implementation both clearly document the NULL-for-non-bitmap behavior as an intentional simplification, not an oversight.
- No behavior change.
- Existing tests still pass.

Out of scope:
- Do not implement real per-object-kind default-object tracking (pens/brushes/fonts/regions) — no evidenced need, would be general GDI scope creep.

---

### TASK-24H-0609: Add a bitmap-lifecycle leak regression test for g_diagCompatBitmaps
Status: DONE — added TestCreateBitmapRepeatedLifecycleDoesNotLeak (test_gdi_regressions.cpp), mirroring the existing DC-side test's shape: 5000 CreateBitmap/DeleteObject cycles, asserts g_diagCompatBitmaps returns to baseline and g_diagCompatBitmapsEver increases by exactly the iteration count. Verified passing (22/22 suite).
Priority: P2
Area: GDI
Type: Test
Evidence: src/internal/FreeApiGdi.cpp:42-43; src/wingdi_bitmap.cpp:87-88,105-106; tests/test_gdi_regressions.cpp:368-386 (`TestCreateCompatibleDcRepeatedLifecycleDoesNotLeak`, the equivalent existing test for the DC-side counter `g_diagCompatDcs`); plan.md TASK-0004 (tracks gating `g_diagCompatDcs`/`g_diagCompatBitmaps` behind diagnostics flags — this task is additive test coverage, not a duplicate of that fix)
Depends on: None

Problem:
`g_diagCompatDcs` has a dedicated 5000-iteration create/destroy leak regression test (`TestCreateCompatibleDcRepeatedLifecycleDoesNotLeak`, `tests/test_gdi_regressions.cpp:368-386`), but the equivalent bitmap-side counter, `g_diagCompatBitmaps` (incremented in `src/internal/FreeApiGdi.cpp:42` and `src/wingdi_bitmap.cpp:105`, decremented in `src/wingdi_bitmap.cpp:87`), is never referenced by any test in `tests/test_gdi_regressions.cpp`. There is no direct regression coverage confirming `CreateBitmap`/`DeleteObject` pairs correctly across many iterations without leaking the live-bitmap count.

Required work:
- Add a test mirroring `TestCreateCompatibleDcRepeatedLifecycleDoesNotLeak`'s shape: repeatedly `CreateBitmap`/`DeleteObject` in a loop (matching real usage counts, e.g. 500-5000 iterations) and assert `g_diagCompatBitmaps` returns to its baseline value and `g_diagCompatBitmapsEver` increases by exactly the iteration count.

Acceptance criteria:
- New test passes and would fail if a future change broke the `CreateBitmap`/`DeleteObject` counter pairing.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not gate `g_diagCompatBitmaps` behind `FreeApiDiagnosticsEnabled()`/`FreeApiDiagnosticsFastEnabled()` in this task — that behavior change belongs to plan.md TASK-0004, not this test-coverage task.

---

### TASK-24H-0610: Correct docs/supported-apis.md's stale "1bpp" CreateBitmap claim
Status: DONE — fixed as part of TASK-24H-0603's docs/supported-apis.md CreateBitmap row update (removed the incorrect "1bpp", which no branch implements and no call site uses).
Priority: P3
Area: GDI
Type: Documentation
Evidence: docs/supported-apis.md:46; src/wingdi_bitmap.cpp:97-157 (branches only for `nBitCount` 8, 16, and 32 — no 1-bit branch exists); ../planetblupi/src/decmap.cpp:578,582 (the only two `CreateBitmap` call sites in either game, using 8 and 16 respectively)
Depends on: None

Problem:
`docs/supported-apis.md:46`'s `CreateBitmap` row states: "IMPLEMENTED (1bpp/8bpp/16bpp/32bpp raw-buffer bitmap creation for the minimap)". Direct inspection of `src/wingdi_bitmap.cpp:116-153` shows only three bit-depth branches exist — 8, 16, and 32 — with anything else falling through to an "unsupported bpp" log-and-zero-fill path. There is no 1-bit branch, and no call site in either game passes `nBitCount == 1` (the only two call sites use 8 and 16). The doc row's "1bpp" claim appears to conflate `nPlanes` (always passed as `1`) with `nBitCount`, and is factually inaccurate.

Required work:
- Correct `docs/supported-apis.md:46`'s `CreateBitmap` row to read "8bpp/16bpp/32bpp" (dropping the incorrect "1bpp"), and add a short note that 8bpp is greyscale-only pending TASK-24H-0602/0603's investigation, if that task is still open at the time this is done.

Acceptance criteria:
- `docs/supported-apis.md`'s `CreateBitmap` row accurately reflects the bit depths actually implemented in `src/wingdi_bitmap.cpp`.
- No behavior change; documentation only.
- Existing tests still pass.

Out of scope:
- Do not add a real 1-bit (monochrome) `CreateBitmap` path — no evidenced call site in either game.

---

### TASK-24H-0611: Add regression test coverage for GetPixel/SetPixel's Memory-DC-kind (selected-bitmap) path
Status: DONE — added TestGetSetPixelRoundTripOnMemoryDcWithSelectedBitmap (tests/test_gdi_regressions.cpp): round-trip, untouched-pixel, and out-of-bounds safety on a Memory DC via CreateCompatibleDC+SelectObject. Verified 26/26 in all three build trees.
Priority: P3
Area: GDI
Type: Test
Evidence: src/wingdi_blit.cpp:162-167 (GetPixel Memory-DC branch), :189-193 (SetPixel Memory-DC branch); tests/test_gdi_regressions.cpp:286-301 (`TestGetSetPixelRoundTripOnSurfaceDc`, Surface-DC-kind only); confirmed via exhaustive grep of both games' source that the only real `GetPixel`/`SetPixel` call sites (`../planetblupi/src/ddutil.cpp:360-361,391`, `../planetblupi/src/pixmap.cpp:730`, `../free-eggbert/src/ddutil.cpp:289-290,314`) all route through a DirectDraw surface's `GetDC()`, which reaches free-api's Surface-DC-kind path, not the Memory-DC/selected-bitmap kind
Depends on: None

Problem:
`GetPixel`/`SetPixel` handle two distinct DC kinds — Surface (`src/wingdi_blit.cpp:157-161,184-188`) and Memory-with-selected-bitmap (`:162-167,189-193`). Only the Surface-DC-kind path has a regression test (`TestGetSetPixelRoundTripOnSurfaceDc`). A careful trace of both games' actual call sites shows both real usages go through a DirectDraw surface's `GetDC()` (bridged to free-api's Surface-DC-kind), so the Memory-DC-kind branch currently has no evidenced live game call site — this is a defensive-completeness gap, not a known-used-behavior gap.

Required work:
- Add a test that creates a memory DC via `CreateCompatibleDC`, selects a bitmap via `SelectObject`, and performs a `SetPixel`/`GetPixel` round-trip directly on that DC (not via a Surface DC), asserting correct read/write and out-of-bounds safety, mirroring `TestGetSetPixelRoundTripOnSurfaceDc`'s structure.

Acceptance criteria:
- New test exercises the Memory-DC-kind branch of both `GetPixel` and `SetPixel` and passes.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not change `GetPixel`/`SetPixel`'s behavior — test-only task.
- Do not add color-key/palette-matching behavior beyond what already exists.

---

### TASK-24H-0612: Add a safety regression test for CreateBitmap's unsupported-bit-depth fallback branch
Status: DONE — added TestCreateBitmapUnsupportedBitDepthFallsBackToZeroedBuffer (tests/test_gdi_regressions.cpp): asserts a 24bpp call succeeds, reports correct dimensions, and produces a fully zeroed pixel buffer. Verified 26/26 in all three build trees.
Priority: P3
Area: GDI
Type: Test
Evidence: src/wingdi_bitmap.cpp:151-153 (`else { SDL_Log("free-api CreateBitmap: unsupported bpp=%u, pixels zeroed", nBitCount); }`); no existing test in tests/test_gdi_regressions.cpp passes any `nBitCount` other than 8, 16, or 32
Depends on: None

Problem:
`CreateBitmap`'s fallback branch for any `nBitCount` other than 8, 16, or 32 logs a warning and leaves the pixel buffer zero-filled (already zeroed at allocation time, `src/wingdi_bitmap.cpp:111`). No test currently exercises this branch, so there is no regression guard confirming it stays crash-safe and produces a fully zeroed (not garbage) buffer if a future caller passes an unexpected bit depth.

Required work:
- Add a test calling `CreateBitmap` with an unsupported `nBitCount` (e.g. 24) and non-null `lpBits`, asserting the call succeeds (non-null `HBITMAP`), does not crash, and the resulting pixel data reads back as fully zeroed/transparent via `GetPixel` or `GetObjectA`.

Acceptance criteria:
- New test passes and would catch a future regression that leaves this branch's output undefined instead of zero-filled.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not add real decoding support for any bit depth beyond 8/16/32 — no evidenced call site needs it.

---

### TASK-24H-0613: Add regression test coverage for StretchBlt's early-rejection branches
Status: DONE — added TestStretchBltEarlyRejectionBranchesReturnFalseAndLeaveDestUntouched (test_gdi_regressions.cpp), covering all three branches: non-SRCCOPY rop (using real Win32 SRCAND's raw value, not a new #define), invalid/mismatched DC pair (Memory-kind destination; source with no bitmap selected), and degenerate zero/negative width/height on each of the four dimension args. Uses only real allocated handles of the wrong kind, never a raw invented pointer (per the documented AsCompatDC garbage-pointer segfault finding). Verified passing (22/22 suite).
Priority: P2
Area: GDI
Type: Test
Evidence: src/wingdi_blit.cpp:25-30 (non-SRCCOPY rop rejection), :34-39 (invalid/mismatched DC-pair rejection), :41-43 (degenerate zero/negative width/height rejection); grep of tests/test_gdi_regressions.cpp confirms every existing `StretchBlt` call passes `SRCCOPY` with valid DCs and positive dimensions — none of these three early-return branches are exercised
Depends on: None

Problem:
`StretchBlt` (`src/wingdi_blit.cpp:13-147`) is the highest-value blit function in the codebase (subject of the flagship fix in TASK-24H-0601) and has three defensive early-rejection branches — non-`SRCCOPY` ROP, invalid or mismatched-kind DC pair, and degenerate (zero or negative) width/height — none of which are exercised by any existing test. While neither game is known to trigger these paths today, they sit in the same function as the flagship P0 fix and deserve baseline coverage so a future refactor of the function's entry checks can't silently break them.

Required work:
- Add tests asserting `StretchBlt` returns `FALSE` and leaves the destination untouched for: (a) a non-`SRCCOPY` `rop` value, (b) a destination or source `HDC` that isn't a valid Surface/Memory-with-selected-bitmap pair (e.g. a memory DC with no bitmap selected), and (c) zero or negative `wDest`/`hDest`/`wSrc`/`hSrc`.

Acceptance criteria:
- All three rejection branches are exercised by at least one assertion each.
- New tests pass; existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not change any of `StretchBlt`'s rejection logic — test-only task.
- Do not add support for any ROP other than `SRCCOPY`.

---

### TASK-24H-0614: Verify examples/04_gdi_minimap.cpp still behaves correctly after the StretchBlt and CreateBitmap GDI fixes
Status: DONE (partial, headless-verifiable portion only) — rebuilt fresh under -DFREE_API_BUILD_EXAMPLES=ON, compiles cleanly with no warnings/errors. Ran it directly under SDL_VIDEODRIVER=dummy: it correctly prints GetDeviceCaps(SIZEPALETTE)=0 and the expected "modern TrueColor host" analysis, meaning CreateBitmap's 8-bit and 16-bit paths and both StretchBlt calls all executed successfully (no crash) before reaching its message loop. Confirmed via a background-process + /proc/status check that it then blocks in its own interactive while(g_running) loop exactly as designed (waiting for a real WM_CLOSE), not crashed or hung abnormally -- this matches examples/README.md's own "meant to be run with a real display, not headless" note. Full visual/pixel-level confirmation ("no obviously corrupted bitmap output") requires a real display and remains a human-verification item, not claimed here.
Priority: P3
Area: GDI
Type: Verification
Evidence: examples/04_gdi_minimap.cpp (consumer of `FreeApiCreateSurfaceDC`/`FreeApiDestroySurfaceDC`, per docs/audit-24h-free-api.md §3.23); depends on TASK-24H-0601's StretchBlt Y-clamp change and TASK-24H-0603's CreateBitmap 8-bit path change
Depends on: TASK-24H-0601, TASK-24H-0603

Problem:
Two GDI behavior changes land in this backlog: `StretchBlt`'s scaled-path Y-clamp fix (TASK-24H-0601) and whatever decision TASK-24H-0603 applies to `CreateBitmap`'s 8-bit path. `examples/04_gdi_minimap.cpp` is free-api's own example program exercising this exact code path (minimap-style bitmap creation and blitting) and should be re-verified to still build and produce sane output after both land, since it's the one piece of free-api-owned code (besides the test suite) that demonstrates this subsystem end-to-end.

Required work:
- After TASK-24H-0601 and TASK-24H-0603 are complete, rebuild and run `examples/04_gdi_minimap.cpp` and confirm it still compiles cleanly and produces output consistent with its documented intent (no crash, no obviously corrupted bitmap output).

Acceptance criteria:
- `examples/04_gdi_minimap.cpp` builds without warnings/errors introduced by the two dependency tasks.
- Running the example completes without crashing and produces output consistent with pre-fix behavior for all in-bounds cases (only the previously-buggy out-of-bounds/8-bit-palette cases are expected to differ).
- Existing tests still pass.

Out of scope:
- Do not modify `examples/04_gdi_minimap.cpp`'s own logic unless it directly exercises one of the two fixed behaviors and needs updating to reflect the new, correct output.

---

### TASK-24H-0615: Remove the now-dead NormalizePath function after LoadImageA migrates to NormalizeFilesystemPath
Status: DONE — TASK-24H-0605 landed this session; re-grepped src/ to confirm NormalizePath had zero remaining call sites, then removed its declaration (src/internal/FreeApiPath.hpp) and definition (src/internal/FreeApiPath.cpp). Fixed a stale reference to it in src/winapi.cpp's module-index comment. Verified: standalone build 23/23, free-eggbert and planetblupi trees + free-direct build cleanly with no dangling references.
Priority: P3
Area: GDI
Type: Cleanup
Evidence: src/internal/FreeApiPath.hpp:7; src/internal/FreeApiPath.cpp:7-16; grep confirms `NormalizePath`'s only call site anywhere in `src/` is `src/wingdi_bitmap.cpp:26` (LoadImageA)
Depends on: TASK-24H-0605

Problem:
`FreeApi::Internal::NormalizePath` (the weaker, backslash-only path normalizer) has exactly one call site in the entire codebase: `LoadImageA` at `src/wingdi_bitmap.cpp:26`. Once TASK-24H-0605 migrates that call to `NormalizeFilesystemPath`, `NormalizePath` becomes fully dead code with zero remaining callers.

Required work:
- After TASK-24H-0605 lands, remove `NormalizePath`'s declaration from `src/internal/FreeApiPath.hpp` and its definition from `src/internal/FreeApiPath.cpp`.
- Grep the full `src/` tree once more before removing to confirm no new call site was introduced in the interim.

Acceptance criteria:
- `NormalizePath` no longer exists anywhere in the codebase.
- Build succeeds with no dangling references.
- Existing tests still pass.
- No unrelated API is added or removed.

Out of scope:
- Do not touch `NormalizeFilesystemPath`, `free_api_fopen`'s internal normalization, or `NormalizeMidiPath` — this task removes exactly one now-unused internal function.
- Do not perform this removal before TASK-24H-0605 lands (it is the only thing that makes `NormalizePath` dead).

---

### TASK-24H-0616: Add rejection-path regression tests for GetObjectA's degenerate-argument guards
Status: DONE — added TestGetObjectARejectsDegenerateArguments (tests/test_gdi_regressions.cpp): all three guards (NULL handle, NULL buffer, c<=0) exercised, plus a positive control proving the same handle succeeds with valid arguments. Verified 26/26 in all three build trees.
Priority: P3
Area: GDI
Type: Test
Evidence: src/wingdi_bitmap.cpp:51-60 (`if (!h || !pv || c <= 0) { return 0; }`); grep of tests/test_gdi_regressions.cpp confirms every existing `GetObjectA` call passes a valid handle, non-null buffer, and `c == sizeof(BITMAP)` — none of the three guard conditions are exercised
Depends on: None

Problem:
`GetObjectA` (`src/wingdi_bitmap.cpp:51-74`) has three degenerate-argument guards (`h == NULL`, `pv == NULL`, `c <= 0`) that all return `0` safely, none of which are exercised by any existing test. Both games always call this with a valid, just-created bitmap handle and a full-sized buffer, so this is purely defensive-completeness coverage, not a known-used-behavior gap.

Required work:
- Add tests asserting `GetObjectA` returns `0` and does not crash for: a `NULL` handle, a `NULL` output buffer, and `c == 0` (or negative).

Acceptance criteria:
- All three guard branches are exercised by at least one assertion each.
- New tests pass; existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not change `GetObjectA`'s behavior — test-only task.
- Do not add support for object kinds other than bitmaps.

## Files and Paths

### TASK-24H-0701: Resolve `_findfirst` session-table leak with exact confirmed mechanics (sharper than TASK-0009)
Status: DONE — chose approach (a): `_findnext` auto-erases the session on exhaustion (src/crt_io.cpp). A subsequent `_findnext`/`_findclose` on the now-erased handle safely returns -1, matching real Win32's unknown-handle contract; no crash. 17/17 in all three build modes.
Priority: P1
Area: Files
Type: Bugfix
Evidence: src/crt_io.cpp:44-138 (`FindSession`, `g_findSessions`, `g_findSessionsMutex`, `g_nextFindHandle`); ../free-eggbert/src/event.cpp:4741-4747; plan.md TASK-0009 (superseded by this task); docs/audit-24h-free-api.md §4.23, §5 item 3
Depends on: None

Problem:
This session traced the exact mechanics: `_findfirst` (line 63) pre-computes the full `session.matches` list and inserts it into `g_findSessions` under a strictly-incrementing `g_nextFindHandle`. `_findnext` (line 103) checks `session.index >= session.matches.size()` on exhaustion and returns -1 but does not erase the map entry (only `_findclose`, line 121, calls `g_findSessions.erase`). free-eggbert's design-mission file picker (`event.cpp:4741-4747`) calls `_findfirst`/`_findnext` in a fully-draining `do { ... } while (_findnext(...) == 0 ...)` loop and never calls `_findclose` anywhere in the codebase (confirmed via exhaustive grep: zero `_findclose` call sites in free-eggbert). Every visit to that screen leaks exactly one `FindSession` entry, permanently, for the process lifetime. Bounded (not a hot path) but real, and unbounded over a long play session.

Required work:
- Pick one of the following two approaches and implement it (do not do both):
  - (a) Auto-erase the session from `g_findSessions` the moment `_findnext` (or `_findfirst` itself, for a zero-match session) detects exhaustion, since a real caller has no further use for a handle that can never return another match. Verify this doesn't break a caller that calls `_findnext` again after exhaustion (real Win32 returns -1 again, harmlessly, on a missing handle) or that calls `_findclose` on an already-auto-erased handle (must still return a harmless -1, not crash).
  - (b) Leave real Win32-matching semantics (caller must call `_findclose`) exactly as-is, and instead only document this as a known, low-severity, game-side (not free-api) resource-management gap in `docs/out-of-scope.md` and `docs/supported-apis.md`.
- Whichever choice is made, update `tests/test_file_paths.cpp`'s drain-without-close regression test (TASK-24H-0702) so its assertion matches the chosen behavior.

Acceptance criteria:
- The chosen approach is verified against free-eggbert's actual call pattern (`event.cpp:4741-4747`) exactly as traced above.
- TASK-24H-0702's test passes and its assertion reflects the final chosen behavior (table stays bounded for choice (a), or the known growth is explicitly locked in as documented behavior for choice (b)).
- Existing tests still pass, including `test_file_paths.cpp`'s existing happy-path `_findclose` tests.
- No unrelated API is added.

Out of scope:
- Do not implement a general Windows handle-leak detector or handle-table GC framework.
- Do not change `_findfirst`/`_findnext`'s wildcard-matching behavior (`MatchesSimpleWildcard`) — this task is scoped strictly to session lifecycle, not matching semantics.

---

### TASK-24H-0702: Add regression test proving repeated drain-without-`_findclose` cycles grow `g_findSessions`
Status: DONE — since TASK-24H-0701 landed the auto-erase fix rather than leaving the leak in place, this test proves the fix instead of characterizing the pre-fix leak: `TestFindFirstFindNextRepeatedDrainWithoutCloseDoesNotLeakSession` (tests/test_file_paths.cpp) runs 5 drain-without-close cycles and asserts each session is already auto-erased by the time a post-drain `_findclose` is attempted (returns -1, "nothing to close"), proven indirectly since `g_findSessions` is file-local.
Priority: P1
Area: Files
Type: Test
Evidence: src/crt_io.cpp:44-138; tests/test_file_paths.cpp:219-296; docs/audit-24h-free-api.md §4.23
Depends on: None

Problem:
`tests/test_file_paths.cpp` has exactly two `_findfirst`-related tests, and both always call `_findclose` (line ~272) after draining — the leak-inducing scenario (drain via `_findnext` to exhaustion, then never call `_findclose`, repeated across multiple sessions) is confirmed not exercised by any existing test. Without this test, TASK-24H-0701's fix-or-document decision has no automated way to detect a regression in either direction.

Required work:
- Add a new test to `tests/test_file_paths.cpp` that: creates a small fixture directory with a couple of matching files, calls `_findfirst`/`_findnext` in a fully-draining loop matching free-eggbert's exact shape (`event.cpp:4741-4747`) N times (e.g. 5) without ever calling `_findclose`, and asserts on the resulting handle numbering/table growth (e.g. successive `_findfirst` calls return strictly increasing handles, proving each prior session's slot was never reclaimed) to characterize current behavior precisely.
- Keep the test isolated from the other `_findfirst` tests in the file (own fixture directory, own cleanup of files it creates, independent of session-table cleanup which is the very thing under test).

Acceptance criteria:
- The new test passes against current (pre-TASK-24H-0701) behavior, explicitly demonstrating the leak's exact confirmed shape.
- Once TASK-24H-0701 lands, this test's assertion is updated to match whichever behavior was chosen (bounded reuse for auto-erase, or continued growth for document-only) rather than deleted.
- Existing `test_file_paths.cpp` tests still pass unmodified otherwise.
- No unrelated API is added.

Out of scope:
- Do not assert on `g_findSessions`'s internal size directly (it's file-local/anonymous-namespace) — test only through the public `_findfirst`/`_findnext`/`_findclose` handle contract.
- Do not add a general handle-leak-detection test harness beyond this one scenario.

---

### TASK-24H-0703: Add parity/characterization tests across all four path-normalization implementations before any consolidation
Status: DONE — NOTE: only 3 implementations remain, not 4 (NormalizePath was removed as dead code this cycle, TASK-24H-0615, after LoadImageA migrated to NormalizeFilesystemPath, TASK-24H-0605). Added TestNormalizeFilesystemPathCharacterization (tests/test_file_paths.cpp): direct, table-driven test of NormalizeFilesystemPath covering drive-letter-strip, backslash-convert, leading-slash-strip, plain-passthrough, and null-input. Added TestFreeApiFopenPrefixNormalizationCharacterization: proves free_api_fopen's prefix-normalization currently produces the identical relative path NormalizeFilesystemPath does for the same drive-letter+backslash shape. NormalizeMidiPath (src/MidiMusic.cpp) has file-local `static` linkage and can't be forward-declared without exposing a new internal symbol purely for test access -- its progressive-suffix-uppercase fallback already has real indirect characterization via test_mci_sequences.cpp's TestMidiOpenFindsUppercaseFixtureViaLowercaseName, which is left as its existing coverage. Verified passing (23/23 suite).
Priority: P2
Area: Files
Type: Test
Evidence: src/internal/FreeApiPath.cpp:7-43; include/windows.h:140-176; src/MidiMusic.cpp:219-275; docs/audit-24h-free-api.md §4.20, §5 item 20
Depends on: None

Problem:
Four independent, non-shared path-normalization implementations exist — `FreeApi::Internal::NormalizePath` (backslash-only, `FreeApiPath.cpp:7-16`), `FreeApi::Internal::NormalizeFilesystemPath` (drive-letter-strip + backslash + leading-slash strip, `FreeApiPath.cpp:18-43`), `free_api_fopen`'s inline reimplementation (`include/windows.h:140-176`, plus a case-insensitive-basename fallback the other two lack), and `NormalizeMidiPath` (`src/MidiMusic.cpp:219-275`, a different progressive-suffix-uppercase fallback strategy). None call into each other. Before any consolidation refactor (TASK-24H-0704/0705/0706) touches this logic, there is no existing test suite that pins down each implementation's exact current input/output behavior side-by-side, so a refactor would have no reliable before/after equivalence check.

Required work:
- Add a focused test file (or extend `tests/test_file_paths.cpp`) with a table of representative input paths (drive-letter-prefixed, backslash-mixed, leading-slash-rooted, plain relative, already-correct) exercised against each of the four current implementations independently, asserting their current documented outputs exactly as they behave today.
- Include at least one case per implementation's unique behavior (e.g. `NormalizePath` leaving a leading slash intact where `NormalizeFilesystemPath` strips it; `free_api_fopen`'s basename-uppercase fallback; `NormalizeMidiPath`'s progressive-suffix fallback).

Acceptance criteria:
- The new tests pass against current, unmodified behavior of all four implementations.
- These tests are referenced by name in TASK-24H-0704/0705/0706 as the before/after equivalence proof for those refactors.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not modify any of the four implementations in this task — characterization only.
- Do not attempt to unify the fallback strategies (case-insensitive-basename vs. progressive-suffix) into one shared algorithm here — that's explicitly deferred to later, narrower tasks if ever pursued.

---

### TASK-24H-0704: Extract a shared core from `NormalizePath` and `NormalizeFilesystemPath`
Status: OBSOLETE — superseded by TASK-24H-0615 (this cycle): `NormalizePath` was removed entirely as dead code once `LoadImageA` (its only caller) migrated to `NormalizeFilesystemPath` (`TASK-24H-0605`). This task's premise ("extract a shared core from NormalizePath and NormalizeFilesystemPath") is no longer possible or meaningful — `src/internal/FreeApiPath.cpp` now has exactly one normalization function, not two, so there is nothing left to unify at that specific site. The remaining, still-real duplication is between `NormalizeFilesystemPath` and the other two implementations (`free_api_fopen`, `NormalizeMidiPath`) — see the re-scoped `TASK-24H-0705`/`0706` below, which now describe migrating those two callers to reuse `NormalizeFilesystemPath` directly (via a forward declaration) instead of via a separate new "shared core" symbol this task would have introduced.
Priority: P2
Area: Files
Type: Refactor
Evidence: src/internal/FreeApiPath.cpp:7-43; src/internal/FreeApiPath.hpp; docs/audit-24h-free-api.md §4.20, §4.15, §5 item 20
Depends on: TASK-24H-0703

Problem:
`FreeApi::Internal::NormalizePath` (backslash-only, used only by `LoadImageA`) and `FreeApi::Internal::NormalizeFilesystemPath` (full drive-letter-strip + backslash + leading-slash strip, used by most file I/O) live side-by-side in the same file with materially overlapping logic (both do the backslash-to-forward-slash pass) but do not share an implementation — a future fix to the backslash pass would need to be applied twice.

Required work:
- Introduce one shared core normalization function taking explicit opt-in flags (e.g. strip-drive-letter, strip-leading-slashes) and reimplement both `NormalizePath` and `NormalizeFilesystemPath` as thin callers of it with the flag combination that reproduces their exact current behavior.
- Keep both existing public function signatures and their call sites (`LoadImageA` and all `NormalizeFilesystemPath` callers) completely unchanged — this is an internal-only refactor.

Acceptance criteria:
- TASK-24H-0703's parity tests pass unmodified before and after this refactor, proving zero behavioral change.
- `NormalizePath` and `NormalizeFilesystemPath` remain the only two symbols any external caller uses; the new shared core is an implementation detail (not exported beyond `FreeApiPath.hpp`/`.cpp`, or kept anonymous-namespace/internal if reasonable).
- Existing tests (`test_file_paths.cpp`, `test_file_regressions.cpp`, `test_gdi_regressions.cpp`) still pass.
- No unrelated API is added.

Out of scope:
- Do not touch `free_api_fopen` (`include/windows.h`) or `NormalizeMidiPath` (`src/MidiMusic.cpp`) in this task — see TASK-24H-0705/0706.
- Do not add wildcard, UNC-path, or relative-`..`-resolution handling — none is evidenced by either game.

---

### TASK-24H-0705: Migrate `free_api_fopen`'s prefix-normalization to the shared core
Status: DONE — NOTE re-scoped: since TASK-24H-0704 became obsolete (NormalizePath removed, see that entry), this migrates free_api_fopen directly to NormalizeFilesystemPath itself (via a forward declaration in include/windows.h) rather than a separate new "shared core" symbol. Replaced free_api_fopen's hand-rolled drive-letter-strip/backslash-convert/leading-slash-strip block (include/windows.h) with a single NormalizeFilesystemPath(path) call; the case-insensitive-basename fallback logic is untouched. Confirmed no new link dependency: both target games' own executables (SPEEDY_BLUPI_WINDOWS, PLANET_BLUPI_WINDOWS) already always link free-api and both built/linked cleanly. Verified: TASK-24H-0703's new characterization tests pass unmodified; full 23-test suite passes in all three build trees (standalone, free-eggbert, planetblupi).
Priority: P2
Area: Files
Type: Refactor
Evidence: include/windows.h:140-176; docs/audit-24h-free-api.md §4.20
Depends on: TASK-24H-0704

Problem:
`free_api_fopen` (`include/windows.h:140-176`) reimplements the same drive-letter-strip + backslash-to-forward-slash + leading-slash-strip prefix logic as the shared core introduced in TASK-24H-0704, independently, inline in a header. Its unique case-insensitive-basename-uppercase fallback (lines ~160-172) is a genuinely different behavior from the other implementations and must be preserved as-is.

Required work:
- Replace `free_api_fopen`'s hand-rolled prefix-stripping block with a call into the shared core function from TASK-24H-0704, keeping its case-insensitive-basename fallback logic unchanged.
- Verify the header-only `static inline` function can call the shared core without introducing a new link dependency for any translation unit that only includes `<windows.h>` (the free-api static/shared library is already linked by every consumer; confirm this holds, or keep the prefix logic duplicated with an explanatory comment if it genuinely cannot be shared without a new link requirement).

Acceptance criteria:
- TASK-24H-0703's parity tests covering `free_api_fopen`'s behavior pass unmodified before and after.
- `tests/test_file_paths.cpp`'s existing `fopen`-related tests (case-insensitive fallback, backslash path shapes) still pass.
- No unrelated API is added.

Out of scope:
- Do not change the case-insensitive-basename fallback algorithm itself.
- Do not remove the `#define fopen free_api_fopen` macro redirection or its documented narrow-scope justification.

---

### TASK-24H-0706: Migrate `NormalizeMidiPath`'s prefix-normalization to the shared core
Status: DONE -- `NormalizeMidiPath`'s Step 1 (src/MidiMusic.cpp) now calls `FreeApi::Internal::NormalizeFilesystemPath` directly (via a new `#include "internal/FreeApiPath.hpp"`) instead of its own hand-rolled backslash-only loop, matching TASK-24H-0705's established precedent for `free_api_fopen`. Steps 2/3 (exists-check, progressive-suffix-uppercase fallback) left entirely unchanged. **Risk found and resolved before implementing, not assumed safe by analogy alone:** `NormalizeFilesystemPath` does more than the old loop -- it also strips a leading drive-letter prefix (never produced by either game's MIDI-path construction; confirmed safe no-op) and strips leading slashes (relevant only for an ABSOLUTE input path). Traced both games' real MCI_OPEN element-name construction (`CSound::PlayMusic`, `GetCurrentDir()+strcat()`; `GetCurrentDir` derives from `_pgmptr`, which on the non-Windows path both games actually run on is set directly to `argv[0]`) and confirmed this project's own documented invocation (`docs/target-game-verification.md`: `./bin/SPEEDY_BLUPI_WINDOWS`, `./bin/PLANET_BLUPI_WINDOWS`) is always relative -- so `argv[0]`/`_pgmptr`/the resulting MIDI path are relative in the documented, tested usage, and the leading-slash-strip is a no-op there, never observed to change behavior. Documented this reasoning directly in `NormalizeMidiPath`'s doc comment for future readers, since an absolute-path invocation (undocumented/unsupported) would behave differently than before this change -- an accepted trade-off matching TASK-24H-0705's identical, already-running precedent for `free_api_fopen`, not a newly-introduced one. Verified 28/28 passing in standalone build/, build-asan/ (FREE_API_SANITIZE=address), ../free-eggbert/cmake-build-debug (Ninja), ../planetblupi/build (Make) -- in particular `test_mci_sequences.cpp`'s exact-match and uppercase-suffix-fallback tests (`TestMidiOpenFindsUppercaseFixtureViaLowercaseName`), confirmed still passing and confirmed the underlying assertions actually ran (not silently skipped) via direct log inspection. FREE_API_SANITIZE=thread not re-run -- no threading/locking touched.
Priority: P2
Area: Files
Type: Refactor
Evidence: src/MidiMusic.cpp:219-275; docs/audit-24h-free-api.md §4.20
Depends on: TASK-24H-0704

Problem:
`NormalizeMidiPath` (`src/MidiMusic.cpp:219-275`) reimplements its own backslash-to-forward-slash conversion (step 1, lines ~228-232) independently of the shared core introduced in TASK-24H-0704, before applying its own unique progressive-suffix-uppercase fallback search (step 3, lines ~238-263), which must be preserved exactly as-is.

Required work:
- Replace `NormalizeMidiPath`'s manual backslash-conversion loop with a call into the shared core function (backslash-only mode, matching `NormalizePath`'s flag combination) from TASK-24H-0704.
- Leave the "try as-is" existence check and the progressive-suffix-uppercase fallback search entirely unchanged.

Acceptance criteria:
- TASK-24H-0703's parity tests covering `NormalizeMidiPath`'s behavior pass unmodified before and after.
- `test_mci_sequences.cpp` and any other MIDI-path-dependent tests still pass.
- No unrelated API is added.

Out of scope:
- Do not change the progressive-suffix-uppercase fallback algorithm.
- Do not attempt to unify this fallback with `free_api_fopen`'s different basename-only fallback — they are genuinely different algorithms serving different call sites.

---

### TASK-24H-0707: Document `_lopen`/`_lread`/`_lclose`'s unsynchronized handle table as a known limitation
Status: DONE — added a code comment above g_openFiles/g_nextFileHandle (src/winbase_file.cpp) and a matching note to docs/supported-apis.md's row. Re-confirmed both games' only call sites are single-threaded ddutil.cpp chains. No behavior change. Verified 26/26 in all three build trees.
Priority: P3
Area: Files
Type: Documentation
Evidence: src/winbase_file.cpp:29-30,37-85; docs/audit-24h-free-api.md §4.21
Depends on: None

Problem:
`g_openFiles`/`g_nextFileHandle` (`src/winbase_file.cpp:29-30`) have no mutex protecting them, unlike the equivalent `_findfirst` session table (`g_findSessionsMutex`, `src/crt_io.cpp:49`). This is a latent data race if `_lopen`/`_lread`/`_lclose` were ever called concurrently from multiple threads. Neither game is multi-threaded around file I/O (confirmed by this session's evidence read of both games' call sites), so this is theoretical today, not an observed defect.

Required work:
- Add a short, explicit code comment above `g_openFiles`/`g_nextFileHandle` in `src/winbase_file.cpp` noting the lack of synchronization and that it is intentional/deferred because no evidenced caller uses these functions from more than one thread.
- Add the same note to `docs/supported-apis.md` and/or `docs/out-of-scope.md`'s `_lopen`/`_lread`/`_lclose` entry.

Acceptance criteria:
- The known limitation is documented in both the source comment and at least one `docs/*.md` file.
- No code behavior changes.
- Existing tests still pass.

Out of scope:
- Do not add a mutex or any other locking to `g_openFiles`/`g_nextFileHandle` — there is no evidence either game needs it, and adding it would be unneeded defensive overhead per project policy.
- Do not add thread-safety to any other subsystem as part of this task.

---

### TASK-24H-0708: Add a regression test locking in the leading-backslash filesystem-root-escape fix
Status: DONE — TestMkdirBareBackslashUserLiteralStaysRelativeToCwd (tests/test_file_regressions.cpp) chdir()s into a disposable temp root and calls _mkdir("\User") with the exact bare literal free-eggbert uses, asserting it lands relative to CWD and confirming nothing was created at the real /User.
Priority: P1
Area: Files
Type: Test
Evidence: src/internal/FreeApiPath.cpp:18-43; src/crt_direct.cpp:28-39; src/winbase_file.cpp:97-129; tests/test_file_regressions.cpp:84-136; ../free-eggbert/src/event.cpp:4193
Depends on: None

Problem:
`NormalizeFilesystemPath` strips leading slashes after backslash-conversion specifically so a Windows-rooted path like `\User` (free-eggbert's real call shape, `event.cpp:4193`, `_mkdir("\User")`) is treated as relative to the current working directory instead of escaping to the real filesystem root. This session confirmed the fix still holds by reading the source, but `tests/test_file_regressions.cpp`'s existing backslash tests (`TestCreateDirectoryAWithBackslashPath`, `TestMkdirWithBackslashPath`, lines 84-136) all prepend an already-absolute temp-root path before the backslash segment (e.g. `root + "\\User"`) — none of them pass a bare rooted literal like `"\User"` with nothing prepended, so none actually prove the path doesn't resolve to the real `/User` on disk.

Required work:
- Add a test that changes the working directory to a disposable temp root, then calls `_mkdir("\User")` (or `CreateDirectoryA`) with the exact bare literal free-eggbert uses, and asserts the resulting directory exists at `<temp-root>/User` — and, if feasible in the test environment, asserts nothing was created at the real filesystem root `/User`.

Acceptance criteria:
- The new test fails if `NormalizeFilesystemPath`'s leading-slash-stripping loop (`FreeApiPath.cpp:38-40`) is ever removed or altered to preserve a leading slash.
- The new test passes against current behavior.
- Existing `test_file_regressions.cpp` tests still pass.
- No unrelated API is added.

Out of scope:
- Do not add general absolute-path support or a configurable sandbox-root feature — this task only locks in the existing, already-implemented escape prevention.
- Do not modify `NormalizeFilesystemPath` itself.

---

### TASK-24H-0709: Strengthen `CreateDirectoryA`'s already-exists test to assert `FALSE`/`ERROR_ALREADY_EXISTS`
Status: DONE — RE-SCOPED: this task's premise was wrong. SDL_CreateDirectory itself reports success for an already-existing path (SDL_filesystem.h), so CreateDirectoryA's own ERROR_ALREADY_EXISTS branch (src/winbase_file.cpp) is unreachable in practice; actual current behavior is idempotent TRUE, not FALSE/ERROR_ALREADY_EXISTS. Confirmed via direct test run (the originally-planned assertions failed against real behavior). Confirmed harmless: neither game checks CreateDirectoryA's return value (both call it fire-and-forget: free-eggbert/src/misc.cpp:184, planetblupi/src/misc.cpp:220). Strengthened TestCreateDirectoryACreatesRealDirectory (tests/test_file_regressions.cpp) to assert the actual idempotent-TRUE behavior instead, and documented the Win32-semantics deviation in docs/out-of-scope.md ("CreateDirectoryA's already-exists return value"). Did not change CreateDirectoryA's implementation, per this task's own out-of-scope clause. Verified 24/24 standalone.
Priority: P2
Area: Files
Type: Test
Evidence: src/winbase_file.cpp:97-129; tests/test_file_regressions.cpp:62-81
Depends on: None

Problem:
`CreateDirectoryA` correctly distinguishes "already exists" (via a `std::filesystem::exists` check that sets `ERROR_ALREADY_EXISTS` and returns `FALSE`, `src/winbase_file.cpp:118-122`) from a real failure, matching real Win32 semantics. However, `tests/test_file_regressions.cpp`'s `TestCreateDirectoryACreatesRealDirectory` (lines 62-81) calls `CreateDirectoryA` a second time on the same directory and explicitly discards the return value (`(void)createdAgain;`, line 76) rather than asserting on it, so this correct behavior is not actually locked in by any test.

Required work:
- Update `TestCreateDirectoryACreatesRealDirectory` (or add a new adjacent test) to assert the second call returns `FALSE` and `GetLastError() == ERROR_ALREADY_EXISTS`, instead of discarding the return value.

Acceptance criteria:
- The strengthened test fails if `CreateDirectoryA`'s already-exists branch (`src/winbase_file.cpp:118-122`) ever regresses to returning `TRUE` or a different error code.
- The test passes against current behavior.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not change `CreateDirectoryA`'s implementation — this task only strengthens an existing test's assertions.

---

### TASK-24H-0710: Document `RemoveDirectoryA`'s zero evidenced call sites and zero test coverage
Status: DONE — added a new row to docs/supported-apis.md (previously had no row at all). Confirmed its only exerciser (tests/basic_test.cpp) is inside #ifdef _WIN32 blocks that never compile on Linux. Verified 26/26 in all three build trees.
Priority: P3
Area: Files
Type: Documentation
Evidence: src/winbase_file.cpp:131-137; docs/supported-apis.md
Depends on: None

Problem:
`RemoveDirectoryA` is implemented (`src/winbase_file.cpp:131-137`, routed through `NormalizeFilesystemPath`) but has zero call sites in either `../free-eggbert` or `../planetblupi` (confirmed via grep — the only references anywhere are the implementation itself, its header declaration, and Windows-only (`#ifdef _WIN32`) cleanup code in `tests/basic_test.cpp`, which never runs in this project's Linux-only CI). On the platform this project actually tests, `RemoveDirectoryA`'s implementation itself is exercised by no test at all.

Required work:
- Add a row (or update the existing row) for `RemoveDirectoryA` in `docs/supported-apis.md` explicitly noting: no evidenced call site in either game, kept for API completeness (mirrors `CreateDirectoryA`/`DeleteFileA`), and its only current exerciser is Windows-only test cleanup code that doesn't run on Linux CI.

Acceptance criteria:
- `docs/supported-apis.md` accurately reflects `RemoveDirectoryA`'s real usage and test-coverage status.
- No behavior change; documentation only.
- Existing tests still pass.

Out of scope:
- Do not remove `RemoveDirectoryA` — it stays per the project's keep+document default for evidenced-safe, unused-but-plausible API surface.
- Do not add a real Linux-side test for it unless a future evidenced call site appears — this task is documentation only.

---

### TASK-24H-0711: Document that planetblupi has zero find-session API usage and free-eggbert is the sole consumer
Status: DONE — folded into the docs/supported-apis.md `_findfirst` row update alongside TASK-24H-0117/0712/0804.
Priority: P3
Area: Files
Type: Documentation
Evidence: src/crt_io.cpp:44-138; ../planetblupi/src/decio.cpp:112,198,339; ../free-eggbert/src/event.cpp:4741-4747; docs/supported-apis.md
Depends on: None

Problem:
This session confirmed planetblupi has zero `_findfirst`/`_findnext`/`_findclose` call sites anywhere — all of its data-file access uses deterministically constructed filenames (e.g. `sprintf(filename, "data/world%.3d.blp", rank);` in `decio.cpp:112,198,339`). free-eggbert's design-mission picker (`event.cpp:4741-4747`) is confirmed as the *only* consumer of the find-session API across both target games. `docs/supported-apis.md`'s `_findfirst`/`_findnext`/`_findclose` row currently lists usage as "Yes" for free-eggbert but doesn't state the planetblupi side explicitly as a confirmed zero (as opposed to simply unlisted).

Required work:
- Update `docs/supported-apis.md`'s `_findfirst`/`_findnext`/`_findclose` row (and/or `docs/out-of-scope.md`) to explicitly state planetblupi has zero find-session API usage (all file access via deterministic filenames) and that free-eggbert's design-mission picker is the sole consumer across both games.

Acceptance criteria:
- `docs/supported-apis.md` and/or `docs/out-of-scope.md` explicitly reflects this confirmed usage split.
- No behavior change; documentation only.
- Existing tests still pass.

Out of scope:
- Do not add any wildcard or enumeration behavior beyond what free-eggbert already uses.
- Do not imply planetblupi needs or will get find-session support — it does not and should not.

---

### TASK-24H-0712: Add the `_findfirst` leak caveat to `docs/supported-apis.md`'s status table
Status: DONE — leak is now fixed (TASK-24H-0701), not merely documented; docs/supported-apis.md's `_findfirst` row updated. Duplicate of TASK-24H-0117/0804, closed together.
Priority: P3
Area: Files
Type: Documentation
Evidence: docs/supported-apis.md; docs/audit-24h-free-api.md §5 item 22; src/crt_io.cpp:44-138
Depends on: None

Problem:
`docs/supported-apis.md`'s `_findfirst`/`_findnext`/`_findclose` row currently only says "IMPLEMENTED (real `std::filesystem`-backed directory listing, scoped to a directory + simple `*.ext` wildcard)" with no mention of the confirmed session-table leak when a caller drains without calling `_findclose` (§4.23, TASK-24H-0701/0702). NOTE: overlaps with TASK-24H-0117 (Scope theme) — implement once, close both.

Required work:
- Add a short caveat to that row (or an adjacent footnote) noting the confirmed leak behavior and cross-referencing TASK-24H-0701 for its fix-or-document status.

Acceptance criteria:
- The row accurately reflects the leak caveat as of this task's completion.
- If TASK-24H-0701 later changes the underlying behavior, this row is updated again as part of that task's own acceptance criteria (not blocked on this task).
- No behavior change; documentation only.
- Existing tests still pass.

Out of scope:
- Do not preempt TASK-24H-0701's fix-or-document decision — this task only documents the currently-confirmed behavior.

---

### TASK-24H-0713: Deduplicate verbatim `_lopen`/`_lread`/`_lclose` declarations between `io.h` and `winbase.h`
Status: DONE — duplicate of TASK-24H-0103; implemented once there (see that entry).
Priority: P2
Area: Files
Type: Cleanup
Evidence: include/io.h:58-62; include/winbase.h:81-89; docs/audit-24h-free-api.md §3.7
Depends on: None

Problem:
`_lopen`/`_lread`/`_lclose` are declared verbatim in both `include/io.h` (lines 58-62) and `include/winbase.h` (lines 81-89), independently. Both headers currently agree exactly, so this compiles fine today, but it's a real drift risk. NOTE: this task duplicates TASK-24H-0103 (Scope theme) — implement once, close both.

Required work:
- Pick one header as canonical for these three declarations (real Win32/MSVC historically declares them in `<io.h>`) and have the other header either include it or drop its duplicate declaration, so there is exactly one textual declaration site.

Acceptance criteria:
- `_lopen`/`_lread`/`_lclose` are declared in exactly one place in the public header set (directly or via include), with identical resulting visibility to both `<io.h>`-only and `<winbase.h>`-only includers as today.
- `test_header_compile.cpp` and all other existing tests still pass.
- No unrelated API is added or removed.

Out of scope:
- Do not change the three functions' signatures or implementation (`src/winbase_file.cpp:37-85`).
- Do not consolidate any other duplicated declarations beyond these three as part of this task.

---

### TASK-24H-0714: Add regression test coverage for `DeleteFileA`
Status: DONE — TestDeleteFileARemovesRealFileAndFailsSafelyForMissingFile (tests/test_file_regressions.cpp) covers the real-delete and already-deleted/nonexistent-file cases.
Priority: P1
Area: Files
Type: Test
Evidence: src/winbase_file.cpp:87-95; ../free-eggbert/src/event.cpp:5170; docs/audit-24h-free-api.md §2.8
Depends on: None

Problem:
`DeleteFileA` (`src/winbase_file.cpp:87-95`, routed through `NormalizeFilesystemPath`) has a real, evidenced call site in free-eggbert (`event.cpp:5170`, `DeleteFileA("data/demo.3d.blp")`, called before re-recording a demo file) but has zero test coverage anywhere in `tests/` — confirmed via grep, no test file references `DeleteFileA` at all.

Required work:
- Add a test to `tests/test_file_regressions.cpp` that creates a fixture file at a relative path matching free-eggbert's shape, calls `DeleteFileA` on it, and asserts the file no longer exists on disk and the call returns `TRUE`.
- Add a companion assertion that calling `DeleteFileA` on a nonexistent file returns `FALSE` without crashing.

Acceptance criteria:
- Both new assertions pass against current behavior.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not add recursive-delete, wildcard-delete, or directory-delete-via-`DeleteFileA` support — none is evidenced by either game.
- Do not modify `DeleteFileA`'s implementation.

## Resources and Strings

### TASK-24H-0801: Add negative-path test proving `REQUIRE_STRINGS`/`VERIFY_ID` actually fail configure on broken input
Status: DONE — new CTest tests (CMakeLists.txt) using a small fixture (cmake/test-fixtures/minimal-stringtable.rc + minimal-resource.h) and WILL_FAIL: extract_string_table_require_strings_fails_on_missing_rc, extract_string_table_verify_id_fails_on_text_mismatch. A positive-control test (extract_string_table_fixture_positive_control) proves the fixture itself is valid. All confirmed failing at the exact expected FATAL_ERROR line via `ctest -VV`. 22/22 in all three build modes.
Priority: P1
Area: Resources
Type: Test
Evidence: cmake/ExtractStringTable.cmake:23-34,55-67,153-163; docs/audit-24h-free-api.md §4.24; precedent pattern at CMakeLists.txt:351-352 (`check_no_hardcoded_paths` running `${CMAKE_COMMAND} -P` against a standalone `.cmake` script)
Depends on: None

Problem:
`cmake/ExtractStringTable.cmake`'s `REQUIRE_STRINGS` (fails configure if a target game's `.rc` is missing or yields zero strings) and `VERIFY_ID`/`VERIFY_TEXT` (fails configure if a known sentinel ID doesn't resolve to the expected text) are the two gates that keep a broken STRINGTABLE extraction from silently degrading to placeholder text in shipped target-game UI. Their correctness is currently trusted by code-reading only — no test actually feeds them broken input and confirms they hard-fail; a future edit to the script's parsing/branching logic could silently defang either gate with nothing to catch it.

Required work:
- Add a CTest (following the existing `${CMAKE_COMMAND} -P` pattern already used for `cmake/CheckNoHardcodedPaths.cmake`) that invokes `cmake/ExtractStringTable.cmake` standalone against a deliberately-broken fixture: one run with `REQUIRE_STRINGS=TRUE` and a missing/empty `.rc`, one run with a real `.rc` but a wrong `VERIFY_ID`/`VERIFY_TEXT` pair.
- Assert both runs fail (non-zero exit / `FATAL_ERROR`), e.g. via CTest's `WILL_FAIL` property or a small wrapper checking the exit code.

Acceptance criteria:
- The new negative-path test(s) fail as expected against the broken fixtures and would themselves fail (i.e. catch a regression) if `REQUIRE_STRINGS`/`VERIFY_ID` were ever accidentally softened.
- The existing real target-game configures (free-eggbert, planetblupi) are unaffected and still succeed.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not change `ExtractStringTable.cmake`'s actual gating behavior or logic.
- Do not soften `REQUIRE_STRINGS`/`VERIFY_ID` — this task only adds coverage proving they still work.
- Do not build a general `.rc`/`.res` compiler test harness beyond this narrow STRINGTABLE extractor's own gates.

---

### TASK-24H-0802: Add negative-path test proving `USED_IDS_FILE`'s missing-ID and duplicate-ID gates actually fail configure
Status: DONE — new CTest tests (CMakeLists.txt) using the same fixture plus cmake/test-fixtures/used-ids-missing.txt and used-ids-duplicate.txt: extract_string_table_used_ids_fails_on_missing_id, extract_string_table_used_ids_fails_on_duplicate_id. Both confirmed failing at the exact expected FATAL_ERROR line. 22/22 in all three build modes.
Priority: P1
Area: Resources
Type: Test
Evidence: cmake/ExtractStringTable.cmake:167-241 (missing-ID check at :239, duplicate-ID check at :234); docs/used-string-ids.md "Duplicate-ID validation"; docs/audit-24h-free-api.md §4.24, §2.9
Depends on: None

Problem:
`USED_IDS_FILE` is the gate that catches one specific used string ID going missing from the extracted table (a typo'd ID, a deleted `.rc` entry a game still calls, or a parser regression on one entry) and, independently, catches a duplicate ID within a manifest (which would make the "N unique used string ID(s) verified present" count misleading). Both are `FATAL_ERROR` paths trusted by code-reading only — like the sibling `REQUIRE_STRINGS`/`VERIFY_ID` gates, nothing exercises them against actually-broken input to confirm they still fire.

Required work:
- Add a CTest that invokes `cmake/ExtractStringTable.cmake` with `USED_IDS_FILE` pointed at a small synthetic manifest listing an ID known not to be in the extracted table, and assert configure fails.
- Add a second case (or the same test) with a synthetic manifest containing a duplicated ID (bare repeat or one already covered by an `A-B` range), and assert configure fails with the duplicate-specific message.

Acceptance criteria:
- Both negative-path cases fail as expected against the synthetic manifests.
- The real `cmake/used-string-ids/free-eggbert.txt` (308 unique IDs) and `planetblupi.txt` (257 unique IDs) continue to verify successfully, unaffected.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not change `USED_IDS_FILE`'s parsing, dedup, or gating logic.
- Do not re-derive or alter the real manifests' contents.
- Do not build general `.rc`/`.res` tooling beyond this narrow check.

---

### TASK-24H-0803: Add direct unit-test coverage for `LoadResource`/`LockResource`/`SizeofResource`/`FreeResource`/`UnlockResource`'s stub contracts
Status: DONE — added TestResourceStubFunctionsReturnDocumentedContracts (tests/test_resources.cpp): direct assertions for all 5 functions' documented return contracts, including NULL-handle safety for each. Verified 24/24 standalone.
Priority: P2
Area: Resources
Type: Test
Evidence: src/winbase_file.cpp:157-186; tests/test_resources.cpp; tests/test_file_regressions.cpp:184-295; docs/audit-24h-free-api.md §4.25, §2.9
Depends on: None

Problem:
`FindResourceA`'s permanent always-miss contract is directly tested (`tests/test_resources.cpp`'s `TestFindResourceAAlwaysMisses`, `tests/test_file_regressions.cpp`'s end-to-end fallback test), but the five downstream stub functions it gates — `LoadResource` (always `NULL`), `SizeofResource` (always `0`), `LockResource` (pass-through of its input handle), `UnlockResource` (always `FALSE`), and `FreeResource` (always `FALSE`) — have zero direct test invocations anywhere confirming their individual return contracts, including null-handle safety.

Required work:
- Add direct calls to each of the five functions in `tests/test_resources.cpp`, asserting their exact documented return values (including passing `NULL`/`0` handles where applicable) match the current implementation.

Acceptance criteria:
- Each of the five functions has at least one direct, passing assertion of its return contract.
- Existing `test_resources.cpp` and `test_file_regressions.cpp` assertions are unchanged and still pass.
- No unrelated API is added.

Out of scope:
- Do not implement real resource-loading behavior behind any of these five functions.
- Do not change their signatures or return values — this is lock-in coverage for existing, correct behavior only.

---

### TASK-24H-0804: Surface the `_findfirst` session-table-leak caveat in `docs/supported-apis.md`'s status row
Status: DONE — leak is now fixed (TASK-24H-0701), not merely documented; docs/supported-apis.md's `_findfirst` row updated. Duplicate of TASK-24H-0117/0712, closed together.
Priority: P2
Area: Resources
Type: Documentation
Evidence: docs/supported-apis.md:50,60; plan.md TASK-0009; docs/audit-24h-free-api.md §5 item 22, §4.23
Depends on: None

Problem:
`docs/supported-apis.md:50`'s `_findfirst`/`_findnext`/`_findclose` row is labeled plain `IMPLEMENTED`, with no caveat — unlike `docs/supported-apis.md:60`'s `GlobalMemoryStatus` row (`PARTIAL (plausible dwTotalPhys only)`) or the `FindResourceA(RT_BITMAP)` row two lines below it, both of which use a parenthetical caveat to flag a known limitation inline. `_findfirst`'s own implementation has a confirmed session-table leak already tracked as `plan.md` `TASK-0009` and `TASK-24H-0701`, but that caveat isn't surfaced in this doc's status column at all. NOTE: overlaps with TASK-24H-0117/TASK-24H-0712 — implement once, close all three.

Required work:
- Add a short parenthetical caveat to the `_findfirst`/`_findnext`/`_findclose` row's status text (matching the existing "IMPLEMENTED (...)" caveat style already used elsewhere in this same table), noting the known session-table-leak behavior and pointing at `TASK-0009`.

Acceptance criteria:
- The row's status text reflects the caveat.
- No change to `_findfirst`/`_findnext`/`_findclose`'s actual behavior.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not fix the leak itself here — that is `TASK-0009`'s job.
- Do not duplicate `TASK-0001`'s full 28-symbol documentation-gap task; this is one row's caveat text only.

---

### TASK-24H-0805: Document planetblupi's 6 orphaned `TX_INFO_SETUP` IDs in `docs/used-string-ids.md`
Status: DONE — re-verified via grep against resource.h and all three .rc files: TX_INFO_SETUP1-5 (162-166) present, TX_INFO_SETUP6-10b (167-172) absent from .rc and source. Added the note. Verified 26/26 in all three build trees (docs-only).
Priority: P3
Area: Resources
Type: Documentation
Evidence: ../planetblupi/include/resource.h:120-130; ../planetblupi/resource/blupi-e.rc:366-370 (and blupi-d.rc/blupi-f.rc, same lines); ../planetblupi/src/event.cpp:2291-2296; docs/used-string-ids.md:29-35; docs/audit-24h-free-api.md §2.9
Depends on: None

Problem:
planetblupi's `include/resource.h:120-130` defines `TX_INFO_SETUP1` through `TX_INFO_SETUP10b` (IDs 162-172), but only `TX_INFO_SETUP1`-`TX_INFO_SETUP5` (162-166) appear in any of the three `.rc` files' `STRINGTABLE` data or in source (`src/event.cpp:2291-2296`, via `DrawTextCenter`). `TX_INFO_SETUP6` through `TX_INFO_SETUP10b` (167-172) are defined in `resource.h` but appear in neither the `.rc` files nor any code reference in any of the three languages — confirmed genuinely orphaned on the game's own side (not a free-api gap). This is the specific, previously-undocumented reason `resource.h`'s own defined-ID range doesn't line up 1:1 with the 257-unique-used-ID manifest count that `docs/used-string-ids.md` already explains in general terms.

Required work:
- Add a short note to `docs/used-string-ids.md` (near its existing "not every resource.h-defined ID is used" discussion around lines 29-35) citing these 6 specific orphaned IDs, their `resource.h` line numbers, and that they're absent from all three `.rc` files and from source — framed as a game-source characteristic for future-maintainer context, not a free-api issue.

Acceptance criteria:
- The note appears in `docs/used-string-ids.md`, citing the exact IDs and evidence.
- No behavior change.
- Existing tests still pass.

Out of scope:
- Do not modify planetblupi's own source or `resource.h` — it is a sibling project, not free-api's to change.
- Do not add any handling, extraction, or fallback logic for these unused IDs.

---

### TASK-24H-0806: Document `mciGetDeviceIDA`'s fixed-return-value stub behavior and its confirmed-harmless device-ID collision at teardown
Status: DONE — added a docs/out-of-scope.md note to the existing MCI AVI section citing the exact collision mechanism and its confirmed-harmless teardown-only reachability, cross-referencing TASK-24H-0808's direct test. Verified 26/26 in all three build trees.
Priority: P3
Area: Resources
Type: Documentation
Evidence: src/winmm.cpp:308-312; src/MidiMusic.cpp:160,584,646-665; ../free-eggbert/src/movie.cpp:52-62,227-230; ../free-eggbert/src/blupi.cpp:397-425; ../free-eggbert/src/sound.cpp:615,738; docs/out-of-scope.md:9-48; docs/audit-24h-free-api.md §4.27
Depends on: None

Problem:
`mciGetDeviceIDA` (`src/winmm.cpp:308-312`) is a permanent stub that always returns the fixed value `1`, regardless of the requested device-type string, and has zero decision documentation anywhere. This session traced the precise mechanics: free-api's MIDI sequencer path also numbers its own device IDs starting at 1 (`src/MidiMusic.cpp:160,584`), so `mciGetDeviceIDA`'s hardcoded `1` can coincide with a real, currently-open MIDI session's actual device ID. In free-eggbert this is reachable exactly once, at final process teardown: `FinishObjects()` (`blupi.cpp:397-425`) deletes `g_pMovie` — triggering `termAVI()` → `mciGetDeviceIDA` → `MCI_CLOSE` on device `1` — *before* it calls `g_pSound->StopMusic()`'s own explicit close. This is confirmed harmless because `MidiMusicSendCommand`'s `MCI_CLOSE` handler treats an already-closed/unknown ID as non-fatal (`src/MidiMusic.cpp:646-665`), and it's the last teardown step before process exit regardless.

Required work:
- Add a documentation note (in `docs/out-of-scope.md`'s existing MCI/`termAVI()` narrative) stating `mciGetDeviceIDA`'s exact fixed-return-value (`1`) behavior and the confirmed-harmless collision mechanism with the MIDI sequencer's own device numbering, so a future reader doesn't need to re-derive it.

Acceptance criteria:
- The note is added, citing the exact evidence (file:line) on both the free-api and free-eggbert sides.
- No behavior change.
- Existing tests still pass.

Out of scope:
- Do not change `mciGetDeviceIDA`'s return value or add real device-ID tracking/lookup.
- Do not implement AVI/video playback — this remains permanently declined per the existing out-of-scope decision.

---

### TASK-24H-0807: Add `UnlockResource` to `docs/out-of-scope.md`'s "Compile-only stubs" table row
Status: DONE — added the row, gated by the same permanent FindResourceA safe-miss contract as its four siblings already in the table. Verified 26/26 in all three build trees.
Priority: P3
Area: Resources
Type: Documentation
Evidence: docs/out-of-scope.md:85; src/winbase_file.cpp:176-186
Depends on: None

Problem:
`docs/out-of-scope.md:85`'s "Compile-only stubs" table row lists the resource-stub family as `FindResourceA, LoadResource, SizeofResource, LockResource, FreeResource`, but omits `UnlockResource` (`src/winbase_file.cpp:176-180`), which is implemented with the exact same permanent-safe-no-op shape (`(void)hResData; return FALSE;`) as its listed siblings.

Required work:
- Add `UnlockResource` to that row's symbol list.

Acceptance criteria:
- The row lists all six resource-stub symbols.
- No behavior change.
- Existing tests still pass.

Out of scope:
- Do not change `UnlockResource`'s implementation or any other stub in that table.

---

### TASK-24H-0808: Add a regression test for the `mciGetDeviceIDA`/`MCI_CLOSE` collision against a real open sequencer session
Status: DONE — added TestMciGetDeviceIdaClosesARealOpenSequencerSessionAtCollidingId (tests/test_mci_sequences.cpp), run first in main() so it lands on device id 1 (nextId starts at 1). Opens a real sequencer session (asserts id==1), calls mciGetDeviceIDA("avivideo")+MCI_CLOSE matching termAVI()'s exact shape, asserts the close succeeds, confirms the session was genuinely erased (subsequent MCI_PLAY on that id returns MCIERR_INVALID_DEVICE_ID), and confirms a redundant second MCI_CLOSE is a harmless no-op. Verified 24/24 standalone.
Priority: P2
Area: Resources
Type: Test
Evidence: tests/test_mci_sequences.cpp:281-295; src/winmm.cpp:308-312; src/MidiMusic.cpp:160,584,646-665
Depends on: None

Problem:
`tests/test_mci_sequences.cpp:287-295`'s `TestMciGetDeviceIdaResultSafelyClosable` calls `mciGetDeviceIDA` + `MCI_CLOSE` in isolation with no prior session open, so its `mciId` never actually matches a real, active MIDI session — the test explicitly accepts either an "unknown id" or a "real close" outcome (`// not fatal either way`) and only proves the call sequence doesn't crash. It does not exercise the specific device-ID collision mechanism found this session (see TASK-24H-0806): `mciGetDeviceIDA`'s hardcoded return of `1` coinciding with a genuinely open MIDI sequencer session (which is also assigned id `1`, being the first-ever `MCI_OPEN`).

Required work:
- Add a regression test that opens a real MIDI sequencer session first (via the existing `MCI_OPEN` "sequencer" path, assigning device id `1`), then calls `mciGetDeviceIDA("avivideo")` + `MCI_CLOSE` (matching `termAVI()`'s exact call shape) against that same id, and asserts the documented outcome: the open session closes without crashing, and a subsequent redundant `MCI_CLOSE` on the now-already-closed id is accepted as a harmless no-op.

Acceptance criteria:
- The new test passes and genuinely exercises the collision (an actually-open session at the colliding id), not an empty-session no-op like the existing test.
- Existing `test_mci_sequences.cpp` assertions are unchanged and still pass.
- No unrelated API is added.

Out of scope:
- Do not change `mciGetDeviceIDA`'s or `MCI_CLOSE`'s behavior — this is lock-in coverage for existing, confirmed-safe behavior, not a fix.
- Do not implement AVI/video playback.

## WinMM / MIDI / MCI

### TASK-24H-0901: Sharpen the stale MIDI-looping "TODO" comments in MidiMusic.cpp
Status: DONE — rewrote both the file docstring line and MidiMusicSendCommand's doc-comment bullet to state native MCI-level looping is intentionally unimplemented (both games' MM_MCINOTIFY handlers re-issue playback themselves), removing the bare "TODO" wording. README.md's identical stale framing already fixed separately (TASK-24H-1202). Comment-only; verified 26/26 in all three build trees.
Priority: P3
Area: WinMM
Type: Cleanup
Evidence: src/MidiMusic.cpp:15, src/MidiMusic.cpp:527-528; docs/audit-24h-free-api.md §4.26; ../free-eggbert/src/blupi.cpp:562-580; ../planetblupi/src/blupi.cpp:483-501
Depends on: None

Problem:
`src/MidiMusic.cpp`'s file docstring (line 15: "Looping is not supported (TODO); playback stops at end-of-song.") and `MidiMusicSendCommand`'s doc comment (line 527-528: "MCI_PLAY looping: TODO") both imply looping is a missing feature. This session traced both games' `WndProc` `MM_MCINOTIFY` handlers (free-eggbert `blupi.cpp:562-580`, planetblupi `blupi.cpp:483-501`) and confirmed both call `SuspendMusic()` (MCI_CLOSE) then `RestartMusic()` (MCI_OPEN+MCI_PLAY) whenever `wParam == MCI_NOTIFY_SUCCESSFUL`, so music genuinely loops end-to-end during real gameplay. The gap is real MCI-level native looping (correctly not implemented), not player-visible looping (which works). This is plan.md `TASK-0008`, restated here with the precise game-side mechanism as evidence.

Required work:
- Rewrite the file docstring line to state that native MCI-level looping is intentionally unimplemented because both games already re-issue playback themselves via their `MM_MCINOTIFY` handler on song-end (cite the exact close-then-reopen mechanism), not a "TODO."
- Rewrite `MidiMusicSendCommand`'s doc-comment bullet the same way, removing the bare "TODO" wording.

Acceptance criteria:
- No comment in `src/MidiMusic.cpp` implies looping is a missing/broken feature.
- Comment-only change; `MidiMusicSendCommand`'s behavior is byte-for-byte unchanged.
- Existing tests still pass (`test_mci_sequences.cpp` in particular).
- No unrelated API is added.

Out of scope:
- Do not modify `README.md`'s "Limitations" section (lines ~164-166), which has the identical stale framing — that is user-facing documentation and is handled by the Documentation theme's backlog (TASK-24H-1202), not this task.
- Do not implement real MCI-level native looping (e.g. an `MCI_PLAY` loop flag) — no evidenced need, both games already loop correctly via their own notify handler.
- Do not implement AVI/video playback or add a video codec dependency — this was explicitly evaluated and declined this session (see docs/audit-24h-free-api.md §1/§5/§6).

---

### TASK-24H-0902: Add an end-to-end regression test proving MIDI music genuinely loops via notify-driven replay
Status: DONE — added TestMidiGenuinelyLoopsViaNotifyDrivenReplay (tests/test_mci_sequences.cpp): 3 cycles of open->play(MCI_NOTIFY)->mandatory-wait-for-MM_MCINOTIFY-via-PeekMessageA->close->reopen, only proceeding to the next cycle after actually observing that cycle's own fresh notification (PM_REMOVE consumes it, so no stale/leftover notification can satisfy a later cycle). Fails if MM_MCINOTIFY stops being posted or a reopened session never completes. Verified 24/24 standalone.
Priority: P2
Area: WinMM
Type: Test
Evidence: tests/test_mci_sequences.cpp:172-221 (existing 30-cycle stress test); src/MidiMusic.cpp:379-391; ../free-eggbert/src/blupi.cpp:562-580; ../planetblupi/src/blupi.cpp:483-501
Depends on: None

Problem:
`test_mci_sequences.cpp`'s existing `TestNotifyTriggeredCloseAndReopenStressTest` proves the notify-triggered close/reopen pattern doesn't crash (it guards a real prior use-after-free), but it drives the open/play/close cycle directly from the test body rather than reacting to a delivered `MM_MCINOTIFY` message the way both games' real `WndProc` does. There is currently no test that lets a short MIDI fixture play to completion, receives `MM_MCINOTIFY` via the real message queue, and — acting as a minimal stand-in for the game's own handler — reopens and replays the same file more than once, positively proving the "loops end-to-end via notify" claim from TASK-24H-0901 rather than merely asserting no crash.

Required work:
- Add a test to `tests/test_mci_sequences.cpp` that opens a short MIDI fixture with `MCI_NOTIFY`, waits for `MM_MCINOTIFY` with `MCI_NOTIFY_SUCCESSFUL` via `PeekMessageA`/`GetMessageA` (matching real message-loop dispatch, not a direct function call), and on receiving it, closes and reopens+replays the same file, repeating for at least 2-3 full notify-driven cycles.
- Assert each cycle actually receives its own fresh `MM_MCINOTIFY` (not a stale/leftover one), proving playback restarted and completed again rather than the test observing one notification repeatedly.

Acceptance criteria:
- New test fails if `MM_MCINOTIFY` stops being posted on song completion, or if a reopened session never reaches completion.
- Test passes against the current, correct implementation.
- Existing tests, including the pre-existing 30-cycle stress test, still pass unmodified.
- No unrelated API is added.

Out of scope:
- Do not implement real MCI-level native looping — this test proves the existing notify-driven mechanism, it does not add a new playback mode.
- Do not modify `MidiMusicSendCommand`'s or `MixerThread`'s behavior — test-only change.
- Do not implement AVI/video playback or add a video codec dependency — this was explicitly evaluated and declined this session (see docs/audit-24h-free-api.md §1/§5/§6).

---

### TASK-24H-0903: Add regression test coverage for the missing-soundfont silent-success MCI_PLAY path
Status: DONE — TestMissingSoundFontStillSucceedsAndNotifiesPromptly (tests/test_mci_sequences.cpp). Confirmed via direct evidence read that no .sf2 ships anywhere in free-api/free-eggbert/planetblupi and FREE_API_SOUNDFONT is unset by default, so this environment already naturally exercises the no-soundfont path in every sequencer test -- this makes that deliberate and explicit rather than an unlabeled coincidence, without needing subprocess/env-var isolation.
Priority: P1
Area: WinMM
Type: Test
Evidence: src/MidiMusic.cpp:609-635 (`MCI_PLAY` no-soundfont branch); src/MidiMusic.cpp:103-123 (`LoadSoundFont` fallback chain); docs/audit-24h-free-api.md §4.26
Depends on: None

Problem:
When no SoundFont is found, `EnsureMidiBackend()` still returns success but never starts the mixer thread, and `MidiMusicSendCommand`'s `MCI_PLAY` handler detects `!g_midi.soundFont`, sets `playing=false`/`finished=true`, and immediately posts `MM_MCINOTIFY` itself so the game's notify-driven replay loop (see TASK-24H-0901/0902) doesn't stall waiting for a notification that would otherwise never arrive. This is a real, deliberate "degrade to silent success" behavior guarding against an indefinite hang, but it has zero test coverage today — no test in the repository sets `FREE_API_SOUNDFONT` to an empty/nonexistent path or otherwise forces the no-soundfont branch.

Required work:
- Add a test (new file or addition to `tests/test_mci_sequences.cpp`) that forces the no-SoundFont path (e.g. via `FREE_API_SOUNDFONT` pointed at a nonexistent file, run in a subprocess or isolated test binary so it doesn't disturb other tests' real SoundFont loading) and verifies `MCI_OPEN`+`MCI_PLAY` both still return success (0).
- Verify `MM_MCINOTIFY` with `MCI_NOTIFY_SUCCESSFUL` is still posted promptly even though no audio is actually rendered.

Acceptance criteria:
- New test fails if the no-soundfont path ever regresses to a hard failure return code or silently stops posting `MM_MCINOTIFY`.
- Test passes against the current, correct implementation.
- Existing tests still pass, and the new test does not require a real `.sf2` file to be present.
- No unrelated API is added.

Out of scope:
- Do not change `EnsureMidiBackend`/`MidiMusicSendCommand` behavior — test-only change.
- Do not add a bundled test SoundFont asset merely to make this test easier — the point is to test the *absence* path.
- Do not implement AVI/video playback or add a video codec dependency — this was explicitly evaluated and declined this session (see docs/audit-24h-free-api.md §1/§5/§6).

---

### TASK-24H-0904: Add regression test coverage for NormalizeMidiPath's uppercase case-fallback behavior
Status: DONE — TestMidiOpenFindsUppercaseFixtureViaLowercaseName (tests/test_mci_sequences.cpp) exercises the file-local NormalizeMidiPath indirectly through the public MCI_OPEN path: an uppercase-named fixture opened via a lowercase element name, plus an exact-match control case.
Priority: P1
Area: WinMM
Type: Test
Evidence: src/MidiMusic.cpp:219-275 (`NormalizeMidiPath`); ../free-eggbert/src/sound.cpp:591-593 (`sound/music%.3d.blp` lowercase construction); docs/audit-24h-free-api.md §4.20
Depends on: None

Problem:
`NormalizeMidiPath` (file-local `static`, not exported by any header) progressively uppercases larger path suffixes from the basename outward, probing `std::filesystem::exists` at each step, specifically to find real on-disk MIDI assets when the game constructs a lowercase path (confirmed: free-eggbert's `CSound::PlayMusic` builds `sound/music%.3d.blp` via `sprintf`, exactly the lowercase shape this function's own doc comment uses as its worked example) on a case-sensitive filesystem where the shipped asset may be uppercase-named. This is plausibly the exact mechanism that makes MIDI music load at all for free-eggbert on Linux, yet it has zero test coverage — it cannot be unit-tested directly since it is file-local, and no existing test creates a deliberately-mismatched-case fixture to exercise it indirectly through the public `MCI_OPEN` path.

Required work:
- Add an integration-level test that writes a minimal MIDI fixture file with an uppercase name (e.g. `MUSICTEST.MID`), then calls `mciSendCommandA(MCI_OPEN, "sequencer", ...)` with a lowercase-cased element name (e.g. `musictest.mid`) that only matches after the uppercase fallback.
- Assert `MCI_OPEN` succeeds and the resulting session actually plays (or at minimum loads a non-empty song), proving the fallback path is reached and works.
- Include at least one case where the as-given path already matches (baseline, no fallback needed) as a control.

Acceptance criteria:
- New test fails if the uppercase-suffix fallback logic in `NormalizeMidiPath` regresses (e.g. wrong suffix boundary, case-fold error).
- Test passes against the current, correct implementation.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not export `NormalizeMidiPath` from a header or change its linkage — test it indirectly through the public `mciSendCommandA` surface only.
- Do not consolidate `NormalizeMidiPath` with the other three independent path-normalization implementations found this session — that general consolidation is TASK-24H-0703/0706 (Files theme).
- Do not implement AVI/video playback or add a video codec dependency — this was explicitly evaluated and declined this session (see docs/audit-24h-free-api.md §1/§5/§6).

---

### TASK-24H-0905: Consolidate duplicate MCIDEVICEID/mciSendCommandA/MCI_* declarations across digitalv.h, mmsystem.h, and mciapi.h
Status: DONE — mmsystem.h made canonical (see TASK-24H-0104/0105 for the per-symbol detail): MCIDEVICEID guarded by FREE_API_MCIDEVICEID_DEFINED; mciSendCommandA + its mciSendCommand macro guarded by FREE_API_MCISENDCOMMANDA_DECLARED; the 7 shared MCI_* constants each individually #ifndef-guarded. digitalv.h and mciapi.h both check the same guards instead of redefining. mciapi.h now includes <mmsystem.h> so it remains independently compilable (confirmed via an ad-hoc standalone-compile probe; not added to tests/test_header_compile.cpp since that file's own scope rule is "only headers actually included by one of the two target games," and neither game includes mciapi.h -- confirmed via grep, only a commented-out reference exists in free-eggbert's movie.cpp). No signature/behavior changes. Verified: full 23-test suite passes in all three build trees (standalone, free-eggbert, planetblupi); both games' own executables (SPEEDY_BLUPI_WINDOWS, PLANET_BLUPI_WINDOWS) build/link cleanly.
Priority: P2
Area: WinMM
Type: Refactor
Evidence: include/mciapi.h:9; include/mmsystem.h:30,84,251,258; include/digitalv.h:73,135,143,145; docs/audit-24h-free-api.md §3.4/§3.8/§3.11
Depends on: None

Problem:
`MCIDEVICEID` is independently typedef'd in both `include/mmsystem.h:30` and `include/mciapi.h:9`; `mciSendCommandA` (and its `mciSendCommand` macro alias) is independently declared identically in both `include/digitalv.h:135,145` and `include/mmsystem.h:251,258`; several `MCI_OPEN`/`CLOSE`/`PLAY`/`NOTIFY`/`WAIT`/`OPEN_TYPE`/`OPEN_ELEMENT` constants are similarly duplicated between the two headers. NOTE: this task duplicates TASK-24H-0104/TASK-24H-0105 (Scope theme) — implement once, close all three.

Required work:
- Pick one authoritative header for `MCIDEVICEID` and the shared `MCI_*` constant/function declarations (`mmsystem.h`, matching its role as the primary WinMM compatibility subset).
- Have `digitalv.h` and `mciapi.h` include `mmsystem.h` (or the specific minimal header) instead of re-declaring the same symbols, removing the duplicate typedefs/declarations.

Acceptance criteria:
- `MCIDEVICEID`, `mciSendCommandA`, and the shared `MCI_*` constants are declared in exactly one place, included by the others.
- The public header include-order-fragility noted in the audit no longer applies (any single-header include of `digitalv.h`, `mmsystem.h`, or `mciapi.h` alone still compiles).
- `test_header_compile.cpp` and all existing tests still pass in all three build modes (target-game-via-Ninja, target-game-via-Make, standalone).
- No unrelated API is added or removed; no `SDL_*` type leaks into any public header.

Out of scope:
- Do not change any function's actual signature or behavior — declaration consolidation only.
- Do not merge `digitalv.h`/`mciapi.h`/`mmsystem.h` into a single file — keep the existing file boundaries, just remove the duplication between them.
- Do not implement AVI/video playback or add a video codec dependency — this was explicitly evaluated and declined this session (see docs/audit-24h-free-api.md §1/§5/§6).

---

### TASK-24H-0906: Add missing `@note Status:` annotations to mciapi.h
Status: DONE — duplicate of TASK-24H-0108; implemented once there (see that entry).
Priority: P3
Area: WinMM
Type: Documentation
Evidence: include/mciapi.h (entire file, no annotations); docs/audit-24h-free-api.md §3 ("Documentation convention"), §3.8
Depends on: None

Problem:
Most public headers in `include/` annotate each declaration with a `/** @brief ... @note Status: {STUB|PARTIAL|IMPLEMENTED|HEADER_ONLY} */` comment. `mciapi.h` is one of only two fully-populated (non-trivial-shim) headers missing this convention entirely (the other, `winerror.h`, is out of scope for this WinMM-themed task). NOTE: overlaps with TASK-24H-0108 (Scope theme) — implement once, close both.

Required work:
- Add `@note Status:` annotations to every declaration in `include/mciapi.h`, matching the status already established for the same symbols in `mmsystem.h`/`digitalv.h` (e.g. `MCIDEVICEID` as `IMPLEMENTED`).

Acceptance criteria:
- Every declaration in `include/mciapi.h` has a `@note Status:` annotation consistent with its sibling declaration's documented status elsewhere.
- No code/behavior change — documentation-only.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not annotate `winerror.h` — that header is general, not WinMM-specific, and is left to the broader header-hygiene backlog (TASK-24H-0108).
- Do not change any declaration's actual signature.
- Do not implement AVI/video playback or add a video codec dependency — this was explicitly evaluated and declined this session (see docs/audit-24h-free-api.md §1/§5/§6).

---

### TASK-24H-0907: Correct docs/supported-apis.md's cdaudio row to reflect confirmed dead/absent status in both games
Status: DONE — RE-SCOPED: this task's own premise was wrong for free-eggbert. Re-verified this session: free-eggbert has TWO alternate sound backend files (sound.cpp guarded `#if !_BASS || _LEGACY`; soundbass.cpp guarded `#if _BASS && !_LEGACY`) -- with `_BASS`/`_LEGACY` both undefined (0) in this build, sound.cpp is the LIVE one (not soundbass.cpp, which is dead), and sound.cpp's own cdaudio reference (PlayMusic -> PlayCDAudio) is reachable, gated behind a real config option (`CDAudio=` line, read into g_bCDAudio in blupi.cpp, passed via SetCDAudio) -- not dead code at all. planetblupi genuinely has zero cdaudio references (that half of the original claim held). Updated the row to "Yes (live, config-gated)" / "No (zero references anywhere in source)" instead of the originally-planned "dead"/"No". Verified 26/26 in all three build trees.
Priority: P3
Area: WinMM
Type: Documentation
Evidence: docs/supported-apis.md:58; src/MidiMusic.cpp:542-546; docs/audit-24h-free-api.md §2.7, §2.10 pattern, §6
Depends on: None

Problem:
`docs/supported-apis.md:58` lists `"cdaudio" graceful decline` with "Yes | Yes" in the "Required by free-eggbert" / "Required by planetblupi" columns, implying both games actively exercise this decline path at runtime. This session confirmed the opposite: planetblupi has zero cdaudio references anywhere in its source, and free-eggbert's only cdaudio-adjacent code lives entirely inside the dead `soundbass.cpp` (gated `#if _BASS && !_LEGACY`, which evaluates `FALSE` given free-eggbert's hardcoded build `#define`s). The graceful decline is real and tested (`test_mci_sequences.cpp`), but it is dead-reach in free-eggbert and entirely absent in planetblupi — the table's "Yes | Yes" is factually inaccurate, matching the table's own existing convention elsewhere (e.g. row 33/34 correctly marks free-eggbert's `SetTimer` as "dead").

Required work:
- Update the cdaudio row's "Required by free-eggbert" cell to `dead (soundbass.cpp, #if _BASS && !_LEGACY == FALSE)` and "Required by planetblupi" cell to `No (not referenced anywhere)`, matching the audit's confirmed findings.
- Keep the "Current status"/"Test coverage" cells as-is (the decline behavior itself is correctly implemented and tested, independent of whether either game reaches it today).

Acceptance criteria:
- The cdaudio row in `docs/supported-apis.md` accurately reflects that this path is unreached by either game's current build, using the same "dead"/"No" convention already established elsewhere in the same table.
- No code/behavior change — documentation-only.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not remove the cdaudio graceful-decline implementation — it remains correct, deliberate, permanent behavior per docs/out-of-scope.md, independent of current reachability.
- Do not implement AVI/video playback or add a video codec dependency — this was explicitly evaluated and declined this session (see docs/audit-24h-free-api.md §1/§5/§6).

---

### TASK-24H-0908: Fix inaccurate "MCI_STOP" claim in MidiMusicSendCommand's doc comment
Status: DONE — confirmed via grep that MCI_STOP is not defined anywhere in include/ and there is no if(uMsg==MCI_STOP) branch. Rewrote the doc comment's "Supported commands" line to remove the false claim and state MCI_STOP falls through to MCIERR_UNSUPPORTED_FUNCTION. Comment-only; verified 26/26 in all three build trees.
Priority: P3
Area: WinMM
Type: Cleanup
Evidence: src/MidiMusic.cpp:523 ("Supported commands: MCI_OPEN, MCI_PLAY, MCI_STOP (same as MCI_CLOSE here), MCI_CLOSE, MCI_SET."); src/MidiMusic.cpp:596-676 (actual `if (uMsg == ...)` branches)
Depends on: None

Problem:
`MidiMusicSendCommand`'s doc comment (line 523) claims `MCI_STOP` is a supported command, treated "same as MCI_CLOSE here." This is false on inspection: `MCI_STOP` is not defined as a constant anywhere in `include/` (confirmed by exhaustive grep across `mmsystem.h`/`digitalv.h`/`mciapi.h`), there is no `if (uMsg == MCI_STOP)` branch anywhere in the function body (only `MCI_OPEN`/`MCI_PLAY`/`MCI_CLOSE`/`MCI_SET` are handled), and neither game ever calls `MCI_STOP` (zero hits in either game's `sound.cpp`). Any real `MCI_STOP` call would fall through to the function's final `MIDI_LOG("mciSendCommand: unhandled msg=...")` and return `MCIERR_UNSUPPORTED_FUNCTION` — the exact opposite of "same as MCI_CLOSE."

Required work:
- Remove the false `MCI_STOP` claim from the doc comment's "Supported commands" line.
- Optionally add a one-line note that `MCI_STOP` is neither defined nor needed (no evidenced call site in either game), to preempt a future contributor assuming it's handled.

Acceptance criteria:
- The doc comment no longer claims `MCI_STOP` is supported or treated as `MCI_CLOSE`.
- No code/behavior change — comment-only.
- Existing tests still pass.
- No unrelated API is added; `MCI_STOP` is not newly defined or implemented.

Out of scope:
- Do not implement `MCI_STOP` handling — no evidenced call site in either game.
- Do not add an `MCI_STOP` constant to any header speculatively.
- Do not implement AVI/video playback or add a video codec dependency — this was explicitly evaluated and declined this session (see docs/audit-24h-free-api.md §1/§5/§6).

---

### TASK-24H-0909: Fold this session's MCI AVI feasibility conclusion into docs/out-of-scope.md
Status: DONE — added a paragraph to docs/out-of-scope.md's existing MCI AVI section citing the Cinepak+MS Video 1 codec dependency and the permanent-decline recommendation, cross-referencing docs/audit-24h-free-api.md §1/§5/§6 as the prior-session source. Implemented together with duplicate TASK-24H-1207. Verified 26/26 in all three build trees.
Priority: P3
Area: WinMM
Type: Documentation
Evidence: docs/out-of-scope.md:9-48 (existing "MCI digital-video / AVI movie playback" section); docs/audit-24h-free-api.md §1, §5 item 15, §6
Depends on: None

Problem:
The user explicitly approved re-opening MCI AVI/digital-video this session as a feasibility audit only (no implementation) — completed, see `docs/audit-24h-free-api.md` §1/§5/§6, recommendation "decline permanently." `docs/out-of-scope.md`'s existing MCI AVI section already documents the original safe-skip investigation but predates this session's feasibility research and does not mention the concrete codec dependency this session identified (Cinepak and MS Video 1, confirmed from planetblupi's real shipped `.avi` assets) or explicitly note that this session independently re-confirmed the decline rather than merely inheriting the prior decision unchanged. NOTE: overlaps with TASK-24H-1207 (Documentation theme) — implement once, close both.

Required work:
- Add a short paragraph to `docs/out-of-scope.md`'s existing "MCI digital-video / AVI movie playback" section noting this session's independent feasibility re-audit, the confirmed Cinepak + MS Video 1 codec/video-decode dependency that real playback would require, and that the recommendation is to decline permanently, unchanged.
- Cross-reference `docs/audit-24h-free-api.md` §1/§5/§6 as the source of this finding.

Acceptance criteria:
- `docs/out-of-scope.md`'s MCI AVI section explicitly reflects this session's feasibility conclusion (codec dependency + permanent-decline recommendation), not just the prior session's safe-skip trace.
- No code/behavior change — documentation-only.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do NOT implement AVI/video playback or add a video codec dependency — this was explicitly evaluated and declined this session (see docs/audit-24h-free-api.md §1/§5/§6).
- Do not modify `src/winmm.cpp`'s AVI-probe-shape check or `tests/test_mci_avivideo_regressions.cpp` — behavior is already correct and locked in.
- Do not touch the unrelated MIDI-looping or cdaudio sections of the same file — those are separate findings covered by other tasks (TASK-24H-0901, TASK-24H-0907).

---

### TASK-24H-0910: Remove dead, mmsystem.h-shadowed local MCIERR_* constant duplicates in MidiMusic.cpp
Status: DONE — removed the three dead #ifndef-guarded local static constexpr MCIERROR fallbacks and the stale "backward compatibility" comment. Confirmed dead by inspection (mmsystem.h is included before the guards run, so all three #ifndef checks were always false) and by successful rebuild with zero errors (proves nothing referenced the removed local definitions). Verified 26/26 in all three build trees.
Priority: P3
Area: WinMM
Type: Cleanup
Evidence: src/MidiMusic.cpp:78-89; include/mmsystem.h:141-143
Depends on: None

Problem:
`src/MidiMusic.cpp:78-89` defines three `#ifndef`-guarded local `static constexpr MCIERROR` fallbacks (`MCIERR_UNSUPPORTED_FUNCTION`, `MCIERR_INVALID_DEVICE_ID`, `MCIERR_INTERNAL`) under a comment claiming they're "kept... for backward compatibility within this file" because they're "not in the project mmsystem.h subset." This is stale: `include/mmsystem.h:141-143` already `#define`s all three constants with the identical numeric values, and `MidiMusic.cpp` includes `mmsystem.h` (line 36) before these guards run — so all three `#ifndef` blocks are always false and the local `static constexpr` fallbacks never actually compile. The code is harmless dead weight, but the comment actively misleads a future reader into thinking these local copies are load-bearing.

Required work:
- Remove the three dead `#ifndef`-guarded local `static constexpr MCIERROR` definitions at `src/MidiMusic.cpp:78-89`.
- Remove or correct the stale comment above them that claims they're needed for "backward compatibility."

Acceptance criteria:
- `src/MidiMusic.cpp` no longer contains unreachable/dead constant definitions or the misleading comment describing them.
- All `MCIERR_*` references in `src/MidiMusic.cpp` resolve to `include/mmsystem.h`'s definitions with identical numeric values (268/259/305) — no behavior change.
- Existing tests still pass, in particular `test_mci_sequences.cpp` and `test_mci_avivideo_regressions.cpp` (which exercise `MidiMusicGetErrorString`'s error-code formatting).
- No unrelated API is added.

Out of scope:
- Do not change any `MCIERR_*` numeric value — this is dead-code removal only, not a behavior change.
- Do not touch `include/mmsystem.h`'s own `MCIERR_*` definitions.
- Do not implement AVI/video playback or add a video codec dependency — this was explicitly evaluated and declined this session (see docs/audit-24h-free-api.md §1/§5/§6).

## Joystick

### TASK-24H-1001: Correct docs/target-games.md's stale joystick description
Status: DONE — rewrote docs/target-games.md's joystick bullet to state the confirmed dead-reach finding (m_somethingJoystick assigned 0 once in the constructor, never reassigned) and that free-api's backend is real, not a stub; cross-referenced docs/out-of-scope.md and docs/supported-apis.md instead of duplicating detail. Re-verified the m_somethingJoystick claim directly against event.cpp this session. Verified 25/25 in all three build trees.
Priority: P2
Area: Joystick
Type: Documentation
Evidence: docs/target-games.md:21-24; src/winmm.cpp:187-254; ../free-eggbert/src/event.cpp:1770,2048; docs/audit-24h-free-api.md §2.10, §4.28
Depends on: None

Problem:
docs/target-games.md:21-24 makes two claims this session's audit disproves: it says free-eggbert's joystick input is "used, live, per-frame" (the code path is confirmed structurally unreachable — `m_somethingJoystick` is assigned `0` once in its constructor and never reassigned anywhere else in the codebase), and that free-api currently has a "safe-stub (0 devices reported)" (free-api's `joyGetPosEx`/`joyGetNumDevs` are a real, correct, SDL_Joystick-backed implementation, not a stub). Both sentences describe a state of the world that no longer exists.

Required work:
- Rewrite docs/target-games.md:21-24 to state free-eggbert's joystick reads (`event.cpp:2069-2127`) are gated by `m_somethingJoystick`, which is confirmed always NULL at runtime, so the joystick code path never executes in the current shipped game, and that free-api's backend (`src/winmm.cpp`) is a real, tested SDL_Joystick implementation kept for link-time completeness.
- Cross-reference docs/out-of-scope.md and docs/supported-apis.md rather than duplicating full detail.

Acceptance criteria:
- docs/target-games.md no longer claims free-eggbert's joystick input is "live, per-frame" or that free-api "stubs" it to 0 devices.
- The corrected text is consistent with docs/audit-24h-free-api.md §2.10/§4.28 and docs/out-of-scope.md's TASK-0103 note.
- Existing tests still pass (docs-only change).
- No unrelated API is added.

Out of scope:
- Do not change src/winmm.cpp behavior; this is a documentation-only correction.
- Do not add DirectInput, raw gamepad, or any new joystick function to close this gap — the fix is describing reality accurately, not expanding scope.
- Do not propose making free-eggbert's own code actually poll the joystick; that is out of free-api's repo scope.

---

### TASK-24H-1002: Fix stale "Status: STUB" annotation on JOYINFOEX
Status: DONE — changed @note Status: STUB to Status: IMPLEMENTED on JOYINFOEX's doc comment (include/mmsystem.h), matching joyGetPosEx/joyGetNumDevs' own accurate annotation. No other doc comment touched. Verified 25/25 in all three build trees.
Priority: P2
Area: Joystick
Type: Documentation
Evidence: include/mmsystem.h:37-51 (Status: STUB) vs :192-207 (joyGetPosEx/joyGetNumDevs, both Status: IMPLEMENTED); src/winmm.cpp:187-254
Depends on: None

Problem:
The `JOYINFOEX` struct's doc comment (include/mmsystem.h:50) is annotated `@note Status: STUB`, which per this file's own convention (STUB = "compile-only or safe placeholder") is inaccurate: the struct is fully populated field-by-field by a real, tested SDL_Joystick-backed implementation, and the two functions that consume it a few lines below are both correctly annotated `Status: IMPLEMENTED`. A future maintainer scanning for STUB symbols would be misled into treating this as an unimplemented area.

Required work:
- Change `@note Status: STUB` to `@note Status: IMPLEMENTED` on the JOYINFOEX doc comment at include/mmsystem.h:50, matching the accurate status of the functions that populate it.
- Leave the rest of the doc comment (which already correctly describes which 5 fields are read) unchanged.

Acceptance criteria:
- include/mmsystem.h's JOYINFOEX doc comment's Status annotation matches the real, tested behavior of joyGetPosEx/joyGetNumDevs.
- No other doc comment in the file is altered.
- Existing tests still pass (header-comment-only change; no behavior change).
- No unrelated API is added.

Out of scope:
- Do not change the JOYINFOEX struct's fields or layout.
- Do not add DirectInput, raw gamepad, or any new joystick function.
- Do not touch any other struct's Status annotation in this pass — keep this atomic to JOYINFOEX.

---

### TASK-24H-1003: Update supported-apis.md joystick row to reflect confirmed dead-reach status
Status: DONE — changed the joystick row's "Required by free-eggbert" column from "Yes (optional)" to "dead", matching the table's own established vocabulary (SetTimer/CreateDirectoryA rows). "Current status" column left unchanged. Verified 25/25 in all three build trees.
Priority: P2
Area: Joystick
Type: Documentation
Evidence: docs/supported-apis.md:59 (joyGetPosEx/joyGetNumDevs row) vs :33-34 (SetTimer/timeSetEvent rows' "dead" convention); docs/audit-24h-free-api.md §2.10; ../free-eggbert/src/event.cpp:1770,2048
Depends on: None

Problem:
docs/supported-apis.md:59 lists joyGetPosEx/joyGetNumDevs' "Required by free-eggbert" column as "Yes (optional)" — but this session confirmed free-eggbert's joystick call path is structurally unreachable at runtime, not merely "optional." The same table already has an established vocabulary for exactly this situation ("dead" for a live call site whose result is never reachable/consumed, used at lines 33-34 for SetTimer/KillTimer and timeSetEvent/timeKillEvent), which this row doesn't use.

Required work:
- Update docs/supported-apis.md's joystick row's "Required by free-eggbert" column to use the table's existing "dead" (or equivalent "dead-reach") vocabulary instead of "Yes (optional)".
- Keep the "Current status" column's "IMPLEMENTED (real SDL_Joystick-backed...)" wording unchanged, since that part is already accurate.

Acceptance criteria:
- The joystick row's free-eggbert column uses the same "dead"/"live" vocabulary already established elsewhere in the same table.
- No other row in docs/supported-apis.md is altered.
- Existing tests still pass (docs-only change).
- No unrelated API is added.

Out of scope:
- Do not remove the joystick row or mark it "not implemented" — the backend is real and correct; only the game-side reachability changed.
- Do not add DirectInput, raw gamepad, or any new joystick function.
- Do not fold in the broader TASK-0001 "28 missing rows" cleanup; keep this change scoped to the one existing joystick row.

---

### TASK-24H-1004: Strengthen out-of-scope.md's joystick note with exhaustive-reassignment evidence
Status: DONE — added the exact assignment/read line citations to docs/out-of-scope.md's joystick note. Verified 26/26 in all three build trees.
Priority: P3
Area: Joystick
Type: Documentation
Evidence: docs/out-of-scope.md:101-111; ../free-eggbert/include/event.hpp:208; ../free-eggbert/src/event.cpp:1770,2048,2603,2720
Depends on: None

Problem:
docs/out-of-scope.md:101-111 (the TASK-0103 resolution note) already correctly says `m_somethingJoystick` "is never set to anything but 0," but this session went further and confirmed via exhaustive grep across the entire free-eggbert codebase that the field is assigned exactly once (its constructor, `event.cpp:1770`) and is only ever read afterward (`event.cpp:2048`, `2603`, `2720`) — never reassigned anywhere. The existing note doesn't capture this stronger, more precise "structurally unreachable" conclusion.

Required work:
- Add one or two sentences to docs/out-of-scope.md:101-111 citing the exact assignment site (`event.cpp:1770`) and the read/gate sites (`event.cpp:2048`, `2603`, `2720`), stating the field is provably never reassigned anywhere in the codebase (not just "never observed" to change).

Acceptance criteria:
- docs/out-of-scope.md's joystick note cites the specific line numbers backing the "never reassigned" claim.
- The rest of the note (joyGetDevCapsA row, TASK-0103 resolution wording) is left otherwise intact.
- Existing tests still pass (docs-only change).
- No unrelated API is added.

Out of scope:
- Do not weaken or remove the existing joyGetDevCapsA "not implemented" row — no new evidence justifies implementing it.
- Do not add DirectInput, raw gamepad, or any new joystick function.
- Do not propose modifying free-eggbert's own source to make it poll the joystick; that repo is out of free-api's scope.

---

### TASK-24H-1005: Document the SDL_INIT_JOYSTICK teardown-free side effect at its source
Status: DONE — added a comment at EnsureJoystickSubsystem (src/winmm.cpp) explaining the no-teardown behavior is deliberate/accepted. No SDL_QuitSubSystem added. Verified 26/26 in all three build trees.
Priority: P3
Area: Joystick
Type: Documentation
Evidence: src/winmm.cpp:24-28 (EnsureJoystickSubsystem); docs/audit-24h-free-api.md §4.28, Known Risk List item 16
Depends on: None

Problem:
Any call to `joyGetNumDevs()`/`joyGetPosEx()` lazily initializes `SDL_INIT_JOYSTICK` via `EnsureJoystickSubsystem` (src/winmm.cpp:24-28) and it is never torn down anywhere in the codebase (confirmed by grep — only the video subsystem has a matching `SDL_QuitSubSystem` call). The audit explicitly classifies this as harmless-but-worth-documenting (§4.28, Known Risk #16: "backlog P3 doc item only; do not add teardown logic without evidence it matters"), but there is currently no comment at the call site itself explaining this is a deliberate, accepted state rather than an oversight.

Required work:
- Add a short comment directly above/inside `EnsureJoystickSubsystem` (src/winmm.cpp:24-28) noting that `SDL_INIT_JOYSTICK` is intentionally never torn down (unlike video), that this is a known, accepted, process-lifetime side effect, and pointing to this audit finding so a future contributor doesn't "fix" it without new evidence.

Acceptance criteria:
- A comment exists at src/winmm.cpp's `EnsureJoystickSubsystem` explaining the no-teardown behavior is deliberate/accepted, not an oversight.
- No `SDL_QuitSubSystem(SDL_INIT_JOYSTICK)` call or other teardown/cleanup logic is added.
- Existing tests still pass (comment-only change).
- No unrelated API is added.

Out of scope:
- Do not add `SDL_QuitSubSystem(SDL_INIT_JOYSTICK)` or any other teardown/cleanup logic — no evidence shows this causes a problem, and defensive teardown here would be unnecessary complexity per the audit's own explicit guidance.
- Do not add DirectInput, raw gamepad, or any new joystick function.
- Do not change EnsureJoystickSubsystem's actual behavior, only add documentation.

## Diagnostics / Logging

### TASK-24H-1101: Gate winuser_window.cpp's unconditional per-launch logging behind FreeApiDiagnosticsEnabled()
Status: DONE — all 13 SDL_Log sites in CreateWindowExA/ShowWindow/UpdateWindow/SetFocus wrapped in FreeApiDiagnosticsEnabled(). Confirmed quiet by default (grep for the 4 message prefixes returns 0 hits under test_winuser_regressions) and fully restored under FREE_API_DIAGNOSTICS=1 (1026 hits). 17/17 in all three build modes.
Priority: P1
Area: Diagnostics
Type: Bugfix
Evidence: src/winuser_window.cpp:42,53,59,82,89,93,98,126 (CreateWindowExA), :200,228 (ShowWindow), :262 (UpdateWindow), :313,315 (SetFocus); docs/audit-24h-free-api.md §4.29, §5 item 6
Depends on: None

Problem:
`src/winuser_window.cpp` is the single largest source of unconditional `SDL_Log` calls in the codebase — 13 call sites across `CreateWindowExA` (8), `ShowWindow` (2), `UpdateWindow` (1), and `SetFocus` (2) — every one of which fires on every successful window creation and normal game launch, with no env-var opt-out, unlike the rest of the codebase's disciplined `FreeApiDiagnosticsEnabled()` gating (e.g. `winuser_timer.cpp`, `wingdi_dc.cpp`). This directly violates the "quiet by default in hot/startup paths unless explicitly flagged" requirement, since it produces guaranteed log spam on every launch of both target games with no way to silence it short of a code change. NOTE: duplicates TASK-24H-0303 (Window/Cursor theme) — implement once, close both.

Required work:
- Wrap each of the 13 `SDL_Log` call sites in `CreateWindowExA`/`ShowWindow`/`UpdateWindow`/`SetFocus` in `if (FreeApiDiagnosticsEnabled()) { ... }`, matching the exact pattern already used in `src/winuser_timer.cpp:29-30,46-47` and `src/wingdi_dc.cpp:49-50,57`.
- Do not change any log message text, format specifiers, or the information logged — only add the gate.
- Preserve the existing return values and control flow of all four functions exactly.

Acceptance criteria:
- Running either target game (or the existing test suite) with no `FREE_DIRECT_DIAGNOSTICS`/`FREE_API_DIAGNOSTICS` env var set produces zero `SDL_Log` output from `CreateWindowExA`, `ShowWindow`, `UpdateWindow`, or `SetFocus`.
- Setting `FREE_API_DIAGNOSTICS=1` restores all 13 log lines exactly as before (same text/format).
- Existing tests (`test_winuser_regressions`, `test_gdi_regressions`, `test_eggbert_loop`, `test_planetblupi_loop`) still pass unmodified.
- No unrelated API is added; no other function in `winuser_window.cpp` is touched.

Out of scope:
- Do not touch the message loop or `StretchBlt` — both are already confirmed clean and correctly gated.
- Do not add a new logging framework or a new env var; reuse `FreeApiDiagnosticsEnabled()` exactly as used elsewhere.
- Do not implement any new WinAPI behavior in these functions.

---

### TASK-24H-1102: Gate LoadImageA's unconditional success-path log behind FreeApiGdiDebugEnabled()
Status: DONE — success-path log wrapped in FreeApiGdiDebugEnabled(); the two failure-path logs (missing-resource, SDL_LoadBMP failure) left unconditional. 17/17 in all three build modes.
Priority: P1
Area: Diagnostics
Type: Bugfix
Evidence: src/wingdi_bitmap.cpp:47; docs/audit-24h-free-api.md §4.15, §4.29, §5 item 6
Depends on: None

Problem:
`LoadImageA`'s success path logs unconditionally on every successful bitmap load (`"free-api LoadImageA: loaded bitmap '%s' -> %dx%d"`, `src/wingdi_bitmap.cpp:47`), unlike its own failure-path logs at lines 22 and 29 which are appropriately rare. Every asset a game loads at startup (and any loaded later) produces one guaranteed, ungated log line, inconsistent with the rest of `wingdi_*.cpp`'s discipline. NOTE: duplicates TASK-24H-0606 (GDI theme) — implement once, close both.

Required work:
- Wrap the success-path `SDL_Log` at `src/wingdi_bitmap.cpp:47` in `if (FreeApiGdiDebugEnabled()) { ... }`, matching `wingdi_blit.cpp`'s exact pattern.
- Leave the two failure-path logs at lines 22 and 29 unconditional (failure logging is compliant with the quiet-by-default policy).
- Do not change the log message text or format.

Acceptance criteria:
- Loading any number of bitmaps via `LoadImageA` with no `FREE_API_DEBUG_GDI` env var set produces zero success-path log lines.
- Setting `FREE_API_DEBUG_GDI=1` restores the exact original success-path log line.
- The two failure-path logs (missing-resource-path, `SDL_LoadBMP` failure) remain unconditional and unchanged.
- Existing tests (`test_gdi_regressions`, any `LoadImageA`-touching test) still pass unmodified.
- No unrelated API is added.

Out of scope:
- Do not gate the failure-path logs in `LoadImageA` — those must stay visible by default per the "do not change error visibility" rule.
- Do not touch `NormalizePath` vs `NormalizeFilesystemPath` (a separate, already-tracked §4.15 finding, not a logging issue — see TASK-24H-0605).
- Do not add a new debug flag; reuse `FreeApiGdiDebugEnabled()`.

---

### TASK-24H-1103: Gate FreeApiSetWindowFullscreen's failure-path log for consistency with its own diag-gated logs
Status: DONE — the failure-path SDL_Log at src/wingdi_dc.cpp now reuses the already-computed `diag` local, matching the entry/success logs' gating in the same function.
Priority: P2
Area: Diagnostics
Type: Cleanup
Evidence: src/wingdi_dc.cpp:44-65 (diag flag at :49, gated logs at :50 and :57, unconditional log at :63)
Depends on: None

Problem:
`FreeApiSetWindowFullscreen` computes a local `diag` flag from `FreeApiDiagnosticsEnabled()` and correctly gates its entry log (`:50`) and its success log (`:57`) behind it, but its failure log at `:63` (`"SDL_SetWindowFullscreen(%s) failed: %s"`) is left unconditional — an internal inconsistency in a function that otherwise fully commits to diag-gating. Failure logging is allowed unconditionally by policy, but the inconsistency (same function, same `diag` variable already in scope, one branch ignores it) is a code-hygiene gap worth closing for uniformity.

Required work:
- Wrap the `SDL_Log` at `src/wingdi_dc.cpp:63` in `if (diag) { ... }`, reusing the `diag` local already computed at line 49.
- Do not change the log message text or format.

Acceptance criteria:
- Calling the fullscreen-toggle path in a way that makes `SDL_SetWindowFullscreen` fail, with no diagnostics env var set, produces zero log output from this function.
- Setting `FREE_API_DIAGNOSTICS=1` restores the exact original failure log line alongside the entry/success logs.
- Existing tests still pass unmodified.
- No unrelated API is added.

Out of scope:
- Do not address the separate, already-tracked `FreeApiSetWindowFullscreen` missing-header-declaration finding (TASK-24H-0101) — that is a header-hygiene issue, not a logging issue.
- Do not change fullscreen behavior itself.

---

### TASK-24H-1104: Add a quiet-by-default regression test for window and bitmap logging
Status: DONE — added a minimal file-local SDL_SetLogOutputFunction-based capture helper in both tests/test_winuser_regressions.cpp (TestWindowStartupSequenceIsQuietByDefault) and tests/test_gdi_regressions.cpp (TestLoadImageASuccessPathIsQuietByDefault), asserting zero captured log lines for the CreateWindowExA->ShowWindow->UpdateWindow->SetFocus sequence and a successful LoadImageA call respectively, both with no diagnostics env var set. The winuser test initially FAILED against current code (proving it's a real regression guard, not a tautology): it caught a genuine, previously-undocumented gap in EnsureVideoSubsystem's unconditional "SDL video initialized" log, fixed as TASK-24H-1227. Verified 24/24 in all three build trees.
Priority: P2
Area: Diagnostics
Type: Test
Evidence: tests/test_winuser_regressions.cpp; tests/test_gdi_regressions.cpp; src/winuser_window.cpp; src/wingdi_bitmap.cpp
Depends on: TASK-24H-1101, TASK-24H-1102

Problem:
No test in the suite currently asserts that a normal window-creation / bitmap-load sequence produces zero log output by default; there is also no existing test infrastructure for capturing `SDL_Log` output at all. Without this, TASK-24H-1101/1102's fix has no regression guard and could silently regress (e.g. a future edit re-introducing an ungated `SDL_Log`).

Required work:
- Add a small, reusable log-capture helper (e.g. via `SDL_SetLogOutputFunction`) usable from test files, recording line counts/text during a test body.
- Add a test case in `tests/test_winuser_regressions.cpp` that runs the existing `CreateWindowExA` → `ShowWindow` → `UpdateWindow` → `SetFocus` sequence (reusing the pattern already at `TestShowWindowUpdateWindowSetFocusSequence`) with no diagnostics env var set and asserts zero captured log lines from `free-api`.
- Add a test case in `tests/test_gdi_regressions.cpp` that loads a bitmap via `LoadImageA` with no `FREE_API_DEBUG_GDI` set and asserts zero captured log lines.
- Restore any previously-installed log output function after each test to avoid interfering with other tests.

Acceptance criteria:
- New tests fail against the pre-TASK-24H-1101/1102 code and pass against the fixed code (verified by running them against a stash/revert if convenient).
- Both new tests are registered in `CMakeLists.txt`'s existing test targets and run under `ctest`.
- Existing tests still pass unmodified.
- No unrelated API is added.

Out of scope:
- Do not build a general-purpose logging/test framework; keep the capture helper minimal and file-local or in a shared test-utility header already used by the existing tests.
- Do not assert on log content/text, only on the absence/presence of unconditional output, to avoid brittle string-matching.

---

### TASK-24H-1105: Clarify or rename the misleading FreeApiDiagnosticsFastEnabled() alias
Status: DONE — added explanatory comments to the declaration (src/internal/FreeApiDiagnostics.hpp) and definition (FreeApiDiagnostics.cpp) stating it's intentionally identical to FreeApiDiagnosticsEnabled(), exists only to mark hot-path call sites semantically. No call sites touched, no behavior change.
Priority: P2
Area: Diagnostics
Type: Cleanup
Evidence: src/internal/FreeApiDiagnostics.cpp:68-71 (`bool FreeApiDiagnosticsFastEnabled() { return FreeApiDiagnosticsEnabled(); }`); src/internal/FreeApiDiagnostics.hpp:37; call sites: src/winuser_message.cpp:93,166; src/internal/FreeApiMessageQueue.cpp:30,208
Depends on: None

Problem:
`FreeApiDiagnosticsFastEnabled()` is a pure alias — its body is exactly `return FreeApiDiagnosticsEnabled();`, the same cached static-int check, not a distinct or cheaper code path despite the "Fast" name. It is used at 4 call sites in genuinely hot code (message dispatch, message queue coalescing), where a reader would reasonably assume "Fast" means a different, lighter-weight check exists. This is confusing but not incorrect (the underlying check is already O(1) via the cached static), so no behavior is wrong today — it's a naming/documentation risk for future maintainers.

Required work:
- Add a clear comment on `FreeApiDiagnosticsFastEnabled()`'s declaration in `src/internal/FreeApiDiagnostics.hpp:37` and its definition in `FreeApiDiagnostics.cpp`, explaining it is intentionally identical to `FreeApiDiagnosticsEnabled()` and exists only to mark hot-path call sites semantically (a documentation fix, not a behavior change).
- Do not modify any of the 4 call sites; their behavior is already correct.

Acceptance criteria:
- `FreeApiDiagnosticsFastEnabled()`'s purpose (identical-but-semantically-distinct alias) is unambiguous from reading the header alone.
- Behavior at all 4 call sites is byte-for-byte unchanged.
- Existing tests still pass unmodified.
- No unrelated API is added.

Out of scope:
- Do not introduce an actually-faster/differently-cached code path — there is no evidenced need for one; this is a documentation fix, not a performance change.
- Do not change `FreeApiDiagnosticsEnabled()`'s own caching mechanism, and do not remove `FreeApiDiagnosticsFastEnabled()` or its call sites without a separate, explicitly-approved cleanup task.

---

### TASK-24H-1106: Verify EnsureVideoSubsystem's init log is genuinely once-per-process for both target games
Status: DONE — SUPERSEDED by TASK-24H-1227 (this session): while building TASK-24H-1104's quiet-by-default test, discovered this log actually WAS refiring more than once per process (every create/destroy/recreate-all-windows cycle test tooling exercises, exactly the scenario this task's own Problem section flagged as the theoretical risk) and gated it behind FreeApiDiagnosticsEnabled() rather than merely documenting the informal "today it doesn't happen in real gameplay" assumption this task originally scoped. This is a strictly stronger outcome than the comment-only fix originally planned: the log is now genuinely quiet by default regardless of window-lifecycle usage, not just quiet under a currently-true but unenforced assumption about both games' call patterns. See src/internal/FreeApiSdlVideo.cpp and TASK-24H-1227 for detail.
Priority: P2
Area: Diagnostics
Type: Verification
Evidence: src/internal/FreeApiSdlVideo.cpp:13-42 (log lines :20,25); docs/audit-24h-free-api.md §4.8, §4.29
Depends on: None

Problem:
`EnsureVideoSubsystem`'s init-success/failure logs are unconditional, classified in the audit as "legitimately rare, startup-once." However, its design is lazy and idempotent (`ShutdownVideoSubsystemIfLastWindow` resets `g_videoInitialized = false` whenever the last window is destroyed), meaning any code path that destroys and recreates all windows during a session would cause this log to re-fire more than once. This session traced both games' only `DestroyWindow`/`CreateWindowEx` call sites and confirmed neither is a runtime toggle — so today this is genuinely startup-once, but the finding should be documented rather than left as an unstated assumption.

Required work:
- Confirm via the games' source that neither destroys and recreates windows during normal gameplay (already traced this session; re-verify if source has changed).
- Add a code comment on `EnsureVideoSubsystem`/`ShutdownVideoSubsystemIfLastWindow` in `src/internal/FreeApiSdlVideo.cpp` noting this log is startup-once given current evidenced usage, and that a future caller which tears down/recreates all windows at runtime would need to revisit this gating decision.

Acceptance criteria:
- A comment documenting the once-per-process claim and its evidence exists next to the log calls.
- No behavior change; existing tests still pass unmodified.
- No unrelated API is added.

Out of scope:
- Do not add gating unless new evidence shows this actually re-fires during normal play — today it does not.
- Do not modify the lazy-init/idempotent-shutdown design itself.

---

### TASK-24H-1107: Document FreeApiGdi.cpp's SDL_ConvertSurface failure log as compliant, intentional error-path logging
Status: DONE — added a comment above the log call (src/internal/FreeApiGdi.cpp) stating it's an intentional, permanent unconditional error log per project policy. No behavior change. Verified 25/25 in all three build trees.
Priority: P2
Area: Diagnostics
Type: Documentation
Evidence: src/internal/FreeApiGdi.cpp:35
Depends on: None

Problem:
`CreateCompatBitmapFromSurface`'s `SDL_ConvertSurface` failure log at `src/internal/FreeApiGdi.cpp:35` is unconditional, which is compliant with the quiet-by-default policy (it is a genuine failure path, not a success/startup path), but it has never been explicitly documented as an intentional exception versus an oversight.

Required work:
- Add a one-line comment above the log call in `src/internal/FreeApiGdi.cpp` stating this is an intentional, permanent unconditional error log (failure path only, matches project policy of "errors stay visible by default").
- No code/behavior change.

Acceptance criteria:
- The comment exists and accurately reflects the call's actual behavior (failure-path only, never on the success path).
- No unrelated code is touched; existing tests still pass unmodified.

Out of scope:
- Do not gate this log — failure-path logging without a flag is explicitly allowed by policy.
- Do not change `CreateCompatBitmapFromSurface`'s conversion logic.

---

### TASK-24H-1108: Verify winmm.cpp's timeSetEvent/timeKillEvent logs stay bounded to startup/failure frequency
Status: DONE — confirmed via grep that planetblupi has zero timeSetEvent/timeKillEvent call sites (uses SetTimer/WM_TIMER instead) and free-eggbert has exactly one each (blupi.cpp:891,628). Added a doc comment above timeSetEvent (src/winmm.cpp) citing this. NOTE: timeSetEvent's success log is now gated (TASK-24H-1227/1113), stronger than this task's original "document as-is" scope; the remaining unconditional logs (invalid-args, SDL_INIT_EVENTS-failed, SDL_AddTimer-failed, timeKillEvent's unknown-id warning) are failure paths, confirmed to fire at most once per process per the call-site count above. Verified 25/25 in all three build trees.
Priority: P2
Area: Diagnostics
Type: Verification
Evidence: src/winmm.cpp:119,124,141,153 (timeSetEvent), :173 (timeKillEvent); free-eggbert/src/blupi.cpp:628,891 (sole call sites)
Depends on: None

Problem:
`timeSetEvent`'s success log (`:153`) and its invalid-args/subsystem-init/timer-creation failure logs (`:119,124,141`), plus `timeKillEvent`'s unknown-ID warning (`:173`), are all unconditional. This session confirmed free-eggbert calls `timeSetEvent` exactly once (`blupi.cpp:891`, at init) and `timeKillEvent` exactly once (`blupi.cpp:628`, at shutdown), making these genuinely startup/shutdown-once for free-eggbert. planetblupi does not appear to use `timeSetEvent`/`timeKillEvent` at all (it uses `SetTimer`/`WM_TIMER` instead) — this should be explicitly confirmed and recorded rather than assumed.

Required work:
- Confirm planetblupi has zero `timeSetEvent`/`timeKillEvent` call sites (grep its source tree).
- Add a code comment near `timeSetEvent`/`timeKillEvent` in `src/winmm.cpp` documenting the confirmed once-per-process call pattern for free-eggbert and the confirmed non-usage by planetblupi, so the unconditional logs are understood as a deliberate, evidenced exception rather than an oversight.

Acceptance criteria:
- The comment exists and cites the exact call sites confirmed.
- No behavior change; existing tests (`test_timer_regressions`, `test_eggbert_loop`) still pass unmodified.
- No unrelated API is added.

Out of scope:
- Do not gate these logs unless new evidence shows repeated calls during normal play.
- Do not fix the unrelated `timeKillEvent` return-code inconsistency (TASK-24H-0501) — that is a correctness cosmetic issue, not a logging issue.

---

### TASK-24H-1109: Prevent MidiMusic.cpp's audio-backend-init failure logs from repeating on every subsequent MCI_OPEN
Status: DONE — added MidiState::backendInitFailed latch (src/MidiMusic.cpp), checked at the top of EnsureMidiBackend() and set on both failure branches; failure still logs once (not silenced), just deduplicated on repeat. New dedicated test binary tests/test_midi_backend_failure.cpp (isolated process -- the latch is permanent for the process lifetime, so it can't safely share a binary with other passing MCI tests): forces a real SDL_InitSubSystem(AUDIO) failure via SDL_AUDIODRIVER=<bogus>, calls MCI_OPEN("sequencer") 5x, asserts all 5 fail AND the failure log (captured via SDL_SetLogOutputFunction) fires exactly once. Verified passing (23/23 suite, standalone build).
Priority: P2
Area: Diagnostics
Type: Bugfix
Evidence: src/MidiMusic.cpp:418-440 (EnsureMidiBackend, guard at :420, failure logs at :423,432), called from :563 on every MCI_OPEN
Depends on: None

Problem:
`EnsureMidiBackend`'s early-out guard (`if (g_midi.device != 0) return true;`, `:420`) only short-circuits after a *successful* init. If `SDL_InitSubSystem(SDL_INIT_AUDIO)` or `SDL_OpenAudioDeviceStream` fails, `g_midi.device` stays at its unset value, so every subsequent `MCI_OPEN` call (one per song/track load, invoked many times during a normal playthrough, not just at startup) re-invokes `EnsureMidiBackend` and re-logs the same unconditional failure message. On a machine with no usable audio device, the failure log can fire dozens of times across a play session rather than once — a real "fires repeatedly without opt-out" gap, distinct from the genuinely-rare sites in this theme.

Required work:
- Add a latch (e.g. `g_midi.backendInitFailed`) set on first failure and checked at the top of `EnsureMidiBackend`, so the failure logs (`:423`,`:432`) fire at most once per process, matching the already-logged-once behavior of a successful init.
- Ensure `EnsureMidiBackend` still returns `false` on every call after the first failure (behavior unchanged) — only the logging frequency changes.
- Keep the log message text and format unchanged; deduplicate repeated identical failures, do not delete the information.

Acceptance criteria:
- Simulating a persistent audio init failure and calling `MCI_OPEN` multiple times produces exactly one failure log line, not one per call.
- `EnsureMidiBackend` still returns `false` on every call after the first failure.
- Existing tests (`test_mci_sequences`) still pass unmodified.
- No unrelated API is added.

Out of scope:
- Do not silence the failure entirely — it must still log once, per the "do not change error visibility" rule.
- Do not add a retry mechanism or attempt to recover the audio backend automatically — that is a new feature, not a logging fix.

---

### TASK-24H-1110: Document _lopen/CreateDirectoryA failure-log call-frequency bounds
Status: DONE — added comments above both logs (src/winbase_file.cpp). _lopen: confirmed each game has exactly one call site (ddutil.cpp), a single non-looping bitmap-open, so its failure log cannot fire more than once per bitmap-load attempt -- matches this task's original premise. CreateDirectoryA: this task's own premise was PARTLY WRONG -- re-verified this session and found planetblupi's AddUserPath (misc.cpp) call site is NOT compiled out (only free-eggbert's is, behind "#if _CD || _LEGACY", never defined); planetblupi's is live, called from decio.cpp's save/load paths. Corrected the comment to state the real, verified finding instead of repeating the task's stale "never fires today" claim. Still an acceptable unconditional failure log either way (real filesystem failures are rare by nature). Verified 25/25 in all three build trees.
Priority: P2
Area: Diagnostics
Type: Verification
Evidence: src/winbase_file.cpp:37-57 (_lopen, log at :49), :97-129 (CreateDirectoryA, log at :125); free-eggbert &amp; planetblupi's sole call sites; include/windows.h:140-173 (free_api_fopen, the comparison case-fallback pattern)
Depends on: None

Problem:
`_lopen`'s and `CreateDirectoryA`'s failure logs are unconditional. The audit flagged `_lopen` as a possible concern if a game probes several candidate filenames/cases per logical file open (as `free_api_fopen` does internally, without logging, in `include/windows.h`). This session traced both concerns to ground: each game has exactly one `_lopen` call site (`ddutil.cpp`), not a multi-candidate probe loop, so its failure log cannot fire more than once per bitmap-load attempt; and both games' only `CreateDirectoryA` call sites are inside `AddUserPath`, itself entirely compiled out (`#if _CD || _LEGACY`, both macros never defined anywhere in either game's build), so `CreateDirectoryA`'s failure log never fires in the actual shipped build at all today.

Required work:
- Add a code comment near `_lopen`'s failure log in `src/winbase_file.cpp` documenting the confirmed single-call-site-per-game finding.
- Add a code comment near `CreateDirectoryA`'s failure log documenting that its only known call sites in both games are currently dead code, so the log's real-world frequency today is zero, and this should be re-checked if `_CD`/`_LEGACY` are ever defined by a build variant.

Acceptance criteria:
- Both comments exist and cite the exact evidence (call-site file/line, macro-undefined confirmation).
- No behavior change; existing tests (`test_file_regressions`, `test_file_paths`) still pass unmodified.
- No unrelated API is added.

Out of scope:
- Do not gate these logs — both are confirmed-rare (or currently unreachable) failure paths, compliant with policy as-is.
- Do not remove or refactor the dead `_CD`/`_LEGACY` code path in the target games' own source — that is out of free-api's repo scope entirely.

---

### TASK-24H-1111: Document mciSendCommandA's AVI-decline log as a confirmed once-per-process startup log
Status: DONE — added a comment above the log (src/winmm.cpp) citing the confirmed call graph: CMovie::Create() has a single call site in each game (blupi.cpp), calls initAVI() exactly once, and permanently latches m_bEnable=FALSE on failure so later playback attempts never reach mciSendCommandA again. Re-verified this session directly against planetblupi's movie.cpp/blupi.cpp source. Verified 25/25 in all three build trees.
Priority: P2
Area: Diagnostics
Type: Verification
Evidence: src/winmm.cpp:299-303 (MCI_OPEN avivideo decline log at :300); planetblupi/src/movie.cpp:231-244, free-eggbert/src/movie.cpp:234-247 (CMovie::Create()'s permanent m_bEnable=FALSE latch after the first initAVI() failure)
Depends on: None

Problem:
`mciSendCommandA`'s AVI-decline informational log (`"MCI_OPEN device-type-only (avivideo) — video playback not implemented..."`, `:300`) is unconditional and, on first read, looks like it could fire once per movie-start attempt — both games queue multiple movies over a playthrough. This session traced the actual call graph and confirmed it does not: `CMovie::Create()` calls `initAVI()` exactly once at startup and permanently latches `m_bEnable = FALSE` on the first failure; every later `StartMovie()` call short-circuits before ever reaching `mciSendCommandA` again. So this log is genuinely startup-once for both games, not a per-movie/per-level repeat.

Required work:
- Add a code comment above `src/winmm.cpp:299-303` documenting this confirmed once-per-process behavior and citing the `CMovie::Create()`/`m_bEnable` latch mechanism that guarantees it, so a future reader doesn't need to re-derive this.

Acceptance criteria:
- The comment exists and accurately describes the confirmed call pattern.
- No behavior change; existing tests (`test_mci_avivideo_regressions`) still pass unmodified.
- No unrelated API is added.

Out of scope:
- Do not gate this log — it is confirmed startup-once and informational about a permanent, deliberate design decision, which should stay visible.
- Do not change the AVI-decline behavior itself or revisit the AVI-unsupported decision (already a settled, evidenced decision).

---

### TASK-24H-1112: Consolidate reviewed rare-logging dispositions into docs/out-of-scope.md
Status: DONE — added "Unconditional logging: reviewed and intentional sites" section listing all 6 reviewed sites with file:line and rationale, matching the code comments added by TASK-24H-1106-1111. No code changed. Verified 26/26 in all three build trees.
Priority: P3
Area: Diagnostics
Type: Documentation
Evidence: docs/audit-24h-free-api.md §4.29, §5 item 6; docs/out-of-scope.md; TASK-24H-1106 through TASK-24H-1111
Depends on: TASK-24H-1106, TASK-24H-1107, TASK-24H-1108, TASK-24H-1109, TASK-24H-1110, TASK-24H-1111

Problem:
After TASK-24H-1106 through 1111 land, the individual code-comment justifications for each "reviewed, compliant, rare" unconditional log site are scattered across 5 different source files. There is no single place a future contributor or auditor can check to see "which unconditional logs in this codebase are intentional and why" without re-reading all of them.

Required work:
- Add a short new section to `docs/out-of-scope.md` titled something like "Unconditional logging: reviewed and intentional sites", listing each of the 6 reviewed sites (`FreeApiSdlVideo.cpp` init, `FreeApiGdi.cpp` conversion failure, `winmm.cpp` timeSetEvent/timeKillEvent, `MidiMusic.cpp` audio-backend failure, `winbase_file.cpp` `_lopen`/`CreateDirectoryA`, `winmm.cpp` mciSendCommandA AVI decline) with a one-line rationale and file:line reference each.

Acceptance criteria:
- The new doc section exists, lists all 6 reviewed items with accurate file:line references, and matches the code comments added by the depended-on tasks.
- No code is changed by this task.

Out of scope:
- Do not re-litigate any of the already-settled dispositions from TASK-24H-1106 through 1111.
- Do not document the two gated fixes (TASK-24H-1101/1102) here — those are behavior changes, not "reviewed and kept as-is" sites.

---

### TASK-24H-1113: Add a source-scan guard against future ungated SDL_Log additions
Status: DONE — added tests/test_sdl_log_gating.cpp (new CTest binary), a text-scan heuristic over src/**/*.cpp recognizing 4 gating shapes actually used in this codebase (same-line "if (GATE) SDL_Log(...)", block "if (GATE) { SDL_Log(...) }", early-return "if (!GATE) return;" earlier in the same function, and "#if defined(__ANDROID__)" compile-time guards), plus a short explicit file:line allowlist for the 16 confirmed-intentional unconditional failure/startup/warning sites. Found and fixed 2 real, previously-uncaught ungated logs while building it: winmm.cpp's timeSetEvent success-path log (see its own doc comment) and confirmed EnsureVideoSubsystem's fix from TASK-24H-1227 was needed. Verified with a genuine negative control (temporarily injected an ungated SDL_Log, confirmed the test caught it, reverted). Verified 25/25 in all three build trees. NOTE: the allowlist is file:line-precise, not content-hash-based -- an unrelated edit that shifts line numbers above an allowlisted site will require updating its entry; this is an accepted tradeoff of the "simple heuristic, not full static analysis" scope this task specifies.
Priority: P2
Area: Diagnostics
Type: Test
Evidence: docs/audit-24h-free-api.md §4.29, §5 item 6; tests/ (no existing static-scan test)
Depends on: TASK-24H-1101, TASK-24H-1102, TASK-24H-1103

Problem:
This theme's tasks fix all currently-known unconditional hot/normal-run `SDL_Log` sites, but nothing in the test suite prevents a future contributor from re-introducing an ungated `SDL_Log` call in a frequently-hit code path. A lightweight, low-maintenance guard would catch that class of regression cheaply, without requiring a new logging framework.

Required work:
- Add a small test (script or C++ test executable, consistent with the existing test suite's style) that scans `src/**/*.cpp` for `SDL_Log(` call sites and flags any that are not immediately preceded by (or wrapped in) one of the recognized gates: `FreeApiDiagnosticsEnabled()`, `FreeApiDiagnosticsFastEnabled()`, `FreeApiGdiDebugEnabled()`, `midiDebugEnabled()`/`MIDI_LOG`, `g_debugInput`/`InputLog`, or an explicit `#if defined(__ANDROID__)` compile-time guard.
- Maintain a short, explicit allowlist (in the test itself, with a comment citing this theme's tasks) for the confirmed-intentional unconditional failure/startup sites documented by TASK-24H-1106 through 1111, so the test only fails on genuinely new, undocumented additions.
- Register the new test in `CMakeLists.txt` alongside the existing test targets.

Acceptance criteria:
- The test passes against the current (post-1101/1102/1103) codebase.
- Introducing a new, ungated `SDL_Log` call anywhere in `src/**/*.cpp` outside the allowlist causes the test to fail with a clear message naming the offending file:line.
- Existing tests still pass unmodified.
- No unrelated API is added.

Out of scope:
- Do not attempt full static analysis (e.g. control-flow-aware gating detection) — a simple, documented allowlist plus a "preceding gate check" heuristic is sufficient and keeps the test maintainable.
- Do not fail the build on Android-only (`#if defined(__ANDROID__)`) logging sites; those are compile-time scoped to a platform outside the default Linux target-game build and are not part of this theme's audit sweep.

## Documentation

### TASK-24H-1201: Fix README.md "Project Structure" section's stale single-file `src/winapi.cpp` claim
Status: DONE — replaced the "src/ -> winapi.cpp" leaf with the real current file layout (all src/*.cpp files by area, src/internal/*, and winapi.cpp noted as the empty redirect-comment file). Verified `grep -n "└── winapi.cpp" README.md` returns no match. Verified 25/25 in all three build trees.
Priority: P2
Area: Docs
Type: Documentation
Evidence: README.md:207-219; docs/audit-24h-free-api.md §1 item 7, §3 intro; src/winapi.cpp:1-31
Depends on: None

Problem:
README.md's "Project Structure" section shows `src/` containing only `winapi.cpp` as the implementation file. This has been stale since commit `51b75e6` split the file into ~25 files (`winbase.cpp`, `winuser_window.cpp`, `wingdi_blit.cpp`, `src/internal/*.cpp`, etc.); `src/winapi.cpp` today is a 32-line file containing only a doc-comment redirect map to its ~20 successor files.

Required work:
- Replace the `src/` line(s) in README.md's "Project Structure" code block with a layout that reflects reality — either list the real top-level `src/*.cpp` files and the `src/internal/` subdirectory, or point readers at `src/winapi.cpp`'s own doc-comment map (which is already accurate) instead of duplicating the full file list.

Acceptance criteria:
- `grep -n "└── winapi.cpp" README.md` (or equivalent stale single-file claim) returns no match.
- The "Project Structure" section's `src/` entry either lists real files present in `ls src/*.cpp src/internal/*.cpp` or explicitly defers to `src/winapi.cpp`'s doc comment for the authoritative, current map.
- No behavior change; documentation only.

Out of scope:
- Do not rewrite unrelated README sections.
- Do not change `src/winapi.cpp`'s own redirect comment (already accurate).

---

### TASK-24H-1202: Reword README.md's MIDI looping and cdaudio "TODO" claims to match confirmed-working/intentional behavior
Status: DONE — reworded both bullets to state confirmed-working/intentional behavior instead of "TODO". Verified both stale-text greps return no match. Verified 25/25 in all three build trees.
Priority: P2
Area: Docs
Type: Documentation
Evidence: README.md:162-168; docs/audit-24h-free-api.md §4.26; plan.md TASK-0008
Depends on: None

Problem:
README.md's "Limitations / TODOs" list under "MIDI / MCI Music" says `"Looping (MCI_PLAY with loop flag): TODO — playback stops at end-of-song"`, which this session's audit disproves: both games' `MM_MCINOTIFY` handlers re-issue playback themselves on song-end, so music genuinely loops end-to-end in real gameplay. The same list's `"CD audio (cdaudio): TODO — gracefully declined"` overstates a confirmed-intentional, permanent, working decline as unfinished "TODO" work. This is a distinct, user-facing wording fix from `plan.md` TASK-0008/TASK-24H-0901, which only fix the internal `src/MidiMusic.cpp` source comment.

Required work:
- Reword the looping bullet to state that both games handle looping themselves via `MM_MCINOTIFY` re-triggering, and that native MCI-level looping is intentionally unimplemented because it's never needed — not a "TODO".
- Reword the cdaudio bullet to state it is a deliberate, permanent, working decline, not an open TODO.

Acceptance criteria:
- `grep -n "TODO — playback stops at end-of-song"` and `grep -n "TODO — gracefully declined"` against README.md return no match.
- The reworded bullets accurately state both behaviors are confirmed-working/intentional, not gaps.
- No code or behavior change; documentation only.

Out of scope:
- Do not implement native MCI-level looping.
- Do not edit `src/MidiMusic.cpp` (covered by `plan.md` TASK-0008 / TASK-24H-0901).

---

### TASK-24H-1203: Update README.md's input-pipeline "Verified:" line to reflect current test coverage
Status: DONE — rewrote README.md's "Verified:" line to mention the full VK_* sweep, the F10 SYSKEY quirk, the unmapped-scancode case, boundary/negative lParam packing, and the 2000-event stress test, not just the original 5 message types. No test coverage added, documentation only.
Priority: P3
Area: Docs
Type: Documentation
Evidence: README.md:94; tests/test_input_pipeline.cpp:274-356
Depends on: None

Problem:
README.md's input pipeline section states `"Verified: WM_MOUSEMOVE, WM_LBUTTONDOWN/UP, WM_RBUTTONDOWN, WM_KEYDOWN (VK_SPACE), WM_KEYUP (VK_ESCAPE)."`, which understates current coverage. `tests/test_input_pipeline.cpp` now covers a full `VK_*` sweep, the F10→`WM_SYSKEYDOWN`/`WM_SYSKEYUP` quirk, and boundary/negative `lParam` packing plus a 2000-event stress test.

Required work:
- Rewrite the "Verified:" line to list the actual current coverage: the base message types already listed, the full `VK_*` sweep, the F10 SYSKEY quirk, boundary/negative lParam packing, and the stress test — at whatever level of summary keeps the line readable.

Acceptance criteria:
- The "Verified:" line (or its replacement) mentions the `VK_*` sweep, the F10 quirk, and the stress test, not just the five original message types.
- No behavior change; documentation only.

Out of scope:
- Do not add new test coverage — this task only documents what already exists.

---

### TASK-24H-1204: Fix Documentation.md's stale single-file `src/winapi.cpp` claims
Status: DONE — updated the architecture diagram to reference the real successor files/directories, and replaced the reading-order table's single winapi.cpp row with rows naming the real successor files per responsibility (message queue, GDI, timers, path normalization, diagnostics), plus a final row noting winapi.cpp is now just an empty redirect-comment file. Verified both stale-text greps return no match. Verified 25/25 in all three build trees.
Priority: P2
Area: Docs
Type: Documentation
Evidence: Documentation.md:14-24, :26-44 (specifically lines 21 and 42); src/winapi.cpp:1-31
Depends on: None

Problem:
Documentation.md has two stale references to `src/winapi.cpp` as if it were still the live implementation file: the architecture diagram (line 21) and the "Detailed documentation" reading-order table (line 42, claiming `src/winapi.cpp` contains "message queue, SDL event translation, GDI internals, timer internals, path normalization, diagnostics"). `src/winapi.cpp` is now an empty 32-line redirect-comment file; the real implementation lives across ~25 successor files.

Required work:
- Update the architecture diagram (line 21) to name the real successor files/directories (e.g. `src/*.cpp` + `src/internal/*.cpp`) instead of `src/winapi.cpp`.
- Update the reading-order table row (line 42) to either list the real successor files that hold each responsibility (message queue → `src/internal/FreeApiMessageQueue.cpp`, GDI internals → `src/internal/FreeApiGdi.cpp`/`src/wingdi_*.cpp`, timers → `src/internal/FreeApiTimers.cpp`/`src/winuser_timer.cpp`/`src/winmm.cpp`, path normalization → `src/internal/FreeApiPath.cpp`, diagnostics → `src/internal/FreeApiDiagnostics.cpp`) or point at `src/winapi.cpp`'s own accurate redirect-comment map.

Acceptance criteria:
- `grep -n "implemented in src/winapi.cpp"` and `grep -n "src/winapi.cpp.*message queue, SDL event translation"` against Documentation.md return no match.
- The reading-order table's replacement row(s) match real file names present in `ls src/*.cpp src/internal/*.cpp`.
- No behavior change; documentation only.

Out of scope:
- Do not restructure the rest of Documentation.md's reading-order table beyond the `winapi.cpp` row.

---

### TASK-24H-1205: Document `WM_ACTIVATEAPP(0)` focus-loss suppression in docs/out-of-scope.md
Status: DONE — duplicate of TASK-24H-0206; implemented once there (see that entry).
Priority: P2
Area: Docs
Type: Documentation
Evidence: src/internal/FreeApiMessageQueue.cpp:276-286; docs/audit-24h-free-api.md §4.6, §5 item 11; docs/out-of-scope.md
Depends on: None

Problem:
`src/internal/FreeApiMessageQueue.cpp` deliberately never translates `SDL_EVENT_WINDOW_FOCUS_LOST` into `WM_ACTIVATEAPP(0)`, to avoid the game freezing on spurious focus-loss events — a real, permanent, working Win32-behavior deviation. `docs/out-of-scope.md` does not currently document it. NOTE: duplicates TASK-24H-0206 (WinUser message-loop theme) — implement once, close both.

Required work:
- Add a new entry to `docs/out-of-scope.md`, matching the doc's existing style, describing the `WM_ACTIVATEAPP(0)` suppression: what triggers it, why, and that it's permanent and tested.
- Cite `src/internal/FreeApiMessageQueue.cpp:276-286`.

Acceptance criteria:
- `docs/out-of-scope.md` contains a section mentioning `WM_ACTIVATEAPP` and focus-loss suppression.
- The new entry follows the same problem/evidence/decision structure as the existing "MCI digital-video" section.
- No behavior change; documentation only.

Out of scope:
- Do not change the suppression behavior itself.
- Do not add real `WM_ACTIVATEAPP(0)` delivery — the suppression is confirmed intentional and must stay.

---

### TASK-24H-1206: Document the rationale for `include/windows.h`'s global `#define fopen free_api_fopen` override
Status: DONE — duplicate of TASK-24H-0113; implemented once there (see that entry).
Priority: P2
Area: Docs
Type: Documentation
Evidence: include/windows.h:140,175; docs/headers.md; docs/audit-24h-free-api.md §3.16
Depends on: None

Problem:
`include/windows.h` globally redefines the standard-library `fopen` symbol for every consuming C++ translation unit — a broad-reaching mechanism for a narrow path-normalization need. `docs/headers.md` does not currently explain the rationale. NOTE: duplicates TASK-24H-0113 (Scope theme) — implement once, close both.

Required work:
- Add a short, explicit rationale note to `docs/headers.md` explaining why `fopen` is globally redefined rather than requiring callers to opt in via a differently-named function: both target games call plain `fopen` directly throughout their existing source, so a macro override is the only way to get path normalization without editing either game's source.

Acceptance criteria:
- `docs/headers.md` contains a note explicitly discussing the `#define fopen free_api_fopen` mechanism and its rationale.
- No behavior change; documentation only.

Out of scope:
- Do not change the macro mechanism itself.
- Do not rename or remove `free_api_fopen`.

---

### TASK-24H-1207: Cross-reference the new 24-hour audit's AVI feasibility research from docs/out-of-scope.md
Status: DONE — duplicate of TASK-24H-0909; implemented once there (see that entry).
Priority: P3
Area: Docs
Type: Documentation
Evidence: docs/out-of-scope.md:9-48; docs/audit-24h-free-api.md §1, §5 item 15, §6
Depends on: None

Problem:
`docs/out-of-scope.md`'s "MCI digital-video / AVI movie playback" section does not reference this session's independent feasibility re-confirmation. NOTE: duplicates TASK-24H-0909 (WinMM theme) — implement once, close both.

Required work:
- Add one sentence to the AVI section of `docs/out-of-scope.md` cross-referencing `docs/audit-24h-free-api.md` (§1/§5/§6) as an independent re-confirmation of the same decline decision.

Acceptance criteria:
- `grep -n "audit-24h-free-api" docs/out-of-scope.md` returns at least one match.
- The AVI section's decision text and evidence remain otherwise unchanged.
- No behavior change; documentation only.

Out of scope:
- Do not duplicate the full feasibility research into `docs/out-of-scope.md` — a pointer only.
- Do not reopen the AVI implementation question.

---

### TASK-24H-1208: Create docs/testing.md consolidating the scattered "how to run tests" instructions
Status: DONE — created docs/testing.md: standalone/target-game build+test invocations, the SDL_VIDEODRIVER=dummy/SDL_AUDIODRIVER=dummy requirement and why, running one test binary directly, the ExtractStringTable.cmake negative-path reproduction command, sanitizer runs, and the free-direct standalone check. Linked from Documentation.md's Build section (also fixed its stale ctest command to include the dummy-driver vars) and README.md's Build Instructions. NEXT.md §7's commands intentionally left intact per this task's own out-of-scope clause.
Priority: P2
Area: Docs
Type: Documentation
Evidence: NEXT.md:271-308; Documentation.md:75-81; README.md:85-94
Depends on: TASK-24H-0009

Problem:
Instructions for running Free API's test suite are currently scattered across `NEXT.md` §7 (the most complete set, but a rotating handoff document), `Documentation.md`, and `README.md`. `NEXT.md` has no permanent home in `docs/`. NOTE: overlaps with TASK-24H-0009 (Build theme, which consolidates build/test commands into docs/cmake-options.md) — coordinate so testing-specific narrative lives here and command reference lives there, without duplicating content.

Required work:
- Create `docs/testing.md` consolidating: the standalone build+test invocation, the two target-game build+test invocations, the `SDL_VIDEODRIVER=dummy`/`SDL_AUDIODRIVER=dummy` requirement and why it's needed, running one specific test binary directly, and the CMake string-table verification reproduction command.
- Add a pointer to `docs/testing.md` from `Documentation.md`'s "Build" section and README.md's build instructions.

Acceptance criteria:
- `docs/testing.md` exists and contains runnable `ctest`/`cmake --build` command blocks.
- `Documentation.md` and/or README.md link to `docs/testing.md`.
- Existing tests still pass.

Out of scope:
- Do not remove the commands from `NEXT.md` §7 — that section can stay as a session-log convenience.
- Do not add new test infrastructure.

---

### TASK-24H-1209: Create a target-game verification / playtest doc, incorporating the newly-found planetblupi gameplay-access sequence
Status: DONE — created docs/target-game-verification.md: launch instructions for both games, MIDI-audio sign-off checklist (cross-references TASK-24H-1221), rendering/blitting/image-loading sign-off checklist (cross-references TASK-24H-1222), the LoadStringA/"RES_<id>" check, the save/load check, both planetblupi gameplay-access sequences (Enter x4 for MK_SHIFT, Enter x2 + click "Privé"/"Build" for MK_CONTROL, both linking to docs/target-games.md's full derivation), and the MK_SHIFT/MK_CONTROL playtest steps themselves (cross-references TASK-24H-0401/0403). NEXT.md updated to link here instead of duplicating steps. Documentation only, no behavior change.
Priority: P2
Area: Docs
Type: Documentation
Evidence: docs/audit-24h-free-api.md §1 item 4, §2; NEXT.md §8 items 5-7
Depends on: None

Problem:
There is currently no `docs/` file describing how to manually verify either target game beyond scattered checklist items in `NEXT.md` §8. This session's audit found the exact keyboard path (`Enter × 4` from process launch) to reach live planetblupi gameplay, which should make the `MK_SHIFT`/`MK_CONTROL` playtest (TASK-24H-0401/0403) tractable but is not yet written up anywhere durable.

Required work:
- Create `docs/target-game-verification.md` describing: how to launch each target game against the current free-api tree, the rendering/MIDI-audio sign-off checks already listed in `NEXT.md` §8 items 5-6, and the planetblupi `Enter × 4` keyboard sequence to reach live gameplay.
- Cross-reference this doc from `NEXT.md` §8's relevant items so the checklist doesn't duplicate the how-to steps.

Acceptance criteria:
- `docs/target-game-verification.md` exists and contains the `Enter × 4` sequence and both games' launch instructions.
- `NEXT.md` §8 items 5-7 link to the new doc rather than re-describing the steps inline.
- No behavior change; documentation only.

Out of scope:
- Do not perform the actual human playtest as part of this task — see TASK-24H-0401/0403.
- Do not touch joystick, MCI digital-video, DirectDraw, DirectSound, DirectPlay, or `free-direct` code.

---

### TASK-24H-1210: Document `PeekMessageA`'s filter-ignoring behavior as intentional
Status: DONE — duplicate of TASK-24H-0201; implemented once there (see that entry).
Priority: P2
Area: Docs
Type: Documentation
Evidence: src/winuser_message.cpp:25-29; include/winuser.h:264-265; docs/supported-apis.md:28; docs/audit-24h-free-api.md §4.7, §5 item 9
Depends on: None

Problem:
`PeekMessageA` fully ignores its filter arguments — harmless today but undocumented as intentional. NOTE: duplicates TASK-24H-0201 (WinUser message-loop theme) — implement once, close both.

Required work:
- Update `include/winuser.h`'s `PeekMessageA` doc comment to explicitly state that filter arguments are currently ignored, and that this is acceptable because no evidenced call site passes non-zero filters.
- Update `docs/supported-apis.md`'s `PeekMessageA` row to note the filter-ignoring behavior explicitly rather than a bare `"IMPLEMENTED"`.

Acceptance criteria:
- `include/winuser.h`'s `PeekMessageA` doc comment mentions that filter arguments are ignored.
- `docs/supported-apis.md`'s `PeekMessageA` row text changes to reflect the same.
- No behavior change; existing tests still pass.

Out of scope:
- Do not implement real filter support without an evidenced call site passing non-zero filters.

---

### TASK-24H-1211: Document `WaitMessage`'s polling-based (not true blocking) contract explicitly
Status: DONE — duplicate of TASK-24H-0204; implemented once there (see that entry).
Priority: P2
Area: Docs
Type: Documentation
Evidence: src/winuser_message.cpp:235-257; include/winuser.h:277-278; docs/supported-apis.md:29; docs/audit-24h-free-api.md §4.7, §5 item 10
Depends on: None

Problem:
`WaitMessage` is not a true blocking wait — functionally adequate for both games' current idle-loop usage, but undocumented as such. NOTE: duplicates TASK-24H-0204 (WinUser message-loop theme) — implement once, close both.

Required work:
- Update `include/winuser.h`'s `WaitMessage` doc comment to state the actual contract: checks the queue, sleeps ~1ms once if empty, then returns `TRUE` unconditionally.
- Update `docs/supported-apis.md`'s `WaitMessage` row to match, replacing "non-busy idle wait" with wording that makes the unconditional-`TRUE`-return behavior explicit.

Acceptance criteria:
- `include/winuser.h`'s `WaitMessage` doc comment explicitly states it returns `TRUE` unconditionally rather than blocking until real work arrives.
- `docs/supported-apis.md`'s `WaitMessage` row reflects the same.
- No behavior change; existing tests still pass.

Out of scope:
- Do not implement a true blocking wait without new evidence that either game's behavior depends on it.

---

### TASK-24H-1212: Document `GetSystemMetrics`'s fixed-value rationale explicitly
Status: DONE — duplicate of TASK-24H-0307; implemented once there (see that entry). Also aligned docs/supported-apis.md's status label with the header's PARTIAL tag.
Priority: P3
Area: Docs
Type: Documentation
Evidence: src/winuser_misc.cpp:57; include/winuser.h:360-361; docs/supported-apis.md:27; docs/audit-24h-free-api.md §5 item 4
Depends on: None

Problem:
`GetSystemMetrics` returns fixed values for `SM_CXSCREEN`/`SM_CYSCREEN`/`SM_CYCAPTION` — confirmed correct, but `docs/supported-apis.md`'s row currently just says `"IMPLEMENTED"` (inconsistent with the header's own `@note Status: PARTIAL` tag) with no rationale. NOTE: overlaps with TASK-24H-0307 (Window/Cursor theme) — implement once, close both.

Required work:
- Update `include/winuser.h`'s `GetSystemMetrics` doc comment to note that only `SM_CXSCREEN`/`SM_CYSCREEN`/`SM_CYCAPTION` are handled with fixed, confirmed-correct values.
- Update `docs/supported-apis.md`'s `GetSystemMetrics` row to align its status label with the header's `PARTIAL` tag and add the fixed-value rationale.

Acceptance criteria:
- `include/winuser.h`'s `GetSystemMetrics` doc comment mentions the three confirmed-used indices and that values are fixed by design.
- `docs/supported-apis.md`'s `GetSystemMetrics` row status label matches the header's `@note Status:` tag.
- No behavior change; existing tests still pass.

Out of scope:
- Do not implement real display-metric queries for other `SM_*` indices without an evidenced call site.

---

### TASK-24H-1213: Document `MoveWindow`'s confirmed-non-issue status (only call site is dead-reach)
Status: DONE — extended include/winuser.h's MoveWindow doc comment to note the dead-reach call-site finding, cross-referencing docs/out-of-scope.md (TASK-24H-0309). Verified 26/26 in all three build trees.
Priority: P3
Area: Docs
Type: Documentation
Evidence: src/winuser_window.cpp:233-243; include/winuser.h:333-334; docs/audit-24h-free-api.md §5 item 5
Depends on: None

Problem:
`MoveWindow` not updating other logical window state is a confirmed non-issue (its only live call site in either game is inside the dead-reach AVI-movie code path), but `include/winuser.h`'s existing doc comment doesn't mention this finding. NOTE: overlaps with TASK-24H-0309 (Window/Cursor theme) — implement once, close both.

Required work:
- Extend `include/winuser.h`'s `MoveWindow` doc comment to note that its only evidenced call site in either game is inside the AVI-movie code path, which is itself unreachable, so the current SDL-position/size-only behavior is confirmed sufficient.

Acceptance criteria:
- `include/winuser.h`'s `MoveWindow` doc comment mentions the dead-reach call-site finding.
- No new row is added to `docs/supported-apis.md` by this task (that 28-symbol gap is `plan.md` TASK-0001's job).
- No behavior change; existing tests still pass.

Out of scope:
- Do not add a `docs/supported-apis.md` row for `MoveWindow` — covered by `plan.md` TASK-0001 and TASK-24H-0313.
- Do not change `MoveWindow`'s implementation.

---

### TASK-24H-1214: Document `GetDeviceCaps`'s index-ignored scope in docs/supported-apis.md
Status: DONE — duplicate of TASK-24H-0604; implemented once there (see that entry), including the docs/supported-apis.md row update this task specifically asked for.
Priority: P3
Area: Docs
Type: Documentation
Evidence: src/wingdi_misc.cpp:5-15; include/wingdi.h:110-114; docs/supported-apis.md:42; docs/audit-24h-free-api.md §4.18
Depends on: None

Problem:
`GetDeviceCaps` returns `0` for literally any `index` value, not just `SIZEPALETTE`. `include/wingdi.h`'s doc comment already states this scoping clearly, but `docs/supported-apis.md`'s row just says `"IMPLEMENTED"` with no such qualifier. NOTE: overlaps with TASK-24H-0604 (GDI theme) — implement once, close both.

Required work:
- Update `docs/supported-apis.md`'s `GetDeviceCaps` row to state explicitly that `index` is ignored for every value (always returns 0), matching `include/wingdi.h`'s existing doc-comment precision.

Acceptance criteria:
- `docs/supported-apis.md`'s `GetDeviceCaps` row text explicitly states the function returns 0 regardless of `index`, not just for `SIZEPALETTE`.
- No behavior change; existing tests still pass.

Out of scope:
- Do not change `GetDeviceCaps`'s behavior for any index.

---

### TASK-24H-1215: Add an explicit keep-rationale note for `OutputDebugStringW` in docs/out-of-scope.md
Status: DONE — duplicate of TASK-24H-0109; implemented once there (see that entry).
Priority: P2
Area: Docs
Type: Verification
Evidence: include/debugapi.h; docs/audit-24h-free-api.md §3.3, §5 item 7
Depends on: None

Problem:
`OutputDebugStringW` is a real, working `W`-suffixed function with zero evidenced call site in either target game. NOTE: duplicates TASK-24H-0109 (Scope theme) — implement once, close both.

Required work:
- Re-confirm via grep (`grep -rn "OutputDebugStringW" ../free-eggbert/src ../planetblupi/src`) that neither game calls it.
- Add an explicit note to `docs/out-of-scope.md`'s "Unicode / `W`-suffixed API variants" section naming `OutputDebugStringW` specifically: real (non-stub), zero evidenced call site, kept rather than downgraded because doing so costs nothing.

Acceptance criteria:
- `grep -n "OutputDebugStringW" docs/out-of-scope.md` returns at least one match.
- No behavior change; existing tests still pass.

Out of scope:
- Do not downgrade `OutputDebugStringW` to a stub as part of this task.
- Do not implement real wide-character behavior elsewhere.

---

### TASK-24H-1216: Add an explicit "intentionally-unused-but-kept" decision note for `_chdir`/`_getcwd`
Status: DONE — duplicate of TASK-24H-0110; implemented once there (see that entry).
Priority: P2
Area: Docs
Type: Documentation
Evidence: src/crt_direct.cpp:18-27; include/direct.h:21-22; docs/headers.md:19; docs/audit-24h-free-api.md §3.5, §5 item 8
Depends on: None

Problem:
`_chdir`/`_getcwd` have zero call sites in either target game. `docs/headers.md`'s `direct.h` row already notes they're "confirmed unused," but doesn't state the explicit keep-and-document decision. NOTE: duplicates TASK-24H-0110 (Scope theme) — implement once, close both.

Required work:
- Extend `docs/headers.md`'s `direct.h` row to state the explicit decision: `_chdir`/`_getcwd` are kept and documented as intentionally-unused, not removed, per the project's default policy.

Acceptance criteria:
- `docs/headers.md` contains explicit "keep, documented, intentionally-unused" language for `_chdir`/`_getcwd`.
- No behavior change; existing tests still pass.

Out of scope:
- Do not remove `_chdir`/`_getcwd` from `include/direct.h`/`src/crt_direct.cpp`.

---

### TASK-24H-1217: Add missing `@note Status:` doc-comment tags to basestd.h, mciapi.h, and winerror.h
Status: DONE — duplicate of TASK-24H-0108; implemented once there (see that entry), which also covers minwindef.h and the 3 include_non_windows/ files this task didn't mention.
Priority: P3
Area: Docs
Type: Cleanup
Evidence: include/basestd.h:9-14; include/mciapi.h:8; include/winerror.h:8-17; docs/audit-24h-free-api.md §3
Depends on: None

Problem:
`basestd.h`, `mciapi.h`, and `winerror.h` have no `@note Status:` annotations at all, unlike their fully-annotated siblings. NOTE: duplicates TASK-24H-0108/TASK-24H-0906 — implement once, close all three.

Required work:
- Add `@note Status: HEADER_ONLY` (or the appropriate tag) doc comments to each typedef in `include/basestd.h`.
- Add the same to `MCIDEVICEID` in `include/mciapi.h`.
- Add the same to `E_FAIL`, `ERROR_ALREADY_EXISTS`, `ERROR_INVALID_PARAMETER` in `include/winerror.h`.

Acceptance criteria:
- `grep -c "@note Status:" include/basestd.h include/mciapi.h include/winerror.h` each return a non-zero count matching the number of declarations in that file.
- Existing tests still pass (`test_header_compile.cpp` in particular).
- No behavior change; comment-only.

Out of scope:
- Do not add annotations to `include_non_windows/sys/timeb.h`, `include_non_windows/Windows.h`, or `include_non_windows/WinUser.h` — trivial pure-`#include` shims, covered by TASK-24H-0108.
- Do not change any declaration's actual type or signature.

---

### TASK-24H-1218: Add a pointer to docs/audit-24h-free-api.md from Documentation.md's doc index
Status: DONE — added the pointer to Documentation.md's introductory section, alongside the existing plan.md/supported-apis.md pointers. Verified 26/26 in all three build trees.
Priority: P3
Area: Docs
Type: Documentation
Evidence: Documentation.md:1-12; docs/audit-24h-free-api.md
Depends on: None

Problem:
`docs/audit-24h-free-api.md` is a substantial new audit document produced this session, but `Documentation.md`'s top-level index has no pointer to it.

Required work:
- Add one sentence/link to `Documentation.md`'s introductory section pointing at `docs/audit-24h-free-api.md`, describing it as the latest deep-audit findings and task backlog source, alongside the existing `plan.md`/`docs/supported-apis.md` pointers.

Acceptance criteria:
- `grep -n "audit-24h-free-api" Documentation.md` returns at least one match.
- The existing `docs/scope.md`/`docs/supported-apis.md`/`plan.md` pointers are unchanged.
- No behavior change; documentation only.

Out of scope:
- Do not restructure Documentation.md's introduction beyond adding this pointer.

---

### TASK-24H-1219: Document the free-direct bridge-exception symbols consumed without public header declarations
Status: DONE — folded into TASK-24H-0102's docs/scope.md edit (same subsection covers all four documented bridge functions plus the ReadRssKB internal-reach case).
Priority: P2
Area: Docs
Type: Documentation
Evidence: docs/scope.md:33-54; docs/audit-24h-free-api.md §1 item 2, §3.23, §5 item 2; src/wingdi_dc.cpp:12,30,44; ../free-direct/src/directdraw/DirectDraw.cpp:22,1149,1189,1191,1373; src/internal/Diagnostics.cpp:18,72
Depends on: None

Problem:
`docs/scope.md`'s "Boundary with `free-direct`" section describes the GDI-vs-DirectX symbol-family split but does not currently list the actual bridge-exception symbols `free-direct` consumes without any public header declaration. NOTE: overlaps with TASK-24H-0102 (Scope theme) — implement once, close both.

Required work:
- Add a subsection to `docs/scope.md`'s "Boundary with `free-direct`" section listing the four confirmed bridge-exception points by name (`FreeApiRunWinMain` — already publicly declared; `FreeApiCreateSurfaceDC`, `FreeApiDestroySurfaceDC`, `FreeApiSetWindowFullscreen` — consumed but undeclared in any public header; `FreeApi::Platform::ReadRssKB` — an internal implementation detail reached directly) with their current file locations.

Acceptance criteria:
- `docs/scope.md` names all four non-`FreeApiRunWinMain` symbols explicitly.
- The new subsection distinguishes the three GDI-bridge functions from the `ReadRssKB` internal-reach case.
- No behavior change; documentation only.

Out of scope:
- Do not add the actual public header declarations — that is TASK-24H-0101's job (code change); this task documents current state only.
- Do not modify `../free-direct` source.

---

### TASK-24H-1220: Document `HFONT`/`HPALETTE` as intentionally-vestigial-but-harmless in include/minwindef.h
Status: DONE — duplicate of TASK-24H-0111; implemented once there (see that entry).
Priority: P3
Area: Docs
Type: Documentation
Evidence: include/minwindef.h:94-104; docs/audit-24h-free-api.md §3.9
Depends on: None

Problem:
`HFONT` and `HPALETTE` are declared as plain, undocumented typedefs in `include/minwindef.h`, with no consuming function anywhere. NOTE: duplicates TASK-24H-0111 (Scope theme) — implement once, close both.

Required work:
- Add a short doc comment above `HFONT` and `HPALETTE` in `include/minwindef.h` noting they are vestigial (no consuming function exists in the current public surface), kept for compile-compatibility/harmlessness rather than active use.

Acceptance criteria:
- `include/minwindef.h` has a comment near `HFONT`/`HPALETTE` explicitly stating they are currently unused by any function signature.
- Existing tests still pass (`test_header_compile.cpp` in particular).
- No behavior change; comment-only.

Out of scope:
- Do not remove `HFONT`/`HPALETTE`.
- Do not add any font/palette-creation API.

---

### TASK-24H-1221: Human-playtest MIDI audio sign-off for both target games
Status: TODO
Priority: P1
Area: WinMM
Type: Verification
See also: docs/target-game-verification.md §2 (the same checklist below, formalized as a runnable checklist, TASK-24H-1209)
Evidence: NEXT.md (multiple sessions' prose mentions "MIDI audio... still needed" under human playtests, with no formal task ever filed for it); TASK-24H-1109 (MCI_OPEN log-spam fix, session 4); src/MidiMusic.cpp; docs/README's "SoundFont requirement" section
Depends on: None

Problem:
Every session since the original 24-hour audit has listed "MIDI audio" as an outstanding human-verification item in `NEXT.md`'s prose, but no `TASK-24H-*` entry was ever filed for it — `TASK-24H-1209`'s evidence line cites "NEXT.md §8 items 5-6" for this, but that citation doesn't resolve to anything concrete in any preserved version of `NEXT.md` (found by session 4's independent audit). MIDI playback has real automated coverage for the mechanical MCI_OPEN/PLAY/CLOSE sequence (`tests/test_mci_sequences.cpp`) and for the specific log-spam regression this session fixed (`tests/test_midi_backend_failure.cpp`), but neither of those hears actual audio or exercises a real soundfont-equipped system across a real play session — only a human with real speakers/headphones and a real audio device can confirm music is actually audible, isn't corrupted/glitching, and that the MCI_OPEN log-spam fix holds up outside a synthetic single-process test.

Required work (a human playtest, not an implementation task):
- Ensure a `.sf2` SoundFont is available at one of the documented lookup locations (see README's "SoundFont requirement" section) or set `FREE_API_SOUNDFONT`.
- Launch `../free-eggbert`'s built game; confirm the process starts and reaches a screen/state where background MIDI music would normally play (per that game's own menu/level flow). Listen for audible, non-corrupted music (not silence-by-error, not garbled/stuttering playback).
- Launch `../planetblupi`'s built game; repeat the same audible-music check for its own menu/gameplay music.
- For both games, watch the console/log output (run with `FREE_API_DEBUG_MIDI=1` for detail if needed) across a real session involving multiple level/track loads (e.g. navigating menus, starting and restarting missions) and confirm no repeated `[midi] SDL_InitSubSystem(AUDIO) failed`/`SDL_OpenAudioDeviceStream failed` log spam appears more than once per process, per `TASK-24H-1109`'s fix (this can only be fully exercised on a machine with a genuinely unavailable audio device — if the test machine's audio works fine, note that the log-spam scenario itself wasn't triggered and only confirm normal playback instead).
- Record the exact steps and pass/fail outcome for both games separately in `NEXT.md`.

Acceptance criteria:
- Both target games are confirmed to start successfully as part of this playtest (a prerequisite check, not the focus).
- MIDI/background music is confirmed audible and non-corrupted in both games (or the absence of a soundfont is confirmed to degrade gracefully to silence, not an error/crash, per documented behavior).
- No repeated `MCI_OPEN`-triggered audio-backend-failure log spam is observed across a real multi-level-load session in either game.
- Outcome (pass/fail, with specifics) is recorded in `NEXT.md`, distinctly for each game.
- If any behavior fails to match the expected (audible music, or graceful silent fallback, or non-repeating failure logs), a new follow-up bug task is filed citing the exact observed vs. expected behavior.
- No source or test files are modified as part of performing this playtest itself (recording the outcome in `NEXT.md` is the only expected file change).

Out of scope:
- Do not implement MCI digital-video/AVI playback as part of this task — permanently out of scope (see `docs/out-of-scope.md`).
- Do not add new MIDI/MCI APIs, new debug flags, or change `MidiState`'s backend-failure latch as part of this playtest — this task verifies existing, already-implemented behavior only.
- Do not attempt this task headlessly/via SDL's dummy audio driver — that cannot produce or verify actually-audible sound; a real audio backend and real listening are required.

---

### TASK-24H-1222: Human-playtest rendering/asset-loading/save-load sign-off for both target games
Status: TODO
Priority: P1
Area: GDI
Type: Verification
See also: docs/target-game-verification.md §§3-5 (the same checklist below, formalized as a runnable checklist, TASK-24H-1209)
Evidence: NEXT.md (multiple sessions' prose mentions "rendering" as still needed under human playtests, with no formal task ever filed for it); TASK-24H-1109/0605/0601 (this cycle's real GDI/rendering-path fixes); docs/out-of-scope.md's "RES_<id>" placeholder note; docs/scope.md
Depends on: None

Problem:
Like MIDI audio (`TASK-24H-1221`), "rendering" has been mentioned as an outstanding human-verification item in `NEXT.md`'s prose across multiple sessions but was never filed as a real, trackable task. Free API's GDI/blit subsystem has extensive automated coverage of individual functions in isolation (`tests/test_gdi_regressions.cpp`) — including this cycle's flagship `StretchBlt` Y-clamp fix (`TASK-24H-0601`) and this session's `LoadImageA` path-normalization fix (`TASK-24H-0605`) — but no test can confirm the *visual, end-to-end* result actually looks correct on screen across a real play session, nor that `LoadStringA`'s real string-table data never surfaces its `"RES_<id>"` debug placeholder in shipped game UI (`docs/out-of-scope.md`'s explicit "never acceptable in shipped game UI" rule), nor that save/load round-trips still work when driven through a real interactive session rather than a synthetic test fixture.

Required work (a human playtest, not an implementation task):
- Launch `../free-eggbert`'s built game; confirm the process starts, the window appears, and sprite/background assets visibly render (menus, in-level graphics) without corruption, wrong colors, missing tiles, or obviously misplaced content.
- Launch `../planetblupi`'s built game; repeat the same visual check, including its level-editor/build-mode screen if convenient (exercises the `CreateBitmap` 8-bit greyscale-only path documented as a known limitation in fullscreen mode — note if this is visibly wrong, though it's already a known, accepted gap).
- For both games, watch all visible UI text (menus, buttons, tooltips) across normal navigation and confirm no string ever displays as a literal `"RES_<id>"` placeholder (e.g. `"RES_106"`) — every real UI string should show real, readable text.
- For both games, exercise a real save and load cycle (or the closest equivalent each game exposes — e.g. planetblupi's mission/private-level save slots) and confirm the game resumes in the expected state, not a crash or corrupted/blank state.
- Record the exact steps and pass/fail outcome for both games separately in `NEXT.md`.

Acceptance criteria:
- Both target games are confirmed to start successfully as part of this playtest (a prerequisite check, not the focus).
- Rendering/blitting/image loading is confirmed visually correct (no corruption, wrong colors, or missing assets) in both games.
- No `"RES_<id>"` placeholder string is observed in either game's shipped UI during normal navigation.
- A real save/load cycle is confirmed to work correctly in at least one of the two games (both if the tester has time).
- Outcome (pass/fail, with specifics) is recorded in `NEXT.md`, distinctly for each game.
- If any behavior fails to match the expected (correct rendering, no `RES_<id>` leakage, working save/load), a new follow-up bug task is filed citing the exact observed vs. expected behavior.
- No source or test files are modified as part of performing this playtest itself (recording the outcome in `NEXT.md` is the only expected file change).

Out of scope:
- Do not fix `CreateBitmap`'s known 8-bit greyscale-only limitation as part of this task — already documented, accepted, out of scope (`TASK-24H-0602`/`0603`).
- Do not fix `FreeApiDestroySurfaceDC`'s garbage-pointer segfault as part of this task — already documented, accepted, out of scope.
- Do not add any new GDI/rendering API, debug flag, or resource-loading behavior as part of this playtest — this task verifies existing, already-implemented behavior only.
- Do not attempt this task headlessly/via SDL's dummy video driver — that cannot produce or verify actually-correct visual output; a real display backend and real looking are required.

---

### TASK-24H-1223: Add direct test coverage for FreeApiRunWinMain (real process-bootstrap bridge)
Status: DONE — added tests/test_winmain_bridge.cpp (new binary): asserts a NULL entryPoint is rejected with -1 and never invoked; asserts the full real contract (hInstance/hPrevInstance always NULL, nCmdShow always SW_SHOW, argv[1..] joined into lpCmdLine excluding argv[0], entryPoint's return value passes through unchanged); asserts lpCmdLine is NULL (not an empty string) when there are no extra arguments. Needed a dummy WinMain definition to satisfy the linker (src/winmain_bridge.cpp's weak `main()` references it; pulling in FreeApiRunWinMain's object file from the static library also drags in that reference, regardless of the weak main() losing to this test's own strong main() -- confirmed dead code, documented in the test file). Found and fixed a REAL bug in the test itself during verification: the first version compared lpCmdLine's content AFTER FreeApiRunWinMain had already returned, a genuine use-after-free (lpCmdLine points into a local std::string inside FreeApiRunWinMain, small-string-optimized onto that function's own stack frame, valid only for the duration of the entryPoint(...) call -- exactly matching real Win32 WinMain's lpCmdLine lifetime contract) that happened to "work" in the standalone and free-eggbert build trees but failed in planetblupi's, depending on whether anything clobbered the stale stack memory first. Fixed by capturing the content into a fixed buffer inside FakeWinMain itself, while the pointer is still valid.
Priority: P1
Area: Build
Type: Test
Evidence: src/winmain_bridge.cpp:50-89 (FreeApiRunWinMain); include/windows.h:89,99 (FREE_API_WINMAIN_PROC/declaration); ../free-direct/... FREE_API_IMPLEMENT_WINMAIN() macro usage (real bootstrap path for both games); found by a session-4 strict test-coverage audit fork -- zero prior coverage, flagged as "the single highest-risk untested function" since a regression here means a real game fails to launch entirely, not a wrong pixel
Depends on: None

Problem:
`FreeApiRunWinMain` is the real process-bootstrap bridge both target games launch through (via `include/windows.h`'s `FREE_API_IMPLEMENT_WINMAIN()` macro, used by the `../free-direct` bridge), but had zero automated test coverage: `tests/test_eggbert_loop.cpp` and `tests/test_planetblupi_loop.cpp` both bypass it entirely with a hand-written `int main()`.

Required work:
- Add direct tests asserting: NULL entryPoint rejection; the full argv/argc -> hInstance/hPrevInstance/lpCmdLine/nCmdShow contract; exit-code passthrough; the NULL-vs-empty-string lpCmdLine distinction when there are no extra arguments.

Acceptance criteria:
- New tests directly exercise `FreeApiRunWinMain`, not just indirectly via a game loop test.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not change `FreeApiRunWinMain`'s actual behavior — test-only task; no bug was found in the real function itself (only in this test's own first draft, fixed before landing).
- Do not add Android-specific (`__ANDROID__`) test coverage — out of scope for this session's Linux-only test environment.

---

### TASK-24H-1224: Add direct test coverage and documentation for wsprintfA
Status: DONE — added TestWsprintfAFormatsAndHandlesEdgeCases (tests/test_winuser_regressions.cpp): exercises the exact real-usage shape both games call (`wsprintfA(buf, "Data1 : %d, dwdata: %d, pFile: %d", ...)`, verified via grep against both games' sound.cpp/soundbass.cpp), NULL lpOut/lpFmt safety, and the internal 1024-byte buffer-edge truncation behavior (return value reports the untruncated would-be length per vsnprintf's own contract, but the actual written buffer content is safely bounded). Also added a docs/supported-apis.md row -- this function wasn't documented anywhere (neither supported-apis.md nor out-of-scope.md) despite being real, live logic on both games' sound-diagnostic paths.
Priority: P1
Area: WinUser
Type: Test
Evidence: src/winuser_message.cpp:277-288 (wsprintfA); ../free-eggbert/src/soundbass.cpp:142, sound.cpp:117; ../planetblupi/src/sound.cpp:99 (identical 3x-%d call shape in both games); found by a session-4 strict test-coverage audit fork -- real, live, used-by-both-games logic with zero test coverage and zero documentation trail
Depends on: None

Problem:
`wsprintfA` is a real variadic `vsnprintf` wrapper (format-string/buffer-edge risk class), live on both games' sound-diagnostic paths, but had zero test coverage and wasn't listed in `docs/supported-apis.md` or `docs/out-of-scope.md` at all.

Required work:
- Add a direct test matching both games' real call shape, plus NULL-argument safety and buffer-edge truncation coverage.
- Add a `docs/supported-apis.md` row.

Acceptance criteria:
- New test directly exercises `wsprintfA`, matching real game usage.
- `docs/supported-apis.md` documents the symbol.
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not change `wsprintfA`'s implementation — test/documentation-only task; no bug was found in the real function.
- Do not add support for format specifiers beyond what `vsnprintf` already provides (no custom Win32-specific format extensions) — none is evidenced by either game's real usage.

---

### TASK-24H-1225: Add direct test coverage and documentation for OutputDebugStringA
Status: DONE — added TestOutputDebugStringAPrintsToStdoutAndIsNullSafe (tests/test_winuser_regressions.cpp, POSIX-only): verifies NULL-input safety and captures real stdout output via a dup2-based fd redirect to prove the exact printed content, matching both games' real call shape (plain string literal, no format specifiers). Hit and fixed two genuine, instructive bugs in the TEST itself while verifying across all three build trees (not in the real OutputDebugStringA implementation, which needed no changes): (1) the test's own read-back `fopen(absolutePath, "r")` call was silently rewritten by free-api's own global `#define fopen free_api_fopen` macro (this test file includes <windows.h>), which strips a leading slash from absolute paths -- turning the read-back into an unfindable relative lookup, while mkstemp/open/access (not macro-redirected) all correctly saw the real file the whole time, making this a confusing, inconsistent-looking failure. Fixed by using a relative temp filename throughout. (2) stdout is fully buffered when not a TTY, so a Check() call's printf output sitting unflushed in the buffer immediately before the dup2 redirect got flushed into the temp file ahead of (and, since fgets only reads one line, instead of) the real target content once fflush ran after the redirect. Fixed with an explicit fflush() immediately before dup2. Documented both as a general test-authoring caveat in docs/headers.md's fopen section, since any future test using an absolute path with a raw fopen() call in a file that includes <windows.h> would hit the same silent redirection.
Priority: P1
Area: WinBase
Type: Test
Evidence: src/winbase.cpp:64-68 (OutputDebugStringA); ../free-eggbert/src/misc.cpp:32; ../planetblupi/src/wave.cpp:234,264,268 (both games' real DirectSound-failure diagnostic call sites, plain string literals); found by a session-4 strict test-coverage audit fork -- real, live logic with zero test coverage, completely unclassified in both docs/supported-apis.md and docs/out-of-scope.md
Depends on: None

Problem:
`OutputDebugStringA` is real, live logic (a null-checked `printf` to stdout, not a stub) on both games' DirectSound-failure diagnostic paths, but had zero test coverage and wasn't documented anywhere at all.

Required work:
- Add a direct test verifying NULL-input safety and the actual printed content, matching both games' real call shape.

Acceptance criteria:
- New test directly exercises `OutputDebugStringA`, verifying real output content, not just "doesn't crash".
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not change `OutputDebugStringA`'s implementation — test-only task; no bug was found in the real function (only in this test's own first two drafts, both fixed before landing).
- Do not add Windows-specific coverage for this POSIX-only test technique (dup2-based fd redirection) — out of scope for this session's Linux-only test environment.

---

### TASK-24H-1226: Add direct test coverage for SetWindowTextA
Status: DONE — added TestSetWindowTextASetsRealWindowTitle (tests/test_winuser_regressions.cpp): creates a real window, calls SetWindowTextA, and verifies via SDL_GetWindowTitle that the real SDL window's title actually changed (not just "doesn't crash"); also covers NULL hWnd (returns FALSE) and NULL lpString (treated as empty title, not an error). Companion to TASK-24H-0313 (which added the missing docs/supported-apis.md row) -- the audit fork that found this gap noted 0313 only tracked the documentation gap, not a missing test.
Priority: P1
Area: WinUser
Type: Test
Evidence: src/winuser_window.cpp:293-302 (SetWindowTextA); ../free-eggbert/src/blupi.cpp:532,541; ../planetblupi/src/blupi.cpp:453,462 (both games' real window-title-setting call sites); found by a session-4 strict test-coverage audit fork -- real, live logic (a genuine SDL_SetWindowTitle call) with zero test coverage
Depends on: None

Problem:
`SetWindowTextA` is real, live logic used by both games to set their window title, but had zero test coverage — only documented as a missing row (`TASK-24H-0313`), never as a missing test.

Required work:
- Add a direct test that creates a real window, calls `SetWindowTextA`, and verifies the actual SDL window title changed via `SDL_GetWindowTitle`.

Acceptance criteria:
- New test directly exercises `SetWindowTextA`, verifying the real title change, not just "doesn't crash".
- Existing tests still pass.
- No unrelated API is added.

Out of scope:
- Do not change `SetWindowTextA`'s implementation — test-only task; no bug was found in the real function.

---

### TASK-24H-1227: Gate EnsureVideoSubsystem's unconditional "SDL video initialized" log behind FreeApiDiagnosticsEnabled()
Status: DONE — wrapped the success-path SDL_Log in FreeApiDiagnosticsEnabled(), matching TASK-24H-1101/1102's exact pattern; left the failure-path log ("SDL_INIT_VIDEO failed") unconditional, matching the established success-gated/failure-unconditional convention. Found while building TASK-24H-1104's quiet-by-default test: EnsureVideoSubsystem tears down the video subsystem via ShutdownVideoSubsystemIfLastWindow whenever the last registered window is destroyed, and re-initializes (re-logging unconditionally) on the next CreateWindowExA -- so this log was not just a one-time startup log but could refire on every create/destroy/recreate cycle. Confirmed harmless for real gameplay (neither game destroys all its windows mid-session, so this only ever fired once in practice), but was a real, previously-uncaught gap in the same family as TASK-24H-1101/1102. Verified 24/24 in all three build trees.
Priority: P2
Area: Diagnostics
Type: Bugfix
Evidence: src/internal/FreeApiSdlVideo.cpp:19-25 (EnsureVideoSubsystem), :36-42 (ShutdownVideoSubsystemIfLastWindow); found by TASK-24H-1104's new quiet-by-default test failing against the pre-fix code
Depends on: None

Problem:
`EnsureVideoSubsystem` (`src/internal/FreeApiSdlVideo.cpp`) logs "free-api EnsureVideoSubsystem: SDL video initialized" unconditionally on every successful `SDL_InitSubSystem(SDL_INIT_VIDEO)` call, with no `FreeApiDiagnosticsEnabled()` gate — unlike the rest of the codebase's disciplined gating (TASK-24H-1101/1102). Because `ShutdownVideoSubsystemIfLastWindow` tears the subsystem down whenever the last registered window is destroyed, this log is not a true one-time-per-process startup log: it refires every time the window count cycles from zero back to one, with no way to silence it short of a code change.

Required work:
- Wrap the success-path `SDL_Log` call in `if (FreeApiDiagnosticsEnabled()) { ... }`.
- Leave the failure-path `SDL_Log` ("SDL_INIT_VIDEO failed") unconditional, matching the established convention that failure-path logs stay visible by default.

Acceptance criteria:
- A `CreateWindowExA`→`DestroyWindow` cycle repeated with zero windows remaining in between produces zero `SDL_Log` output with no diagnostics env var set.
- Setting `FREE_API_DIAGNOSTICS=1` restores the log exactly as before.
- Existing tests still pass in all three build trees.
- No unrelated API is added.

Out of scope:
- Do not change `ShutdownVideoSubsystemIfLastWindow`'s teardown behavior — this task only gates the log, not the subsystem lifecycle.

---

### TASK-24H-1228: Classify and document CloseHandle (include/handleapi.h) as vestigial-but-harmless
Status: DONE — added a row to docs/out-of-scope.md's Compile-only stubs table, and a correction note to TASK-24H-0114's own Status line. Verified 26/26 in all three build trees.
Priority: P3
Area: Headers
Type: Documentation
Evidence: include/handleapi.h; src/winbase.cpp:59 (definition, only reference anywhere); found by TASK-24H-0115's public-surface classification audit, docs/public-surface-audit.md
Depends on: None

Problem:
`CloseHandle` (`include/handleapi.h`) has zero call sites anywhere: not in `../free-eggbert` or `../planetblupi`, not in any `tests/*.cpp` file, and not in free-api's own `src/` beyond its own definition (`src/winbase.cpp:59`). It doesn't appear in `docs/out-of-scope.md`'s "Compile-only stubs" table, has no row in `docs/supported-apis.md`, and `TASK-24H-0114`'s own problem text (which established the "test-infrastructure-only" exception category) listed it alongside `GetLastError`/`SetLastError`/`RemoveDirectoryA`/`SetEnvironmentVariableA` as if it were test-infrastructure-only too -- but unlike those four, no test actually calls it. Its real classification is `vestigial-but-harmless` (proven unused by both games AND by tests), and it is currently undocumented as such anywhere.

Required work:
- Add a row for `CloseHandle` to `docs/out-of-scope.md`'s "Compile-only stubs" table (or a new short standalone entry, matching the file's existing style), stating it is proven unused anywhere (games, tests, and free-api's own internal code) and is kept only for Win32 header-shape compatibility.
- Correct `TASK-24H-0114`'s own problem text (or add a note) so it no longer implies `CloseHandle` is test-infrastructure-only.

Acceptance criteria:
- `docs/out-of-scope.md` carries a discoverable entry for `CloseHandle`'s true (vestigial-but-harmless) classification.
- `docs/public-surface-audit.md`'s "Mismatches found" section is updated to note this has been resolved, or the row is left as historical record with a pointer to the fix.
- No behavior change; documentation only.

Out of scope:
- Do not remove `CloseHandle` from the header -- removal is a separate decision (see `docs/out-of-scope.md`'s "Removal/hiding policy for proven-unused symbols"), not automatic just because this task documents its true status.
- Do not re-audit any other symbol as part of this task -- scoped to `CloseHandle` only.

---

## Deep Audit Follow-up (audit.md, 2026-07-09)

The 16 tasks below (`TASK-24H-1229`-`1244`) formalize `audit.md`'s "Proposed Tasks" section (§9) into the standard backlog format. `audit.md` was produced by 5 independent parallel reviews (correctness, performance, memory safety, edge cases, architectural risk) against commit `fdc38bf`. Every task below cites the specific audit section/finding ID as evidence; re-verify the underlying claim against current source before implementing, since source may have moved since the audit was written. None of these are implemented yet -- `audit.md`'s own conclusion is explicit that no currently-live game behavior is broken; these are hardening/correctness/process improvements, prioritized by the audit's own severity assessment translated into this project's P0-P3 scale (P1 reserved for gaps in APIs both games actually use with a realistic trigger; P2/P3 for lower-likelihood or purely defensive items).

### TASK-24H-1229: Clear the GDI handle "magic number" before delete to close a double-free path
Status: DONE -- `dc->magic = 0;`/`bitmap->magic = 0;` added immediately before `delete` in `DeleteDC`, `FreeApiDestroySurfaceDC` (src/wingdi_dc.cpp), and `DeleteObject` (src/wingdi_bitmap.cpp); zero is not `kCompatDcMagic`/`kCompatBitmapMagic`, so `AsCompatDC`/`AsCompatBitmap`'s existing validation already rejects a cleared handle with no logic changes needed. New regression test `TestDoubleDeleteIsRejectedNotDoubleFreed` (tests/test_gdi_regressions.cpp) double-deletes an `HBITMAP`, an `HDC` (via `CreateCompatibleDC`/`DeleteDC`), and a surface DC (via `FreeApiCreateSurfaceDC`/`FreeApiDestroySurfaceDC`), asserting the second call on each returns FALSE. Amendment to the acceptance criteria as originally written: the new test does NOT run under `FREE_API_SANITIZE=address` or `=thread` as literally stated -- verifying under ASan surfaced that `AsCompatDC`/`AsCompatBitmap`'s validation must dereference the (now-freed, magic-cleared) handle to determine it's invalid, and both ASan and TSan flag that read of freed memory as a heap-use-after-free by design, regardless of the value read or that the actual double-free is genuinely prevented (the second `delete` is provably never reached). This is an inherent limitation of the raw-pointer-with-in-struct-tag handle scheme, not a flaw in the fix, and closing it would require the handle-table/generation-counter redesign this task's own "Out of scope" section explicitly declines. Resolved with a portable compile-time skip-guard (`__SANITIZE_ADDRESS__`/`__has_feature(address_sanitizer)`/`__SANITIZE_THREAD__`/`__has_feature(thread_sanitizer)`) around the test's call site, with a doc comment explaining why, so the rest of the suite still runs clean under both sanitizers. Incidentally found and fixed via LeakSanitizer during this verification: an unrelated pre-existing resource leak in `TestGetSetPixelRoundTripOnMemoryDcWithSelectedBitmap` (missing `DeleteDC`/`DeleteObject` cleanup). Verified 26/26 passing in: standalone build/, build-asan/ (FREE_API_SANITIZE=address, new test correctly SKIPped, no leaks), build-tsan/ (FREE_API_SANITIZE=thread, new test correctly SKIPped, no use-after-free), ../free-eggbert/cmake-build-debug (Ninja), ../planetblupi/build (Make). (test_winuser_regressions/test_mci_sequences transient failures seen without SDL_VIDEODRIVER=dummy are the pre-existing, already-documented Wayland-display/PulseAudio-internals environment artifacts noted elsewhere in this file -- unrelated to this change, confirmed by also failing identically on an unmodified standalone build.)
Priority: P1
Area: GDI
Type: Bugfix
Evidence: src/wingdi_dc.cpp (`DeleteDC`, `FreeApiDestroySurfaceDC`); src/wingdi_bitmap.cpp (`DeleteObject`); src/internal/FreeApiGdi.cpp (`AsCompatDC`/`AsCompatBitmap` magic-number validation); audit.md §3 Finding C1 / §5 Finding M1 (independently found by both the correctness and memory-safety reviews)
Depends on: None

Problem:
`AsCompatDC`/`AsCompatBitmap` validate a handle by dereferencing it and checking an in-struct magic number, but none of `DeleteDC`/`DeleteObject`/`FreeApiDestroySurfaceDC` clear that magic number before calling `delete`. Freed heap memory is not guaranteed to be overwritten immediately, so a double-delete of the same handle (`DeleteObject(bmp); DeleteObject(bmp);`) can pass the magic-number check a second time against already-freed memory, causing a genuine double-free (heap corruption), not a fail-safe rejection. No existing test exercises double-deletion of the same handle. This is a different, narrower case than the already-declined "garbage/never-valid pointer" fix in `docs/out-of-scope.md` (`FreeApiDestroySurfaceDC`'s documented "do not fix with a general handle-validation/table framework" decision) -- the fix here does not require that framework.

Required work:
- Set the `magic` field to zero (or a distinct tombstone value) immediately before `delete` in `DeleteDC`, `DeleteObject`, and `FreeApiDestroySurfaceDC`.
- Add a regression test that calls each of the three delete functions twice on the same handle and asserts the second call is rejected safely (matching whatever return-value contract the function already documents for an invalid handle), not just "doesn't crash under the test's specific allocator behavior" -- consider running this test under `FREE_API_SANITIZE=address` specifically, since ASan reliably detects a double-free that a plain debug build might not visibly manifest.

Acceptance criteria:
- A double-delete of the same `HDC`/`HBITMAP`/surface-DC handle no longer risks a double-free; the second call is rejected the same way an already-invalid handle is.
- New test passes under both the standalone build and `FREE_API_SANITIZE=address`.
- Existing tests still pass in all three build trees.
- No unrelated API is added; no change to the single-valid-delete behavior for any handle.

Out of scope:
- Do not build a general handle-table/generation-counter framework -- this task closes the double-free specifically, not the broader "garbage pointer" class already explicitly declined elsewhere.
- Do not change `AsCompatDC`/`AsCompatBitmap`'s validation logic beyond what's needed to recognize a cleared/tombstoned magic value as invalid (it likely already does, if zero is not `kCompatDcMagic`/`kCompatBitmapMagic` -- verify first).

---

### TASK-24H-1230: Document wsprintfA's buffer-size hazard (1024-byte internal cap vs. real call sites' 256-byte buffers)
Status: DONE -- added a prominent doc comment on `wsprintfA`'s declaration (include/winuser.h) stating it writes up to 1024 bytes regardless of the caller's actual buffer size, and that any new call site's expected output must stay under its OWN buffer's size. Added a matching, more detailed note to its `docs/supported-apis.md` row, also covering the pointer-as-%d inherited-UB detail for future-maintainer context. Documentation only, no behavior change. Verified standalone build/ still compiles and 26/26 tests pass (part of this session's TASK-24H-1230/1231/1233/1234/1236 batch verification).
Priority: P2
Area: WinUser
Type: Documentation
Evidence: src/winuser_message.cpp (`wsprintfA`, `vsnprintf(lpOut, 1024, ...)`); ../free-eggbert/src/soundbass.cpp:142, sound.cpp:117 (`char holder[256]`); ../planetblupi/src/sound.cpp:99 (same shape); audit.md §3 Finding C4 / §5 Finding M3 / §6 Finding E1 (independently found by three of the five reviews)
Depends on: None

Problem:
`wsprintfA` writes up to 1024 bytes via `vsnprintf`, matching real Win32 `wsprintfA`'s own historically-unsafe no-length-parameter contract. All three real call sites declare a 256-byte stack buffer -- 768 bytes smaller than what `wsprintfA` will write into if given the chance. Currently safe only because the fixed format string (`"Data1 : %d, dwdata: %d, pFile: %d"`) cannot realistically produce more than ~60 bytes of output. A future call site with a longer format string and a buffer under 1024 bytes would get a genuine, silent stack-buffer overflow, and there is no way to fix this in the implementation without breaking the deliberate real-Win32-compatible signature (no size parameter exists to bound against). Separately, both real call sites pass pointer arguments (`pData1`, `pFile`) where the format string expects `int` (`%d`) -- technically undefined behavior per the C standard, though confirmed harmless on this project's actual target ABI (SysV x86-64, where pointer and `int` share the same register class) -- inherited from the original 1998-era game source, not introduced by free-api.

Required work:
- Add a prominent doc comment on `wsprintfA`'s declaration (`include/winuser.h`) stating explicitly that it will write up to 1024 bytes into the caller's buffer regardless of that buffer's actual size, and that any new call site must keep its expected output well under its own actual buffer size, not just under 1024 bytes.
- Add a matching note to `docs/supported-apis.md`'s `wsprintfA` row.
- Note the pointer-as-%d inherited-UB detail as a one-line comment near the real call sites' analysis in `docs/supported-apis.md`, for future-maintainer context (not a fix -- inherited game source, out of free-api's control).

Acceptance criteria:
- The buffer-size hazard is documented in both the header and `docs/supported-apis.md`, discoverable by a future contributor before they add a new call site.
- No behavior change; documentation only.
- Existing tests still pass.

Out of scope:
- Do not change `wsprintfA`'s signature or add a size parameter -- would break the deliberate real-Win32-`wsprintfA`-compatible contract.
- Do not add runtime truncation detection/assertions -- `vsnprintf`'s return value could be checked for truncation against the 1024-byte internal cap, but that does not protect a caller's smaller buffer and would add complexity for a case with no evidenced real trigger.

---

### TASK-24H-1231: Fix CreateBitmap's pitch computation to avoid signed-int overflow
Status: DONE -- `bitmap->pitch = nWidth * 4;` changed to `bitmap->pitch = static_cast<int>(static_cast<int64_t>(nWidth) * 4);` (src/wingdi_bitmap.cpp), matching the pixel-buffer size computation's existing cast-before-multiply pattern one line below. `pitch` kept as `int` per the task's own guidance -- the cast alone resolves the UB (the multiplication itself can no longer overflow; the narrowing store back to `int` is a separate, well-defined, much lower-severity concern for implausibly large widths neither game ever produces). No new dedicated test added: the overflow threshold (~536M) is impractical to construct in a test (would require a multi-GB pixel-buffer allocation attempt), and the task's own "Required work" list only calls for the cast, not a test. Verified standalone build/ compiles and 26/26 tests pass, no behavior change for any in-bounds dimension (part of this session's TASK-24H-1230/1231/1233/1234/1236 batch verification).
Priority: P2
Area: GDI
Type: Bugfix
Evidence: src/wingdi_bitmap.cpp (`CreateBitmap`, `bitmap->pitch = nWidth * 4;` vs. the immediately-following `pixels.resize(static_cast<size_t>(nWidth) * static_cast<size_t>(nHeight) * 4u, ...)`, which already casts correctly); audit.md §5 Finding M2 / §6 Finding E3 (independently found by both the memory-safety and edge-case reviews)
Depends on: None

Problem:
`bitmap->pitch = nWidth * 4;` is a plain `int * int` multiplication with no overflow guard, inconsistent with the pixel-buffer size computation one line below, which correctly casts to `size_t` before multiplying. If `nWidth` exceeds roughly 536,870,911, this specific multiplication overflows (undefined behavior, typically wraps to a negative/garbage value) while the pixel buffer itself would still be sized "correctly" (or fail allocation) -- a corrupted `pitch` would then drive incorrect row-offset math in every downstream `GetPixel`/`SetPixel`/`StretchBlt` access through that bitmap. Not reachable by either game's real, fixed, small bitmap dimensions -- a purely defensive-hardening fix.

Required work:
- Cast to `size_t` (or `int64_t`) before the multiplication in the `pitch` assignment, matching the pattern already used for the pixel-buffer size on the next line.
- Consider whether `pitch` should remain an `int` field at all given this change, or whether a wider type is warranted -- keep the field type as-is unless the cast alone doesn't resolve the class of bug (it should, for this specific computation).

Acceptance criteria:
- The `pitch` computation cannot silently produce an incorrect (overflowed) value for any `nWidth` that would still successfully allocate a pixel buffer.
- Existing tests still pass; no behavior change for any current, in-bounds bitmap dimension.
- No unrelated API is added.

Out of scope:
- Do not add a general "reject implausibly large dimensions" validation layer across all GDI functions -- scoped to this one specific overflow-prone computation.

---

### TASK-24H-1232: Investigate and fix (or explicitly document as accepted) the static destruction order dependency between the MIDI subsystem and the message queue
Status: DONE -- `g_midi` (src/MidiMusic.cpp) converted from a plain namespace-scope `static MidiState g_midi;` to a function-local static (Meyer's singleton) accessed via `static MidiState& GetMidiState()`. All ~50 in-file usages mechanically updated from `g_midi.` to `GetMidiState().`; verified no leftover `g_midi` references remain. Reasoning: `g_messageQueue`/`g_messageQueueMutex` (src/internal/FreeApiMessageQueue.cpp) are ordinary namespace-scope statics whose dynamic initialization completes before main() runs; GetMidiState()'s function-local static is constructed lazily on first call, which necessarily happens during main()'s execution -- strictly after all namespace-scope statics have finished constructing. Per the standard's static-duration destruction-order rule (reverse of construction-completion order, which holds across the whole program, not just within one translation unit), MidiState's destructor -- which stops and joins the mixer thread before releasing its own resources -- is now guaranteed to run before g_messageQueue/g_messageQueueMutex are destroyed, closing the link-order-dependent teardown race by language rule instead of incidental link order. g_midi had internal (`static`) linkage and was only ever referenced within MidiMusic.cpp, so no other file needed changes. Message-queue globals were left as plain namespace-scope statics (they don't need the same fix -- nothing in their lifetime depends on MidiState). Documented in NEXT.md's "Known bugs and limitations" section, including an explicit warning against reverting `GetMidiState()` back to a plain global without re-reading this writeup. Verified 26/26 passing in: standalone build/, build-tsan/ (FREE_API_SANITIZE=thread), build-asan/ (FREE_API_SANITIZE=address), ../free-eggbert/cmake-build-debug (Ninja), ../planetblupi/build (Make). No new automated test added for the destruction-order guarantee itself, per the task's own note that this is inherently hard to unit-test directly -- the guarantee is a language rule, not runtime behavior a test could observe without an artificial teardown harness; the existing MIDI test suite (test_mci_sequences, test_midi_backend_failure) continuing to pass across all three build trees and both sanitizers confirms no regression to normal (non-teardown) MIDI behavior.
Priority: P1
Area: Concurrency
Type: Bugfix
Evidence: src/MidiMusic.cpp (`static MidiState g_midi;`, `MidiState::~MidiState()`'s stop-then-join sequence, `MixerThread`'s `PostMessageA` call while holding `g_midi.mtx`); src/internal/FreeApiMessageQueue.cpp (`g_messageQueue`/`g_messageQueueMutex`); audit.md §7 Finding R1 (rated HIGH severity -- the single highest-severity finding in the audit)
Depends on: None

Problem:
`g_midi` and the message-queue globals are ordinary namespace-scope statics in different translation units. C++ does not guarantee cross-translation-unit static destruction order -- it depends on link order, left unspecified by the standard, which can silently change with a toolchain upgrade, an added source file, or a build-system refactor. The MIDI mixer thread holds `g_midi.mtx` while calling `PostMessageA` (which locks the message-queue mutex and touches the queue). `MidiState`'s destructor correctly stops and joins the mixer thread before releasing its own resources, so teardown is safe today only if `g_midi` happens to be destroyed before `g_messageQueue`. If link order ever reverses this, the mixer thread -- not yet told to stop -- could call into an already-destroyed mutex/deque during process teardown: an intermittent crash or hang at process exit, triggered by build configuration rather than runtime input, and very difficult to reproduce or bisect.

Required work:
- Convert `g_midi` (and, if needed for the same guarantee, the message-queue globals) to function-local statics (Meyer's singletons), whose destruction order C++ *does* guarantee (reverse of first-use order) -- verify the natural first-use order (message queue is used starting at window creation, before any `MCI_OPEN`) actually produces the required destruction order (MIDI destroyed, thread joined, *before* the message queue is destroyed) once converted.
- Alternatively (if the singleton conversion is judged too invasive), add an explicit, ordered shutdown routine (e.g. registered via `std::atexit` at a well-defined point) that joins the mixer thread before any other global teardown can run, and document why this ordering is now guaranteed rather than incidental.
- Add a test or a documented manual verification step confirming the fix actually changes destruction order in the expected direction (this is inherently hard to unit-test directly; at minimum, verify via a debugger or instrumented build that the mixer thread's `join()` completes before `g_messageQueue`'s destructor runs).

Acceptance criteria:
- The MIDI mixer thread is guaranteed (by language rule, not incidental link order) to be fully stopped before the message-queue globals it depends on are destroyed.
- Existing tests still pass in all three build trees and both sanitizer builds.
- The fix (or the reasoning for why it's accepted as low-enough-risk to leave as-is) is documented in `NEXT.md`'s "Do not do yet" section or `docs/out-of-scope.md`, whichever this session judges is a better fit, so a future session doesn't need to re-derive the analysis.

Out of scope:
- Do not restructure the broader threading model beyond fixing this specific destruction-order dependency.
- Do not add a general "safe global shutdown" framework -- a targeted fix for this one dependency is sufficient.

---

### TASK-24H-1233: Move the MIDI mixer thread's PostMessageA call outside its mutex's lock scope
Status: DONE -- `MixerThread` (src/MidiMusic.cpp) now captures the notify HWND/device id into local `needsNotify`/`notifyTarget`/`notifyId` variables while `GetMidiState().mtx` is held, and calls `PostMessageA` only after that lock_guard scope closes. Added a lock-order comment directly on `MidiState::mtx`'s declaration ("this mutex must never be held while acquiring the message-queue mutex") pointing future changes at the established capture-then-post pattern. Scoped exactly to this one call site per the task's own "Out of scope" note -- the other `PostMessageA` call site in `MidiMusicSendCommand`'s `MCI_PLAY` no-SoundFont branch (still inside its own `lk` scope) was deliberately left untouched. Verified 26/26 passing in standalone build/, build-tsan/ (FREE_API_SANITIZE=thread), build-asan/ (FREE_API_SANITIZE=address), ../free-eggbert/cmake-build-debug (Ninja), ../planetblupi/build (Make) -- in particular test_mci_sequences.cpp's notify-driven-loop and stress tests, which exercise this exact path.
Priority: P2
Area: WinMM
Type: Bugfix
Evidence: src/MidiMusic.cpp (`MixerThread`'s notify-on-completion path, `PostMessageA` called while `g_midi.mtx` is held); audit.md §4 Finding P2 (performance) / §7 Finding R2 (latent lock-order risk) -- the same fix addresses both
Depends on: None

Problem:
The lock scope guarding MIDI session lookup and audio rendering also wraps the `PostMessageA` call on song completion, which internally acquires a different mutex (the message-queue mutex) and does unrelated work (WM_MOUSEMOVE/WM_TIMER coalescing scan, diagnostics bookkeeping). This is not a deadlock today (verified no other code path acquires these two mutexes in the reverse order), but it is both an avoidably-widened critical section (making `MCI_OPEN`/`MCI_CLOSE` wait slightly longer than necessary) and a latent lock-order dependency with nothing structurally preventing a future change from introducing a real deadlock.

Required work:
- Capture the notify target (`HWND`) and session/notify data locally while still holding `g_midi.mtx`, release the lock, then call `PostMessageA` outside the lock scope.
- Add a one-line comment documenting the required lock order (MIDI mutex must never be held while acquiring the message-queue mutex) next to `g_midi.mtx`'s declaration, so a future change doesn't reintroduce the same shape of dependency.

Acceptance criteria:
- `PostMessageA` is no longer called while `g_midi.mtx` is held.
- Existing tests still pass, in particular `test_mci_sequences.cpp`'s notify-driven-loop and stress tests, and the sanitizer builds (this touches locking behavior directly).
- No unrelated API is added; no change to notify delivery ordering/content from the caller's perspective.

Out of scope:
- Do not restructure `MidiMusicSendCommand`'s other locking beyond this one call site.

---

### TASK-24H-1234: Remove the dead, unsynchronized g_activeTimerIds set
Status: DONE -- re-verified via exhaustive grep (`g_activeTimerIds\.` across src/, include/, tests/) that it was only ever written (`insert`/`erase` in src/winmm.cpp's `timeSetEvent`/`timeKillEvent`), never read -- the audit finding still held. Removed the declaration (src/internal/FreeApiTimers.hpp), definition (src/internal/FreeApiTimers.cpp), both call sites (src/winmm.cpp), and the now-unused `<unordered_set>` include from FreeApiTimers.hpp. Verified 26/26 passing in standalone build/, build-tsan/ (FREE_API_SANITIZE=thread -- removing dead unsynchronized state introduced no new issue), build-asan/, ../free-eggbert/cmake-build-debug (Ninja), ../planetblupi/build (Make).
Priority: P2
Area: WinMM
Type: Cleanup
Evidence: src/internal/FreeApiTimers.cpp (`g_activeTimerIds` definition); src/winmm.cpp (`timeSetEvent`'s `insert`, `timeKillEvent`'s `erase`); audit.md §6 Finding E2 (confirmed via exhaustive grep that it is never read anywhere -- the diagnostics snapshot's active-timer count is computed from the two properly-mutex-protected timer maps instead)
Depends on: None

Problem:
`g_activeTimerIds` is a plain `std::unordered_set<UINT>` with no mutex, written to by `timeSetEvent`/`timeKillEvent` but never read by any code path. Concurrent unsynchronized `insert`/`erase` on an `unordered_set` is undefined behavior if ever triggered from two threads at once (theoretically possible if a timer callback ever reentrantly called `timeSetEvent`/`timeKillEvent`, though neither game's real callback does this today). Since the data is never consulted, the correct fix is deletion, not adding synchronization for state nothing uses.

Required work:
- Remove `g_activeTimerIds` and its `insert`/`erase` call sites entirely.
- Grep to confirm (again, at implementation time) that nothing reads it before removing -- re-verify this audit finding still holds against current source.

Acceptance criteria:
- `g_activeTimerIds` no longer exists anywhere in the codebase.
- Existing tests still pass, including under `FREE_API_SANITIZE=thread` (removing dead unsynchronized state should never introduce a new issue, but verify).
- No unrelated API is added; no behavior change (the removed state was never observable).

Out of scope:
- Do not add synchronization to `g_activeTimerIds` as an alternative -- it is confirmed dead code; keeping and locking it would add cost for a set nothing consults.

---

### TASK-24H-1235: Fix _findfirst's directory iteration to not throw an uncaught exception on a real, live call path
Status: DONE -- `_findfirst` (src/crt_io.cpp) rewritten to a manual `directory_iterator` loop using the non-throwing `it.increment(ec)` overload (replacing the range-based for's implicit throwing `operator++()`) AND the non-throwing `entry.is_regular_file(statusEc)` overload (replacing the throwing no-arg form) -- both were real throw points, not just the one the task evidence named. A per-entry status-query failure now just skips that entry (matches a real filesystem's "file vanished/unresolvable while listing" behavior); a directory-read failure mid-scan stops the scan but keeps whatever matched so far, still returning -1 only if nothing matched. New regression test `TestFindFirstSkipsUnresolvableSymlinkInsteadOfCrashing` (tests/test_file_paths.cpp, POSIX-only) deterministically reproduces the exact bug without root or timing races: a self-referential symlink (`loop.xch` -> `loop.xch`) placed alongside a real matching file causes `is_regular_file()`'s target resolution to hit ELOOP (a genuine stat() error, distinct from "not found", which `is_regular_file()` already treats as non-error) -- confirmed this test crashes the pre-fix code with an uncaught `std::filesystem::filesystem_error` (`status: Too many levels of symbolic links`) via a deliberate git-stash-revert-and-rerun check, then confirmed it passes cleanly against the fix, correctly finding the one real file and silently skipping the unresolvable symlink. Verified 26/26 passing in: standalone build/, build-asan/ (FREE_API_SANITIZE=address), ../free-eggbert/cmake-build-debug (Ninja), ../planetblupi/build (Make). (FREE_API_SANITIZE=thread run skipped for this task -- no threading/locking touched; existing `g_findSessionsMutex` locking is unchanged.)
Priority: P1
Area: Files
Type: Bugfix
Evidence: src/crt_io.cpp (the `std::filesystem::directory_iterator(dir, ec)` loop, whose range-based-for implicit `operator++()` uses the throwing increment form, not `ec`-reporting); ../free-eggbert/src/event.cpp:4741-4747 (the real, live call site: the design-mission file picker); audit.md §3 Finding C3 (the one finding in the correctness review reachable via a real, live call site under a realistic failure condition)
Depends on: None

Problem:
The `(path, ec)` constructor overload only makes the directory-*open* step non-throwing; the loop's per-iteration `operator++()` still uses the throwing increment. A permission error, or a file removed/changed mid-iteration (e.g. by a concurrent process, or a symlink disappearing), throws `std::filesystem::filesystem_error` uncaught -- with no exception handling anywhere in this codebase, this would very likely reach `std::terminate()` and crash the entire game process, instead of `_findfirst` gracefully returning `-1` as its documented failure contract requires. This is reachable via free-eggbert's real design-mission file picker under a realistic (if uncommon) real-world condition.

Required work:
- Rewrite the loop to use the non-throwing increment form (`it.increment(ec)` in a manual loop), or wrap the loop body in try/catch and translate any thrown `filesystem_error` into `_findfirst`'s documented `-1`/`ENOENT`-style failure return.
- Add a regression test that simulates a mid-iteration failure (e.g. a directory entry that's removed between `_findfirst` and `_findnext`, if that's feasible to construct in a test; otherwise, test the alternate failure mode directly reachable in this sandboxed environment, such as a permission-denied subdirectory) and confirms `_findfirst`/`_findnext` return their documented failure code rather than crashing the test process.

Acceptance criteria:
- A directory-iteration failure mid-scan (permission error, concurrent removal) no longer crashes the process; `_findfirst`/`_findnext` return their documented failure contract instead.
- New test passes; existing tests (`test_file_paths.cpp` in particular) still pass in all three build trees.
- No unrelated API is added; no change to the success-path behavior for any currently-passing scenario.

Out of scope:
- Do not change `_findfirst`/`_findnext`'s success-path semantics or wildcard-matching behavior -- scoped to the failure-mode fix only.

---

### TASK-24H-1236: Eliminate PeekMessageA's per-frame heap allocation in the WM_TIMER-generation path
Status: DONE -- `pendingTimers` (src/winuser_message.cpp, `PeekMessageA`) changed from a fresh per-call `std::vector<MSG>` to `thread_local std::vector<MSG> pendingTimers;` with a `.clear()` at the top of each use, matching `StretchBlt`'s (src/wingdi_blit.cpp) existing `thread_local` reusable-vector pattern for the same class of problem. `clear()` keeps the underlying buffer's capacity across calls (no deallocation), so only the first call per thread that actually finds elapsed timers allocates; subsequent calls reuse the already-sized buffer. No behavior change to WM_TIMER delivery content/ordering. Verified 26/26 passing in standalone build/, build-tsan/, build-asan/, ../free-eggbert/cmake-build-debug (Ninja), ../planetblupi/build (Make) -- in particular test_timer_regressions.cpp and both full-loop integration tests (test_planetblupi_loop.cpp, test_eggbert_loop.cpp).
Priority: P2
Area: WinUser
Type: Performance
Evidence: src/winuser_message.cpp (`PeekMessageA`'s `std::vector<MSG> pendingTimers`, freshly stack-declared on every call where the queue was empty); src/wingdi_blit.cpp (`StretchBlt`'s existing `thread_local std::vector<int> srcXTable` -- the pattern to copy); audit.md §4 Finding P1
Depends on: None

Problem:
A `std::vector<MSG> pendingTimers` is freshly constructed on every `PeekMessageA` call where the internal queue was empty -- essentially every frame for planetblupi, whose live frame-pump is `SetTimer`/`WM_TIMER`. When the timer has elapsed (the common case), a `push_back` triggers one small heap allocation, freed a few lines later. Not unbounded, but avoidable: this exact class of problem is already solved elsewhere in this codebase (`StretchBlt`'s `thread_local` reusable vector).

Required work:
- Apply the same `thread_local` (or otherwise call-scope-persistent, reused-across-calls) reusable-vector pattern already established in `StretchBlt` to `PeekMessageA`'s `pendingTimers` vector.

Acceptance criteria:
- `PeekMessageA`'s WM_TIMER-generation path no longer allocates on the common (timer-elapsed) path after the first call.
- Existing tests still pass, in particular `test_timer_regressions.cpp` and the two full-loop integration tests (`test_planetblupi_loop.cpp`, `test_eggbert_loop.cpp`).
- No unrelated API is added; no behavior change to WM_TIMER delivery content/ordering.

Out of scope:
- Do not restructure `PeekMessageA`'s broader logic beyond this one allocation-avoidance change.

---

### TASK-24H-1237: Add a minimum-version floor to the SDL3/SDL3_image/SDL3_mixer find_package calls
Status: DONE -- determined the actual installed versions in this project's active development environment via each package's `*ConfigVersion.cmake` (`SDL3_DIR`/`SDL3_image_DIR`/`SDL3_mixer_DIR`'s resolved `PACKAGE_VERSION`, cross-checked against `pkg-config --modversion`): SDL3 3.4.0, SDL3_image 3.4.0, SDL3_mixer 3.2.0. Added these as floors to the three `find_package` calls (CMakeLists.txt, only reached when `FREE_API_USE_SYSTEM_SDL3=ON`). Verified: standalone `build/` (the only tree with `FREE_API_USE_SYSTEM_SDL3=ON`) reconfigures and builds cleanly with the floors in place; both target-game trees use their own vendored/FetchContent SDL3 acquisition path (`FREE_API_USE_SYSTEM_SDL3=OFF`), so this `find_package` call isn't reached there at all -- unaffected by this change, confirmed by their own successful reconfigure+build+28/28 test runs during this batch's verification.
Priority: P3
Area: Build
Type: Cleanup
Evidence: CMakeLists.txt (`find_package(SDL3/SDL3_image/SDL3_mixer REQUIRED)`, no version argument); audit.md §7 Finding R5
Depends on: None

Problem:
free-api links against whatever SDL3/SDL3_image/SDL3_mixer version the consuming game or system provides, with no build-time compatibility guard (unlike the two vendored third-party libraries, TinySoundFont and TinyMidiLoader, which are directly committed source and therefore effectively pinned). A future SDL3 release changing an edge-case behavior this project's tests don't cover (e.g. timer-callback granularity, or a filesystem-function semantic like the one this session already found to be idempotent-vs-not depending on platform) could surface only on a machine with a different SDL3 version than was tested against, with no configure-time warning.

Required work:
- Determine the actual SDL3/SDL3_image/SDL3_mixer version(s) this project's testing has been validated against (check the currently-vendored/system-installed version in the active development environment).
- Add that version as a floor to each `find_package` call (e.g. `find_package(SDL3 3.2 REQUIRED)`).

Acceptance criteria:
- All three `find_package` calls specify an explicit minimum version.
- Existing builds (standalone, both target-game subdirectory builds) still configure and pass 26/26 with the currently-used SDL3 version.
- No unrelated build-system change.

Out of scope:
- Do not vendor SDL3 itself -- this project's explicit policy is to never vendor SDL3 (see `docs/cmake-options.md`); this task only adds a version floor to the existing `find_package` calls.

---

### TASK-24H-1238: Add an automated guard against new, unlisted public declarations (scope-policy enforcement)
Status: DONE -- new `cmake/CheckPublicSurfaceBaseline.cmake` extracts every public declaration from `include/*.h`/`include_non_windows/*.h` via a regex set covering the declaration shapes this project's headers actually use (`#define` macros excluding the `FREE_API_..._H` include-guard convention, single-line `WINAPI`-style function declarations, single-line inline function definitions, function-pointer typedefs, typedef'd structs with alias lists, plain struct/enum tags, simple one-line typedefs, `extern` globals), diffs against `cmake/known-public-symbols.txt` (seeded by running the extractor once against the current, already-scoped header set -- 461 symbols), and fails with `FATAL_ERROR` naming the new symbol(s) if the current set has grown. New `cmake/CheckPublicSurfaceBaselineSelfTest.cmake` (matching `CheckNoHardcodedPathsSelfTest.cmake`'s established pattern) exercises the real extractor against a controlled fixture header covering every declaration shape, confirms include guards/comments are correctly excluded, and confirms the baseline-diff logic itself fires on a genuinely new symbol. Manually verified the negative path against the real repo too: temporarily appended an undocumented function declaration to `include/winuser.h`, confirmed `check_public_surface_baseline` failed with a clear message naming it, then reverted. Registered both as CTest tests (`check_public_surface_baseline`, `check_public_surface_baseline_self_test`), labeled `hygiene`. Documented the enforcement mechanism, its lightweight/non-general-parser nature, and the "add real evidence first, then update the baseline" workflow in `docs/scope.md`'s new "Automated enforcement" section. Verified 28/28 passing (26 prior + 2 new) in standalone build/, ../free-eggbert/cmake-build-debug (Ninja, reconfigured), ../planetblupi/build (Make, reconfigured). Sanitizer builds not re-run for this task -- pure build-system/CTest infrastructure, no runtime/threading/memory code touched; they'll pick up the new tests on their next reconfigure for any other task.
Priority: P2
Area: Build
Type: Implementation
Evidence: docs/scope.md ("The rule": every public API must cite a real usage site, enforced only by human/AI diligence today); cmake/CheckNoHardcodedPaths.cmake + cmake/CheckNoHardcodedPathsSelfTest.cmake (the pattern to follow -- a lightweight script-mode CTest with genuine self-test coverage, TASK-24H-0011); audit.md §7 Finding R3
Depends on: None

Problem:
`docs/scope.md`'s citation rule is enforced entirely by human/AI diligence when writing `plan.md` entries -- there is no CI check, lint rule, or test verifying that a new public declaration in `include/*.h` has corresponding evidence. `test_header_compile.cpp` only proves headers compile, not that they're scoped. A future contributor (human or AI) could add a new public function "because it seemed useful," and nothing in the build or test suite would flag it, letting the project's core "stay minimal" value proposition erode silently over time.

Required work:
- Design and implement a lightweight script-mode CTest (matching `check_no_hardcoded_paths`'s established pattern: a `cmake -P` script + CTest entry, with genuine self-test coverage per `TASK-24H-0011`'s pattern, not just a "passes today" check) that extracts every public declaration currently in `include/*.h`/`include_non_windows/*.h` and diffs it against a maintained baseline file (e.g. `cmake/known-public-symbols.txt`), failing loudly with the new symbol's name if the current header set has grown beyond the baseline.
- Seed the baseline from the current, already-audited symbol set (cross-reference `docs/public-surface-audit.md`, TASK-24H-0115's output, as the authoritative current list).
- Document in the failure message (and in `docs/scope.md`) that a new symbol requires a `plan.md` task with real evidence before the baseline is updated to include it -- this is a deliberate speed bump, not a hard block.

Acceptance criteria:
- Adding a new, undocumented public declaration to any `include/*.h` file causes this new test to fail with a clear message naming the offending symbol.
- The test passes against the current, already-scoped header set with zero false positives.
- Existing tests still pass in all three build trees.
- No unrelated API is added as a side effect of building this guard.

Out of scope:
- Do not attempt to also validate that the CITED evidence in `plan.md` is accurate (that remains a human/AI review responsibility) -- this task only catches "a new symbol appeared with no process followed at all."
- Do not block the build on this check by default if that's judged too disruptive -- a CTest-level failure (not a build-time hard error) matches the existing `check_no_hardcoded_paths` precedent and is sufficient.

---

### TASK-24H-1239: Fix GetObjectA to return the actual number of bytes written, not always sizeof(BITMAP)
Status: DONE -- `GetObjectA` (src/wingdi_bitmap.cpp) now returns `copySize` (the actual `min(c, sizeof(BITMAP))` byte count already used for the `memcpy`) instead of unconditionally `sizeof(BITMAP)`. New regression test `TestGetObjectAReturnsActualBytesCopiedNotAlwaysFullSize` (tests/test_gdi_regressions.cpp) calls `GetObjectA` with `c` smaller than `sizeof(BITMAP)` and asserts the return value matches that smaller size, plus a positive control confirming `c >= sizeof(BITMAP)` still returns the full struct size. Verified 28/28 passing in standalone build/, build-tsan/, build-asan/, ../free-eggbert/cmake-build-debug (Ninja), ../planetblupi/build (Make) -- including the pre-existing `TASK-24H-0616` degenerate-argument tests, unaffected.
Priority: P3
Area: GDI
Type: Bugfix
Evidence: src/wingdi_bitmap.cpp (`GetObjectA`, the `memcpy` is correctly bounded to `min(c, sizeof(BITMAP))` but the return value is unconditionally `sizeof(BITMAP)`); audit.md §3 Finding C8
Depends on: None

Problem:
`GetObjectA`'s `memcpy` is correctly bounded to the smaller of the caller's requested size and `sizeof(BITMAP)` -- no overflow. But the function always *returns* `sizeof(BITMAP)`, even when the caller passed a smaller `c` and fewer bytes were actually copied. Real Win32 `GetObjectA` returns the actual byte count written. A caller relying on the return value to know how much of the struct is valid would be misled. No evidence either game does this, but it's a real, easily-fixed deviation from the documented contract.

Required work:
- Change `GetObjectA` to return the actual number of bytes copied (`min(c, sizeof(BITMAP))`), not the unconditional `sizeof(BITMAP)`.
- Add a regression test calling `GetObjectA` with `c` smaller than `sizeof(BITMAP)` and asserting the return value matches the smaller size, not the full struct size.

Acceptance criteria:
- `GetObjectA`'s return value always matches the actual number of bytes written for any `c`.
- New test passes; existing `GetObjectA` tests (including `TASK-24H-0616`'s degenerate-argument tests) still pass.
- No unrelated API is added.

Out of scope:
- Do not change `GetObjectA`'s behavior for object kinds other than bitmaps (no other kind is supported today, per `TASK-24H-0608`'s documented scope).

---

### TASK-24H-1240: Add a symmetric zero-guard to ClientToScreen's scaling division
Status: DONE -- root-cause analysis found the asymmetry was subtler than "ClientToScreen has no guard": it already had a `pw > 0 && ph > 0` check, but that guards the PHYSICAL dimensions, which are `ScreenToClient`'s divisor, not `ClientToScreen`'s -- `ClientToScreen`'s actual divisor is the LOGICAL width/height (`it->second.width`/`height`), left completely unguarded. Added `it->second.width > 0 && it->second.height > 0` to the existing condition (src/winuser_cursor.cpp), so both the window-state lookup's dimensions AND the physical `SDL_GetWindowSize` result are checked before dividing by either. Fallback matches `ScreenToClient`'s: skip the scaling step (leave the point unscaled), still return TRUE. Currently unreachable (CreateWindowExA always populates a positive logical width/height); no new test added per the task's own scope (defensive-only, matching TASK-24H-1231's precedent for an unreachable condition). Verified 28/28 passing in standalone build/, build-tsan/, build-asan/, ../free-eggbert/cmake-build-debug (Ninja), ../planetblupi/build (Make), no behavior change for any currently-reachable case.
Priority: P3
Area: WinUser
Type: Bugfix
Evidence: src/winuser_cursor.cpp (`ScreenToClient`'s explicit `pw > 0 && ph > 0` guard vs. `ClientToScreen`'s unguarded mirror-image division); audit.md §3 Finding C5
Depends on: None

Problem:
`ScreenToClient` explicitly guards its coordinate-scaling division against the window's logical width/height being zero. `ClientToScreen`'s corresponding division has no equivalent guard. Currently unreachable (`CreateWindowExA` always populates a positive width/height, defaulting to 640x480 if given a non-positive value), but the two functions performing the same class of computation should have matching defensive postures, and this is a latent division-by-zero (SIGFPE) risk if that invariant is ever broken by a future change (e.g. an incomplete `MoveWindow` logical-size update, already documented elsewhere as a confirmed-harmless gap today).

Required work:
- Add the same zero-guard `ScreenToClient` already has to `ClientToScreen`'s division, with the same fallback behavior (match whatever `ScreenToClient` does when the guard trips).

Acceptance criteria:
- `ClientToScreen` and `ScreenToClient` have matching defensive behavior for a zero-width/zero-height window state.
- Existing tests still pass; no behavior change for any currently-reachable (positive-dimension) case.
- No unrelated API is added.

Out of scope:
- Do not investigate or fix `MoveWindow`'s stale logical-size tracking as part of this task -- that's a separate, already-documented, confirmed-harmless gap (`TASK-24H-0309`).

---

### TASK-24H-1241: Document SetTimer's globally-keyed (not per-window) timer-ID map as a confirmed-harmless simplification
Status: DONE -- added a new "`SetTimer`'s globally-keyed (not per-window) timer-ID map" section to `docs/out-of-scope.md`, immediately after the existing "Single live window assumption" section, explaining `g_winTimers`'s global (ID-only) keying, why it's safe under the current single-window design, and that it must be revisited before any multi-window support. Added a cross-reference pointer from the existing single-window-assumption section back to this new one, per the acceptance criteria. Documentation only, no behavior change. Verified standalone build/ still compiles and 28/28 tests pass.
Priority: P3
Area: Timers
Type: Documentation
Evidence: src/internal/FreeApiTimers.hpp, src/winuser_timer.cpp (`g_winTimers`, a single global map keyed by timer ID alone, not scoped per-window as real Win32 does); audit.md §3 Finding C6
Depends on: None

Problem:
Real Win32 scopes a timer ID to the window that created it -- the same numeric ID can be reused by different windows without conflict. Here, `g_winTimers` is a single global map keyed by ID alone; a second `SetTimer` call with the same ID from a different window would silently overwrite the first window's timer entry. Confirmed harmless today only because this project's own established single-live-window design (documented in `docs/out-of-scope.md`) means two windows never coexist -- undocumented as a deliberate simplification anywhere.

Required work:
- Add a short note to `docs/out-of-scope.md`, alongside the existing single-window-assumption documentation, stating `SetTimer`'s timer-ID map is global (not per-window), why that's safe under the current single-window design, and that this must be revisited before any multi-window support is ever added.

Acceptance criteria:
- The simplification and its safety rationale are documented in a discoverable location, cross-referenced from the existing single-window-assumption note.
- No behavior change; documentation only.
- Existing tests still pass.

Out of scope:
- Do not scope `g_winTimers` per-window -- no evidenced need, and the project's single-window design makes it unnecessary.

---

### TASK-24H-1242: Document or unify StretchBlt's inconsistent out-of-range source-rect handling between its two internal paths
Status: DONE -- chose to document rather than unify (task's own "contributor's choice" allowance): unifying would require deliberately rewriting `TestStretchBlt1to1OutOfRangeSourceRectClipsSafely` and `TestStretchBltScaledOutOfRangeSourceYClampsToEdgeRowLikeX` (which correctly lock in each path's current behavior) for no functional benefit, since neither target game ever reaches either path with out-of-range input. Added a detailed explanation to `include/wingdi.h`'s `StretchBlt` doc comment (naming both tests that would need updating if ever unified) and a matching, more detailed section to `docs/out-of-scope.md`. No behavior/test change. Verified standalone build/ still compiles and 28/28 tests pass, including both of the referenced tests, unaffected.
Priority: P3
Area: GDI
Type: Documentation
Evidence: src/wingdi_blit.cpp (the 1:1 fast path clips to a no-op for an out-of-range source rect; the scaled path instead clamps per-pixel coordinates to the nearest edge and draws a stretched/duplicated edge-pixel artifact for the same nominal input shape); audit.md §3 Finding C7
Depends on: None

Problem:
For the same nominal "source rect far outside the source bitmap" input, `StretchBlt`'s two internal code paths (1:1 fast path vs. scaled path) produce different behavior: one draws nothing, the other draws a clamped-edge artifact. Both target games always pass in-bounds source rects derived from real bitmap dimensions, so this is not an active bug -- but the inconsistency between the two paths for the same edge-case class is currently unstated anywhere.

Required work:
- Either document the inconsistency explicitly (in `include/wingdi.h`'s `StretchBlt` doc comment and/or `docs/out-of-scope.md`) as an accepted, low-priority quirk given neither path is reachable with out-of-range input by either game, or unify the two paths' out-of-range behavior to match (contributor's choice, given neither game can currently observe the difference).

Acceptance criteria:
- The inconsistency (or its resolution, if unified) is documented or fixed, discoverable by a future maintainer without re-deriving it from source.
- Existing tests still pass, including `TestStretchBlt1to1OutOfRangeSourceRectClipsSafely` and `TestStretchBltScaledOutOfRangeSourceYClampsToEdgeRowLikeX`, which already lock in each path's CURRENT individual behavior -- if unifying, both of those tests will need deliberate, reasoned updates, not just deletion.
- No unrelated API is added.

Out of scope:
- Do not change `StretchBlt`'s in-bounds behavior under any circumstance.

---

### TASK-24H-1243: Consider relaxing g_debugInput's memory order from the default (seq_cst) to relaxed
Status: DONE -- all four `g_debugInput` access sites converted from implicit `bool` conversion (seq_cst) to explicit `.load(std::memory_order_relaxed)`/`.store(..., std::memory_order_relaxed)`: the one write (src/internal/FreeApiSdlVideo.cpp's `EnsureVideoSubsystem`) and three reads (src/internal/FreeApiMessageQueue.cpp's `InputLog` early-return, and two `if (g_debugInput...)` checks in `PumpSdlEvents`). `tests/test_sdl_log_gating.cpp`'s gate-recognition regex (`kGatePattern`) broadened to also recognize the new `g_debugInput.load(...)` shape alongside the plain implicit-conversion form, since its early-return-gate regex no longer matched the literal `if (!g_debugInput) return;` text after this change (a real, caught-by-running-the-suite regression in the test's own pattern-matching, not a gating gap in the source). Verified `TestGDebugInputSurvivesRaceUnderSanitizer` (tests/test_timer_regressions.cpp) still passes under `FREE_API_SANITIZE=thread` -- relaxed ordering remains race-free for this single boolean debug flag with no dependent data, confirming the task's own prediction. Verified 28/28 passing in standalone build/, build-tsan/, build-asan/, ../free-eggbert/cmake-build-debug (Ninja), ../planetblupi/build (Make).
Priority: P3
Area: Diagnostics
Type: Performance
Evidence: src/internal/FreeApiMessageQueue.cpp/.hpp (`std::atomic_bool g_debugInput`, read via implicit bool conversion -- defaults to `memory_order_seq_cst`); audit.md §4 Finding P3
Depends on: None

Problem:
`g_debugInput`'s implicit `bool` conversions default to `memory_order_seq_cst`. On x86/x64 (the primary desktop target) a seq_cst *load* is free (compiles to a plain `MOV`), but on ARM/Android (which this codebase explicitly supports via `#if defined(__ANDROID__)` code paths) a seq_cst load requires a memory barrier -- checked multiple times per message. `g_debugInput` is a debug on/off flag, not synchronizing any other data, so `memory_order_relaxed` would be strictly sufficient and costs nothing to change.

Required work:
- Change `g_debugInput`'s reads to explicit `.load(std::memory_order_relaxed)` calls (the existing ThreadSanitizer-verified data-race fix for this variable, `TestGDebugInputSurvivesRaceUnderSanitizer`, tests for a genuine race, not a specific memory order -- confirm the relaxed change still passes that test, since relaxed ordering is still race-free for a single boolean flag with no dependent data).

Acceptance criteria:
- `g_debugInput` reads use `memory_order_relaxed`; writes may also be relaxed unless a stronger reason is found to keep them stricter.
- `TestGDebugInputSurvivesRaceUnderSanitizer` still passes under `FREE_API_SANITIZE=thread`.
- Existing tests still pass in all three build trees.
- No unrelated API is added.

Out of scope:
- Do not change any other atomic variable's memory order as part of this task -- scoped to `g_debugInput` only; other atomics may have genuine ordering requirements that need separate, individual analysis.

---

### TASK-24H-1244: Verify (and fix if needed) CompatDC::selectedBitmap's dangling-pointer risk against both games' real delete ordering
Status: DONE -- traced every `SelectObject`/`DeleteObject`/`DeleteDC` call site in both target games (exhaustive grep + read). The only `SelectObject` call site in either game is `DDCopyBitmap` (../free-eggbert/src/ddutil.cpp:144, ../planetblupi/src/ddutil.cpp:202): it selects the caller's bitmap into a temporary DC, blits, then deletes that same DC -- all before returning. The bitmap itself is always deleted separately by the caller, always AFTER `DDCopyBitmap` (and thus the one DC referencing it) has already been destroyed. Confirmed via `DDLoadBitmap`/`DDReLoadBitmap`/`DDConnectBitmap`'s exact call order in both trees, including planetblupi's `decmap.cpp:594` minimap path (`Cache` -> `DDConnectBitmap` -> `DDCopyBitmap`, all synchronous, DC destroyed before `Cache` returns, bitmap deleted afterward by `decmap.cpp`). `pixmap.cpp`'s other `DeleteDC` call sites (both games) never call `SelectObject` at all. Confirmed unreachable with real, evidenced call-order proof -- documented in `docs/out-of-scope.md` (new "`CompatDC::selectedBitmap`'s dangling-pointer risk" section) per this task's own "if confirmed unreachable, document" branch. No code change; no new test needed (nothing to regress). Existing tests unaffected (no source touched).
Priority: P2
Area: GDI
Type: Verification
Evidence: src/wingdi_bitmap.cpp (`DeleteObject`, never scans live `CompatDC` instances to clear a `selectedBitmap` reference to the bitmap being deleted); src/wingdi_dc.cpp (`SelectObject`); audit.md §3 Finding C2
Depends on: None

Problem:
`DeleteObject` never scans open `CompatDC`s to clear a `selectedBitmap` pointer referencing the bitmap being deleted. If a caller deletes a bitmap while it is still selected into a DC (rather than deselecting or deleting the DC first), that DC's `selectedBitmap` becomes a dangling pointer; a subsequent `GetPixel`/`SetPixel`/`StretchBlt`/`GetObjectA` call through that DC would read or write freed memory. This has not been confirmed as reachable by either target game's actual delete ordering -- the audit flagged it as "not confirmed reachable, but not confirmed safe either."

Required work:
- Trace both games' actual `DeleteObject`/`DeleteDC`/`SelectObject` call sequences (via `ddutil.cpp` and any other real call sites already cataloged in `docs/supported-apis.md`) and determine definitively whether either game ever deletes a bitmap while it remains selected into a live DC.
- If confirmed unreachable: document the finding (with the exact call-order evidence) in `docs/out-of-scope.md`, matching this project's established "confirmed harmless, not a TODO" pattern.
- If confirmed reachable, or if the call order can't be proven safe with confidence: fix `DeleteObject` to clear any `CompatDC::selectedBitmap` pointers referencing the bitmap being deleted (requires a way to find affected DCs -- consider whether this needs a reverse index, or whether the number of live DCs is always small enough that a linear scan over some existing DC-tracking structure is acceptable; check what tracking already exists before designing a new one).

Acceptance criteria:
- Either a documented, evidenced "confirmed harmless" finding exists in `docs/out-of-scope.md`, or the dangling-pointer path is fixed and a regression test locks in the fix.
- Existing tests still pass in all three build trees.
- No unrelated API is added.

Out of scope:
- Do not build a general reference-counting/dependency-tracking framework across all GDI objects unless the verification step above proves it's actually needed -- prefer the documentation outcome if the call-order trace confirms safety, consistent with this project's default policy of not building generalized safety infrastructure without evidenced need.

---

## Deep Audit Follow-up #2 (audit.md, 2026-07-09 re-audit)

The 6 tasks below (`TASK-24H-1245`-`1250`) formalize the second `audit.md`'s "Proposed Tasks" section (§ "Proposed Tasks"), produced by a fresh, ground-up re-audit performed after every finding from the first audit (`TASK-24H-1229`-`1244`, plus `TASK-24H-0706`) was implemented and pushed. `audit.md` was produced by 5 independent parallel reviews (correctness, performance, memory safety, edge cases, architectural risk) against commit `cdcc119`, each explicitly instructed to re-derive findings fresh rather than reuse the prior audit's conclusions. Two findings were independently reproduced by two reviews each (see `audit.md`'s "Cross-Validated Findings" table) -- both are the same lesson: a fix that closed one instance of a pattern didn't grep for sibling instances of the same pattern. Re-verify the underlying claim against current source before implementing, since source may have moved since the audit was written. `audit.md`'s own conclusion is explicit that no currently-live game behavior is broken by any finding.

### TASK-24H-1245: Apply the PostMessageA-outside-mutex fix to MCI_PLAY's no-SoundFont branch (second call site TASK-24H-1233 left untouched)
Status: DONE -- `MidiMusicSendCommand`'s `MCI_PLAY` handler (src/MidiMusic.cpp) restructured: the session-scan loop now runs inside its own `{ std::lock_guard ... }` scope, setting local `sessionFound`/`silentSuccess`/`needsNotify`/`notifyTarget`/`notifyId` flags instead of returning directly from inside the lock; `PostMessageA` and the two post-scan `MIDI_LOG`/`return` statements now run after that scope closes, matching `MixerThread`'s established capture-then-post pattern. Updated the lock-order comment on `MidiState::mtx`'s declaration to reference both call sites. Verified 28/28 passing in standalone build/, build-tsan/ (FREE_API_SANITIZE=thread), build-asan/ (FREE_API_SANITIZE=address), ../free-eggbert/cmake-build-debug (Ninja), ../planetblupi/build (Make) -- in particular `test_mci_sequences.cpp`'s no-SoundFont test, confirmed still passing with its exact same log output (`MCI_PLAY still succeeds with no SoundFont available`, `MM_MCINOTIFY ... is still posted promptly`).
Priority: P2
Area: WinMM
Type: Bugfix
Evidence: src/MidiMusic.cpp:696 (`MidiMusicSendCommand`'s `MCI_PLAY` handler, `PostMessageA` called while `GetMidiState().mtx` -- acquired at line 667 -- is held); TASK-24H-1233 (fixed the identical pattern in `MixerThread`, explicitly scoped out this second site); audit.md Finding C2 / R1 (independently found by both the correctness and architectural-risk reviews)
Depends on: None

Problem:
`TASK-24H-1233` moved `MixerThread`'s `PostMessageA` call outside its `GetMidiState().mtx` lock scope, established a `needsNotify`/`notifyTarget`/`notifyId` capture-then-post-after-release pattern, and added a lock-order comment on `MidiState::mtx` -- but its own "Out of scope" note explicitly left `MidiMusicSendCommand`'s `MCI_PLAY` handler untouched. That handler's no-SoundFont branch (`src/MidiMusic.cpp` around line 690-702) still calls `PostMessageA(s.notifyHwnd, MM_MCINOTIFY, ...)` while holding the same mutex, for the same class of reason (widened critical section; latent lock-order dependency with nothing structurally preventing a future change from introducing a real deadlock). This branch is real and test-exercised: `test_mci_sequences.cpp` has a dedicated no-SoundFont test that calls `MCI_PLAY` down exactly this path.

Required work:
- Apply the same capture-under-lock/post-after-release pattern `MixerThread` already uses: capture `s.notifyHwnd` and `s.id` into local variables while still holding the lock, let the `std::lock_guard` scope end, then call `PostMessageA` afterward.
- Re-read the lock-order comment already on `MidiState::mtx`'s declaration to confirm this new call site's fix matches the documented rule; update the comment if it needs to reference this second site too.

Acceptance criteria:
- `PostMessageA` is no longer called while `GetMidiState().mtx` is held, at either of the two call sites in `MidiMusic.cpp`.
- Existing tests still pass, in particular `test_mci_sequences.cpp`'s no-SoundFont test and the sanitizer builds (this touches locking behavior directly).
- No unrelated API is added; no change to notify delivery ordering/content from the caller's perspective.

Out of scope:
- Do not restructure `MidiMusicSendCommand`'s other locking beyond this one call site.

---

### TASK-24H-1246: Fix ResolveJoystick's bounds check to compare the same value it uses to index
Status: DONE -- `ResolveJoystick`'s bounds check (src/winmm.cpp) changed from `static_cast<int>(uJoyID) < count` to `count > 0 && uJoyID < static_cast<UINT>(count)`, comparing the same (unsigned) type used to index `ids[uJoyID]` and guarding against a defensively-possible negative `count`. New regression test `TestJoyGetPosExRejectsHighBitSetDeviceIndex` (tests/test_joystick_regressions.cpp) calls `joyGetPosEx` with `0x80000000` and `0xFFFFFFFF`, asserting `JOYERR_UNPLUGGED` (safe rejection, not a crash/OOB read) for both. Verified 28/28 passing in standalone build/, build-tsan/, build-asan/ (with the new leak-detection coverage from TASK-24H-1247 active -- no leak from the new test), ../free-eggbert/cmake-build-debug (Ninja), ../planetblupi/build (Make).
Priority: P2
Area: WinMM
Type: Bugfix
Evidence: src/winmm.cpp:53-54 (`ResolveJoystick`, `if (static_cast<int>(uJoyID) < count) { joystick = SDL_OpenJoystick(ids[uJoyID]); }`); ../free-eggbert/src/event.cpp:2071 (the only real call site, passing `m_joyID`); audit.md Finding E2
Depends on: None

Problem:
The bounds check casts `uJoyID` (a `UINT`) to `int` and compares against `count`, but the array index (`ids[uJoyID]`) uses the original, uncast `uJoyID`. Any `uJoyID` with the high bit set (>= `0x80000000`) casts to a negative `int`, which is `< count` for any `count > 0` -- the check passes, then `ids[uJoyID]` indexes far out of bounds into the `SDL_JoystickID` array `SDL_GetJoysticks` returned, reading garbage memory and passing it to `SDL_OpenJoystick`. This is a genuine defect in the validation logic itself (the check and the indexed value are simply different expressions), not just a theoretical hardening gap -- but free-eggbert's only call site passes `m_joyID`, a small device-index member never plausibly set to an extreme value, so it is not reachable by real gameplay today.

Required work:
- Change the bounds check to compare the same value used for indexing -- e.g. `if (uJoyID < static_cast<UINT>(count))`, guarding against `count` itself ever being negative (it shouldn't be, per `SDL_GetJoysticks`'s contract, but confirm).
- Add a regression test calling the public joystick API (whichever entry point reaches `ResolveJoystick`) with a `uJoyID` value that has the high bit set, asserting it's safely rejected (no crash, no out-of-bounds read) rather than passing the old broken check.

Acceptance criteria:
- `ResolveJoystick`'s bounds check rejects any `uJoyID >= count` using a comparison that can't be defeated by integer-cast sign flipping.
- New test passes; existing joystick tests (`test_joystick_regressions.cpp`) still pass in all three build trees.
- No unrelated API is added; no behavior change for any currently-valid `uJoyID`.

Out of scope:
- Do not add general integer-overflow guards elsewhere in winmm.cpp as part of this task -- scoped to this one specific bounds-check defect.

---

### TASK-24H-1247: Add a scoped LeakSanitizer suppressions file for SDL3, replacing the blanket ASAN_OPTIONS=detect_leaks=0
Status: DONE -- amendment to the task's literal "Required work" (a suppressions file): re-verified the underlying premise first, per this task's own required first step ("determine SDL3's actual leaking call stacks... run with detect_leaks=1"). Ran every one of the 16 real test binaries in build-asan/ with `ASAN_OPTIONS=detect_leaks=1` (dummy SDL video/audio drivers, the environment this project's tests actually run in) and found ZERO leaks, real or false-positive, from SDL3 or anywhere else -- not even the false-positive SDL3 leaks the original `detect_leaks=0` comment's rationale described. Since no false positive currently exists to suppress, a suppressions file would be speculative infrastructure with no evidenced need (against this project's own default policy) -- the simpler, better-evidenced fix is to just remove the blanket override, restoring LeakSanitizer's default-enabled behavior. Removed `ASAN_OPTIONS=detect_leaks=0` from CMakeLists.txt's sanitizer `ENVIRONMENT` wiring entirely; replaced the old comment with one documenting this investigation and the fallback plan (a scoped `LSAN_OPTIONS=suppressions=<file>` naming SDL3's specific stacks) if a real false positive is ever observed in a different configuration. **Verified the fix actually works, both directions, through the real ctest workflow** (per the task's own required verification step, not skipped): temporarily removed the already-fixed `TestGetSetPixelRoundTripOnMemoryDcWithSelectedBitmap` cleanup calls in tests/test_gdi_regressions.cpp, rebuilt, ran `ctest -R test_gdi_regressions` in build-asan/ -- confirmed it now FAILS with a full LeakSanitizer report (104 bytes, 3 allocations, exact stack traces pointing at the test's own code) through the standard, undocumented-workaround-free `ctest` command; reverted the cleanup-call removal, rebuilt, confirmed clean again. Verified 28/28 passing (with real leak detection now active) in standalone build/, build-tsan/ (unaffected -- LSan doesn't apply to TSan builds), build-asan/, ../free-eggbert/cmake-build-debug (Ninja), ../planetblupi/build (Make).
Priority: P2
Area: Build
Type: Implementation
Evidence: CMakeLists.txt:651-656 (`ASAN_OPTIONS=detect_leaks=0` applied via CTest's `ENVIRONMENT` property to every test when `FREE_API_SANITIZE=address`); docs/cmake-options.md:96-108 (documents the rationale: SDL3's own long-lived global allocations read as false-positive leaks); audit.md Finding R2 -- cites this session's own earlier discovery that a real, genuine resource leak in `TestGetSetPixelRoundTripOnMemoryDcWithSelectedBitmap` went undetected through every `ctest`-based ASan pass across multiple sessions, only found by manually running the test binary directly (bypassing the `ENVIRONMENT` property)
Depends on: None

Problem:
The blanket `detect_leaks=0` disables LeakSanitizer for the ENTIRE `ctest` suite, not just for SDL3's own known allocations -- meaning running the documented, standard workflow (`ctest` in a `FREE_API_SANITIZE=address` build tree) can never catch a real leak in free-api's own code via the normal, documented path. This is not hypothetical: it already happened once this session, and was only caught by an off-workflow manual step.

Required work:
- Determine SDL3's actual leaking (or leak-look-alike) allocation call stacks in this project's environment (run with `detect_leaks=1` and `LSAN_OPTIONS=verbosity=1:log_threads=1` or similar to capture real stack traces, or consult SDL3's own known-leak documentation if it exists).
- Write an LSAN suppressions file (e.g. `cmake/lsan-suppressions.txt`) naming SDL3's specific allocation patterns (`leak:SDL_*` or narrower, whichever is precise enough to not also suppress free-api's own leaks).
- Replace `ASAN_OPTIONS=detect_leaks=0` with `ASAN_OPTIONS=detect_leaks=1` plus `LSAN_OPTIONS=suppressions=<path-to-file>` in the CTest `ENVIRONMENT` wiring (CMakeLists.txt).
- Re-verify: intentionally reintroduce the already-fixed test leak (or a new deliberate one in a throwaway branch/local check) to confirm the new suppressions setup still catches a real free-api leak through the normal `ctest` workflow, then confirm it's clean again with the leak fixed. Do not skip this verification step -- the whole point of this task is restoring real detection, and an unverified suppressions file could just as easily suppress everything as intended.

Acceptance criteria:
- `ctest` in a `FREE_API_SANITIZE=address` build tree detects a real, deliberately-reintroduced free-api leak (verified during implementation, not just asserted).
- `ctest` in the same build tree passes cleanly against the current, leak-free codebase, with SDL3's own allocations suppressed.
- Existing tests still pass in all three build trees.
- No unrelated API is added.

Out of scope:
- Do not attempt to also enable leak detection under ThreadSanitizer builds -- TSan and LSan are not normally combined in the same build; this task is scoped to the `FREE_API_SANITIZE=address` configuration only.
- Do not broaden the suppressions file beyond SDL3's own allocations -- a suppression entry that's too broad defeats this task's purpose.

---

### TASK-24H-1248: Apply the pitch-overflow-cast fix to CreateCompatBitmapFromSurface and ScaleCompatBitmap
Status: TODO
Priority: P3
Area: GDI
Type: Bugfix
Evidence: src/internal/FreeApiGdi.cpp:52 (`CreateCompatBitmapFromSurface`, `bitmap->pitch = bitmap->width * 4;`) and :99 (`ScaleCompatBitmap`, `bitmap.pitch = targetWidth * 4;`); TASK-24H-1231 (already fixed the identical pattern in `src/wingdi_bitmap.cpp`'s `CreateBitmap`); audit.md Finding C1 / E1 (independently found by both the correctness and edge-case reviews)
Depends on: None

Problem:
`TASK-24H-1231` fixed `CreateBitmap`'s `pitch = nWidth * 4` signed-overflow-prone computation by casting to `int64_t` before multiplying, but its scope covered only that one function -- two sibling functions in `src/internal/FreeApiGdi.cpp` (the backing code for `LoadImageA`) compute the exact same quantity the exact same unsafe way. `CreateCompatBitmapFromSurface`'s width comes from `SDL_ConvertSurface`'s output on a loaded, real, shipped `.bmp` asset; `ScaleCompatBitmap`'s `targetWidth` comes from `LoadImageA`'s `cx` parameter, which both games' real `DDLoadBitmap` call sites always pass as small, fixed sprite dimensions (most calls pass 0, skipping this function entirely). Not reachable by either game's real assets -- same profile as the original finding.

Required work:
- Apply the identical `static_cast<int>(static_cast<int64_t>(width) * 4)` pattern already used in `src/wingdi_bitmap.cpp`'s `CreateBitmap` to both `CreateCompatBitmapFromSurface` (line 52) and `ScaleCompatBitmap` (line 99) in `src/internal/FreeApiGdi.cpp`.
- Grep the rest of the codebase for any other `* 4` (or similar bpp-multiply) pitch computation this same pattern might apply to, to confirm these are the only two remaining sites -- do not assume the grep from this task's evidence is exhaustive without re-checking.

Acceptance criteria:
- All three pitch computations across the codebase (`CreateBitmap`, `CreateCompatBitmapFromSurface`, `ScaleCompatBitmap`) use the same overflow-safe cast pattern.
- Existing tests still pass; no behavior change for any current, in-bounds bitmap dimension.
- No unrelated API is added.

Out of scope:
- Do not add a general "reject implausibly large dimensions" validation layer across all GDI functions -- scoped to these two specific overflow-prone computations, matching TASK-24H-1231's own scope decision.

---

### TASK-24H-1249: Document or narrow PushMessage's coalescing-scan lock scope
Status: TODO
Priority: P3
Area: WinUser
Type: Documentation
Evidence: src/internal/FreeApiMessageQueue.cpp:81-109 (`PushMessage`'s WM_MOUSEMOVE/WM_TIMER coalescing scans, both linear scans of `g_messageQueue` performed while `g_messageQueueMutex` is held); audit.md Finding P1
Depends on: None

Problem:
Every `WM_MOUSEMOVE`/`WM_TIMER` message triggers a reverse linear scan of `g_messageQueue` while the queue mutex is held, on the single hottest call path in the message system (mouse-move fires at OS event rate). This is self-limiting in practice -- coalescing keeps at most one `WM_MOUSEMOVE` entry per hwnd, so the scan usually terminates within a few hops -- and there's no evidence of an actual observed slowdown (`g_diagQueueHighWater` gives no indication the queue ever grows large), but the assumption that keeps this safe (queue stays short because coalescing is effective) is currently unstated.

Required work:
- Either add an explicit comment above the coalescing scans stating the self-limiting assumption (queue depth stays low because coalescing prevents unbounded accumulation of the same message type) and why it's believed safe without a more targeted data structure, or -- if profiling under a realistic worst-case (many non-coalescible messages queued ahead of a mouse-move burst) shows a real cost -- consider a more targeted lookup (e.g. tracking the last WM_MOUSEMOVE/WM_TIMER-per-hwnd index outside the deque) to avoid the linear scan.
- Prefer documentation unless profiling shows an actual measurable cost -- no evidenced real slowdown exists today.

Acceptance criteria:
- Either the self-limiting assumption is documented at the scan site, or the scan is narrowed with evidence (a profiling result) justifying the added complexity.
- Existing tests still pass, in particular `test_timer_regressions.cpp`'s stress tests and the two full-loop integration tests.
- No unrelated API is added; no behavior change to message coalescing content/ordering.

Out of scope:
- Do not restructure `PushMessage`'s broader logic beyond this one scan's documentation/narrowing.

---

### TASK-24H-1250: Change g_nextTimerId's fetch_add to memory_order_relaxed
Status: TODO
Priority: P3
Area: Timers
Type: Performance
Evidence: src/winuser_timer.cpp:16 (`g_nextTimerId.fetch_add(1)`, implicit default seq_cst); src/internal/FreeApiTimers.cpp:11 (`std::atomic<UINT> g_nextTimerId{1};`); TASK-24H-1243 (the precedent: `g_debugInput`'s reads/writes were already converted to explicit `memory_order_relaxed`); audit.md Finding P2
Depends on: None

Problem:
Every other counter/diagnostic atomic in the codebase explicitly uses `std::memory_order_relaxed` (confirmed via a full grep of every `.fetch_add`/`.fetch_sub` call site) except `g_nextTimerId`, which still uses the implicit-conversion default (seq_cst). `SetTimer` is not a per-frame hot path, so the real-world performance cost is negligible -- this is a consistency gap with the rest of the codebase's established convention, not a measured performance problem.

Required work:
- Change `g_nextTimerId.fetch_add(1)` to `g_nextTimerId.fetch_add(1, std::memory_order_relaxed)`, matching the convention already used by every other counter atomic in the codebase (`g_diagCompatDcs`, `g_diagMessagesPosted`, etc., and now `g_debugInput` per TASK-24H-1243).

Acceptance criteria:
- `g_nextTimerId`'s increment uses `memory_order_relaxed`.
- Existing tests still pass in all three build trees and both sanitizer builds (this touches an atomic shared across the WinUser/WinMM timer-ID-uniqueness coupling documented in `src/internal/FreeApiTimers.hpp`).
- No unrelated API is added; no behavior change to timer ID uniqueness/allocation order.

Out of scope:
- Do not change any other atomic variable's memory order as part of this task -- scoped to `g_nextTimerId` only.
