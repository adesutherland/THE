# cREXX compatibility stabilization worklist

## Baseline

- THE: `main` at `a42e977eb95ba38ef319c345eafcc1d2c224b90e`
- cREXX: `develop` at `d5f0827ca2708eae9d9be182c6d0d53bd6229b74`
- Build under test: `cmake-build-debug`, `USE_CREXX=ON`, cREXXSAA ABI 3
- Initial focused result: 0/9 passed; eight failures and one timeout
- Final original-failure result: 9/9 passed
- Final complete result: 55/55 passed, with all 10 batch Markdown tests executed

## Compatibility finding

cREXX commit `efbb318969483068f12903ee5dc62e0787e6b1c0` added continued
string literals. A quoted literal at the start of the next clause is now
treated as a continuation unless the preceding literal has an explicit clause
terminator. THE profiles use consecutive quoted expressions as `ADDRESS THE`
commands, so their old implicit newline separation no longer preserves the
intended command stream. The same result occurs with `rxc -n`, so this is a
source-language compatibility change rather than a later optimizer-only issue.

## Worklist

- [x] Pin THE and cREXX repository/build identities.
- [x] Reproduce the current focused CTest failure set.
- [x] Isolate the first shared compatibility cause.
- [x] Add explicit clause terminators to shipped Level B profiles.
- [x] Add explicit clause terminators to generated Level B test profiles.
- [x] Rebuild THE so the release payload contains the corrected profiles.
- [x] Re-run the original nine-test failure set.
- [x] Run the complete THE CTest suite and classify any remaining failures.
- [x] Record final test counts and remaining compatibility risks.

## Closeout

- The nine initially failing tests now pass.
- The complete suite passes: 55 tests run, 55 passed, 0 failed, 0 skipped.
- Runtime profile and batch cREXX source copying are represented by explicit
  CMake outputs and `ALL` targets, so an incremental build cannot leave stale
  compatibility resources beside the THE executable.
- The cREXX executable used by batch tests now follows
  `CMAKE_EXECUTABLE_SUFFIX`; on macOS this removes the stale `.exe` assumption
  that had silently skipped all 10 tests.
- Generated batch Markdown profiles now terminate implicit `ADDRESS THE`
  command literals explicitly, matching the shipped and test profiles.

The remaining compatibility boundary is the legacy macro surface excluded by
the scope guard below. No current build or CTest regression points into it.

## Scope guard

This pass changes THE compatibility sources and tests only. It does not modify
the cREXX repository, public cREXX ABI, THE editor command behavior, or legacy
macro sources that are not part of the Level B build/test surface.
