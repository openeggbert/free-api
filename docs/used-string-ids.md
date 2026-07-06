# Free API — Used LoadStringA/STRINGTABLE ID Manifests

This document records how `cmake/used-string-ids/free-eggbert.txt` and
`cmake/used-string-ids/planetblupi.txt` were derived, so a future session can
re-verify or extend them instead of re-deriving everything from scratch. See
[`cmake-options.md`](cmake-options.md) for how `FREE_API_TARGET_GAME` selects
which manifest applies, and [`out-of-scope.md`](out-of-scope.md)'s
`"RES_<id>"` note for why a used ID silently missing from the generated table
matters (it means shipped game UI would show a raw `RES_<id>` placeholder
instead of real text).

## What these manifests are

Each file is a hand-maintained, plain-text list of every numeric
`STRINGTABLE` ID a target game's own source actually passes to
`LoadString(A)`, backed by a specific `file:line` citation for every entry —
never guessed. Each ID appears exactly once (see "Duplicate-ID validation"
below): **free-eggbert has 308 unique used string IDs; planetblupi has 257
unique used string IDs.** `cmake/ExtractStringTable.cmake`'s `USED_IDS_FILE`
option reads this file at CMake configure time and fails loudly
(`FATAL_ERROR`, listing every missing ID) if any listed ID is absent from
that run's extracted `STRINGTABLE` table — catching a *specific* used string
going missing (e.g. a typo'd ID, a deleted `.rc` entry a game still calls, or
a parser regression on one particular entry), which the pre-existing
`REQUIRE_STRINGS`/`VERIFY_ID` checks do not: those only catch "the whole
table came out empty" or "one known sentinel ID (106) changed," not "one of
the other 300-odd IDs a game actually uses went missing."

This is deliberately **not** "every ID defined in `resource.h`" — both
games' `resource.h` headers define more IDs than are ever passed to
`LoadString` (e.g. dialog/menu/icon resource IDs, or STRINGTABLE entries
left over from removed features). Cross-checking confirmed this directly
(see "Verification" below): free-eggbert's 308 unique used string IDs are a
subset of its 364 extracted strings; planetblupi's 257 unique used string
IDs are all 257 of its extracted strings (no unused strings at all).

## How each ID was found (methodology)

For each game, every `LoadString(...)` call site in `../free-eggbert/src`
and `../planetblupi/src` was enumerated (`grep -rn "LoadString(" src/*.cpp`,
manually excluding the `LoadString(UINT, char*, int)` wrapper's own
declaration/definition/forwarding call in each game's `misc.h`/`misc.cpp` —
that wrapper resolves to `LoadStringA` via `g_hInstance`, since neither game
defines `UNICODE`). Each call site's first argument was classified into one
of:

1. **A literal or symbolic numeric ID** (e.g. `LoadString(TX_HELP, ...)`,
   `LoadString(0x66, ...)`) — resolved directly via the game's own
   `resource.h`/`resrc1.h` `#define`s.
2. **A helper-wrapper function** (e.g. free-eggbert's and planetblupi's own
   `CEvent::DrawTextCenter(int res, ...)`, planetblupi's `GetText`/`GetErr`
   in `menu.cpp`) — traced to *that* function's own callers instead.
3. **A computed expression** (e.g. `TX_LOST1 + GetWorld() % 5`, a table
   lookup, a loop variable) — traced to its concrete bound by reading the
   actual array/loop/enum it's derived from, never assumed. Every computed
   range in the manifests cites the exact evidence for its bound (e.g. "this
   array has 35 entries, and `resource.h` defines exactly 35 consecutive
   `TX_ACTION_*` constants starting at the same base — a 1-for-1 match").
4. **The button-tooltip table**: both games populate a per-phase static
   `Phase`/`Button` array (free-eggbert: `event.cpp:92-1763`; planetblupi:
   `event.cpp:71-1433`) whose `Button.toolTips` field is read by
   `CButton::GetToolTips`/`SetToolTips` and fed straight to `LoadString`.
   Free-eggbert's table uses bare numeric literals; planetblupi's uses
   `TX_*` symbols. Both were parsed structurally (not by ad hoc regex over
   the raw table text) to extract every `toolTips` entry across every
   `Phase`, then resolved to numeric IDs.

## Verification

After deriving each list, it was cross-checked against a *live* extraction
of the game's own `.rc` (`cmake -P cmake/ExtractStringTable.cmake` with
`RC_FILE`/`RESOURCE_HEADERS` set, no `USED_IDS_FILE`) by diffing the
manifest's expanded, de-duplicated ID set against the extracted table's
actual ID set:

* **planetblupi**: exact match, 257 unique used string IDs == 257 extracted
  strings, zero difference either direction. Every `STRINGTABLE` entry in
  `blupi-e.rc` is used, and every used ID this audit found is real.
* **free-eggbert**: the manifest's 308 unique used string IDs are a strict
  subset of the 364 extracted from `Eggbert2.rc` (zero "used but not
  extracted" — the dangerous direction, which would mean a real gap); the
  56 extracted-but-unused IDs are just strings the game's `.rc` defines but
  never calls `LoadString` on.

Both manifests were also validated end-to-end via `cmake/ExtractStringTable.cmake`'s
`USED_IDS_FILE` option itself (configure-time `list(FIND ...)` check against
the real extracted table, not just the offline diff above) by rebuilding
through both `../free-eggbert` and `../planetblupi` — see the CMake and
CTest output cited in the session's commit message for exact counts.

## Duplicate-ID validation

An ID appearing more than once in a manifest — whether a copy-pasted bare
ID or one already covered by an earlier "A-B" range — makes the "N used
string ID(s) verified present" count misleading (it silently counts the
same ID twice) and is easy to introduce by hand. `USED_IDS_FILE` therefore
tracks each ID's first occurrence and fails configure loudly
(`FATAL_ERROR`, listing every duplicate) if any ID repeats, checked
*before* the missing-ID check. This is why both manifests are guaranteed
to have exactly one entry per used ID: free-eggbert's 308 and
planetblupi's 257 are unique-ID counts, not raw line counts.

A prior pass of `cmake/used-string-ids/free-eggbert.txt` had IDs 194
(`TX_CONTENT`) and 288 (`TX_GAMESAVED`) listed twice each — once under
their symbolic name and once under the numeric-literal call site that
happens to share the same value (`0xC2` and `0x120u` respectively). Both
were merged into a single entry citing both call sites; the unique-ID
count (308) was unaffected since both call sites always referred to the
same underlying ID.

## Re-deriving or extending a manifest

If a game's source changes in a way that adds, removes, or changes a
`LoadString` call site:

1. Re-run the enumeration/tracing steps above for the changed file(s) only.
2. Update the corresponding `cmake/used-string-ids/<game>.txt`, keeping the
   `file:line` citation style (or citing the specific computed-range
   evidence, as in the existing entries).
3. Re-run the verification step above (diff against a live
   `cmake -P cmake/ExtractStringTable.cmake` extraction) before committing —
   an incorrect manifest entry would either falsely fail configure (ID
   listed that isn't real) or, worse, give false confidence by omitting a
   genuinely-used ID. If the same ID has more than one call-site citation,
   put them in a single entry (see "Duplicate-ID validation" above) rather
   than listing the ID twice.
4. Reconfigure through both `../free-eggbert` and `../planetblupi` and
   confirm the "all N unique used string ID(s) ... verified present"
   `STATUS` message still appears with the expected count.

Do not use the resource-defines' own numeric range as a shortcut for "which
IDs are used" — as the free-eggbert cross-check above shows, a game's
`resource.h` can define many more IDs than it actually loads through
`LoadString`, and silently treating "defined" as "used" would make this
manifest worthless as a *used*-ID check (it would just re-derive
`REQUIRE_STRINGS`'s existing "table is non-empty" guarantee under a
different name).
