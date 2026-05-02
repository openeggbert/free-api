# Free API WinAPI Subset

This document explains the public WinAPI-like subset exposed by the current Free API headers and how it is implemented in the uploaded source tree.

## 1. Core type system

Free API starts by recreating the small set of Win32 types that old C/C++ code expects to find in Microsoft headers.

### `minwindef.h`

`minwindef.h` defines scalar types and common constants:

| Symbol | Meaning in this project |
|---|---|
| `MAX_PATH` | Set to `260`, matching the classic Windows path buffer convention. |
| `TRUE`, `FALSE` | Integer `1` and `0`. |
| `DWORD` | `uint32_t`. |
| `BOOL` | `int`. |
| `BYTE`, `WORD` | 8-bit and 16-bit unsigned integer types. |
| `FLOAT` | `float`. |
| `INT`, `UINT` | `int`, `unsigned int`. |
| Pointer aliases | `LPVOID`, `LPCVOID`, `LPDWORD`, `LPBOOL`, etc. |

Learning note: old WinAPI code often uses pointer typedefs heavily. `LPSTR` means “long pointer to string” historically; in modern flat-memory C/C++ it simply means `char*`.

### `winnt.h`

`winnt.h` defines character and string aliases, basic long integer types, `HRESULT`, `GUID`, and COM-style forward declarations.

Important symbols:

- `CHAR`, `WCHAR`, `BOOLEAN`
- `LPSTR`, `LPCSTR`, `LPWSTR`, `LPCWSTR`
- `LONG`, `HRESULT`, `ULONG`, `USHORT`, `UCHAR`
- `IUnknown` forward declaration
- `GUID`, `IID`, `CLSID` and pointer/reference aliases
- `TCHAR`, `LPTSTR`, `LPCTSTR` depending on `UNICODE`

Learning note: this project is mostly ANSI-oriented. The `UNICODE` alias layer exists, but most actual implemented functions are the `A` variants.

### `windef.h`

`windef.h` defines opaque handles and message parameter types:

| Symbol | Meaning |
|---|---|
| `HANDLE` | `void*` |
| `HWND`, `HINSTANCE`, `HMODULE`, `HDC`, `HBITMAP`, etc. | All aliases of `HANDLE`. |
| `INT_PTR`, `UINT_PTR`, `LONG_PTR`, `ULONG_PTR`, `DWORD_PTR` | Pointer-sized integer types. |
| `LRESULT` | Return type of a window procedure. |
| `WPARAM` | Unsigned pointer-sized message parameter. |
| `LPARAM` | Signed pointer-sized message parameter. |
| `CALLBACK`, `WINAPI`, `APIENTRY`, etc. | Calling-convention macros. They expand to empty definitions on this non-Windows implementation. |

Learning note: real WinAPI uses distinct handle types for readability, but they are intentionally opaque. You normally pass them back to API calls rather than inspect them. In Free API, `HWND` is currently an `SDL_Window*` cast to `void*`; `HBITMAP` and `HDC` are private C++ structs cast to handles.

## 2. `windows.h`: central public API

`windows.h` includes `minwindef.h`, `windef.h`, and `winnt.h`, then declares the primary compatibility subset.

### 2.1 WinBase-like helpers

| API | Current behavior |
|---|---|
| `Sleep(DWORD)` | Calls `std::this_thread::sleep_for`. |
| `GetTickCount()` | Returns milliseconds from `std::chrono::steady_clock`. |
| `CloseHandle(HANDLE)` | Returns `TRUE` regardless of handle; does not free generic objects. |
| `OutputDebugStringA(LPCSTR)` | Prints the string to `stdout`. |
| `OutputDebugStringW(LPCWSTR)` | Prints each wide character as a narrow `char`; ASCII-oriented. |
| `GlobalMemoryStatus(LPMEMORYSTATUS)` | Fills fixed approximate values, not real system telemetry. |

The `MEMORYSTATUS` structure is exposed because old code may query memory before deciding how much data to load. In this implementation the values are compatibility placeholders.

### 2.2 Window-related data structures

| Structure | Purpose |
|---|---|
| `RECT` | Four integers: `left`, `top`, `right`, `bottom`. Used for window/client rectangles and invalidation rectangles. |
| `POINT` | Two integers: `x`, `y`. Used for cursor coordinates. |
| `MSG` | One queued message: `hwnd`, `message`, `wParam`, `lParam`, `time`, `pt`. |
| `CREATESTRUCTA` | Data passed to a window procedure during `WM_CREATE`. |
| `WNDCLASSA` | Window class registration record containing `lpfnWndProc` and `lpszClassName`. |
| `WNDPROC` | Function pointer type: `LRESULT CALLBACK Proc(HWND, UINT, WPARAM, LPARAM)`. |

Learning note: classic WinAPI programs register a class first, then create windows from that class. The class mostly exists to attach a `WNDPROC` to windows.

### 2.3 Window class and window creation

#### `RegisterClassA`

`RegisterClassA` stores the pair:

```text
class name -> WNDPROC
```

The implementation rejects missing class names and missing window procedures. It returns `1` on success, not a real Windows atom value.

#### `CreateWindowExA` and `CreateWindowA`

`CreateWindowExA` does the real work:

1. Validates that `lpClassName` exists.
2. Looks up the previously registered `WNDPROC`.
3. Initializes SDL video lazily.
4. Creates an SDL window with a mapped subset of style flags.
5. Converts the SDL window pointer into `HWND`.
6. Stores `HWND -> WNDPROC` in an internal map.
7. Sets this window as the focus window.
8. Calls the window procedure with `WM_CREATE` and a `CREATESTRUCTA`.

`CreateWindowA` simply calls `CreateWindowExA` with `dwExStyle = 0`.

Implemented style interpretation is narrow:

- If `WS_VISIBLE` is not set, SDL window starts hidden.
- If `WS_POPUP` is set and `WS_CAPTION` is not set, SDL creates a borderless window.
- Width/height default to `640x480` when invalid.
- Negative X/Y are treated as centered window positions.

Unsupported or ignored parameters include menu handling, parent/child semantics, security, z-order details, most extended styles, and real non-client-area behavior.

### 2.4 Window lifetime and state

| API | Behavior |
|---|---|
| `DestroyWindow(HWND)` | Removes the window procedure mapping, destroys the SDL window, and shuts down SDL video when no windows remain. |
| `ShowWindow(HWND, int)` | Maps `SW_HIDE`, minimize, maximize, restore, and show-ish values to SDL calls. |
| `UpdateWindow(HWND)` | Raises the SDL window. Does not send `WM_PAINT`. |
| `MoveWindow(HWND, ...)` | Sets SDL window position and size; ignores repaint behavior. |
| `SetWindowTextA(HWND, LPCSTR)` | Sets SDL window title. |
| `GetClientRect(HWND, LPRECT)` | Returns `{0, 0, windowWidth, windowHeight}` from SDL. |
| `AdjustWindowRect(LPRECT, DWORD, BOOL)` | No-op success if `lpRect` is non-null. |
| `SetFocus(HWND)` | Stores focus window and raises SDL window. Returns previous focus handle. |

Learning note: real Windows has a difference between window rectangle and client rectangle. Free API mostly works with the SDL content size and ignores non-client borders/caption geometry.

### 2.5 Message constants

The header exposes the message constants currently needed by the game/runtime:

- Lifecycle: `WM_NULL`, `WM_CREATE`, `WM_DESTROY`, `WM_CLOSE`, `WM_QUIT`
- Activation/display/palette: `WM_SYSCOLORCHANGE`, `WM_ACTIVATEAPP`, `WM_DISPLAYCHANGE`, `WM_QUERYNEWPALETTE`, `WM_PALETTECHANGED`
- Cursor/mouse: `WM_SETCURSOR`, `WM_NCMOUSEMOVE`, `WM_MOUSEMOVE`, `WM_LBUTTONDOWN`, `WM_LBUTTONUP`, `WM_RBUTTONDOWN`, `WM_RBUTTONUP`, `WM_MBUTTONDOWN`, `WM_MBUTTONUP`, `WM_MOUSEWHEEL`
- Keyboard/text: `WM_KEYDOWN`, `WM_KEYUP`, `WM_SYSKEYDOWN`, `WM_SYSKEYUP`, `WM_CHAR`
- Timers/custom: `WM_TIMER`, `WM_USER`

The mouse-button modifier flags are also exposed:

- `MK_LBUTTON`
- `MK_RBUTTON`
- `MK_SHIFT`
- `MK_CONTROL`
- `MK_MBUTTON`

### 2.6 Message queue and dispatch

The internal message queue is a global `std::queue<MSG>` protected by a mutex. Messages can come from:

- SDL event pumping in `PeekMessageA`
- application calls to `PostMessageA`
- application calls to `PostQuitMessage`
- `SetTimer` polling inside `PeekMessageA`
- multimedia timer callbacks that may call `PostMessageA`
- MIDI end-of-song notification through `PostMessageA`

#### `PeekMessageA`

`PeekMessageA` does more than just inspect the queue:

1. Pumps SDL events every time.
2. Converts pending SDL events into WinAPI-like `MSG` records.
3. Generates elapsed `WM_TIMER` messages for timers created by `SetTimer`.
4. Returns the first queued message.
5. Removes it only if `PM_REMOVE` is set.

Current limitations:

- It ignores `hWnd`, `wMsgFilterMin`, and `wMsgFilterMax` filters.
- It sleeps briefly when the queue is empty to avoid busy-spinning.

#### `GetMessageA`

`GetMessageA` loops until `PeekMessageA(..., PM_REMOVE)` returns a message. It returns:

- `FALSE` if the message is `WM_QUIT`
- `TRUE` for normal messages

Learning note: this is why old WinAPI loops often look like:

```c
MSG msg;
while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
}
```

#### `TranslateMessage`

Real Windows may generate `WM_CHAR` from key messages here. Free API currently returns `TRUE` for non-null messages and does not synthesize text. Instead, SDL text input events are directly converted to `WM_CHAR` during event pumping.

#### `DispatchMessageA`

`DispatchMessageA` looks up the `WNDPROC` for `MSG.hwnd` and calls it. If the handle is missing but there is one registered window and the message handle is `NULL`, it falls back to that window. Otherwise it calls `DefWindowProcA`.

#### `DefWindowProcA`

The default procedure handles:

- `WM_CLOSE`: destroys the window and posts quit.
- `WM_DESTROY`: posts quit.
- all other messages: returns `0`.

### 2.7 SDL input translation

SDL events are translated into WinAPI messages inside `PumpSdlEvents`, called by `PeekMessageA` and `WaitMessage`.

| SDL event | Free API message |
|---|---|
| `SDL_EVENT_QUIT` | `WM_QUIT` |
| `SDL_EVENT_WINDOW_CLOSE_REQUESTED` | `WM_CLOSE` |
| `SDL_EVENT_WINDOW_FOCUS_GAINED` | `WM_ACTIVATEAPP` with `wParam = 1` |
| `SDL_EVENT_WINDOW_FOCUS_LOST` | Suppressed intentionally. |
| `SDL_EVENT_KEY_DOWN` | `WM_KEYDOWN` or `WM_SYSKEYDOWN` for F10. |
| `SDL_EVENT_KEY_UP` | `WM_KEYUP` or `WM_SYSKEYUP` for F10. |
| `SDL_EVENT_TEXT_INPUT` | One `WM_CHAR` per ASCII byte. |
| `SDL_EVENT_MOUSE_MOTION` | `WM_MOUSEMOVE`. |
| `SDL_EVENT_MOUSE_BUTTON_DOWN/UP` | `WM_LBUTTON*`, `WM_RBUTTON*`, or `WM_MBUTTON*`. |

Mouse coordinates are packed into `lParam` as:

```text
low word  = x
high word = y
```

That means legacy code can read them with `LOWORD(lParam)` and `HIWORD(lParam)`.

Mouse `wParam` contains current button flags and selected modifiers:

- `MK_LBUTTON`
- `MK_RBUTTON`
- `MK_MBUTTON`
- `MK_SHIFT`
- `MK_CONTROL`

Keyboard mapping supports:

- Letters `A-Z`
- Digits `0-9`
- Arrows, Home, End, Page Up, Page Down, Insert, Delete
- Return, Escape, Space, Tab, Backspace
- Shift, Control, Alt/Menu, Pause
- `F1` through `F12`

`F10` is mapped to system-key messages to better match Windows behavior.

Debug logging can be enabled with environment variables:

```bash
FREE_API_DEBUG_INPUT=1
FREE_API_DEBUG_MOUSE=1
FREE_API_DEBUG_REAL_INPUT=1
```

### 2.8 User timers: `SetTimer` and `KillTimer`

`SetTimer` stores a timer record containing `HWND`, ID, interval, and last-fire tick. It does not install a platform timer callback. Instead, `PeekMessageA` checks elapsed timers and pushes `WM_TIMER` messages.

Important details:

- If `nIDEvent == 0`, Free API generates an ID.
- `lpTimerFunc` is ignored.
- `WM_TIMER.wParam` receives the timer ID.
- `WM_TIMER.lParam` is `0`.
- Timers only fire while the message loop calls `PeekMessageA` or `GetMessageA`.

This is different from `timeSetEvent`, which uses an SDL timer callback thread. See [WinMM_MCI.md](WinMM_MCI.md).

## 3. GDI-like bitmap and device-context subset

Free API does not implement full GDI. It implements enough of a bitmap/DC/blit pipeline for legacy code paths that move raw pixels.

### 3.1 Public GDI-like types

| Type | Purpose |
|---|---|
| `BITMAP` | Metadata returned by `GetObjectA` for compatible bitmaps. |
| `RGBQUAD` | BMP/DIB color quad layout. Exposed but not deeply implemented. |
| `PALETTEENTRY` | Palette entry layout. Used by DirectDraw-like code too. |
| `BITMAPFILEHEADER`, `BITMAPINFOHEADER` | BMP metadata structures. Exposed for compatibility. |
| `HBITMAP`, `HDC`, `HGDIOBJ` | Opaque handle aliases. |
| `COLORREF` | `0x00BBGGRR` color value. |

### 3.2 Internal model

Internally, `src/winapi.cpp` has two private structures:

- `CompatBitmap`: width, height, pitch, bits-per-pixel, RGBA32 pixel vector.
- `CompatDC`: either a memory DC with a selected bitmap or a surface DC pointing at external RGBA32 pixels.

The public headers do not expose these structures.

### 3.3 Bitmap and DC functions

| API | Behavior |
|---|---|
| `LoadImageA` | Supports `IMAGE_BITMAP` with `LR_LOADFROMFILE`; loads BMP through `SDL_LoadBMP`, converts to RGBA32, optionally scales. Resource loading is not implemented. |
| `GetObjectA` | Fills a `BITMAP` structure for a Free API compatible bitmap. |
| `DeleteObject` | Deletes Free API compatible bitmap objects. |
| `CreateBitmap` | Creates an RGBA32 internal bitmap from 8-bit, 16-bit RGB565, or 32-bit raw pixels. 8-bit pixels become grayscale because palette expansion is not supported. |
| `CreateCompatibleDC` | Allocates a memory DC. |
| `SelectObject` | Selects a compatible bitmap into a memory DC. Returns previous selected bitmap. |
| `DeleteDC` | Deletes a Free API compatible DC. |
| `StretchBlt` | Supports `SRCCOPY` from a memory DC with selected bitmap to an internal surface DC. Performs nearest-neighbor scaling. |
| `GetPixel` | Reads an RGB color from a surface DC or selected bitmap. |
| `SetPixel` | Writes an RGB color into a surface DC or selected bitmap. |
| `GetDeviceCaps` | Returns `256` for `SIZEPALETTE`, otherwise `0`. |
| `GetSystemPaletteEntries` | Fills grayscale palette entries. |

Learning note: real GDI lets you select pens, brushes, fonts, bitmaps, palettes, and many other objects into a device context. Free API currently treats DCs almost exclusively as bitmap/surface pixel containers.

### 3.4 Unsupported GDI/resource pieces

These APIs are present but not meaningful yet:

- `FindResourceA`
- `LoadResource`
- `SizeofResource`
- `UnlockResource`
- `FreeResource`

`LockResource` simply returns the handle pointer it was given, which is only useful if the caller already has a meaningful pointer. Since `LoadResource` returns `NULL`, this is effectively a placeholder in normal use.

## 4. File/path helpers

### 4.1 `io.h`

| API | Behavior |
|---|---|
| `_lopen` | Calls POSIX `open` with `O_RDONLY`; ignores `iReadWrite`. |
| `_lread` | Calls POSIX `read`; returns byte count or `0`. |
| `_lclose` | Calls POSIX `close`. |
| `_findfirst` | Stub: clears output structure and returns `-1`. |
| `_findnext` | Stub: returns `-1`. |
| `_findclose` | Stub-ish: returns `0`. |

The `_finddata_t` structure is present for source compatibility.

### 4.2 `direct.h`

`direct.h` exposes inline POSIX wrappers:

| API | Behavior |
|---|---|
| `_chdir` | Calls `chdir`. |
| `_getcwd` | Calls `getcwd`. |
| `_mkdir` | Calls `mkdir(path, 0777)`. |

### 4.3 `CreateDirectoryA`

`CreateDirectoryA` normalizes Windows-style paths:

- Removes a drive prefix like `C:`.
- Converts `\` to `/`.
- Removes leading slashes so paths become relative rather than rooted at POSIX `/`.
- Calls `mkdir(path, 0755)`.
- Treats `EEXIST` as success.
- Ignores `SECURITY_ATTRIBUTES`.

It does not recursively create missing parent directories.

### 4.4 `DeleteFileA`

`DeleteFileA` calls `remove(lpFileName)` directly. It does not currently apply the same backslash/drive-letter normalization as `CreateDirectoryA` or the `fopen` wrapper.

### 4.5 `fopen` wrapper

In C++ translation units that include `windows.h`, Free API redefines `fopen` to `free_api_fopen`.

The wrapper:

1. Removes a Windows drive letter.
2. Converts backslashes to slashes.
3. Removes leading slashes.
4. Tries `::fopen`.
5. If that fails, uppercases only the basename and retries.

This is specifically useful for old game paths such as `data\config.def` or uppercase asset files on case-sensitive Linux file systems.

## 5. Utility macros and constants

### Memory macros

- `ZeroMemory(Destination, Length)` -> `memset(..., 0, ...)`
- `FillMemory(Destination, Length, Fill)` -> `memset`
- `CopyMemory(Destination, Source, Length)` -> `memmove`

### Word packing macros

- `MAKELONG(a, b)`
- `LOWORD(l)`
- `HIWORD(l)`

These are important for decoding `LPARAM` values, especially mouse coordinates.

### Resources and integer-resource macro

- `RT_BITMAP`
- `MAKEINTRESOURCEA`
- `MAKEINTRESOURCE`

The macros exist, but actual resource loading is not implemented.

### Colors

- `RGB(r, g, b)` packs a Windows `COLORREF` as `0x00BBGGRR`.
- `CLR_INVALID` is `0xFFFFFFFF`.

### Window/style constants

The header exposes a small set of classic constants: `CS_HREDRAW`, `CS_VREDRAW`, `WS_VISIBLE`, `WS_POPUP`, `WS_CHILD`, `WS_CAPTION`, `WS_POPUPWINDOW`, `WS_OVERLAPPEDWINDOW`, `WS_EX_TOPMOST`, `SW_HIDE`, `SW_SHOW`, `MB_OK`, screen metric constants, bitmap loading constants, and `SRCCOPY`.

Most style constants are defined so legacy code compiles and can make basic decisions. Only a few are interpreted by `CreateWindowExA` and `ShowWindow`.

## 6. Other public headers

### `wtypes.h`

Defines minimal OLE/WTypes aliases:

- `VARTYPE`
- `SCODE`
- `DATE`
- `CLIPFORMAT`
- `byte`

No OLE/COM Automation behavior is implemented.

### `commdlg.h`

Includes `windows.h` and marks common dialogs as a stub area. No common dialog functions are declared or implemented.

### `windowsx.h`

Includes `windows.h` and marks Windowsx helper macros as a stub area. No helper macros are currently defined there.

## 7. WinMain bridge

### Public type

```c
typedef int (WINAPI* FREE_API_WINMAIN_PROC)(HINSTANCE, HINSTANCE, LPSTR, int);
```

### `FreeApiRunWinMain`

`FreeApiRunWinMain` adapts a standard `main(int argc, char** argv)` environment to a WinMain-style function.

Behavior:

1. Returns `-1` if the entry point pointer is null.
2. Stores `argv[0]` in `_pgmptr` when available.
3. Builds `lpCmdLine` from `argv[1]` onward, separated by spaces.
4. Calls the entry point with `hInstance = NULL`, `hPrevInstance = NULL`, `nCmdShow = SW_SHOW`.

### `FREE_API_IMPLEMENT_WINMAIN()`

On non-Windows C++ builds, this macro declares `WinMain`, creates a `main`, calls `FreeApiRunWinMain`, and then begins the `WinMain` body.

Example:

```cpp
FREE_API_IMPLEMENT_WINMAIN()
{
    // WinMain body here
    return 0;
}
```

Learning note: classic Windows applications start at `WinMain` rather than normal C `main`. This bridge lets old source keep a Windows-style entry point while still compiling on Linux/macOS/etc.
