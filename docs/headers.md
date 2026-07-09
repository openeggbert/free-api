# Free API — Header Usage Audit

Living copy of `plan.md` §4's header-usage findings — which headers each
target game actually `#include`s, and what's really used from them. `plan.md`
is a point-in-time audit and is not updated after the fact; this file is.

**Re-verify this table whenever either game's own `#include` list changes,**
or when adding/removing a public header in `include/`.

| Header | free-eggbert | planetblupi | Notes |
|---|---|---|---|
| `windows.h` | Yes (~12 files) | Yes (~15 files) | Central facade; required by nearly every source file in both games. |
| `windowsx.h` | Yes (`blupi.cpp`, `ddutil.cpp`, `movie.cpp`) | Yes (`blupi.cpp`, `ddutil.cpp`, `movie.cpp`) | In **both** games, only the `GetStockBrush` macro is actually used (once, at startup). `ddutil.cpp`/`movie.cpp` include it but use nothing from it in either game. |
| `wtypes.h` | Yes (`misc.hpp:5`, `blupi.cpp:14`) | Not included | free-eggbert only; types (`VARTYPE`/`SCODE`/`DATE`/`CLIPFORMAT`) compile but are never exercised at runtime in either game. |
| `mmsystem.h` | Yes (`blupi.cpp`, `movie.cpp`) | Yes (`blupi.cpp`, `movie.cpp`) | WinMM subset: MCI, MIDI-out, timers. `timeSetEvent`'s device-ID counter is deliberately shared with `windows.h`'s `SetTimer` (TASK-24H-0502) — see `src/internal/FreeApiTimers.hpp`. |
| `digitalv.h` | Yes (`movie.cpp`) | Yes (`movie.cpp`) | MCI digital-video (AVI) + sequencer parameter structs. |
| `commdlg.h` | Yes (`movie.cpp:9`) — **zero API calls** | Yes (`movie.cpp:6`) — **zero API calls** | Vestigial dead include in both games (no `GetOpenFileName` etc. anywhere). Keep as empty stub only. |
| `io.h` | Yes (`event.cpp:12,14`, for `_findfirst`/`_findnext`) | Not included; `_findfirst` family confirmed unused | free-eggbert only. |
| `direct.h` | Not literally included; `_mkdir` is called (`event.cpp:4193`) | Yes (`movie.cpp:10`) — **zero API calls** (`_chdir`/`_mkdir`/`_getcwd` confirmed unused) | Dead/vestigial in planetblupi; free-eggbert needs `_mkdir` to remain declared/working regardless of which header path supplies it. `_chdir`/`_getcwd` specifically: confirmed zero call sites in either game's core source, and a **decided** keep-and-document choice (not left open) — kept because `direct.h` must stay for the live `_mkdir`, so declaring these two alongside it costs nothing; removing working code without stronger evidence goes against this project's default policy (TASK-24H-0110/1216; see `plan.md` `TASK-0006`). |
| `sys/timeb.h` (`ftime`/`struct timeb`) | Yes (`blupi.cpp`, `pixmap.cpp`) | Not used | free-eggbert only (already covered by `tests/test_timeb.cpp`). |
| `ddraw.h`, `dsound.h`, `dplay.h`/`"dplay.h"` | Yes | Yes | **Out of scope** — belongs to sibling project `free-direct`. Listed only because these headers co-occur with real WinAPI/GDI symbols in the same files. See `docs/scope.md`'s DirectX boundary statement. |

See [`docs/supported-apis.md`](supported-apis.md) for the symbol-level (not
header-level) status table, and [`plan.md`](../plan.md) §4 for the
original audit this table was derived from.

## Why `windows.h` globally redefines `fopen` (TASK-24H-0113/1206)

`include/windows.h` contains `#undef fopen` / `#define fopen free_api_fopen`
— a broad-reaching macro that silently redirects **every** call to the
standard-library `fopen()` in any translation unit that includes
`<windows.h>` (nearly every source file in both games — see the header-usage
row above) to `free_api_fopen`, which normalizes Windows-style backslash
paths and does a case-insensitive fallback lookup before handing off to the
real `fopen`.

This shape (a blanket macro override) was chosen over a narrower
alternative (e.g. a differently-named wrapper function, like
`FreeApiFopen`, that call sites opt into) for one concrete reason: **both
games call plain `fopen()` directly, pervasively, with zero awareness that
free-api exists** — they were written as real Windows programs, not against
this compatibility layer. A narrower, opt-in wrapper would require editing
every `fopen()` call site in both games' own source to normalize paths,
which is exactly the kind of upstream-game-source change this project
avoids. The macro's blast radius (every `fopen` call in scope, not just a
chosen few) is deliberate, not accidental — it is the only way to get path
normalization transparently, without touching either game's source at all.

See `tests/test_file_regressions.cpp` for the regression coverage over
`free_api_fopen`'s backslash/case-fallback behavior.

**Test-author caveat (found while writing `TASK-24H-1225`):** this applies
to test files too, not just game source — any `tests/*.cpp` that
`#include <windows.h>` gets this same global `fopen`→`free_api_fopen`
redirect for its *own* `fopen()` calls, including ones that have nothing to
do with the WinAPI surface under test. Concretely: a test that does
`fopen("/tmp/some-file", "r")` will have that absolute path silently
normalized by `NormalizeFilesystemPath` — which strips the leading
slash — turning it into a *relative* lookup that can never find the file.
`mkstemp`/`open`/`access` and other non-`fopen` POSIX calls are **not**
affected (only the literal `fopen` token is macro-redirected), which makes
this a confusing, inconsistent-looking failure if you don't know the macro
is there: those other calls succeed against the real absolute path while
`fopen()` alone fails. Prefer relative temp-file paths (resolved against
the test's CWD) in any test file that includes `<windows.h>`.
