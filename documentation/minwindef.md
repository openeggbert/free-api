# minwindef.h

`minwindef.h` provides the minimal Win32-compatible basic definitions currently
used by Free API.

This file is intentionally a small compatibility subset. It should not grow
just because the real Windows SDK `minwindef.h` contains more symbols. New
symbols should be added only when Free API, Free Direct, or a supported target
application actually needs them.

Status: `HEADER_ONLY`

## MAX_PATH

`MAX_PATH` is the traditional Win32 path buffer size.

```cpp
#define MAX_PATH 260
```

The value includes space for the terminating null character. Older Win32 code
often allocates path buffers like this:

```cpp
char path[MAX_PATH];
```

Free API provides this constant for source compatibility with old Win32-style
code. It does not mean that all supported platforms or filesystems are limited
to 260 characters.

Status: `REVIEWED`

## FALSE

`FALSE` is the conventional Win32 false value.

```cpp
#define FALSE 0
```

It is normally used with `BOOL`.

```cpp
BOOL result = SomeFunction();

if (result == FALSE) {
    // false or failure
}
```

A zero `BOOL` value normally means false. Depending on the API, it may also
mean failure.

Status: `REVIEWED`

## TRUE

`TRUE` is the conventional Win32 true value.

```cpp
#define TRUE 1
```

It is normally used with `BOOL`.

```cpp
BOOL result = SomeFunction();

if (result != FALSE) {
    // true-like result
}
```

In C-style APIs, any non-zero `BOOL` value may be treated as true. Code should
therefore often check against `FALSE` instead of assuming every successful
result is exactly equal to `TRUE`.

Status: `REVIEWED`

## DWORD

`DWORD` is an unsigned 32-bit integer type.

```cpp
typedef uint32_t DWORD;
```

It is commonly used for flags, sizes, timestamps, style values, error codes,
and other 32-bit Win32-compatible values.

Status: `REVIEWED`

## BOOL

`BOOL` is the classic Win32 integer boolean type.

```cpp
typedef int BOOL;
```

Unlike C++ `bool`, `BOOL` is an integer type. Win32-style APIs often return
`BOOL` to report success or failure.

Use `BOOL`, `TRUE`, and `FALSE` when mirroring Win32-compatible APIs. For new
internal C++ code that does not need Win32 source compatibility, prefer
`bool`, `true`, and `false`.

Status: `REVIEWED`

## BYTE

`BYTE` is an unsigned 8-bit byte type.

```cpp
typedef unsigned char BYTE;
```

It is commonly used for raw binary data, byte buffers, color components, and
packed structures.

Status: `REVIEWED`

## WORD

`WORD` is an unsigned 16-bit integer type.

```cpp
typedef unsigned short WORD;
```

It is commonly used in old Win32 structures, resource formats, packed values,
and helper macros such as `LOWORD` and `HIWORD` when those macros are present
in the API subset.

Status: `REVIEWED`

## FLOAT

`FLOAT` is a Win32-style single-precision floating-point type.

```cpp
typedef float FLOAT;
```

It exists mainly for compatibility with Windows-style type names.

Status: `REVIEWED`

## PFLOAT

`PFLOAT` is a pointer to `FLOAT`.

```cpp
typedef FLOAT *PFLOAT;
```

Status: `REVIEWED`

## PBOOL

`PBOOL` is a pointer to `BOOL`.

```cpp
typedef BOOL *PBOOL;
```

Status: `REVIEWED`

## LPBOOL

`LPBOOL` is a pointer to `BOOL`.

```cpp
typedef BOOL *LPBOOL;
```

Historically, `LP` meant "long pointer". In modern flat-memory systems it
simply means a pointer.

Status: `REVIEWED`

## PBYTE

`PBYTE` is a pointer to `BYTE`.

```cpp
typedef BYTE *PBYTE;
```

Status: `REVIEWED`

## LPBYTE

`LPBYTE` is a pointer to `BYTE`.

```cpp
typedef BYTE *LPBYTE;
```

Status: `REVIEWED`

## PINT

`PINT` is a pointer to `int`.

```cpp
typedef int *PINT;
```

Status: `REVIEWED`

## LPINT

`LPINT` is a pointer to `int`.

```cpp
typedef int *LPINT;
```

Status: `REVIEWED`

## PWORD

`PWORD` is a pointer to `WORD`.

```cpp
typedef WORD *PWORD;
```

Status: `REVIEWED`

## LPWORD

`LPWORD` is a pointer to `WORD`.

```cpp
typedef WORD *LPWORD;
```

Status: `REVIEWED`

## LPDWORD

`LPDWORD` is a pointer to `DWORD`.

```cpp
typedef DWORD *LPDWORD;
```

It is commonly used by APIs that return a 32-bit value through an output
parameter.

Status: `REVIEWED`

## LPVOID

`LPVOID` is a pointer to mutable untyped data.

```cpp
typedef void *LPVOID;
```

It is used when an API accepts or returns a generic mutable memory pointer.

Status: `REVIEWED`

## LPCVOID

`LPCVOID` is a pointer to constant untyped data.

```cpp
typedef const void *LPCVOID;
```

It is used when an API accepts a generic input buffer that should not be
modified by the function.

Status: `REVIEWED`

## INT

`INT` is a Win32-style signed integer type.

```cpp
typedef int INT;
```

It is usually equivalent to C/C++ `int`, but the Win32-style name is kept for
source compatibility.

Status: `REVIEWED`

## UINT

`UINT` is a Win32-style unsigned integer type.

```cpp
typedef unsigned int UINT;
```

It is often used for message identifiers, flags, counts, and other non-negative
integer values.

Status: `REVIEWED`

## PUINT

`PUINT` is a pointer to `UINT`.

```cpp
typedef unsigned int *PUINT;
```

Status: `REVIEWED`

## HANDLE

`HANDLE` is the generic opaque Win32-compatible handle type.

```cpp
typedef void* HANDLE;
```

Native Win32 uses handles to refer to objects managed by the operating system
or by Win32 subsystems. In this Free API subset, `HANDLE` is represented as a
pointer-sized opaque value.

Specific handle types are aliases built on top of `HANDLE`.

This keeps old Win32-style code easy to compile, but it also means the C++ type
system does not strongly distinguish between different handle categories.

Status: `PARTIAL`

## HGLOBAL

`HGLOBAL` is a handle to a global memory block or loaded resource data block.

```cpp
typedef HANDLE HGLOBAL;
```

Older Win32 code often encounters this type when working with resource APIs or
classic global memory APIs.

Status: `PARTIAL`

## HINSTANCE

`HINSTANCE` is a handle to an application instance.

```cpp
typedef HANDLE HINSTANCE;
```

Classic Win32 programs receive this value in `WinMain`.

```cpp
int WinMain(HINSTANCE hInstance,
            HINSTANCE hPrevInstance,
            LPSTR lpCmdLine,
            int nCmdShow);
```

It is commonly passed to APIs that register window classes, create windows, or
load resources.

Status: `PARTIAL`

## HMODULE

`HMODULE` is a handle to a loaded module, such as an executable or dynamic
library.

```cpp
typedef HANDLE HMODULE;
```

In native Win32, `HMODULE` and `HINSTANCE` are closely related. Free API keeps
this alias for source compatibility.

Status: `PARTIAL`

## HRSRC

`HRSRC` is a handle to a resource descriptor.

```cpp
typedef HANDLE HRSRC;
```

It is normally obtained from resource lookup APIs such as `FindResource` and
then used to load the actual resource data.

Status: `PARTIAL`

## HWND

`HWND` is a handle to a window.

```cpp
typedef HANDLE HWND;
```

It is used by windowing and message APIs such as `CreateWindowEx`,
`ShowWindow`, and `PostMessage`.

Free API currently keeps this alias in `minwindef.h` for existing code, even
though a stricter header layout could place it in a User/windowing header.

Status: `PARTIAL`

## HDC

`HDC` is a handle to a device context.

```cpp
typedef HANDLE HDC;
```

A device context is a GDI-style drawing target.

Free API currently keeps this alias in `minwindef.h` for existing code, even
though a stricter header layout could place it in a GDI-related header.

Status: `PARTIAL`

## HGDIOBJ

`HGDIOBJ` is a generic handle to a GDI object.

```cpp
typedef HANDLE HGDIOBJ;
```

Bitmaps, brushes, fonts, and palettes may be passed through APIs that accept a
generic GDI object handle.

Status: `PARTIAL`

## HBRUSH

`HBRUSH` is a handle to a GDI brush object.

```cpp
typedef HANDLE HBRUSH;
```

Brushes are used by GDI-style drawing code and by window class background
settings.

Status: `PARTIAL`

## HBITMAP

`HBITMAP` is a handle to a GDI bitmap object.

```cpp
typedef HANDLE HBITMAP;
```

A bitmap can be selected into a memory device context or used as a source for
blitting operations.

Status: `PARTIAL`

## HPALETTE

`HPALETTE` is a handle to a logical color palette.

```cpp
typedef HANDLE HPALETTE;
```

This type mostly matters for older palette-based graphics code, especially
8-bit or indexed-color applications.

Status: `STUB`

## HICON

`HICON` is a handle to an icon object or icon resource.

```cpp
typedef HANDLE HICON;
```

Icons may be associated with windows, window classes, or loaded from resources.

Status: `PARTIAL`

## HCURSOR

`HCURSOR` is a handle to a cursor object or cursor resource.

```cpp
typedef HANDLE HCURSOR;
```

Cursors may be associated with window classes or set dynamically by windowing
code.

Status: `PARTIAL`

## HMENU

`HMENU` is a handle to a menu or submenu.

```cpp
typedef HANDLE HMENU;
```

Classic Win32 GUI programs use this type for menu bars, popup menus, and
command dispatch.

Status: `STUB`

## HFONT

`HFONT` is a handle to a GDI font object.

```cpp
typedef HANDLE HFONT;
```

A font can be selected into a device context before drawing text.

Status: `STUB`