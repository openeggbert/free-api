# Free API — Examples

Standalone demonstrations of Free API's WinAPI-compatible functionality,
independent of either target game (Free Eggbert / Planet Blupi). Each
example is a single, self-contained `.cpp` file showing one area of the
API, matching real usage patterns cited in [`plan.md`](../plan.md) and
[`docs/supported-apis.md`](../docs/supported-apis.md).

Unlike the test suite (`tests/`, which runs headless under
`SDL_VIDEODRIVER=dummy`), these examples open a real window and expect a
real display (and, for the MIDI example, audio) backend — run them
directly, not under CI.

| Example | Demonstrates |
|---|---|
| `01_window_and_message_loop.cpp` | `RegisterClassA`/`CreateWindowExA`/`AdjustWindowRect`, the `ShowWindow`/`UpdateWindow`/`SetFocus` startup sequence, and the exact `PeekMessage`/`GetMessage`/`Dispatch` loop idiom both games use. |
| `02_timers.cpp` | Both games' live frame-pump mechanisms side by side: `SetTimer`/`KillTimer`/`WM_TIMER` (Planet Blupi) and `timeSetEvent`/`timeKillEvent` (Free Eggbert), including the cross-thread `PostMessageA` safety the latter depends on. |
| `03_input_and_cursor.cpp` | Mouse/keyboard message translation (`WM_MOUSEMOVE` bit-exact `lParam` packing, `MK_*` flags, `VK_*` codes, the `WM_SYSKEYDOWN`+`VK_F10` quirk) and `ShowCursor`/`SetCursor`/`LoadCursorA`. |
| `04_gdi_minimap.cpp` | `CreateBitmap`'s 8-bit-indexed vs. 16-bit RGB565 conversion paths and `StretchBlt`, rendered side by side so you can *see* the difference `GetDeviceCaps(SIZEPALETTE)`'s value makes to Planet Blupi's real minimap rendering path (plan.md TASK-0060). |
| `05_midi_playback.cpp` | The full `MCI_OPEN`("sequencer")→`MCI_PLAY`(`MCI_NOTIFY`)→`MM_MCINOTIFY`→`MCI_CLOSE` sequence, including the notify-triggered close-then-reopen loop both games' own `WM_MM_MCINOTIFY` handler runs for looping background music. |

## Building

```bash
cmake -B build -DFREE_API_USE_SYSTEM_SDL3=ON -DFREE_API_BUILD_EXAMPLES=ON
cmake --build build
```

(Examples build alongside a standalone system-SDL3 configuration, or as a
subdirectory of either target game — see
[`docs/cmake-options.md`](../docs/cmake-options.md) for all build modes.)

## Running

```bash
./build/01_window_and_message_loop
./build/02_timers
./build/03_input_and_cursor
./build/04_gdi_minimap
./build/05_midi_playback                      # generates and plays a short built-in melody
./build/05_midi_playback path/to/your/file.mid # or play your own MIDI file
```

`05_midi_playback` needs a SoundFont (`.sf2`) to produce audible sound —
see the main [`README.md`](../README.md)'s "SoundFont requirement" section
for the lookup order (`FREE_API_SOUNDFONT` env var, or
`assets/soundfont/default.sf2` / `soundfont/default.sf2`). Without one,
playback proceeds silently — the console still prints the open/play/loop
sequence either way.

Every example closes on window-close or Escape.
