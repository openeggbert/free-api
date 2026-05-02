# TODO: Embedded WinAPI-like Resources in FreeAPI

## Goal

Implement a small WinAPI-like embedded resource system in FreeAPI.

The purpose is to emulate the classic Windows resource API enough for legacy C/C++ games that expect functions such as:

- `FindResourceA`
- `LoadResource`
- `SizeofResource`
- `LockResource`
- `FreeResource`
- optionally `LoadImageA` without `LR_LOADFROMFILE`

This should not become a full Windows `.rc` / `.res` parser. The goal is a portable, CMake-generated embedded resource registry backed by static binary data compiled directly into the executable or library.

## Non-goals

This task is not intended to implement:

- full Windows resource compiler compatibility,
- full `.rc` parsing,
- full `.res` file loading,
- real Windows module resource sections,
- localized resources,
- resource language selection,
- complete `HMODULE` semantics,
- Wine-like resource emulation.

The first version should be a narrow compatibility layer for the needs of the supported legacy games.

## Design Overview

CMake should generate a C++ source file containing binary resource data.

Example generated structure:

```cpp
static const unsigned char RESOURCE_IMAGE_INIT_BLP[] = {
    0x42, 0x4D, 0x00, ...
};

static const unsigned char RESOURCE_SOUND_CLICK_WAV[] = {
    0x52, 0x49, 0x46, 0x46, ...
};
````

A generated registry table should describe the resources:

```cpp
struct FreeApiEmbeddedResource
{
    const char* typeName;
    const char* name;
    unsigned int typeId;
    unsigned int nameId;
    bool typeIsInteger;
    bool nameIsInteger;
    const unsigned char* data;
    std::size_t size;
};
```

The public WinAPI-like functions should then look up resources in this internal registry.

## Public API Behavior

The public declarations should stay in the existing WinAPI-compatible public headers, most likely `include/windows.h`.

The implementation should remain internal to FreeAPI.

### `FindResourceA`

Required behavior:

* Accept both string resource names and integer resources created with `MAKEINTRESOURCEA`.
* Accept both string resource types and integer resource types.
* Return an opaque `HRSRC` handle if found.
* Return `NULL` if not found.

Example supported calls:

```cpp
FindResourceA(hInstance, MAKEINTRESOURCEA(101), RT_BITMAP);
FindResourceA(hInstance, "INIT", "BITMAP");
```

### `LoadResource`

Required behavior:

* Accept the `HRSRC` returned by `FindResourceA`.
* Return an opaque `HGLOBAL` handle.
* In the embedded-resource implementation, the returned handle may internally be the same pointer as the resource handle.

### `SizeofResource`

Required behavior:

* Return the size in bytes of the embedded resource.
* Return `0` for invalid or missing resource handles.

### `LockResource`

Required behavior:

* Return a pointer to the embedded static binary data.
* The returned pointer must remain valid for the lifetime of the program.
* The returned pointer must not be freed by the caller.

### `FreeResource`

Required behavior:

* Behave as a compatibility no-op.
* Embedded resources are static memory and should not be freed.
* Returning `FALSE` is acceptable and close to modern WinAPI behavior, where `FreeResource` is obsolete.

## `MAKEINTRESOURCEA` Support

FreeAPI must distinguish between string pointers and integer resource identifiers.

Classic WinAPI encodes integer resources as pointer values with the high word set to zero.

Suggested helper:

```cpp
static bool FreeApiIsIntResource(const char* value)
{
    return ((reinterpret_cast<std::uintptr_t>(value) >> 16u) == 0u);
}

static unsigned int FreeApiResourceId(const char* value)
{
    return static_cast<unsigned int>(
        reinterpret_cast<std::uintptr_t>(value) & 0xFFFFu
    );
}
```

This allows support for:

```cpp
MAKEINTRESOURCEA(101)
```

without treating it as a normal C string.

## CMake Generation

Add a CMake step that converts selected files into a generated `.cpp` file.

Example output:

```text
build/generated/FreeApiEmbeddedResources.cpp
build/generated/FreeApiEmbeddedResources.hpp
```

The generated `.cpp` should contain:

* one `static const unsigned char[]` per embedded resource,
* a resource registry table,
* resource count metadata.

The generated `.hpp` should expose only internal declarations needed by the FreeAPI implementation.

This generated header must not become a public API header.

## Suggested Internal Files

Possible structure:

```text
free-api/
  include/
    windows.h

  src/
    ResourceApi.cpp
    winapi.cpp

  src/internal/
    EmbeddedResources.hpp
    ResourceRegistry.hpp

  generated/
    FreeApiEmbeddedResources.cpp
    FreeApiEmbeddedResources.hpp
```

Public users should still include only:

```cpp
#include <windows.h>
```

They should not include any FreeAPI internal resource headers.

## Optional: `LoadImageA` Resource Support

The current disk-based `LoadImageA` behavior must not be broken.

Suggested behavior:

```text
If LR_LOADFROMFILE is set:
    keep existing disk loading behavior.

If LR_LOADFROMFILE is not set:
    try to load the image from embedded resources.
```

For example:

```cpp
LoadImageA(hInstance, MAKEINTRESOURCEA(101), IMAGE_BITMAP, 0, 0, 0);
```

could internally do:

```text
FindResourceA(hInstance, MAKEINTRESOURCEA(101), RT_BITMAP)
LoadResource(...)
LockResource(...)
decode bitmap from memory
```

This step should be optional and can be implemented after the basic resource registry works.

## Important Compatibility Notes

### Public headers

Public headers must remain WinAPI-like.

Do not expose:

* SDL types,
* C++ implementation classes,
* internal resource registry structs,
* generated resource arrays.

### Internal implementation

Internal files may use:

* `std::vector`
* `std::string`
* `std::unordered_map`
* generated C++ arrays
* SDL helpers if needed for image decoding

but these details must remain private.

## Doxygen Documentation

Add concise Doxygen comments for the public resource functions.

Each function should document:

* what subset is implemented,
* whether string and integer resource IDs are supported,
* ownership rules,
* unsupported Windows behavior.

Suggested status notes:

```cpp
/**
 * @brief Finds an embedded WinAPI-like resource.
 *
 * Supports string names and MAKEINTRESOURCEA integer identifiers.
 * This implementation searches FreeAPI's generated embedded resource registry.
 *
 * @note Status: PARTIAL
 * @note This is not a full Windows module resource implementation.
 */
HRSRC WINAPI FindResourceA(HMODULE hModule, LPCSTR lpName, LPCSTR lpType);
```

## Testing Plan

Add tests for:

1. string resource lookup,
2. integer resource lookup via `MAKEINTRESOURCEA`,
3. missing resource returns `NULL`,
4. `SizeofResource` returns correct byte size,
5. `LockResource` returns stable non-null pointer,
6. `FreeResource` does not invalidate embedded data,
7. optional `LoadImageA` resource loading if implemented.

Example test cases:

```text
FindResourceA(NULL, "TEST_TEXT", "TEXT") succeeds.
FindResourceA(NULL, MAKEINTRESOURCEA(101), RT_BITMAP) succeeds.
FindResourceA(NULL, "MISSING", "TEXT") returns NULL.
SizeofResource(NULL, hRes) returns expected file size.
LockResource(hGlobal) returns the expected first bytes.
```

## Implementation Phases

### Phase 1: Internal resource registry

* Add internal resource representation.
* Add lookup helpers.
* Support string and integer resource identifiers.
* No CMake generation yet; use one manually defined test resource.

### Phase 2: Implement WinAPI resource functions

Implement:

* `FindResourceA`
* `LoadResource`
* `SizeofResource`
* `LockResource`
* `FreeResource`

Keep behavior narrow and documented.

### Phase 3: CMake binary embedding

* Add generator script or CMake command.
* Convert selected resource files into generated C++ arrays.
* Compile generated resource source into FreeAPI.

### Phase 4: Optional `LoadImageA` integration

* If `LR_LOADFROMFILE` is not used, attempt embedded bitmap resource loading.
* Keep existing file-based loading unchanged.

### Phase 5: Documentation and tests

* Add Doxygen comments.
* Add unit tests.
* Document limitations in `Documentation.md`.

## Risks

* Incorrect handling of `MAKEINTRESOURCEA` may crash if integer resources are treated as C strings.
* Returning pointers to temporary buffers would be wrong; embedded resource data must be static.
* Public headers must not expose implementation details.
* Full Windows resource semantics are much larger than needed; avoid scope creep.

## Success Criteria

The task is complete when:

* embedded resources can be registered/generated at build time,
* `FindResourceA` can find them by string or integer ID,
* `LoadResource`, `SizeofResource`, and `LockResource` work correctly,
* `FreeResource` is a safe compatibility no-op,
* existing disk-based `LoadImageA` behavior still works,
* no SDL/internal types are exposed in public headers,
* behavior is documented with Doxygen comments,
* tests cover the basic resource workflow.


------------







Tady je anglická verze, kterou můžeš přidat do toho TODO Markdown souboru:

````md
## Important Clarification: `resource/` Is Not the WinAPI Standard

A directory named `resource/`, `res/`, `resources/`, or similar is not itself a WinAPI standard.

The classic Windows resource model is based on:

- `resource.h` — symbolic numeric resource IDs
- `.rc` files — Windows resource scripts
- referenced resource files such as `.ico`, `.cur`, `.bmp`, `.wav`, etc.
- the Windows resource compiler (`rc.exe`)
- the linker embedding compiled resources into the PE executable or DLL
- WinAPI functions querying the embedded resource section at runtime

Typical Windows flow:

```text
resource.h + *.rc + referenced files
        |
        v
Windows resource compiler
        |
        v
*.res
        |
        v
linked into EXE/DLL
        |
        v
FindResource / LoadResource / SizeofResource / LockResource / LoadString / LoadIcon / LoadCursor
````

Therefore, FreeAPI should not treat a directory named `resource/` as special by itself.

Instead, FreeAPI should emulate the WinAPI resource model:

```text
.rc + resource.h + referenced files
        |
        v
CMake resource generator
        |
        v
generated C++ binary arrays + resource registry
        |
        v
FreeAPI resource functions
```

The actual directory name should be configurable. A project may keep resource files in:

```text
resource/
res/
resources/
assets/resource/
game/resources/
```

or any other location.

The important inputs are the `.rc` resource script, the resource ID header, and the files referenced by the `.rc` script.

## Source Files vs Generated Resources

For Visual Studio projects, a directory may contain files such as:

```text
resource.h
*.rc
*.ico
*.cur
*.bmp
*.aps
```

The important source-of-truth files are:

* `.rc`
* `resource.h`
* referenced resource files such as `.ico`, `.cur`, `.bmp`, `.wav`, etc.

The following should normally be ignored:

* `.aps`

`.aps` files are Visual Studio resource editor cache files. They are not the canonical resource source and should not be used as input for FreeAPI resource generation.

## FreeAPI Resource Emulation Goal

FreeAPI should emulate the WinAPI resource API, not the physical folder layout.

The goal is that legacy game code can call APIs such as:

```cpp
FindResourceA(hInstance, MAKEINTRESOURCEA(IDR_MAINFRAME), RT_ICON);
LoadResource(hInstance, hRes);
SizeofResource(hInstance, hRes);
LockResource(hGlobal);

LoadStringA(hInstance, IDS_TITLE, buffer, bufferSize);
LoadCursorA(hInstance, MAKEINTRESOURCEA(IDC_POINTER));
LoadIconA(hInstance, MAKEINTRESOURCEA(IDI_MAINICON));
```

and receive behavior close enough to classic WinAPI for the supported games.

## Recommended Architecture

FreeAPI should provide an internal embedded resource registry.

The registry should support:

* resource type as string or integer ID,
* resource name as string or integer ID,
* raw byte pointer,
* byte size,
* optional language field for future extension.

Example internal concept:

```cpp
struct FreeApiResourceEntry
{
    FreeApiResourceName type;
    FreeApiResourceName name;
    const unsigned char* data;
    std::size_t size;
    unsigned short language;
};
```

The public WinAPI-like functions should query this registry.

## Required WinAPI Functions

The first implementation should support:

* `FindResourceA`
* `LoadResource`
* `SizeofResource`
* `LockResource`
* `FreeResource`

Expected behavior:

```text
FindResourceA   -> searches the embedded resource registry
LoadResource    -> returns an opaque handle for the found resource
SizeofResource  -> returns the resource size in bytes
LockResource    -> returns a stable pointer to embedded static data
FreeResource    -> compatibility no-op
```

`FreeResource` should not actually free embedded resources because the data is static and lives for the lifetime of the program.

Returning `FALSE` is acceptable because modern WinAPI treats `FreeResource` as obsolete.

## `MAKEINTRESOURCEA` Support

Classic WinAPI supports both string resource names and integer resource IDs.

Integer resources are encoded as pointer-like values using `MAKEINTRESOURCEA`.

FreeAPI must not treat every `LPCSTR` resource argument as a normal C string.

It must detect integer resources safely.

Suggested helper logic:

```cpp
static bool FreeApiIsIntResource(const char* value)
{
    return ((reinterpret_cast<std::uintptr_t>(value) >> 16u) == 0u);
}

static unsigned int FreeApiResourceId(const char* value)
{
    return static_cast<unsigned int>(
        reinterpret_cast<std::uintptr_t>(value) & 0xFFFFu
    );
}
```

This is necessary for calls such as:

```cpp
FindResourceA(hInstance, MAKEINTRESOURCEA(101), RT_BITMAP);
LoadIconA(hInstance, MAKEINTRESOURCEA(IDI_MAINICON));
LoadCursorA(hInstance, MAKEINTRESOURCEA(IDC_POINTER));
```

## CMake Generator

A CMake-based generator should read the resource inputs and produce generated C++ files.

The generator should accept configurable input paths, not hardcoded `resource/`.

Suggested generated files:

```text
build/generated/FreeApiEmbeddedResources.cpp
build/generated/FreeApiEmbeddedResources.hpp
```

The generator should:

1. parse `resource.h` for numeric resource IDs,
2. parse a useful subset of `.rc` files,
3. resolve referenced files relative to the `.rc` file,
4. embed referenced files as `static const unsigned char[]`,
5. generate a resource registry table,
6. compile the generated `.cpp` into the target.

The initial `.rc` parser should support only the subset needed by the target games.

Recommended first supported resource types:

* `ICON`
* `CURSOR`
* `STRINGTABLE`
* optionally `BITMAP`
* optionally `RCDATA`

A full Windows resource compiler is not required.

## `LoadStringA`

`LoadStringA` should eventually use generated string-table data instead of placeholder values such as `RES_<id>`.

The first implementation does not need to exactly reproduce the internal Windows `RT_STRING` block format.

A simple generated mapping is enough:

```text
numeric string ID -> string text
```

Example:

```cpp
LoadStringA(hInstance, IDS_PLAY, buffer, bufferSize);
```

should copy the generated string into `buffer` if the string ID exists.

If the string is missing, FreeAPI may keep a fallback behavior for compatibility/debugging.

## `LoadIconA` and `LoadCursorA`

`LoadIconA` and `LoadCursorA` should eventually query the embedded resource registry.

Minimal first behavior:

* find the registered icon/cursor resource by name or integer ID,
* return a non-null opaque handle representing that resource,
* keep the old dummy fallback when the resource is missing.

Later improvements may convert embedded icon/cursor data into SDL-specific objects internally.

Public headers must not expose SDL types.

## `LoadImageA` Integration

Existing disk-based `LoadImageA` behavior must not be broken.

Recommended behavior:

```text
If LR_LOADFROMFILE is set:
    keep current disk loading behavior.

If LR_LOADFROMFILE is not set:
    try to load from embedded resources.
```

Example:

```cpp
LoadImageA(hInstance, MAKEINTRESOURCEA(101), IMAGE_BITMAP, 0, 0, 0);
```

could internally use:

```text
FindResourceA
LoadResource
LockResource
decode bitmap bytes
```

This should be implemented only after the basic resource registry is stable.

## `HMODULE` Limitation

For the first version, it is acceptable to ignore `hModule` and use one global application resource registry.

This should be documented clearly.

Future versions may support multiple module-like resource tables.

## Public API Restrictions

FreeAPI public headers must remain WinAPI-like.

Do not expose:

* SDL types,
* generated resource arrays,
* internal registry structures,
* C++ implementation classes.

Internal/private files may use C++ and SDL implementation details, but public users should still only include headers such as:

```cpp
#include <windows.h>
```

## Testing Requirements

Add tests for:

1. string resource lookup,
2. integer resource lookup via `MAKEINTRESOURCEA`,
3. missing resource returns `NULL`,
4. `SizeofResource` returns correct byte size,
5. `LockResource` returns expected bytes,
6. `FreeResource` does not invalidate data,
7. `LoadStringA` returns generated strings,
8. optional `LoadImageA` resource loading if implemented.

Example cases:

```text
FindResourceA(NULL, "TEST_TEXT", "TEXT") succeeds.
FindResourceA(NULL, MAKEINTRESOURCEA(101), RT_BITMAP) succeeds.
FindResourceA(NULL, "MISSING", "TEXT") returns NULL.
SizeofResource(NULL, hRes) returns expected size.
LockResource(hGlobal) returns stable non-null data.
FreeResource(hGlobal) does not invalidate the data.
```

## Success Criteria

This task is complete when:

* FreeAPI can register generated embedded resources,
* resources can be found by string name or integer ID,
* `FindResourceA`, `LoadResource`, `SizeofResource`, and `LockResource` work,
* `FreeResource` is a safe compatibility no-op,
* `LoadStringA` can use generated string-table data,
* existing disk-based `LoadImageA` behavior still works,
* no SDL/internal types are exposed in public headers,
* the resource input directory is configurable and not hardcoded to `resource/`,
* `.aps` files are ignored,
* limitations are documented with Doxygen comments.

```
```
