# Free API Performance and Correctness TODO

This TODO file summarizes the remaining performance and correctness work for
`free-api`, especially around `src/winapi.cpp`, GDI-like bitmap/DC handling,
message dispatch, timers, and DirectDraw compatibility.

The goal is **not** to reimplement the whole WinAPI. The goal is to keep the
current small compatibility subset fast enough and correct enough for the
current Free API / Free Direct / Speedy Blupi use case.

## General Constraints

- Do not redesign the whole project.
- Do not change public function signatures unless absolutely necessary.
- Do not add large Doxygen comments to public headers.
- Keep public headers small and close to Windows SDK / Wine style.
- Put long explanations into Markdown documentation.
- Preserve current game-visible behavior unless a change is explicitly listed here.
- Prefer small, reviewable commits.
- Keep compatibility with the current Speedy Blupi / Planet Blupi use case.
- Do not replace the GDI-like software path with SDL_Renderer, SDL_GPU, OpenGL, or another backend as part of these TODOs.

## Priority Legend

- `P0`: Should be fixed first. Likely visible performance or correctness problem.
- `P1`: Important cleanup or medium-risk performance issue.
- `P2`: Nice-to-have optimization or architectural improvement.

## P0: Do Not Hold the Message Queue Mutex While Sleeping

Current risk: `PeekMessageA()` may hold `g_messageQueueMutex` while calling
`SDL_Delay(1)` when the message queue is empty.

This can delay timer threads or other threads trying to call `PostMessageA()`.
Even a 1 ms delay can matter because legacy games may rely on timer callbacks
posting update messages regularly.

Required change:

- Check and pop the message queue while holding the mutex.
- Release the mutex before calling `SDL_Delay(1)`.
- Avoid double-sleeping between `PeekMessageA()` and `GetMessageA()` if possible.
- Do not change the visible message loop behavior.

Suggested shape:

```cpp
{
    std::lock_guard<std::mutex> lock(g_messageQueueMutex);

    if (!g_messageQueue.empty()) {
        *lpMsg = g_messageQueue.front();

        if ((wRemoveMsg & PM_REMOVE) != 0) {
            g_messageQueue.pop();
        }

        return TRUE;
    }
}

SDL_Delay(1);
return FALSE;
```

## P0: Keep `GetPixel` / `SetPixel` Correct for DirectDraw Color Matching

The game uses the classic DirectDraw helper pattern:

```cpp
rgbT = GetPixel(hdc, 0, 0);
SetPixel(hdc, 0, 0, rgb);
pdds->ReleaseDC(hdc);

pdds->Lock(NULL, &ddsd, 0, NULL);
dw = *(DWORD*)ddsd.lpSurface;
pdds->Unlock(NULL);

SetPixel(hdc, 0, 0, rgbT);
```

This means `GetPixel()` and `SetPixel()` are correctness-critical even if they
are slow.

Required behavior:

- `IDirectDrawSurface::GetDC()` must expose a DC backed by the same pixel memory
  that `IDirectDrawSurface::Lock()` later exposes through `DDSURFACEDESC::lpSurface`.
- `SetPixel(hdc, 0, 0, rgb)` must write into the same backing pixel buffer that
  `Lock()` reads from.
- `ReleaseDC()` must not discard the written pixel data.
- `GetPixel()` / `SetPixel()` must not be removed or stubbed.
- Correctness is more important than micro-optimizing these functions.
- These functions are acceptable if slow when used rarely.
- Do not break color-key or color-match behavior.

## P0: Avoid `DDERR_WASSTILLDRAWING` Busy-Wait Problems

The game may call:

```cpp
while (pdds->Lock(NULL, &ddsd, 0, NULL) == DDERR_WASSTILLDRAWING) {
}
```

This is a busy-wait loop. If Free Direct repeatedly returns
`DDERR_WASSTILLDRAWING`, CPU usage can spike badly.

Required behavior:

- For normal software/system-memory surfaces, `Lock()` should usually return
  `DD_OK` immediately.
- Fill `DDSURFACEDESC::lpSurface`, pitch, width/height, and pixel format
  consistently.
- Do not claim one pixel format while exposing memory in another incompatible
  format.
- If the internal surface is RGBA32, the DirectDraw pixel format reported by
  `Lock()` should match that layout or be intentionally translated.

## P0: Remove Hot-Path Rendering Logs

Rendering functions must not log on every frame during normal gameplay.

Required changes:

- `StretchBlt()` must not call `SDL_Log()` for every successful blit.
- Detailed GDI/blit logs should be behind a dedicated environment flag, for example:
  `FREE_API_DEBUG_GDI=1`.
- Error logs for unsupported calls may remain, but avoid spamming the same error
  every frame.
- Timer setup/kill logs should also be gated if they become noisy.

Suggested helper:

```cpp
bool FreeApiGdiDebugEnabled()
{
    static int cached = -1;
    if (cached < 0) {
        const char* value = SDL_getenv("FREE_API_DEBUG_GDI");
        cached = (value && *value && std::strcmp(value, "0") != 0) ? 1 : 0;
    }
    return cached != 0;
}
```

## P0: Improve the 1:1 `StretchBlt` Fast Path

A non-scaled `SRCCOPY` blit is common and should be very fast.

Current risk: even if a fast path exists, it may still copy per pixel and write
four bytes manually.

Required changes:

- Detect the 1:1 case:
  `wDest == wSrc && hDest == hSrc`.
- Clip source and destination rectangles once before the row loop.
- Avoid bounds checks inside the inner pixel loop.
- Prefer row copies where behavior allows it.
- Do not write outside the source or destination buffer.
- Preserve current alpha behavior unless intentionally changed.

Important decision:

- If destination alpha must always become `255`, plain `memcpy()` may change
  behavior if the source alpha is not already `255`.
- If source alpha is guaranteed or normalized to `255`, use row `memcpy()`.
- If alpha must be forced to `255`, use a faster 32-bit loop instead of four
  separate byte writes per pixel, after confirming the pixel layout.

Possible optimized shape when alpha is already safe:

```cpp
memcpy(dstRow, srcRow, static_cast<size_t>(copyWidth) * 4u);
```

Possible optimized shape when alpha must be forced:

```cpp
dst32[i] = src32[i] | alphaMask;
```

Only use the 32-bit form after confirming the exact memory layout.

## P1: Fix Scaled `StretchBlt` Source Clipping Semantics

Current risk: an optimized scaled path may clamp source coordinates to the
nearest edge pixel.

That can change behavior. The older implementation skipped pixels when the
computed source coordinate was outside the source bitmap.

Required decision:

- Either preserve old skip behavior, or
- correctly clip the source and destination rectangles before scaling.

Avoid silently changing rendering from "outside source = no write" to
"outside source = repeat edge pixel" unless that behavior is explicitly wanted.

## P1: Avoid Per-Blit Allocation in Scaled `StretchBlt`

A scaled `StretchBlt()` path may precompute an `srcX` lookup table using:

```cpp
std::vector<int> srcXTable(width);
```

This avoids division in the inner pixel loop, but it may allocate memory in the
render hot path.

Required follow-up:

- Measure how often scaled blits occur.
- If scaled blits are rare, this is acceptable for now.
- If scaled blits are frequent, replace per-call allocation with one of:
  - fixed-point incremental stepping,
  - a reusable `thread_local` vector,
  - a small cached scratch buffer.

Preferred long-term approach:

- Use fixed-point stepping to avoid both division and allocation.

## P1: Keep Fullscreen Scaling Out of GDI `StretchBlt`

Fullscreen mode will need scaling, but it should not stretch every sprite
through the GDI-like `StretchBlt()` path.

Preferred model:

1. Render the game into its original internal backbuffer resolution.
2. At the end of the frame, scale the completed framebuffer once to the window
   or fullscreen output.

This should happen in the presentation layer, not through many individual
GDI-like blits.

Good conceptual flow:

```text
game sprites / DirectDraw / GDI subset
    -> software backbuffer at original game resolution
    -> one final SDL/software/GPU presentation scale
    -> fullscreen window
```

Recommended options:

- Use SDL texture presentation if the project already has an SDL renderer path.
- Or use an SDL surface scaling path once per frame.
- Keep `StretchBlt()` for compatibility operations such as minimap, UI, or
  bitmap conversion.
- Do not make `StretchBlt()` the general fullscreen scaler for every object.

## P1: Gate Diagnostic Counters in Hot Paths

Some diagnostic atomic counters may be incremented even when diagnostics are
disabled.

Required changes:

- In hot paths such as:
  - `PushMessage()`,
  - `PumpSdlEvents()`,
  - `DispatchMessageA()`,
  update diagnostic atomics only when diagnostics are enabled.
- Cache the diagnostics flag so the environment is not queried repeatedly.
- Preserve existing behavior when `FREE_API_DIAGNOSTICS=1` or
  `FREE_DIRECT_DIAGNOSTICS=1`.

Possible helper:

```cpp
bool FreeApiDiagnosticsFastEnabled()
{
    return FreeApiDiagnosticsEnabled();
}
```

If `FreeApiDiagnosticsEnabled()` already caches the environment check, this can
simply wrap it or be renamed for clarity.

## P1: Gate Object Lifetime Diagnostics if They Become Hot

Object lifetime counters for bitmaps, DCs, and SDL surfaces are less critical
than per-message or per-pixel work. They usually run during loading or setup.

However, if profiling shows frequent calls to:

- `CreateCompatibleDC()`,
- `DeleteDC()`,
- `CreateBitmap()`,
- `DeleteObject()`,
- `FreeApiCreateSurfaceDC()`,
- `FreeApiDestroySurfaceDC()`,

then consider gating their diagnostic atomics too.

Do not prioritize this before fixing message queue locking and render hot-path
work.

## P1: Verify Window Lookup Map Maintenance

A map from `SDL_WindowID` to `HWND` is good and avoids repeated linear scans.

Required behavior:

- Insert into `g_windowsById` after successful `SDL_CreateWindow()`.
- Erase from `g_windowsById` in `DestroyWindow()`.
- `FindWindowById()` should use the map.
- Keep fallback behavior for safety if needed.
- Make sure destroyed windows cannot leave stale entries.

## P2: Consider `std::vector<uint32_t>` for 32-bit Pixel Buffers

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

Recommendation:

- Do not do this as part of the first optimization pass.
- First fix `StretchBlt()`, logging, message queue locking, and diagnostics.
- Revisit only after profiling.

## P2: Add Focused Performance/Correctness Tests

Useful tests:

- 1:1 `StretchBlt()` copies the expected rectangle.
- Clipped 1:1 `StretchBlt()` does not write outside destination bounds.
- Scaled `StretchBlt()` keeps nearest-neighbor behavior.
- Out-of-range source rectangles behave intentionally.
- `GetPixel()` / `SetPixel()` write to the same memory visible through
  DirectDraw `Lock()`.
- `PeekMessageA()` does not hold the queue mutex while sleeping.
- `timeSetEvent()` callbacks can still post messages while the main loop is idle.

Do not add broad tests that require a full game launch unless the existing test
infrastructure already supports that.

## P2: Add Lightweight Profiling Counters

Optional profiling counters can help confirm whether the real hot paths are:

- `StretchBlt()`,
- `BltFast()`,
- `GetPixel()` / `SetPixel()`,
- `CreateBitmap()`,
- `CreateCompatibleDC()`,
- message dispatch,
- timer callbacks,
- final fullscreen presentation.

Keep them disabled by default and gated behind an environment variable.

## Current Working Assumptions

- `GetPixel()` / `SetPixel()` are probably not a major performance problem if
  they are mainly used by DirectDraw color matching.
- `std::vector<uint8_t>` is not itself the main performance problem.
- The largest likely performance risks are:
  - CPU `StretchBlt()` in hot paths,
  - logging during rendering,
  - message queue mutex held during sleep,
  - repeated allocation in scaled blits,
  - inconsistent surface/DC/Lock pixel memory semantics.
- Fullscreen scaling should be one final presentation step per frame, not many
  per-object GDI-style stretches.

## Suggested Implementation Order

1. Fix `PeekMessageA()` so it never sleeps while holding `g_messageQueueMutex`.
2. Gate or remove hot-path `SDL_Log()` calls.
3. Improve the 1:1 `StretchBlt()` fast path.
4. Verify `GetPixel()` / `SetPixel()` / `GetDC()` / `ReleaseDC()` / `Lock()`
   memory consistency.
5. Make sure `Lock()` normally returns `DD_OK` for accessible software surfaces.
6. Fix scaled `StretchBlt()` source clipping semantics.
7. Avoid per-call allocation in scaled `StretchBlt()` if profiling shows it matters.
8. Keep fullscreen scaling in the presentation layer.
9. Gate remaining hot-path diagnostic atomics.
10. Add focused tests for blitting and color matching.

## Done Criteria

This TODO can be considered mostly complete when:

- Normal gameplay does not spam logs.
- Idle message loops do not block timer message posting by sleeping under a mutex.
- 1:1 blits are row-based or otherwise clearly optimized.
- Scaled blits avoid division in the inner loop.
- Scaled blits have intentional and documented clipping behavior.
- DirectDraw color matching still works.
- DirectDraw surface `Lock()` does not cause busy-wait CPU spikes.
- Fullscreen scaling happens as a final presentation step, not as repeated
  per-sprite `StretchBlt()` work.
- Existing tests pass.
- Speedy Blupi still launches, renders, accepts input, and plays normally.
