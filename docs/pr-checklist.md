# Free API — Pull Request Review Checklist

A quick, consistent checklist for reviewing changes to Free API — for human
reviewers and AI-assisted sessions alike. See [`docs/scope.md`](scope.md)
for the full policy this checklist enforces.

1. **Does this cite a real usage site?** Every new public symbol (function,
   type, constant, message) must cite a `file:line` in `../free-eggbert` or
   `../planetblupi`. If it doesn't, it doesn't belong in Free API — no
   matter how common or "obviously useful" it seems, unless it's explicit
   scope-control cleanup, tests, or documentation.
2. **Does this add a new public header/symbol not already justified in
   `docs/supported-apis.md`?** If so, `docs/supported-apis.md` must be
   updated in the same change to stay accurate.
3. **Does this add a new third-party dependency?** Free API is meant to stay
   small and self-contained (SDL3 + the existing vendored MIDI libraries);
   a new dependency needs a strong, evidenced justification.
4. **Are new/changed behaviors covered by a test derived from actual game
   usage?** Not a synthetic/hypothetical scenario — the test fixture and
   assertions should trace back to a real call site, matching the pattern
   used throughout `tests/`.

Also worth checking, though not blocking every PR:

* Does it stay within the `docs/scope.md` DirectX/GDI boundary with
  `free-direct` (no `DD*`/`DS*`/`DP*` symbols added here)?
* Does it avoid implementing real `W`-suffixed (Unicode) runtime behavior
  (see `docs/out-of-scope.md`)?
* If it touches `cmake/ExtractStringTable.cmake`, does a full
  `cmake -B <dir>` reconfigure (not just `cmake --build`) still pick up the
  change (see `NEXT.md` §5)?
