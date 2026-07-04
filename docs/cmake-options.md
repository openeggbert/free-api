# Free API — Build Modes

Free API never vendors SDL3 itself. It acquires `SDL3::SDL3`,
`SDL3_image::SDL3_image` and `SDL3_mixer::SDL3_mixer` in one of three ways,
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

## 3. Sibling-vendored (developer convenience)

If neither of the above applies and Free API is checked out next to one of
its two target games (`../free-eggbert` or `../planetblupi`), it will reuse
that game's own `cmake/ThirdPartySDL.cmake` vendoring helper. This only works
when that sibling game is also the top-level CMake project (its own
`third_party/SDL` submodules must be checked out), so it is mainly useful
when iterating on Free API from within a full game checkout rather than for
a fully standalone Free API build.

If none of the three apply, configuration fails with a clear error message
explaining the three options above.

## Tests

`FREE_API_BUILD_TESTS` defaults to `ON` (except under Emscripten, where it
defaults to `OFF`). Run with:

```bash
cmake -B build -DFREE_API_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```
