# Image overlay and scaled surface DCs (2026-11-10)

For blupi_recon's higher-resolution images. Off unless the environment asks for it; without the
variables below nothing changes.

## `LoadImageA` image overlay

`FREE_API_IMAGE_OVERLAY=<dir>` names a folder laid out like the game's own (for example
`<dir>/IMAGE08/OBJECT.BLP`). When a game loads an image from a file (`LR_LOADFROMFILE`, `cx` and
`cy` 0), `LoadImageA` looks for the same relative path under `<dir>` first, ignoring case like the
rest of Free API's file access, then for the same name with `.png` or `.bmp`.

The overlay image is used only if it is exactly `FREE_API_IMAGE_SCALE` (1..8, default 2) times as
wide and as high as the original file, whose header is read for the comparison. Otherwise, or if it
cannot be decoded, the original loads and the problem is logged once per file. Images are decoded
by SDL_image: BMP (including 32-bit BMPs with an alpha mask), and PNG where SDL_image has the codec
(not in blupi_recon's web build).

An alpha channel that is not fully opaque is kept (`CompatBitmap::hasAlpha`). Free Direct then
blends with it (below); the games' colour key still applies as well. Images without alpha behave as
before.

The bitmap keeps the scale:

* `GetObjectA` reports the original's size (physical size divided by the scale), which is what
  `DDLoadBitmap` sizes its DirectDraw surface from (`../blupi_recon/src/ddutil.cpp:104-118`);
* GDI coordinates on a memory DC holding it (`StretchBlt`, `GetPixel`, `SetPixel`) are logical.

## `FreeApiCreateScaledSurfaceDC`

The bridge function `FreeApiCreateScaledSurfaceDC(pixels, width, height, pitch, 32, scale)`
(`include/free_api_bridge.h`) wraps a physical buffer whose logical size is `width/scale` x
`height/scale`. Free Direct uses it for `IDirectDrawSurface::GetDC` when
`FREE_DIRECT_BACKING_SCALE` enlarges its surfaces.

* `StretchBlt` multiplies the destination rectangle by the DC's scale and the source rectangle by
  the source bitmap's scale: an overlay image into a surface of the same scale copies 1:1, an
  original image into a scaled surface is enlarged (nearest neighbour), and an overlay image into
  an unscaled surface is reduced.
* `SetPixel`/`GetPixel` address the top-left physical pixel of the logical one. blupi_recon's
  `DDColorMatch` saves pixel (0,0), writes a colour, reads its raw value through `Lock` and
  restores the saved colour; writing one physical pixel keeps that restore exact.

## Alpha: `FreeApiGetSurfaceDCAlpha`

`StretchBlt` from an image with alpha copies the alpha into the 4th byte of the destination's
pixels (both the 1:1 and the scaled path) and records it on the surface DC.
`FreeApiGetSurfaceDCAlpha(hdc)` (`include/free_api_bridge.h`) returns what was drawn since the DC
was created:

* 0: no image;
* 1: only opaque images;
* 2: an image with alpha.

Free Direct's `ReleaseDC` reads it to keep the alpha as the surface's alpha plane (2), drop an
earlier plane (1), or leave it alone (0, e.g. `DDColorMatch`'s `SetPixel`).

## Window

* `FREE_API_WINDOW_SCALE=k` (1..4) creates windows `k` times the requested size. The game keeps
  the requested client size: `GetClientRect`, `ClientToScreen`/`ScreenToClient` and mouse
  messages stay in it.
* `FREE_API_HIGH_PIXEL_DENSITY=1` adds `SDL_WINDOW_HIGH_PIXEL_DENSITY` (a high-DPI display, or the
  browser's `devicePixelRatio` for the web canvas).
* **The game's own full screen.** This applies when Free Direct calls `FreeApiSetWindowFullscreen`
  for `DDSCL_FULLSCREEN` or `SetDisplayMode`. The screen is then the game's display mode, as
  under real DirectDraw:
  * `ClientToScreen`/`ScreenToClient` leave points unchanged;
  * `GetCursorPos`/`SetCursorPos` use the game's logical coordinates (`src/winuser_cursor.cpp`).

  Without this, a full-screen game larger than 640×480 drew its frame at window coordinates and
  squeezed it into part of the screen; blupi_recon's `CPixmap::Display` blits to a primary
  surface without a clipper at the `ClientToScreen` rectangle. Full screen is the way to show
  4× images on a screen smaller than 2560×1920.

Tests: `tests/test_gdi_regressions.cpp` (overlay, alpha, scaled DC) and
`tests/test_winuser_regressions.cpp` (window scale); the full-screen coordinates are covered by
blupi_recon's `freeapi/tests/presentation.cpp`.
