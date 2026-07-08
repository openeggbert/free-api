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
| `GetSystemMetrics` (`SM_CXSCREEN/CYSCREEN/CYCAPTION`) | WinUser | Yes | Yes | IMPLEMENTED | `test_winuser_regressions.cpp` |
| `PeekMessageA`/`GetMessageA`/`TranslateMessage`/`DispatchMessageA` | WinUser | Yes | Yes | IMPLEMENTED | `test_input_pipeline.cpp`, `test_planetblupi_loop.cpp`, `test_eggbert_loop.cpp` |
| `WaitMessage` | WinUser | Yes | Yes | IMPLEMENTED (non-busy idle wait) | `test_winuser_regressions.cpp` |
| `PostQuitMessage`/`PostMessageA` | WinUser | Yes | Yes | IMPLEMENTED (cross-thread-safe) | `test_timer_regressions.cpp` (stress test) |
| `DefWindowProcA` (`WM_CLOSE`→destroy, `WM_DESTROY`→quit) | WinUser | Yes | Yes | IMPLEMENTED | `test_winuser_regressions.cpp` |
| `GetClientRect` | WinUser | Yes | Yes (hot path) | IMPLEMENTED | `test_winuser_regressions.cpp` |
| `SetTimer`/`KillTimer`/`WM_TIMER` | WinUser/Timers | dead | Yes (live) | IMPLEMENTED | `test_timer_regressions.cpp` |
| `timeSetEvent`/`timeKillEvent` | WinMM/Timers | Yes (live) | dead | IMPLEMENTED | `test_timer_regressions.cpp` |
| `GetCursorPos`/`ScreenToClient`/`ClientToScreen`/`SetCursorPos` | WinUser/Input | Yes | Yes (hot path) | IMPLEMENTED | `test_winuser_regressions.cpp` |
| `ShowCursor`/`SetCursor` | WinUser/Input | Yes | Yes | IMPLEMENTED (real SDL-backed) | `test_winuser_regressions.cpp` |
| `LoadCursorA`/`LoadIconA` | WinUser/Resources | Yes | Yes | STUB (acceptable — non-null handle only; no evidence either game inspects the real cursor/icon shape) | `test_winuser_regressions.cpp` |
| VK_* set + `WM_KEYDOWN/UP`/`WM_SYSKEYDOWN/UP` (F10) | WinUser/Input | Yes | Yes | IMPLEMENTED | `test_input_pipeline.cpp` |
| `MK_*` flags in `WM_MOUSEMOVE.wParam` | WinUser/Input | No | Yes | IMPLEMENTED (`MK_LBUTTON`/`MK_RBUTTON` tested; `MK_SHIFT`/`MK_CONTROL` implemented but not automatable in this headless environment) | `test_winuser_regressions.cpp` |
| `WM_MOUSEMOVE`/`WM_LBUTTONDOWN/UP`/`WM_RBUTTONDOWN/UP` | WinUser/Input | Yes | Yes | IMPLEMENTED (bit-exact `lParam` packing, demo-file compatible) | `test_input_pipeline.cpp` |
| `CreateCompatibleDC`/`DeleteDC`/`SelectObject`/`DeleteObject`/`GetObjectA` | GDI | Yes | Yes | IMPLEMENTED | `test_gdi_regressions.cpp` |
| `GetDeviceCaps`(`SIZEPALETTE`)/`GetSystemPaletteEntries` | GDI | Yes | Yes | IMPLEMENTED | `test_gdi_regressions.cpp` |
| `LoadImageA` (`LR_LOADFROMFILE`, extension-agnostic) | GDI | Yes | Yes | IMPLEMENTED | `test_gdi_regressions.cpp`, `test_file_paths.cpp` |
| `StretchBlt` (`SRCCOPY`) | GDI | Yes | Yes | IMPLEMENTED | `test_gdi_regressions.cpp` |
| `GetPixel`/`SetPixel` | GDI | Yes | Yes | IMPLEMENTED (consistent with free-direct's `Lock()`-exposed backing memory) | `test_gdi_regressions.cpp` |
| `CreateBitmap` | GDI | No | Yes | IMPLEMENTED (8bpp/16bpp/32bpp raw-buffer bitmap creation for the minimap; no 1bpp path exists, no call site uses it). 8bpp is greyscale-only (no palette lookup) -- confirmed (`TASK-24H-0602`/`0603`) this only affects planetblupi's minimap in fullscreen mode, which the shipped `data/config.def` (`FullScreen=0`) does not use by default; not fixed, since a real fix would need either a general Win32 palette API or game-specific hard-coding, both out of scope. | `test_gdi_regressions.cpp` |
| `_lopen`/`_lread`/`_lclose` | File | Yes | Yes | IMPLEMENTED | `test_file_regressions.cpp` |
| `CreateDirectoryA` | File | dead | Yes (live) | IMPLEMENTED | `test_file_regressions.cpp` |
| `_mkdir` | File | Yes | No | IMPLEMENTED | `test_file_regressions.cpp` |
| `_findfirst`/`_findnext`/`_findclose` | File | Yes | No (planetblupi has zero find-session API usage; all its data-file access uses deterministic constructed filenames like `world%.3d.blp`) | IMPLEMENTED (real `std::filesystem`-backed directory listing, scoped to a directory + simple `*.ext` wildcard). free-eggbert's design-mission picker (`event.cpp:4741-4747`) is the sole consumer across both games and never calls `_findclose`; `_findnext` auto-erases its session on exhaustion (`plan.md` `TASK-0009`/`TASK-24H-0701`) so this no longer leaks. | `test_file_paths.cpp` |
| Backslash/forward-slash path normalization | File | Yes | Yes (mixed within same game) | IMPLEMENTED | `test_file_regressions.cpp` |
| `FindResourceA`(`RT_BITMAP`) miss→fallback | Resources | Yes | Yes | IMPLEMENTED (as a permanent, correct miss — see `docs/out-of-scope.md`) | `test_file_regressions.cpp`, `test_resources.cpp` |
| `LoadStringA` | Resources | Yes (~90+ sites) | Yes (~50+ sites) | IMPLEMENTED (real per-build string table extracted from whichever game's `.rc` is driving the build) | `test_loadstring_regressions.cpp`, `test_resources.cpp` |
| `mciSendCommandA` `"sequencer"` (MIDI music) | WinMM | Yes | Yes | IMPLEMENTED | `basic_test.cpp`, `test_mci_sequences.cpp` |
| `MM_MCINOTIFY`/`MCI_NOTIFY_SUCCESSFUL` | WinMM | Yes | Yes | IMPLEMENTED | `test_mci_sequences.cpp` |
| `midiOutGetNumDevs`/`Open`/`SetVolume`/`Close` | WinMM | Yes | Yes | IMPLEMENTED | `test_mci_sequences.cpp` |
| `mciSendCommandA` `"avivideo"`/`MCI_DGV_*` (movies) | WinMM | Yes | Yes | STUB (permanently declined — see `docs/out-of-scope.md`) | `test_mci_avivideo_regressions.cpp` |
| `"cdaudio"` graceful decline | WinMM | Yes | Yes | IMPLEMENTED | `test_mci_sequences.cpp` |
| `joyGetPosEx`/`joyGetNumDevs` | WinMM/Joystick | Yes (optional) | No | IMPLEMENTED (real `SDL_Joystick`-backed: 2 axes + first 4 buttons; TASK-0103) | `test_joystick_regressions.cpp` (SDL virtual joystick) |
| `GlobalMemoryStatus` | WinBase | Yes | No | PARTIAL (plausible `dwTotalPhys` only) | none |
| `fopen`/`fread`/`fwrite`/`fclose` (via wrapper) | File | Yes | Yes | IMPLEMENTED (backslash-normalizing, case-insensitive-fallback wrapper) | `test_file_regressions.cpp`, `test_file_paths.cpp` |
