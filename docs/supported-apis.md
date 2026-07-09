# Free API — Supported APIs

This is the hand-maintained, current reference for every public symbol Free
API implements, keyed to real usage in the two target games. It replaces
`plan.md` §4 as the living version of that table — `plan.md` §4 is a
point-in-time audit snapshot and is not updated after the fact; this file
is. See [`docs/scope.md`](scope.md) for the policy this table enforces
(every row must have a real usage site) and [`docs/out-of-scope.md`](out-of-scope.md)
for symbols deliberately *not* implemented.

**Known gap (per `plan.md` TASK-0001):** this table is currently missing
rows for 28 declared public symbols found in a fresh full-header audit —
see `plan.md` §4.6/TASK-0001 for the exact list.

**Status legend:** `IMPLEMENTED` (real behavior) · `PARTIAL` (real behavior,
narrower than full Win32 semantics, sufficient for both games' actual use)
· `STUB` (compile-only or safe placeholder, see out-of-scope.md) · `STUB (acceptable)`
(a stub that is the intentionally-sufficient behavior, not a gap).

| Symbol / behavior | Category | Required by free-eggbert | Required by planetblupi | Current status | Test coverage |
|---|---|---|---|---|---|
| `RegisterClassA` + `WNDCLASSA` | WinUser | Yes | Yes | IMPLEMENTED | `test_winuser_regressions.cpp` |
| `CreateWindowExA` (fullscreen) | WinUser | Yes | Yes | IMPLEMENTED | `test_winuser_regressions.cpp` |
| `CreateWindowA` (windowed) | WinUser | Yes | Yes | IMPLEMENTED | `test_winuser_regressions.cpp` |
| `AdjustWindowRect` | WinUser | Yes (live) | Yes (live) | IMPLEMENTED (deliberate identity transform — see doc comment in `include/winuser.h`) | `test_winuser_regressions.cpp` |
| `ShowWindow`/`UpdateWindow`/`SetFocus` | WinUser | Yes | Yes | IMPLEMENTED | `test_winuser_regressions.cpp` |
| `GetSystemMetrics` (`SM_CXSCREEN/CYSCREEN/CYCAPTION`) | WinUser | Yes | Yes | PARTIAL (fixed values for only these three indices, confirmed sufficient — both games query exactly these three, once, at startup; any other index returns 0) | `test_winuser_regressions.cpp` |
| `PeekMessageA`/`GetMessageA`/`TranslateMessage`/`DispatchMessageA` | WinUser | Yes | Yes | IMPLEMENTED (`PeekMessageA`'s `hWnd`/filter args are intentionally ignored — unfiltered peek always, see `docs/out-of-scope.md`) | `test_input_pipeline.cpp`, `test_planetblupi_loop.cpp`, `test_eggbert_loop.cpp`, `test_winuser_regressions.cpp` (remove/no-remove semantics, `WM_QUIT` return), `test_timer_regressions.cpp` (stress-tested under concurrent posting) |
| `WaitMessage` | WinUser | Yes | Yes | IMPLEMENTED (polling-based: checks queue, sleeps ~1ms once if empty, then returns `TRUE` unconditionally — not a true blocking wait, see `docs/out-of-scope.md`) | `test_winuser_regressions.cpp` |
| `PostQuitMessage`/`PostMessageA` | WinUser | Yes | Yes | IMPLEMENTED (cross-thread-safe; `PostMessageA` performs no `hWnd`/message validation — safe because both games only ever post to windows they themselves created, see `docs/out-of-scope.md`) | `test_timer_regressions.cpp` (stress test) |
| `wsprintfA` | WinUser | Yes (`soundbass.cpp:142`, `sound.cpp:117`, sound-diagnostic path) | Yes (`sound.cpp:99`, same path) | IMPLEMENTED (`vsnprintf` into a fixed 1024-byte buffer — **TASK-24H-1230: this is a real buffer-size hazard, not just an internal detail.** `wsprintfA` writes up to 1024 bytes into the caller's buffer regardless of that buffer's actual size (matching real Win32 `wsprintfA`'s own historically-unsafe no-length-parameter contract). All three real call sites above declare only a 256-byte stack buffer — currently safe only because the fixed format string (`"Data1 : %d, dwdata: %d, pFile: %d"`) can't realistically produce more than ~60 bytes. Any *new* call site must keep its expected output well under its own buffer's size, not just under 1024 bytes. Separately, all three call sites pass pointer arguments where the format string expects `int` (`%d`) — technically UB per the C standard, confirmed harmless on this project's actual target ABI (SysV x86-64, where pointer and `int` share the same register class), inherited from the original 1998-era game source, not introduced by free-api.) | `test_winuser_regressions.cpp` |
| `OutputDebugStringA` | WinBase | Yes (`misc.cpp:32`, DirectSound-failure diagnostic path) | Yes (`wave.cpp:234,264,268`, same shape) | IMPLEMENTED (null-checked `printf` to stdout) | `test_winuser_regressions.cpp` |
| `DefWindowProcA` (`WM_CLOSE`→destroy, `WM_DESTROY`→quit) | WinUser | Yes | Yes | IMPLEMENTED | `test_winuser_regressions.cpp` |
| `DestroyWindow` | WinUser | Yes | Yes | IMPLEMENTED (synchronously dispatches `WM_DESTROY` to the window's own `WndProc` before tearing the window down) | `test_winuser_regressions.cpp` (`TestDestroyWindowDispatchesWmDestroySynchronously`) |
| `MoveWindow` | WinUser | dead-reach only | dead-reach only | PARTIAL — repositions/resizes the real SDL window, but never updates `g_freeApiWindowStates`'s logical width/height (`GetClientRect`/`ClientToScreen`/`ScreenToClient` would go stale after a real resize). Confirmed harmless: the only call sites in either game are inside `movie.cpp`, itself dead-reach since the AVI probe always fails (`TASK-24H-0309`) | none (dead-reach; not exercised by either game in practice) |
| `SetWindowTextA` | WinUser | Yes (`blupi.cpp:532,541`) | Yes (`blupi.cpp:453,462`) | IMPLEMENTED (real `SDL_SetWindowTitle` call) | `test_winuser_regressions.cpp` (`TestSetWindowTextASetsRealWindowTitle`) |
| `GetClientRect` | WinUser | Yes | Yes (hot path) | IMPLEMENTED | `test_winuser_regressions.cpp` |
| `SetRect` | WinUser | Yes (`blupi.cpp:756`) | Yes (`blupi.cpp:648`) | IMPLEMENTED (`include/winuser.h`, header-only inline; real Win32 semantics, null-checked) | none |
| `IntersectRect` | WinUser | Yes (`decnet.cpp:368`, `pixmap.cpp:1610`, `decmove.cpp`: ~14 call sites, `decblupi.cpp:4275,4298`) | Yes (`pixmap.cpp:1083`) | IMPLEMENTED (`include/winuser.h`, header-only inline; real Win32 semantics — computes the overlap rect and returns whether it's non-empty, null-checked) | none |
| `UnionRect` | WinUser | Yes (`pixmap.cpp:1612`) | Yes (`pixmap.cpp:1085`) | IMPLEMENTED (`include/winuser.h`, header-only inline; real Win32 semantics, null-checked) | none |
| `SetTimer`/`KillTimer`/`WM_TIMER` | WinUser/Timers | dead | Yes (live) | IMPLEMENTED | `test_timer_regressions.cpp` |
| `timeSetEvent`/`timeKillEvent` | WinMM/Timers | Yes (live) | dead | IMPLEMENTED | `test_timer_regressions.cpp` |
| `GetCursorPos`/`ScreenToClient`/`ClientToScreen`/`SetCursorPos` | WinUser/Input | Yes | Yes (hot path) | IMPLEMENTED | `test_winuser_regressions.cpp` |
| `ShowCursor`/`SetCursor` | WinUser/Input | Yes | Yes | IMPLEMENTED (real SDL-backed) | `test_winuser_regressions.cpp` |
| `LoadCursorA`/`LoadIconA` | WinUser/Resources | Yes | Yes | STUB (acceptable — non-null handle only; no evidence either game inspects the real cursor/icon shape) | `test_winuser_regressions.cpp` |
| VK_* set + `WM_KEYDOWN/UP`/`WM_SYSKEYDOWN/UP` (F10) | WinUser/Input | Yes | Yes | IMPLEMENTED (evidenced-required entries per `docs/audit-24h-free-api.md` §2.4 confirmed complete; the table also keeps unevidenced-but-harmless extras — digit keys 0-9, `INSERT`/`DELETE`/`PAGEUP`/`PAGEDOWN`/`TAB`/`BACKSPACE`, `VK_MENU`/Alt, and the `KP_ENTER`→`VK_RETURN` alias — kept intentionally, not scope creep, same "keep + document" pattern as `HFONT`/`HPALETTE` and `_chdir`/`_getcwd`, TASK-24H-0409) | `test_input_pipeline.cpp` |
| `MK_*` flags in `WM_MOUSEMOVE.wParam` | WinUser/Input | No | Yes | IMPLEMENTED (`MK_LBUTTON`/`MK_RBUTTON` tested; `MK_SHIFT`/`MK_CONTROL` implemented but not automatable in this headless environment — the two drive **two distinct** planetblupi features, not one generic "modifier": `MK_SHIFT` drives drag-select multi-unit highlight (`event.cpp:3440,3472,3504`, `BlupiHiliDown/Move/Up`), `MK_CONTROL` drives a separate level-editor decor flood-fill (`event.cpp:3843,3877,3908`, `ArrangeFill`). See `TASK-24H-0401`/`0403` for the tracked human-playtest verification of each) | `test_winuser_regressions.cpp` |
| `WM_MOUSEMOVE`/`WM_LBUTTONDOWN/UP`/`WM_RBUTTONDOWN/UP` | WinUser/Input | Yes | Yes | IMPLEMENTED (bit-exact `lParam` packing, demo-file compatible) | `test_input_pipeline.cpp` |
| `WM_CREATE`/`WM_ACTIVATEAPP`/`WM_SYSCOLORCHANGE`/`WM_QUERYNEWPALETTE`/`WM_PALETTECHANGED`/`WM_DISPLAYCHANGE`/`WM_SETCURSOR`, `WM_NCMOUSEMOVE` (planetblupi only) | WinUser | Yes | Yes | IMPLEMENTED (plain dispatched constants — no free-api-side special handling beyond generic `DispatchMessageA` routing, unlike `WM_MOUSEMOVE`/`WM_TIMER`/keyboard messages, which have dedicated translation logic) | none dedicated (routed generically through `DispatchMessageA`, already covered by that function's own tests) |
| `CreateCompatibleDC`/`DeleteDC`/`SelectObject`/`DeleteObject`/`GetObjectA` | GDI | Yes | Yes | IMPLEMENTED | `test_gdi_regressions.cpp` |
| `GetDeviceCaps`(`SIZEPALETTE`)/`GetSystemPaletteEntries` | GDI | Yes | Yes | IMPLEMENTED (`index` is ignored entirely — returns 0 for every index, not just `SIZEPALETTE`; confirmed correct only for the one index either game queries) | `test_gdi_regressions.cpp` |
| `LoadImageA` (`LR_LOADFROMFILE`, extension-agnostic) | GDI | Yes | Yes | IMPLEMENTED | `test_gdi_regressions.cpp`, `test_file_paths.cpp` |
| `StretchBlt` (`SRCCOPY`) | GDI | Yes | Yes | IMPLEMENTED | `test_gdi_regressions.cpp` |
| `GetPixel`/`SetPixel` | GDI | Yes | Yes | IMPLEMENTED (consistent with free-direct's `Lock()`-exposed backing memory) | `test_gdi_regressions.cpp` |
| `CreateBitmap` | GDI | No | Yes | IMPLEMENTED (8bpp/16bpp/32bpp raw-buffer bitmap creation for the minimap; no 1bpp path exists, no call site uses it). 8bpp is greyscale-only (no palette lookup) -- confirmed (`TASK-24H-0602`/`0603`) this only affects planetblupi's minimap in fullscreen mode, which the shipped `data/config.def` (`FullScreen=0`) does not use by default; not fixed, since a real fix would need either a general Win32 palette API or game-specific hard-coding, both out of scope. | `test_gdi_regressions.cpp` |
| `_lopen`/`_lread`/`_lclose` | File | Yes | Yes | IMPLEMENTED (handle table `g_openFiles`/`g_nextFileHandle` is unsynchronized — a latent, intentional-and-deferred data race if ever called concurrently; both games' only call sites are single-threaded `ddutil.cpp` chains) | `test_file_regressions.cpp` |
| `CreateDirectoryA` | File | dead | Yes (live) | IMPLEMENTED (idempotent: returns `TRUE` for an already-existing directory rather than real Win32's `FALSE`/`ERROR_ALREADY_EXISTS` — confirmed harmless, neither game checks the return value, see `docs/out-of-scope.md`) | `test_file_regressions.cpp` |
| `RemoveDirectoryA` | File | No | No | IMPLEMENTED (kept for API completeness, mirrors `CreateDirectoryA`/`DeleteFileA`) | none on Linux — its only exerciser (`tests/basic_test.cpp`) is Windows-only cleanup code (`#ifdef _WIN32`) that never compiles/runs on this project's actual Linux CI target |
| `_mkdir` | File | Yes | No | IMPLEMENTED | `test_file_regressions.cpp` |
| `_findfirst`/`_findnext`/`_findclose` | File | Yes | No (planetblupi has zero find-session API usage; all its data-file access uses deterministic constructed filenames like `world%.3d.blp`) | IMPLEMENTED (real `std::filesystem`-backed directory listing, scoped to a directory + simple `*.ext` wildcard). free-eggbert's design-mission picker (`event.cpp:4741-4747`) is the sole consumer across both games and never calls `_findclose`; `_findnext` auto-erases its session on exhaustion (`plan.md` `TASK-0009`/`TASK-24H-0701`) so this no longer leaks. | `test_file_paths.cpp` |
| Backslash/forward-slash path normalization | File | Yes | Yes (mixed within same game) | IMPLEMENTED | `test_file_regressions.cpp` |
| `FindResourceA`(`RT_BITMAP`) miss→fallback | Resources | Yes | Yes | IMPLEMENTED (as a permanent, correct miss — see `docs/out-of-scope.md`) | `test_file_regressions.cpp`, `test_resources.cpp` |
| `LoadStringA` | Resources | Yes (~90+ sites) | Yes (~50+ sites) | IMPLEMENTED (real per-build string table extracted from whichever game's `.rc` is driving the build) | `test_loadstring_regressions.cpp`, `test_resources.cpp` |
| `mciSendCommandA` `"sequencer"` (MIDI music) | WinMM | Yes | Yes | IMPLEMENTED | `basic_test.cpp`, `test_mci_sequences.cpp` |
| `MM_MCINOTIFY`/`MCI_NOTIFY_SUCCESSFUL` | WinMM | Yes | Yes | IMPLEMENTED | `test_mci_sequences.cpp` |
| `midiOutGetNumDevs`/`Open`/`SetVolume`/`Close` | WinMM | Yes | Yes | IMPLEMENTED | `test_mci_sequences.cpp` |
| `mciSendCommandA` `"avivideo"`/`MCI_DGV_*` (movies) | WinMM | Yes | Yes | STUB (permanently declined — see `docs/out-of-scope.md`) | `test_mci_avivideo_regressions.cpp` |
| `"cdaudio"` graceful decline | WinMM | Yes (live, config-gated) | No (zero references anywhere in source) | IMPLEMENTED | `test_mci_sequences.cpp` |
| `joyGetPosEx`/`joyGetNumDevs` | WinMM/Joystick | dead | No | IMPLEMENTED (real `SDL_Joystick`-backed: 2 axes + first 4 buttons; TASK-0103) | `test_joystick_regressions.cpp` (SDL virtual joystick) |
| `GlobalMemoryStatus` | WinBase | Yes | No | PARTIAL (plausible `dwTotalPhys` only) | none |
| `fopen`/`fread`/`fwrite`/`fclose` (via wrapper) | File | Yes | Yes | IMPLEMENTED (backslash-normalizing, case-insensitive-fallback wrapper) | `test_file_regressions.cpp`, `test_file_paths.cpp` |
