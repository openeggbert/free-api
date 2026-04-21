# Free API

![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)

**Free API** is an experimental C++ project that aims to reimplement a **minimal subset of the Windows API (WinAPI)**, roughly targeting functionality available around **~1998 (Win32 era)**.

The goal is to provide a **small, self-contained compatibility layer** that allows legacy applications and games to run **without depending on Microsoft Windows**.

---

## Overview

Free API is a **platform-agnostic abstraction of WinAPI**, not tied to any specific backend:

```

WinAPI (subset ~1998)
↓
Free API
↓
(platform-specific implementations)

```

Key idea:

* **Free API defines behavior**
* **Backends implement it**

---

## Design Principles

* **No dependency on SDL, CNA, or any specific framework**
* Clean separation between:
  * API (interface)
  * Implementation (backend)
* Minimalism: only what is needed is implemented
* Focus on **real-world use cases**, not completeness

---

## Goals

* Recreate a **minimal subset of WinAPI (~1998)**
* Allow running legacy Win32 applications without Windows
* Provide a **portable runtime layer**
* Keep code **simple, readable, and hackable**
* Enable integration with different backends (SDL, native, etc.)

---

## Non-Goals

* ❌ Full WinAPI compatibility  
* ❌ Kernel-level behavior emulation  
* ❌ Binary compatibility guarantees  
* ❌ Perfect behavior matching  

---

## Scope

Free API targets only selected parts of WinAPI:

### Application Entry

* `WinMain` abstraction
* Basic application lifecycle

### Windowing (minimal)

* Window creation (CreateWindow-like behavior)
* Message loop (GetMessage / DispatchMessage style)
* Basic event handling

### System Utilities

* Timing (GetTickCount-like)
* Basic types and structures
* Minimal memory helpers (if needed)

---

## Architecture

Free API is split into two layers:

### 1. API Layer (this project)

Defines WinAPI-like interface:

```

FreeAPI::WinMain
FreeAPI::CreateWindow
FreeAPI::MessageLoop

```

### 2. Backend Layer (external / optional)

Implements behavior using a platform:

Examples:

* SDL backend (optional)
* Native OS backend
* CNA-based backend
* Testing/mock backend

---

## Example Backend Mapping

Example (not part of core Free API):

```

Free API → SDL backend → SDL 3
Free API → CNA backend → CNA → SDL 3
Free API → Native backend → OS APIs

```

Free API itself **does not depend on any of these**.

---

## Example Use Case

Free API is designed for:

* Running legacy **Win32 applications/games**
* Supporting projects like **Free Direct**
* Reverse engineering and reimplementation
* Studying WinAPI behavior in isolation

---

## Relationship to Other Projects

* **Free API** → WinAPI subset (~1998)
* **Free Direct** → DirectX 3 (2D subset)
* **CNA** → XNA-like framework (independent)

These projects are **separate but composable**.

Example stack:

```

Game
↓
Free Direct
↓
Free API
↓
Backend (SDL / CNA / native)

```

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

mkdir build
cd build
cmake ..
make
````

---

## Project Status

**Work in progress**

Current focus:

* Defining minimal WinAPI subset
* Designing clean API/backend separation
* Supporting basic application lifecycle

---

## Long-Term Vision

* Stable minimal WinAPI runtime
* Usable as a foundation for:

  * Free Direct
  * Legacy game ports
* Multiple interchangeable backends

---

## License

This project is licensed under **MIT**.

