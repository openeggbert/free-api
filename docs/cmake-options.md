# Free API — Build Modes

Free API never vendors SDL3 itself. It acquires `SDL3::SDL3`,
`SDL3_image::SDL3_image` and `SDL3_mixer::SDL3_mixer` in one of two ways,
tried in this order:

## 1. Parent-provided (default when built as a subdirectory)

If `../free-eggbert` or `../planetblupi` already configured these targets
before calling `add_subdirectory(../free-api ...)`, Free API reuses them
as-is. No extra flags needed.

```bash
# from ../free-eggbert or ../planetblupi
cmake -B build
cmake --build build
```

## 2. System SDL3

```bash
cmake -B build -DFREE_API_USE_SYSTEM_SDL3=ON
cmake --build build
```

Requires SDL3, SDL3_image and SDL3_mixer development packages (headers +
CMake config files) to be discoverable by `find_package()` — e.g. via
`CMAKE_PREFIX_PATH` if they're installed to a non-standard location.

If neither of the above applies, configuration fails with a clear error
message explaining the two options above.

A third "sibling-vendored" tier — reusing a sibling game's own
`cmake/ThirdPartySDL.cmake` vendoring helper when Free API itself is the
top-level project — previously existed here but was removed (see
`plan.md` `TASK-24H-0001`): that helper always resolves its vendored-
dependency root relative to `CMAKE_SOURCE_DIR`, which is Free API's own
root in exactly the situation where this fallback could ever be reached, so
it could never actually succeed — it only produced a confusing, wrongly-
attributed "missing submodule" error instead of the clear guidance above.

## Tests

`FREE_API_BUILD_TESTS` defaults to `ON` (except under Emscripten, where it
defaults to `OFF`). Run with:

```bash
cmake -B build -DFREE_API_BUILD_TESTS=ON
cmake --build build
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir build --output-on-failure
```

`SDL_VIDEODRIVER=dummy`/`SDL_AUDIODRIVER=dummy` are **not optional** under
this environment's default (Wayland) display — without them,
`test_winuser_regressions` spuriously fails 6 window-position-exactness
assertions (`ClientToScreen`/`ScreenToClient`/`GetCursorPos`); this is a
confirmed environment artifact, not a real bug (see `NEXT.md` §5). Under a
real X11/Xvfb session or the dummy drivers, all tests pass cleanly.

## LoadStringA / STRINGTABLE target-game selection

`FREE_API_TARGET_GAME` controls which game's resources drive `LoadStringA`'s
real-text table (see `cmake/ExtractStringTable.cmake` and
[`used-string-ids.md`](used-string-ids.md)). Allowed values:

* `auto` (default) — detect via `CMAKE_PROJECT_NAME`: `SPEEDY_BLUPI_WINDOWS`
  selects free-eggbert, `PLANET_BLUPI_WINDOWS` selects planetblupi, anything
  else (including Free API's own standalone `project()`) falls back to the
  ungated developer-convenience sibling lookup.
* `free-eggbert` / `planetblupi` — force that game's resources and its
  fail-loud `REQUIRE_STRINGS`/known-ID/used-ID gating, even if
  `CMAKE_PROJECT_NAME` doesn't currently say so. If Free API is not actually
  being built as that game's own subdirectory, this falls back to reading
  `../free-eggbert` or `../planetblupi`'s resources by sibling path instead
  — useful for exercising the full verification pipeline (including the
  used-ID manifest checks) without a complete game build tree.
* `standalone` — force the ungated placeholder-fallback path even if
  `CMAKE_PROJECT_NAME` would otherwise select a game.

```bash
cmake -B build -DFREE_API_TARGET_GAME=planetblupi
```

Passing an unrecognized value fails configure immediately with a clear
error listing the four allowed values.

## The `../free-direct` bridge build

`../free-direct` (a sibling project implementing the DirectDraw/
DirectSound/DirectPlay subset both target games also need) depends on
Free API via `add_subdirectory(../free-api FREE_API)`, guarded by
`if(NOT TARGET free-api)`. Confirmed working standalone:

```bash
cd ../free-direct
cmake -S . -B build -DFREE_API_USE_SYSTEM_SDL3=ON
cmake --build build
```

This configures, builds, and links `libfree-api.a` → `libfree-direct.a` →
the `FREE_DIRECT` executable cleanly, using
[`include/free_api_bridge.h`](../include/free_api_bridge.h) for the three
free-direct-bridge functions.

**Coverage gap:** `../free-direct/CMakeLists.txt` forces
`FREE_API_BUILD_TESTS OFF` before its `add_subdirectory()` call, so this
standalone build never builds or registers Free API's own 17-test CTest
suite. A passing `../free-direct` build proves configure/build/link only,
not Free API's own regression coverage — run Free API's own test suite
separately (see "Tests" above) for that.

**The diamond dependency case:** when either target game builds with its
`FREEDIRECT` backend, its own `CMakeLists.txt` calls
`add_subdirectory()` on *both* `../free-api` and `../free-direct` in the
same configure run; `free-direct`'s own `if(NOT TARGET free-api)` guard is
what prevents the `free-api` target (and its 17 tests) from being
registered twice. Confirmed via a real Ninja rebuild of `../free-eggbert`
with its `FREEDIRECT` backend: exactly 17 CTest tests registered (not
34), both `SPEEDY_BLUPI_WINDOWS` and `FREE_DIRECT` executables built, all
tests passing.

## Examples

`FREE_API_BUILD_EXAMPLES` defaults to `OFF` (these are interactive
demonstrations with a real window/audio backend, not meant to be built by
default when either target game consumes Free API as a sibling library).
See [`examples/README.md`](../examples/README.md).

```bash
cmake -B build -DFREE_API_USE_SYSTEM_SDL3=ON -DFREE_API_BUILD_EXAMPLES=ON
cmake --build build
```
