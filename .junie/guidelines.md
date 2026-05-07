# free-api guidelines

# Junie Guidelines for Free API

These guidelines are mandatory for AI-assisted work on the Free API compatibility layer.

The goal of Free API is to provide a small WinAPI / WinMM / GDI-like compatibility surface over SDL3/POSIX for old C/C++ game code and for Free Direct. It is not a general application framework and it must not contain game-specific behavior.

## 1. Project role and layer boundaries

Free API has one job:

```text
old game / Free Direct code
        ↓
WinAPI-like / WinMM-like / GDI-like functions
        ↓
Free API implementation
        ↓
SDL3 / POSIX / platform backend
```

Respect these boundaries:

```text
Game code:
- Owns game configuration.
- Reads data/config.blp or equivalent game files.
- Decides whether it wants fullscreen.
- Owns game-specific behavior.

Free Direct:
- Emulates the DirectX 3 subset needed by the game.
- May call Free API where a WinAPI-like function is needed.
- Must not become a place for unrelated Free API internals.

Free API:
- Emulates the small WinAPI / WinMM / GDI subset needed by the project.
- Translates WinAPI-like calls to SDL3/POSIX.
- Owns window/message/GDI/timer compatibility state.
- Must not know about game-specific configuration files.

SDL3/POSIX:
- Backend implementation details.
- Should remain hidden from public Free API headers whenever possible.
```

Hard rule:

```text
Free API must never read data/config.blp or any other game configuration file.
```

If fullscreen, resolution, input, or rendering behavior depends on the game configuration, the game must read that configuration and call the appropriate Free API / Free Direct functions. Free API must not inspect game files.

## 2. General behavior policy

For existing working code, prefer behavior preservation over clever rewrites.

When asked to refactor:

```text
Do:
- Preserve current behavior.
- Move code mechanically.
- Improve file/module structure.
- Fix compile/link errors caused by the refactor.
- Keep public function signatures identical.

Do not:
- Add new features unless explicitly asked.
- Change rendering behavior.
- Change fullscreen behavior.
- Change timing behavior.
- Change input behavior.
- Change game code.
- Move game-specific logic into Free API.
- Rewrite Free Direct unless explicitly asked.
```

A refactor is complete only when the project compiles and the game still behaves the same.

## 3. Public API headers

Public headers should mirror WinAPI-style grouping where practical:

```text
include/
  windows.h
  windef.h
  winbase.h
  winuser.h
  wingdi.h
  mmsystem.h
  digitalv.h
  io.h
```

Guidelines:

```text
windows.h:
- Umbrella header.
- May include windef.h, winbase.h, winuser.h, wingdi.h, etc.
- Should not become a huge implementation dump.

windef.h:
- Basic WinAPI-like types and macros:
  HWND, HDC, HBITMAP, HGDIOBJ, DWORD, UINT, BOOL, RECT, POINT, etc.

winbase.h:
- Basic kernel/base declarations:
  Sleep, GetTickCount, file helpers, resource helpers, module helpers.

winuser.h:
- Window, message, cursor, timer, and input-related declarations.

wingdi.h:
- GDI-like declarations:
  HDC, HBITMAP, StretchBlt, CreateBitmap, GetObjectA, DeleteObject, etc.

mmsystem.h:
- WinMM-like declarations:
  timeSetEvent, timeKillEvent, joy*, midi*, mci*.

digitalv.h:
- MCI/digital video related constants and declarations if needed.

io.h:
- CRT/DOS-like file enumeration:
  _findfirst, _findnext, _findclose, _finddata_t.
```

Public headers should contain declarations, constants, typedefs, structs, and macros. They should not contain implementation-heavy code.

SDL types should not be exposed in public Free API headers unless there is already an intentional public Free API extension that requires them. Prefer opaque WinAPI-like types in public headers.

## 4. Source file organization

Do not keep all implementation in one huge `winapi.cpp`.

Preferred source layout:

```text
src/
  winbase.cpp
  winbase_file.cpp
  winbase_resource.cpp

  winuser_window.cpp
  winuser_message.cpp
  winuser_cursor.cpp
  winuser_timer.cpp
  winuser_misc.cpp

  wingdi_bitmap.cpp
  wingdi_dc.cpp
  wingdi_blit.cpp
  wingdi_misc.cpp

  winmm_timer.cpp
  winmm_midi_mci.cpp
  winmm_joystick.cpp

  crt_io.cpp
  diagnostics.cpp
  winmain_bridge.cpp

src/internal/
  FreeApiDiagnostics.hpp
  FreeApiDiagnostics.cpp

  FreeApiGdi.hpp
  FreeApiGdi.cpp

  FreeApiMessageQueue.hpp
  FreeApiMessageQueue.cpp

  FreeApiWindowRegistry.hpp
  FreeApiWindowRegistry.cpp

  FreeApiTimers.hpp
  FreeApiTimers.cpp

  FreeApiPath.hpp
  FreeApiPath.cpp

  FreeApiSdlVideo.hpp
  FreeApiSdlVideo.cpp

  FreeApiSdlEvents.hpp
  FreeApiSdlEvents.cpp
```

It is acceptable to start with fewer files if some would be nearly empty, but the final result should not be a monolithic `winapi.cpp`.

Recommended minimum split:

```text
src/
  winbase.cpp
  winbase_file.cpp
  winuser_window.cpp
  winuser_message.cpp
  winuser_cursor.cpp
  winuser_timer.cpp
  wingdi_bitmap.cpp
  wingdi_dc.cpp
  wingdi_blit.cpp
  winmm.cpp
  crt_io.cpp
  diagnostics.cpp
  winmain_bridge.cpp

src/internal/
  FreeApiDiagnostics.hpp/.cpp
  FreeApiGdi.hpp/.cpp
  FreeApiMessageQueue.hpp/.cpp
  FreeApiWindowRegistry.hpp/.cpp
  FreeApiTimers.hpp/.cpp
  FreeApiPath.hpp/.cpp
  FreeApiSdlVideo.hpp/.cpp
```

## 5. Function-to-file policy

Use this mapping unless there is a very strong reason not to.

### WinBase

`src/winbase.cpp`

```text
Sleep
GetTickCount
GlobalMemoryStatus
CloseHandle
OutputDebugStringA
OutputDebugStringW
GetModuleHandleA
basic non-file WinBase helpers
```

`src/winbase_file.cpp`

```text
DeleteFileA
CreateDirectoryA
_lopen
_lread
_lclose
file/path compatibility helpers
```

`src/winbase_resource.cpp`

```text
FindResourceA
LoadResource
SizeofResource
LockResource
UnlockResource
FreeResource
resource compatibility helpers
```

If resource support is tiny, it may temporarily stay in `winbase.cpp`.

### WinUser

`src/winuser_window.cpp`

```text
RegisterClassA
CreateWindowExA
CreateWindowA
DestroyWindow
ShowWindow
MoveWindow
UpdateWindow
SetWindowTextA
GetClientRect
SetFocus
window creation/destruction helpers
```

`src/winuser_message.cpp`

```text
PostMessageA
PeekMessageA
GetMessageA
TranslateMessage
DispatchMessageA
DefWindowProcA
PostQuitMessage
WaitMessage
message dispatch behavior
```

`src/winuser_cursor.cpp`

```text
GetCursorPos
ScreenToClient
ClientToScreen
SetCursorPos
SetCursor
ShowCursor
LoadCursorA
LoadIconA
```

`src/winuser_timer.cpp`

```text
SetTimer
KillTimer
WM_TIMER helper behavior
```

`src/winuser_misc.cpp`

```text
MessageBoxA
LoadStringA
GetSystemMetrics
AdjustWindowRect
GetStockBrush
other small WinUser functions
```

### WinGDI

`src/wingdi_bitmap.cpp`

```text
CreateBitmap
LoadImageA
GetObjectA
DeleteObject
bitmap allocation/destruction
bitmap conversion helpers
```

`src/wingdi_dc.cpp`

```text
CreateCompatibleDC
DeleteDC
SelectObject
FreeApiCreateSurfaceDC
FreeApiDestroySurfaceDC
DC-related helpers
```

`src/wingdi_blit.cpp`

```text
StretchBlt
GetPixel
SetPixel
pixel copy/scale helpers
surface blit helpers
```

`src/wingdi_misc.cpp`

```text
GetDeviceCaps
GetSystemPaletteEntries
other small GDI functions
```

### WinMM

`src/winmm_timer.cpp`

```text
timeSetEvent
timeKillEvent
multimedia timer callback bridge
```

`src/winmm_midi_mci.cpp`

```text
midiOutGetNumDevs
midiOutOpen
midiOutSetVolume
midiOutClose
mciSendCommandA
mciGetDeviceIDA
mciGetErrorStringA
```

`src/winmm_joystick.cpp`

```text
joyGetPosEx
joyGetNumDevs
```

If these files would be too small, `src/winmm.cpp` is acceptable as a temporary compromise.

### CRT/io

`src/crt_io.cpp`

```text
_findfirst
_findnext
_findclose
_finddata_t helpers if implemented in .cpp
DOS/CRT-like file enumeration
```

### Startup bridge

`src/winmain_bridge.cpp`

```text
FreeApiRunWinMain
_pgmptr
BuildCommandLine
WinMain startup shim logic
```

Use `std::string::c_str()` when passing a command-line string to C-style APIs that expect null termination.

## 6. Internal namespace policy

Shared internals belong under:

```cpp
namespace FreeApi::Internal {
    ...
}
```

Use an unnamed namespace only for helpers or variables that are used by exactly one `.cpp` file.

Correct:

```cpp
// winbase.cpp
namespace {

uint64_t GetStartTick()
{
    static const auto start = std::chrono::steady_clock::now();
    ...
}

}
```

Incorrect:

```cpp
// winuser_message.cpp
namespace {
    std::deque<MSG> g_messageQueue;
}

// winmm_timer.cpp
namespace {
    std::deque<MSG> g_messageQueue;
}
```

That would create two different message queues and break behavior.

## 7. Shared global state rules

When a variable, type, or helper is needed by more than one `.cpp` file:

```text
- Put its declaration in an internal header.
- Put its definition in exactly one internal .cpp file.
- Use namespace FreeApi::Internal.
- Do not define mutable globals in headers.
- Do not use static mutable globals in headers.
- Do not duplicate state.
```

Correct pattern:

```cpp
// src/internal/FreeApiMessageQueue.hpp
#pragma once

#include "windows.h"
#include <deque>
#include <mutex>
#include <atomic>

namespace FreeApi::Internal {

extern std::deque<MSG> g_messageQueue;
extern std::mutex g_messageQueueMutex;
extern std::atomic_bool g_updateMessagePending;

void PushMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
void PumpSdlEvents();

}
```

```cpp
// src/internal/FreeApiMessageQueue.cpp
#include "internal/FreeApiMessageQueue.hpp"

namespace FreeApi::Internal {

std::deque<MSG> g_messageQueue;
std::mutex g_messageQueueMutex;
std::atomic_bool g_updateMessagePending{false};

}
```

Never do this:

```cpp
// BAD: mutable global definition in header
std::deque<MSG> g_messageQueue;
```

## 8. Internal modules and ownership

### FreeApiGdi

`src/internal/FreeApiGdi.hpp/.cpp` owns:

```text
CompatBitmap
CompatDC
CompatDcKind
bitmap/DC magic constants
AsCompatBitmap
AsCompatDC
CreateCompatBitmapFromSurface
ScaleCompatBitmap
GDI object validation helpers
shared GDI allocation/accounting helpers
```

GDI source files may include this internal header.

### FreeApiWindowRegistry

`src/internal/FreeApiWindowRegistry.hpp/.cpp` owns:

```text
registered window classes
window procedures
HWND to SDL window mappings
SDL window ID to HWND mappings
focus window
per-window state
FindWindowById
GetActiveWindow-like internal helpers
```

Window/message/cursor code may include this internal header.

### FreeApiMessageQueue

`src/internal/FreeApiMessageQueue.hpp/.cpp` owns:

```text
message queue
message queue mutex
update-message coalescing state
mouse move coalescing state if shared
PushMessage
PumpSdlEvents or shared event-pumping helpers
SDL event to WinAPI message translation if shared
keyboard scancode conversion if shared
input debug logging if shared
```

Message functions, timer functions, and SDL event bridge code must use the same queue.

### FreeApiTimers

`src/internal/FreeApiTimers.hpp/.cpp` owns:

```text
WinAPI timer map
multimedia timer map
timer mutexes
active timer IDs
next timer ID
WinTimer struct
MmTimerEntry struct
shared timer helper functions
```

`SetTimer/KillTimer` and `timeSetEvent/timeKillEvent` must not create separate timer registries.

### FreeApiDiagnostics

`src/internal/FreeApiDiagnostics.hpp/.cpp` owns:

```text
diagnostics enabled helpers
GDI debug enabled helpers
diagnostics counters
high-water counters
live byte counters
FreeApiDiagSnapshot
FreeApiDiagTick
RSS/memory helpers
AdjustDiagLiveBytes
```

Public/exported diagnostics functions may be placed in `src/diagnostics.cpp`, but shared counters and helpers belong here.

### FreeApiPath

`src/internal/FreeApiPath.hpp/.cpp` owns:

```text
NormalizePath
slash conversion
path compatibility helpers
```

### FreeApiSdlVideo

`src/internal/FreeApiSdlVideo.hpp/.cpp` owns:

```text
SDL video initialized flag
EnsureVideoSubsystem
shared SDL video setup helpers
```

### FreeApiSdlEvents

Optional. Use this module if SDL event translation grows large.

It may own:

```text
SDL event translation helpers
SDL keyboard/mouse conversion helpers
SDL window event conversion helpers
```

But do not split this if it creates unnecessary tiny files.

## 9. Build system rules

When adding or moving `.cpp` files:

```text
- Update CMakeLists.txt or the relevant build file.
- Do not compile both old and new definitions of the same exported function.
- Remove functions from winapi.cpp after moving them.
- Remove winapi.cpp from the build only when it is empty or intentionally obsolete.
- Keep the build passing after each logical phase.
```

Recommended refactor process:

```text
1. Extract internal shared state first.
2. Build.
3. Move WinBase functions.
4. Build.
5. Move WinUser functions.
6. Build.
7. Move WinGDI functions.
8. Build.
9. Move WinMM functions.
10. Build.
11. Move CRT/io and WinMain bridge.
12. Build.
13. Run the game.
```

Do not perform a giant untested rewrite in one step.

## 10. Threading and message queue rules

The message queue is shared state.

It may be accessed by:

```text
main thread
SDL event pump
SDL timer callback
WinMM timer callback
PostMessageA callers
```

Therefore:

```text
- Protect queue mutations with the existing mutex.
- Preserve existing coalescing behavior.
- Do not create multiple queues.
- Do not dispatch messages from the wrong thread unless existing behavior already does so.
- Do not change timer callback threading semantics unless explicitly asked.
```

`timeSetEvent` may fire on an SDL timer thread. If it calls `PostMessageA`, it must post into the same global queue read by `PeekMessageA` / `GetMessageA`.

## 11. GDI object rules

Current GDI-like handles are compatibility handles, often backed by internal structs.

Rules:

```text
- Preserve magic-number validation.
- Preserve handle casting behavior unless explicitly asked to redesign it.
- Keep SDL objects hidden from public headers.
- Keep bitmap/DC internals in FreeApiGdi.
- Keep allocation/deallocation accounting consistent with diagnostics.
- Do not leak selected bitmaps/DCs.
- Do not change StretchBlt pixel behavior during refactors.
```

If adding new GDI functions, implement only the subset needed by the game unless explicitly requested otherwise.

## 12. SDL usage rules

SDL is a backend dependency of Free API, not the public model.

```text
Allowed:
- Internal .cpp files may include SDL3 headers.
- Internal FreeApi modules may store SDL_Window*, SDL_Surface*, SDL_TimerID, etc.
- Public FreeApi extension functions may use SDL types only if already part of the intentional project design.

Avoid:
- SDL types in public WinAPI-like headers.
- SDL details leaking into game-facing declarations.
- Game-specific SDL behavior inside Free API.
```

## 13. Naming rules

Use WinAPI-like exported names exactly as declared:

```text
CreateWindowExA
PostMessageA
PeekMessageA
StretchBlt
timeSetEvent
mciSendCommandA
```

Do not rename public compatibility functions.

Internal helpers should use clear FreeApi names:

```cpp
FreeApi::Internal::PushMessage(...)
FreeApi::Internal::EnsureVideoSubsystem()
FreeApi::Internal::FindWindowById(...)
```

Prefer explicit names over clever abstractions.

## 14. C linkage and calling convention rules

For exported WinAPI-like C functions:

```text
- Preserve extern "C" where used.
- Preserve WINAPI.
- Preserve return types.
- Preserve parameter types.
- Preserve A/W suffixes.
```

Do not silently change ABI.

Example:

```cpp
extern "C" {

BOOL WINAPI PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    ...
}

}
```

## 15. Configuration and fullscreen rules

Free API must not read game config files.

Forbidden:

```text
Free API reading:
- data/config.blp
- game-specific INI/config files
- Speedy Blupi-specific settings
```

Allowed:

```text
Game reads config.
Game calls Free API / Free Direct.
Free API performs requested window operation.
```

Fullscreen behavior should be controlled by existing public calls or explicit parameters, not by Free API secretly reading game config.

## 16. Diagnostics rules

Diagnostics are useful, but must not affect normal behavior.

```text
- Keep diagnostics optional.
- Preserve environment-variable behavior.
- Avoid expensive diagnostics unless diagnostics are enabled.
- Keep counters thread-safe where needed.
- Keep diagnostics out of public API unless already intentionally exported.
```

Do not make diagnostics required for normal execution.

## 17. Doxygen and comments

Use Doxygen for public headers and important internal modules.

Preferred status note:

```cpp
/**
 * @brief Brief description.
 *
 * Longer explanation if needed.
 *
 * @note Status: PARTIAL
 */
```

Allowed statuses:

```text
TODO
STUB
PARTIAL
IMPLEMENTED
VERIFIED
```

Meaning:

```text
TODO:
- Not implemented or only placeholder declaration exists.

STUB:
- Function exists and returns a safe dummy value.
- Behavior is intentionally incomplete.

PARTIAL:
- Enough behavior exists for current game/project needs.
- Not a full WinAPI-compatible implementation.

IMPLEMENTED:
- Intended project-level behavior is implemented.

VERIFIED:
- Behavior has been tested against the project scenario or a reference.
```

Do not claim `VERIFIED` unless it was actually verified.

## 18. Portability rules

Free API should remain suitable for Linux, Windows, macOS, Android, and Web where practical.

Avoid unnecessary platform-specific code.

If POSIX-specific code is used:

```text
- Keep it isolated.
- Mark it with TODO or platform note if not portable.
- Prefer SDL abstractions when they are suitable.
```

Do not introduce Windows-only dependencies into Free API.

## 19. Testing rules

After a non-trivial change:

```text
- Build the project.
- Run the game if possible.
- Verify startup.
- Verify rendering did not visibly change.
- Verify input still works.
- Verify timers still work.
- Verify fullscreen behavior if touched.
- Verify no game config reading was added to Free API.
```

For refactors, behavior changes are bugs unless explicitly requested.

## 20. Diff discipline

Before making changes:

```text
- Identify the smallest safe change.
- Avoid unrelated cleanup.
- Avoid formatting huge unrelated sections.
- Avoid mixing refactor + feature + style change.
```

After making changes, summarize:

```text
Changed:
- Moved X to Y.
- Extracted shared state Z to FreeApi::Internal.
- Updated build file.
- No behavior changes intended.

Verified:
- Build passes / not run.
- Game run status.
- Known remaining issues.
```

Be honest if something was not tested.

## 21. Common anti-patterns to avoid

Do not do these:

```text
- Put everything into winapi.cpp forever.
- Split winapi.cpp while duplicating globals.
- Add mutable global definitions to headers.
- Introduce game-specific config reading into Free API.
- Change rendering while doing a refactor.
- Change fullscreen while doing a refactor.
- Expose SDL types in public WinAPI-like headers.
- Create a second message queue.
- Create a second timer registry.
- Create a second window registry.
- Rewrite Free Direct to solve a Free API refactor.
- Hide behavior changes inside “cleanup”.
- Perform huge unbuilt changes.
```

## 22. Preferred task execution style for Junie

For large tasks, use this workflow:

```text
1. Inspect current files.
2. Produce a brief implementation plan.
3. Make one logical change.
4. Build.
5. Fix only errors caused by that change.
6. Continue.
7. Provide a final summary.
```

For refactoring:

```text
- First extract shared internal state.
- Then split exported functions by subsystem.
- Then update build files.
- Then remove dead code.
```

For feature work:

```text
- Identify correct layer first.
- If it is game-specific, do not put it in Free API.
- If it is DirectX-like, it probably belongs in Free Direct.
- If it is WinAPI-like, it may belong in Free API.
- If it is XNA-like, it belongs in CNA, not Free API.
```

## 23. Layer decision table

Use this table before adding code:

```text
Question: Does this read or interpret game config?
Answer: Game layer, not Free API.

Question: Does this emulate a WinAPI/WinMM/GDI call?
Answer: Free API.

Question: Does this emulate DirectDraw/DirectX 3 behavior?
Answer: Free Direct.

Question: Does this emulate Microsoft.Xna.Framework?
Answer: CNA / XNA reimplementation layer.

Question: Does this directly call SDL?
Answer: Backend/internal implementation, not public API.

Question: Does this belong to Speedy Blupi specifically?
Answer: Game code or game adapter, not Free API.
```

## 24. Refactor checklist for winapi.cpp

When splitting a monolithic `winapi.cpp`, follow this exact checklist:

```text
[ ] List all exported functions.
[ ] List all unnamed-namespace structs.
[ ] List all unnamed-namespace global variables.
[ ] List all unnamed-namespace helper functions.
[ ] Mark each helper/global as local-only or shared.
[ ] Create FreeApi::Internal modules for shared state.
[ ] Move shared state first.
[ ] Build.
[ ] Move WinBase functions.
[ ] Build.
[ ] Move WinUser functions.
[ ] Build.
[ ] Move WinGDI functions.
[ ] Build.
[ ] Move WinMM functions.
[ ] Build.
[ ] Move CRT/io functions.
[ ] Build.
[ ] Move WinMain bridge.
[ ] Build.
[ ] Remove dead monolithic code.
[ ] Run the game.
[ ] Summarize exact file/function mapping.
```

## 25. Completion criteria

A Free API task is not complete until:

```text
- The project compiles, or the failure is clearly reported.
- Public API behavior is preserved unless intentionally changed.
- No game-specific logic was added to Free API.
- No duplicate global state was introduced.
- SDL remains hidden from public API where practical.
- Build files are updated.
- The final summary explains what changed.
```

If a task cannot be fully completed, report exactly:

```text
- What was completed.
- What remains.
- What failed.
- What was not tested.
```

Do not pretend something was verified if it was not.