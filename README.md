# Free API

![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)

**Free API** is an experimental C++ project that aims to reimplement a **minimal subset of the Windows API (WinAPI)**, roughly targeting functionality available around **~1998 (Win32 era)**.

The goal is to provide a **small, self-contained compatibility layer** that allows legacy applications and games to run **without depending on Microsoft Windows**.

---

## Overview

Free API provides a **WinAPI-compatible subset** implemented using **SDL3**:

```
WinAPI (subset ~1998)
        ↓
    Free API
        ↓
      SDL 3
```

* **Free API** → defines WinAPI-like headers and behavior
* **SDL 3** → internal implementation detail for windowing, events, and timing

---

## Design Principles

* **WinAPI-like public headers** (windows.h, windef.h, etc.)
* **SDL3 used internally** as a portable backend
* Minimalism: only what is needed is implemented
* Focus on **real-world use cases** (legacy games support)

---

## Goals

* Recreate a **minimal subset of WinAPI (~1998)**
* Allow running legacy Win32 applications without Windows
* Provide a **portable runtime layer**
* Keep code **simple, readable, and hackable**
* Maintain **cross-platform support** via SDL3

---

## Features

### Application Entry
* `WinMain` abstraction (mapping to `main`)

### Windowing
* Window creation (`CreateWindowEx`)
* Message loop (`PeekMessage`, `DispatchMessage`)
* Window procedures (`WNDPROC`)

### Input (SDL3 → WinAPI message translation)
* **Mouse motion** — `SDL_EVENT_MOUSE_MOTION` → `WM_MOUSEMOVE` with `lParam` = packed `(y<<16)|x`
* **Mouse buttons** — `SDL_EVENT_MOUSE_BUTTON_DOWN/UP` → `WM_LBUTTONDOWN/UP`, `WM_RBUTTONDOWN/UP`, `WM_MBUTTONDOWN/UP`
* **Mouse button state** — `wParam` contains live `MK_LBUTTON`/`MK_RBUTTON`/`MK_MBUTTON`/`MK_SHIFT`/`MK_CONTROL` flags
* **Keyboard** — `SDL_EVENT_KEY_DOWN/UP` → `WM_KEYDOWN`/`WM_KEYUP` with `wParam` = `VK_*` code
  * Letters A–Z (uppercase VK codes), digits 0–9
  * Navigation: `VK_LEFT/RIGHT/UP/DOWN`, `VK_HOME`, `VK_END`, `VK_PRIOR`, `VK_NEXT`, `VK_INSERT`, `VK_DELETE`
  * Control: `VK_RETURN`, `VK_ESCAPE`, `VK_SPACE`, `VK_TAB`, `VK_BACK`, `VK_SHIFT`, `VK_CONTROL`, `VK_MENU`, `VK_PAUSE`
  * Function keys: `VK_F1`–`VK_F12` (F10 uses `WM_SYSKEYDOWN`/`WM_SYSKEYUP` matching Windows behavior)
* **Text input** — `SDL_EVENT_TEXT_INPUT` → `WM_CHAR` (for name entry screens)
* **Window focus** — `SDL_EVENT_WINDOW_FOCUS_GAINED` → `WM_ACTIVATEAPP(1)`; `SDL_EVENT_WINDOW_FOCUS_LOST` is suppressed (not translated to `WM_ACTIVATEAPP(0)`) to prevent games from entering inactive/suspended state due to spurious desktop focus changes
* **Debug logging** — set env `FREE_API_DEBUG_INPUT=1` (or alias `FREE_API_DEBUG_MOUSE=1`) at runtime to log all translated messages, ENQUEUE/DISPATCH events, GetCursorPos and ScreenToClient calls

#### Input pipeline notes
- Mouse and keyboard events arriving before the first `FOCUS_GAINED` are routed to the first registered window (fallback), so startup events are not silently dropped.
- The `WM_ACTIVATEAPP(0)` suppression is intentional: many SDL environments deliver spurious `FOCUS_LOST` at startup or in headless/virtual environments; sending deactivation would freeze games that guard rendering/input behind `g_bActive`.
- All input events flow through `PeekMessage` → `DispatchMessage` → `WndProc`; no direct callbacks bypass the WinAPI message queue.

#### Input pipeline test
An automated end-to-end test (`tests/test_input_pipeline.cpp`) injects SDL events programmatically and verifies the full pipeline:

```bash
cmake --build build --target test_input_pipeline
./build/bin/test_input_pipeline
# Expected output: [input-pipeline-test] ALL TESTS PASSED
```

Verified: `WM_MOUSEMOVE`, `WM_LBUTTONDOWN/UP`, `WM_RBUTTONDOWN`, `WM_KEYDOWN` (VK_SPACE), `WM_KEYUP` (VK_ESCAPE).

### System Utilities
* Timing (`GetTickCount`, `Sleep`)
* Debugging (`OutputDebugString`)

---

## Project Structure

```
free-api/
├── include/
│    ├── windows.h
│    ├── windef.h
│    ├── winnt.h
│    └── ...
├── src/
│    └── winapi.cpp
└── CMakeLists.txt
```

---

## Build Instructions

```bash
git clone https://github.com/openeggbert/free-api.git
cd free-api

cmake -B build
cmake --build build
```

---

## Project Status

**Work in progress**

Current focus:
* Supporting `free-direct` requirements
* Basic windowing and message pump
* Cross-platform compatibility

---

## License

This project is licensed under **MIT**.

