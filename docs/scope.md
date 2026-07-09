# Free API — Scope Policy

Free API is a narrow, source-level Win32/WinAPI compatibility layer for exactly
two target games:

* **Free Eggbert** (`../free-eggbert`)
* **Planet Blupi** (`../planetblupi`)

Free API is **not** Wine, **not** a general Win32 SDK reimplementation, and
**not** a platform for arbitrary 1998-era Windows games. It must stay small,
tracking the union of what these two games actually call — nothing more.

## The rule

> **Every new public API (function, type, constant, or message) must cite a
> real usage site — `file:line` — in `../free-eggbert` or `../planetblupi`.**
> If it doesn't have one, it doesn't belong in Free API, no matter how common
> or "obviously useful" it seems.

The only exceptions are:

* **Explicit scope-control cleanup** (removing/hiding/documenting something
  already proven unused).
* **Tests and documentation** for behavior that is already in scope.
* **A minimal compile-only stub**, if and only if: it is required to compile
  one of the two games, the game never relies on its real behavior, and the
  stub fails safely / returns a documented harmless value.
* **The `free-direct`-bridge exception** (TASK-0011): a symbol whose only
  real caller is `free-direct`, acting as the rendering/execution bridge
  *for* one of the two target games, may cite that `free-direct` call site
  instead of a `../free-eggbert`/`../planetblupi` one — see "Boundary with
  `free-direct`" below for the exact, currently-named symbol list. Do not
  broaden this into a general "any sibling project's usage counts" rule —
  it stays scoped to `free-direct` specifically.
* **Test-infrastructure-only** (`TASK-24H-0114`): a symbol called only by
  `tests/`, never by either target game, may be kept if it's cheap and used
  by Free API's own verification — e.g. `GetTickCount`/`Sleep`, kept
  because `tests/basic_test.cpp` calls them directly (see
  `docs/out-of-scope.md`'s "Resources policy" table for that worked
  example). This does not license adding new test-only symbols freely —
  it only means an existing symbol's test-only usage is a legitimate,
  documented reason to keep it rather than remove it.

Symbols proven unused by both target games are legitimate candidates for
removal, hiding behind an opt-in macro, or being left as a documented stub —
see `plan.md` §5 for the current classification of such cases.

## Automated enforcement (TASK-24H-1238)

The rule above was, until now, enforced entirely by human/AI diligence when
writing `plan.md` entries — nothing in the build or test suite verified it.
The `check_public_surface_baseline` CTest test (backed by
`cmake/CheckPublicSurfaceBaseline.cmake`) now closes that gap: it extracts
every public declaration currently in `include/*.h`/`include_non_windows/*.h`
and fails, naming the offending symbol, if anything appears that isn't
already listed in `cmake/known-public-symbols.txt`.

This is a **deliberate speed bump, not a hard block**: if a new symbol is
genuinely needed, follow the rule above (write a `plan.md` task with a real
`file:line` citation), then add the symbol's exact name to
`cmake/known-public-symbols.txt` (one per line) to make the test pass again.
Do not add a symbol to the baseline file without that citation existing
first — doing so defeats the entire point of this check.

The extractor is deliberately lightweight (matching
`cmake/CheckNoHardcodedPaths.cmake`'s own "not a general static-analysis
tool" precedent): it recognizes the declaration shapes this project's
headers actually use (`#define` macros, single-line `WINAPI`-style function
declarations, single-line inline function definitions, function-pointer
typedefs, typedef'd structs with alias lists, plain struct/enum tags, simple
one-line typedefs, `extern` globals) via regex, not a real C++ parser. If it
ever misses a new symbol added in some exotic shape it hasn't seen before,
tighten `cmake/CheckPublicSurfaceBaseline.cmake`'s regex set — do not remove
the check because it isn't perfect.

## Boundary with `free-direct`

Both target games' `ddutil.cpp`-style code freely mixes real Win32 GDI calls
with DirectDraw/DirectSound/DirectPlay calls in the same functions, which
makes it easy to conflate what belongs to which sibling project. The
boundary is symbol-family, not file-based:

* **Free API's responsibility (GDI):** `HDC`/`HBITMAP`-based calls — e.g.
  `CreateCompatibleDC`, `SelectObject`, `GetObjectA`, `StretchBlt`,
  `GetPixel`/`SetPixel`, `LoadImageA`. These operate on free-api's own
  GDI-compatible objects.
* **`free-direct`'s responsibility (DirectX):** `ddraw.h`, `dsound.h`,
  `dplay.h`, and every `DD*`/`DS*`/`DP*` symbol/constant/interface —
  e.g. `IDirectDrawSurface::Blt`, `IDirectDraw::CreateSurface`,
  `DDSURFACEDESC`. Free API must **never** grow a DirectX compatibility
  surface of its own, even though these headers/symbols co-occur constantly
  with real WinAPI calls in both games' source (`ddutil.cpp`'s `DDCopyBitmap`
  is the canonical example: it calls free-api's `StretchBlt` and
  free-direct's `IDirectDrawSurface::GetDC`/`ReleaseDC` in the same function).

Do not audit or modify `free-direct` itself based on this note — that
project is out of this repo's scope; this is a boundary statement only.

### The actual bridge-exception surface (`TASK-24H-0102`/`1219`)

The documented `free-direct`-bridge exception (`plan.md` `TASK-0011`) names
`FreeApiRunWinMain` and the three `include/free_api_bridge.h` functions
(`FreeApiCreateSurfaceDC`, `FreeApiDestroySurfaceDC`,
`FreeApiSetWindowFullscreen`) as the symbols `free-direct` is allowed to
consume without a `../free-eggbert`/`../planetblupi` usage citation. This
session's audit found one more, real, currently-unfixed coupling beyond
that list: `../free-direct/src/diagnostics/Diagnostics.cpp:18,70-72`
forward-declares `FreeApi::Platform::ReadRssKB()` raw and calls it
directly — a fully **internal** free-api implementation detail
(`src/platform/PlatformProcessInfo.hpp:11`), never declared in any public
header, and not part of the bridge exception above. Free API already has
its own internal wrapper for the identical value,
`FreeApi::Internal::FreeApiReadRssKB()`
(`src/internal/FreeApiDiagnostics.hpp:39`, implemented
`src/internal/FreeApiDiagnostics.cpp:83-85` as a one-line passthrough to
the same `ReadRssKB()`), which `free-direct` does not use — it reaches
past the wrapper straight into the internal platform symbol instead.

This is documented here as a known, real gap rather than fixed: whether
the right fix is (a) exposing `FreeApiReadRssKB()` (or `ReadRssKB()`
itself) as a fourth, real bridge-header declaration, or (b) leaving
`free-direct`'s reach as an accepted, if informal, coupling since it's a
diagnostics-only value with no gameplay-correctness stakes, is a follow-on
implementation decision for a future task — not decided here.

The full, evidence-based usage audit (headers, functions, types, constants,
messages, GDI/WinMM/file behavior actually used by both games, current
Free API status per symbol, and the resulting task backlog) lives in
[`plan.md`](../plan.md). This document only states the rule; it does not
duplicate those tables.
