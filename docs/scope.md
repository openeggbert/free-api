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

Symbols proven unused by both target games are legitimate candidates for
removal, hiding behind an opt-in macro, or being left as a documented stub —
see `plan.md` §5 for the current classification of such cases.

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

## Full audit

The full, evidence-based usage audit (headers, functions, types, constants,
messages, GDI/WinMM/file behavior actually used by both games, current
Free API status per symbol, and the resulting task backlog) lives in
[`plan.md`](../plan.md). This document only states the rule; it does not
duplicate those tables.
