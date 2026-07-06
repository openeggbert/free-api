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
| `mmsystem.h` | Yes (`blupi.cpp`, `movie.cpp`) | Yes (`blupi.cpp`, `movie.cpp`) | WinMM subset: MCI, MIDI-out, timers. |
| `digitalv.h` | Yes (`movie.cpp`) | Yes (`movie.cpp`) | MCI digital-video (AVI) + sequencer parameter structs. |
| `commdlg.h` | Yes (`movie.cpp:9`) — **zero API calls** | Yes (`movie.cpp:6`) — **zero API calls** | Vestigial dead include in both games (no `GetOpenFileName` etc. anywhere). Keep as empty stub only. |
| `io.h` | Yes (`event.cpp:12,14`, for `_findfirst`/`_findnext`) | Not included; `_findfirst` family confirmed unused | free-eggbert only. |
| `direct.h` | Not literally included; `_mkdir` is called (`event.cpp:4193`) | Yes (`movie.cpp:10`) — **zero API calls** (`_chdir`/`_mkdir`/`_getcwd` confirmed unused) | Dead/vestigial in planetblupi; free-eggbert needs `_mkdir` to remain declared/working regardless of which header path supplies it. |
| `sys/timeb.h` (`ftime`/`struct timeb`) | Yes (`blupi.cpp`, `pixmap.cpp`) | Not used | free-eggbert only (already covered by `tests/test_timeb.cpp`). |
| `ddraw.h`, `dsound.h`, `dplay.h`/`"dplay.h"` | Yes | Yes | **Out of scope** — belongs to sibling project `free-direct`. Listed only because these headers co-occur with real WinAPI/GDI symbols in the same files. See `docs/scope.md`'s DirectX boundary statement. |

See [`docs/supported-apis.md`](supported-apis.md) for the symbol-level (not
header-level) status table, and [`plan.md`](../plan.md) §4 for the
original audit this table was derived from.
