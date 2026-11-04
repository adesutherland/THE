# Integrated verification

[Receipt](receipt.json) records the exact inputs and separate proof boundaries.

- The [full native run](native-full-ctest.log) passed 120 of 122 checks with no
  skips. Its two failures were obsolete fixture predicates: string payloads
  versus nested region objects, and a focus-only expectation for a pane toggle.
  Both corrected tests passed separately, covering all 122 current checks.
  Product source and binaries stayed unchanged; the original failures remain
  in the [JUnit record](native-full-ctest.xml).
- The [browser candidate receipt](browser-candidate.qa.json) and
  [log](browser-candidate.qa.log) qualify the extracted ZIP at `e535081`,
  SHA-256 `27515a9a07ff6f9ee8b784c01fda2fe81e54f14f46f5e199afde9d27f506933a`.
  Actual Wasm/paint checks and the production UI semantic fixture passed;
  the receipt distinguishes their scopes.

Later native fixture/evidence corrections do not change the reviewed product
or this ZIP. These results supplement the independent UI gates; they do not
execute the full 29-case acceptance catalogue or qualify other OS/device
cohorts, installed packages, hosted CI or a release.
