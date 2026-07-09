# WinAPI subset implementation — quality assessment

Date: 2026-07-09. Independent, read-only quality review (not a bug hunt —
see [`audit.md`](../audit.md) and `plan.md`'s "Deep Audit" sections for
those). This assesses *quality*: fidelity to real WinAPI semantics, code
craftsmanship, test depth, and architecture — including real weaknesses,
not just a favorable summary.

## Verdict

**High quality — for what this project actually is.** `free-api` is not
a general WinAPI reimplementation. It deliberately implements only what
source-level evidence from `../free-eggbert` and `../planetblupi` proves
is actually called (see [`docs/scope.md`](scope.md) and
[`docs/out-of-scope.md`](out-of-scope.md)), mechanically enforced by the
`check_public_surface_baseline` CTest guard so the public surface can't
silently grow. Within that scope, the implementation is careful,
consistent, and honestly self-documenting about every shortcut it takes.
**The narrowness is not a compromise on quality — it is the quality
control**: every shortcut is evidenced, tested, and reversible if a new
call site ever needs more.

## Semantic fidelity to real WinAPI

Good where it matters for the two target games; openly narrow where a
general Win32 app would need more — and every narrowing is documented
in-line with the specific evidence that justifies it.

* `PeekMessageA`/`GetMessageA` (`src/winuser_message.cpp:30-160`)
  correctly implement the real, easy-to-get-wrong WinAPI contract:
  `PeekMessage` must never block; only `GetMessage`/`WaitMessage` may
  sleep. This split is explicit and comment-justified.
* `StretchBlt` (`src/wingdi_blit.cpp`) only supports `SRCCOPY` — any
  other raster-op logs and returns `FALSE` rather than emulating it. A
  real shortcut versus the Win32 spec, but honestly declared, not
  silently wrong.
* `mciGetDeviceIDA` (`src/winmm.cpp:368-372`) ignores its `lpszDevice`
  argument and always returns `1`. A real Win32 app with multiple MCI
  devices open at once would break; safe here only because both target
  games ever open exactly one.
* `_lopen` (`src/winbase_file.cpp:43-69`) ignores `iReadWrite` and always
  opens read-only binary — `OF_WRITE`/`OF_READWRITE` semantics aren't
  implemented.

## Code craftsmanship

Consistently good. Comments explain *why*, not *what* — e.g. the
`WM_ACTIVATEAPP(0)`-suppression reasoning in
`FreeApiMessageQueue.cpp:301-316`. Raw `new`/`delete` is confined to
exactly 7 sites, all part of one deliberate opaque-handle pattern
(`CompatDC`/`CompatBitmap`) with magic-tag double-free protection — not
sloppy memory management. The longest functions (`PumpSdlEvents`, 211
lines; `AndroidExtractAssets`, 175 lines) are flat SDL-event/asset
dispatch switches, not deeply nested logic — a defensible shape for this
kind of shim, not a real complexity smell.

## Test quality, not just test count

Real edge-case depth, not smoke tests. Example:
`TestStretchBlt1to1OutOfRangeSourceRectClipsSafely`
(`tests/test_gdi_regressions.cpp:190-244`) asserts exact clipped-pixel
values *and* that untouched destination pixels stay at a sentinel value —
genuine behavioral verification, not just "didn't crash."

## Architecture

Clean internal/public split: zero public headers include `internal/`.
The one place `include/windows.h` forward-declares an internal function
(`FreeApi::Internal::NormalizeFilesystemPath`, line 149) is a documented,
narrow exception for an inline macro shim, not a leak of internal types.
No game-specific (`blupi`/`eggbert`) branching logic was found anywhere
in `src/` — only comment citations pointing to the evidence.

## What a WinAPI purist would flag

Judged purely as "is this a correct, general WinAPI implementation"
rather than "does it satisfy free-eggbert and planetblupi": the
`SRCCOPY`-only `StretchBlt`, the single-device-only MCI layer,
`_lopen`'s ignored access mode, and (already documented in
[`docs/out-of-scope.md`](out-of-scope.md)) the absence of real
`WM_ACTIVATEAPP(0)` delivery and a true blocking `WaitMessage`. All are
legitimate criticisms of a general WinAPI implementation, and all are
irrelevant to this project's actual, stated goal.
