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

