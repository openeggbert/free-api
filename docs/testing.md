# Free API — Running the Tests

Consolidates the scattered "how do I run the tests" instructions previously
split across `NEXT.md`, `Documentation.md`, and `README.md` into one place
(`TASK-24H-1208`). See [`docs/cmake-options.md`](cmake-options.md) for the
full build-mode/CMake-option reference this file complements — that file is
the canonical source for build *options*; this file is the canonical source
for the test-running *workflow*.

## The one thing that isn't optional: `SDL_VIDEODRIVER=dummy`

```bash
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure
```

Under this environment's default (Wayland) display, `test_winuser_regressions`
spuriously fails 6 window-position-exactness assertions
(`ClientToScreen`/`ScreenToClient`/`GetCursorPos`) without these two
environment variables — this is a confirmed environment artifact, not a
real bug. Under a real X11/Xvfb session or the dummy drivers, all tests
pass cleanly. **Always set both**, even when running a single test binary
directly.

## Standalone (no sibling game needed)

```bash
cmake -B build -DFREE_API_USE_SYSTEM_SDL3=ON -DFREE_API_BUILD_TESTS=ON
cmake --build build -j"$(nproc)"
cd build && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure
```

## As a target game's own subdirectory

Both target games build free-api as a real sibling `add_subdirectory()`,
with its own nested `FREE_API` CTest suite:

```bash
# free-eggbert (Ninja)
cd ../free-eggbert/cmake-build-debug && cmake . && ninja -j"$(nproc)"
cd FREE_API && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure

# planetblupi (Make)
cd ../planetblupi/build && cmake . && make -j"$(nproc)"
cd FREE_API && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --output-on-failure
```

Both should report the same test count and 100% pass as the standalone
build — the exact count is the current total of `add_test(...)` entries in
`CMakeLists.txt` (23 as of this writing: run `ctest -N` to get the live
count, since this number grows as tasks add coverage).

## Running one specific test binary directly

Useful for debugging a single failure without the rest of the suite's
noise, or for capturing full stdout without CTest's summarization:

```bash
cd build
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./test_timer_regressions
```

For anything involving background threads (timers, MIDI), prefer
line-buffered output so a crash's true location isn't hidden behind
unflushed buffered PASS lines:

```bash
stdbuf -oL -eL env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./test_gdi_regressions
```

Run just one CTest-registered test by name (regex-matched):

```bash
cd build && ctest -R test_timer_regressions --output-on-failure
```

## Reproducing the `ExtractStringTable.cmake` negative-path checks

`ctest`'s `extract_string_table_*` entries (`CMakeLists.txt`,
`cmake/test-fixtures/`) prove the STRINGTABLE extractor's fail-loud gates
(`REQUIRE_STRINGS`, `VERIFY_ID`/`VERIFY_TEXT`, `USED_IDS_FILE`) actually
fire on broken input, via CTest's `WILL_FAIL` property. To see the exact
`FATAL_ERROR` each one hits (not just pass/fail):

```bash
cd build && ctest -R extract_string_table -VV
```

## Sanitizer-instrumented runs

For cross-thread correctness verification (ThreadSanitizer/AddressSanitizer)
beyond what a normal `ctest` run can catch, see
[`docs/cmake-options.md`](cmake-options.md)'s "Sanitizer-instrumented test
builds" section — as of `TASK-24H-0506`'s session-3 follow-up, a plain
`ctest` inside a `-DFREE_API_SANITIZE=thread`/`address`-configured build
tree just works, no manual `LD_PRELOAD` needed.

## `../free-direct` standalone

`../free-direct` depends on free-api via `add_subdirectory(../free-api FREE_API)`
and its own tests aren't part of free-api's suite, but confirming it still
builds/links against `include/free_api_bridge.h` after a change here is
good practice:

```bash
cd ../free-direct && cmake -S . -B build -DFREE_API_USE_SYSTEM_SDL3=ON
cmake --build build -j"$(nproc)"
```
