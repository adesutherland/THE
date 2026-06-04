# UTF-8 Design and Status

Last updated: 2026-06-04.

This is the single UTF-8 design and status note for THE. It replaces the older
UTF design, handover, and cluster-mapping notes. Keep historical experiments
out of this file unless they still change a current design decision.

## Vision

THE should be Unicode-first while keeping the editor model stable for XEDIT and
KEDIT style commands:

- Store file contents as UTF-8 bytes without lossy conversion.
- Treat interactive character positions as Unicode grapheme clusters.
- Keep logical editor columns separate from physical terminal cells.
- Let physical drivers handle terminal quirks without changing stored text.
- Prove behavior through focused tests and the real no-curses `the --driver llm`
  surface before relying on it in new agent/editor workflows.

The core rule is:

```text
UTF-8 bytes -> logical clusters/TextPos -> driver-neutral frame
driver-neutral frame -> physical terminal or semantic LLM view
```

Terminal profiles can change how a cluster is displayed. They must not change
the bytes, grapheme boundaries, logical cursor position, or editing unit.

## utf8proc Decision

`utf8proc` is already an enabled-by-default dependency for maintained UTF
builds:

- `USE_UTF8=ON` and `USE_UTF8PROC=ON` are the CMake defaults.
- CMake first tries `find_package(utf8proc)`.
- If no package is available, CMake fetches `JuliaStrings/utf8proc` at
  `v2.11.3` with `FetchContent`.
- The current local build used the fetched copy under
  `cmake-build-debug/_deps/utf8proc-src`.

Recommendation: treat `utf8proc` as required for supported UTF behavior. The
current fallback code in `src/textpos.c` is useful for build isolation, but it
is not a complete Unicode implementation. Grapheme boundaries, emoji sequences,
combining behavior, and character widths should be maintained through
`utf8proc`, not through an expanding local Unicode table.

## Architecture

The logical layer owns file bytes, decoded code points, `TextPos`, grapheme
cluster boundaries, logical cell columns, selection/block ranges, command-line
text, prefix text, and normalized input intent.

The physical profile/driver layer owns terminal class policy, display mode,
output transform, physical width, advance width, cursor width, repaint width,
visual mark, and repair strategy.

Important modules:

- `src/textpos.c`: UTF-8 decoding, grapheme clusters, code point and logical
  cell positions.
- `src/utfcluster.c`: classifies clusters such as ASCII, combining, wide,
  emoji, keycap, regional flag, ZWJ families, tag flag, and private use.
- `src/utfterm.c` and `src/utfterm_defaults.h`: terminal profiles and startup
  defaults.
- `src/utflayout.c` and `src/driverlayout.c`: conversions between logical
  columns, user-visible `WIDTH` columns, and physical `ADVANCE` columns.
- `src/rendercell.c`: driver-neutral render clusters, transformed output, and
  width metadata.
- `src/drivers/curses/cursesdriver.c`: terminal lowering, refresh ordering,
  software cursor painting, and UTF repair execution.
- `src/llm/llmdriver.c` and `src/llm/llmsession.c`: semantic snapshots and
  normalized no-curses protocol for `the --driver llm`.

The public driver boundary must remain curses-free. Editor code should use
logical positions and driver operations, not curses cursor state.

## Terminal Profile Model

`SET UTF TERMINAL CLASS` configures physical rendering for each cluster class.
The active profile entry contains:

```text
class, display mode, output method, mark,
WIDTH, ADVANCE, CURSOR, REPAINT,
CURSORSTRATEGY, REPLACESTRATEGY
```

Meanings:

- `WIDTH`: user-visible width reported in metadata and user-facing column
  calculations.
- `ADVANCE`: physical terminal cells used to place following output.
- `CURSOR`: physical cells covered by cursor/background presentation.
- `REPAINT`: physical cleanup footprint for stale glyph fragments.
- `OUTPUT`: `native`, `expanded`, `components`, `base`, or `substitute`.
- `MARK`: `none`, `compressed`, `substituted`, or `unsafe`.
- `CURSORSTRATEGY` and `REPLACESTRATEGY`: separate because cursor movement can
  be safe when text replacement still needs stronger repaint.

`SET UTF DISPLAY GROUPED|COMPONENTS|TOGGLE` chooses the preferred display mode.
Display mode is a view preference; it does not change logical text identity.

THE loads built-in defaults, terminal identity overrides, the platform system
profile, and then the user profile. On macOS the generated platform profile is
`system-osx.the`.

## Current Status

Implemented:

- UTF-8 and wide-character support are on by default.
- Logical positions use UTF-8 bytes, code points, grapheme cluster indexes, and
  logical cell columns through `TextPos`.
- `utf8proc` supplies maintained grapheme breaks and character widths when
  `USE_UTF8PROC=ON`.
- Cluster classification is shared in `src/utfcluster.c`.
- Terminal profiles support `WIDTH`, `ADVANCE`, `CURSOR`, `REPAINT`, display
  modes, output transforms, marks, cursor strategies, and replacement
  strategies.
- Apple Terminal overrides are represented as profile policy, not renderer
  special cases.
- Rendering carries logical width plus profile width, advance, cursor, repaint,
  output method, class, and mark through `TheRenderCluster`.
- The LLM driver exposes row-level UTF annotations with logical width, `WIDTH`,
  `ADVANCE`, `CURSOR`, `REPAINT`, class, output, mark, compressed and
  substituted flags.
- File-area cursor movement, vertical intent, mouse/hit mapping, status/current
  position reporting, box/mark/shift/case operations, CUA overlay, `CINSERT`,
  `CREPLACE`, `COVERLAY`, and SOS logical edits are cluster-aware for the main
  UTF paths covered by tests.
- The real no-curses target is `the --driver llm`; it boots the full editor
  runtime without curses and uses semantic snapshots rather than screen
  scraping.

Out of scope for terminal profile widths:

- Prefix and command-line cells remain fixed prompt/prefix cells unless a
  specific text mutation path decodes UTF text.
- Terminal repair strategies are physical behavior and should not be exposed as
  position authority for agents or macros.

## Apple Terminal Lessons

Keep these lessons because they still affect design:

- Apple Terminal can retain composition state across local repaint. Repainting
  only the cell after a keycap can visually damage the keycap or the following
  text even when logical cursor state is correct.
- Native keycap glyphs were unreliable enough that the macOS profile now uses
  `OUTPUT base`, `MARK compressed`, and one-cell physical widths for keycaps.
  The stored text remains the original keycap sequence.
- Regional flags are separate from keycaps. They remain literal UTF clusters,
  but terminal profiles may need a wider physical footprint than their logical
  model.
- ZWJ sequences must remain one logical grapheme cluster whether the terminal
  shapes them as one glyph, falls back to components, or THE uses a substitute.
- A successful cursor-walk probe does not prove replacement behavior. Cursor
  and replacement strategies must be calibrated independently.

## Probe Tool

`utf_terminal_probe` is the place to investigate terminal behavior before
changing THE rendering. Current useful commands:

```sh
cmake --build cmake-build-debug --target utf_terminal_probe
./cmake-build-debug/utf_terminal_probe list
./cmake-build-debug/utf_terminal_probe view keycap
./cmake-build-debug/utf_terminal_probe cursor keycap 2 2 line
./cmake-build-debug/utf_terminal_probe chain keycap 2 2 first
./cmake-build-debug/utf_terminal_probe calibrate all --profile-dir ./cmake-build-debug/release
```

The probe should save terminal policy, not Unicode semantics.

## Validation Snapshot

Validated locally on 2026-06-04:

```sh
cmake --build cmake-build-debug --target the utf_terminal_probe
ctest --test-dir cmake-build-debug \
  -R 'test_utfcluster|test_utfterm|test_utflayout|test_llmdriver|test_llmruntime|test_the_llm_full_runtime|test_driver_modules|test_sos_logical_edit_queries' \
  --output-on-failure
```

Result: all selected tests passed.

A manual `the --driver llm` probe confirmed:

- capabilities report `driver=llm`, `curses=false`, full THE dispatcher,
  CREXX available in this build, syntax/style spans, parser diagnostics, and
  the supported protocol verbs.
- UTF row annotations report CJK as `wide/native` with width metadata.
- macOS profile policy reports keycaps as `keycap/base/compressed` with
  one-cell physical metadata.
- The normalized `text`/`type` protocol path still feeds bytes to
  `process_key()`, so UTF text entry remains an outstanding item.

## Outstanding Items

1. Apple display regression in the curses driver.
   Revalidate `tests/fixtures/utf-render.txt` in Apple Terminal, especially
   keycap, regional-indicator, regional-flag, modifier, and ZWJ rows. Keep
   fixes profile-driven or driver-local.

2. Windows and Linux validation.
   Prove runtime module loading, wide curses/PDCurses behavior, UTF profiles,
   and `the --driver llm` on Linux and native Windows. Add system profiles only
   after probe evidence exists for those terminals.

3. Probe cleanup.
   Make calibration easier to use, keep replacement probes first-class, clarify
   saved-profile behavior, and keep scripted/nonvisual output stable enough for
   regression comparison.

4. UTF text entry.
   Introduce a first-class UTF text input path. `llm_session` currently sends
   bytes through `process_key()`, and the historical `TEXT` command loops over
   bytes before reaching `textedit_*_utf8()`. The fix should pass whole UTF-8
   code point or cluster text into file-area editing, command-line editing, and
   LLM normalized input, with tests for CJK, combining marks, emoji, and
   invalid/truncated input.

5. Dependency policy.
   Keep `utf8proc` as the supported UTF dependency. Decide whether to remove
   the non-utf8proc maintained path or leave it as explicitly unsupported
   fallback code.

6. Layout/performance cache.
   A shared per-line UTF layout cache is still desirable so rendering, status,
   mouse/hit mapping, selections, and LLM metadata consume the same computed
   byte, cluster, logical, `WIDTH`, and `ADVANCE` map without repeated scans.

7. Remaining legacy column audit.
   Main box/mark/shift/case paths are covered, but any older command path that
   still treats `current_column`, `verify_col`, or `verify_end` as a user-facing
   UTF column should be reviewed opportunistically.

8. Exact profile matching.
   Class-level profiles are enough for current problems. Add exact sequence or
   wildcard `MATCH` rules only if class-level policy proves too coarse.

## Guardrails

Before closing a UTF slice:

- Show the behavior through focused unit tests or `the --driver llm`.
- Keep logical behavior independent of terminal profile widths.
- Keep terminal mechanics inside the physical driver or profile layer.
- Run `git diff --check`.
- Run the relevant UTF and LLM CTest slice.
- Use `utf_terminal_probe` for terminal-specific conclusions before changing
  renderer policy.
