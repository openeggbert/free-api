# Free API Performance and Correctness TODO

> **Status (2026-07-18):** re-verified against current source. Every P0 and
> P1 item from the original version of this TODO, plus most P2 items, are
> done and have been removed from this file. Only one item remains
> genuinely open (see below). Removed items, for traceability:
>
> - P0: `PeekMessageA` no longer holds `g_messageQueueMutex` during
>   `SDL_Delay` — `src/winuser_message.cpp:44-133`.
> - P0: `GetPixel`/`SetPixel`/`GetDC`/`Lock` share the same backing pixel
>   memory — `free-direct/src/directdraw/DirectDraw.cpp:976-1141`.
> - P0: No `DDERR_WASSTILLDRAWING` busy-wait — `Lock()` returns immediately
>   (`free-direct/src/directdraw/DirectDraw.cpp:1147-1171`).
> - P0: Hot-path `StretchBlt` logs gated behind `FreeApiGdiDebugEnabled()`
>   (`src/wingdi_blit.cpp`).
> - P0: 1:1 `StretchBlt` fast path uses row `memcpy` (`src/wingdi_blit.cpp:78-87`).
> - P1: Scaled `StretchBlt` clipping is intentional and documented in-code
>   (`src/wingdi_blit.cpp:117-131`).
> - P1: Scaled `StretchBlt` avoids per-call allocation via a reused
>   `thread_local` table (`src/wingdi_blit.cpp:111`).
> - P1: Fullscreen scaling is never routed through per-sprite `StretchBlt`
>   — no such code path exists in free-api.
> - P1: Diagnostic counters are gated behind a cached env flag
>   (`FreeApiDiagnosticsFastEnabled()`).
> - P1: Window lookup map (`SDL_WindowID` → `HWND`) is correctly
>   inserted/erased and used by `FindWindowById`
>   (`FreeApiWindowRegistry.cpp`, `winuser_window.cpp`).
> - P2: Focused tests exist for `StretchBlt`/`GetPixel`/`SetPixel`/
>   `PeekMessageA`/`timeSetEvent` (`tests/test_gdi_regressions.cpp`,
>   `tests/test_winuser_regressions.cpp`, `tests/test_timer_regressions.cpp`).
> - P2: Lightweight profiling counters — functionally covered by the
>   existing `FreeApiDiagnostics` system (env-gated counters/high-water
>   marks), just not under the exact name this TODO suggested.

## Open: P2 — Consider `std::vector<uint32_t>` for 32-bit Pixel Buffers

Current representation:

```cpp
std::vector<uint8_t> pixels;
```

This is not automatically bad. It is contiguous and valid for raw pixel buffers.

Possible future improvement:

```cpp
std::vector<uint32_t> pixels;
```

Potential benefits:

- Easier 32-bit pixel copies.
- Cleaner code for RGBA32-only internal buffers.
- Faster inner loops in some paths.

Risks:

- May change assumptions about byte ordering.
- May require touching many pixel access sites.
- Could break compatibility if external code expects `uint8_t*` layout.

Recommendation (unchanged from the original TODO, still valid):

- Do not do this speculatively.
- Revisit only after profiling shows the current `uint8_t` representation is
  an actual bottleneck. No evidence that profiling has happened yet.
