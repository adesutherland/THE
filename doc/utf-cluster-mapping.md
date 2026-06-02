# UTF Cluster Mapping Design

Last updated: 2026-06-02.

This document specifies the planned UTF cluster mapping layer. The goal is to
keep editor text and logical cursor semantics independent from terminal-specific
rendering workarounds, while giving physical drivers enough information to draw
known-problem clusters safely.

## Problem Statement

Some Unicode grapheme clusters are valid editor characters but unreliable
terminal cells. Apple Terminal keycaps are the current motivating case: the
stored cluster is a normal Unicode keycap sequence, logical cursor state stays
consistent, and the terminal cursor report can remain plausible, but the visible
terminal rendering jumps or damages repaint state when the cursor crosses a line
containing keycaps.

The editor therefore needs a profile-driven physical rendering policy:

```text
UTF-8 bytes
  -> logical grapheme clusters
  -> logical editor positions
  -> physical cluster class
  -> terminal profile mapping
  -> render output, display width, cursor width, paint width, repair strategy
```

The policy must never change the file bytes, logical cluster boundaries, or
logical editor cursor position.

## Ownership Boundary

The logical layer owns:

- file bytes and decoded codepoints.
- extended grapheme cluster boundaries.
- `TextPos.byte_offset`, `TextPos.codepoint_index`,
  `TextPos.cluster_index`, and `TextPos.cell_column`.
- logical movement, selection, replacement, overlay, and command semantics.
- LLM/semantic snapshots and logical hit targets.

The physical driver/profile layer owns:

- terminal class selection for a cluster.
- output transform such as native, substitute, base, or components.
- terminal display width, cursor width, paint width, and repair strategy.
- terminal-profile overrides such as Apple Terminal choosing compressed keycap
  output.
- visual marker attributes for compressed or substituted clusters.

Shared code may compute cluster facts, but terminal profile decisions are not
logical truth. The LLM driver may expose physical mapping as metadata for
diagnostics, but it must use logical positions as authority. The curses driver
needs the full physical contract because it materializes the editor state onto a
terminal.

## Current Logical Model

THE already tracks both cluster ordinal and logical cell position:

- `TextPos.cluster_index` counts grapheme clusters.
- `TextPos.cell_column` is the editor's logical cell column.
- `TextCluster.cell_width` is derived from codepoint cell widths.
- `utf8_layout_cluster_logical_width()` currently maps a zero-width cluster to
  logical width one, otherwise uses `TextCluster.cell_width`.

This means logical cluster movement and logical column movement are related but
not identical. That distinction should remain. Cluster-aware commands can use
cluster indices; column-oriented editor behavior can continue to use logical
cell columns.

Terminal profile widths must not feed back into `TextPos.cell_column`. For
example, a keycap can remain one logical editor cell while an old physical
profile reserves two terminal cells or a compressed physical profile renders it
as one safe ASCII cell.

## Unicode And utf8proc Role

utf8proc should be used for Unicode facts:

- UTF-8 decoding validation where useful.
- Unicode version reporting.
- codepoint properties.
- `charwidth` and ambiguous-width facts.
- grapheme break classes and stateful grapheme boundary decisions.
- extended pictographic and regional-indicator facts where available through
  bound classes or generated data.

utf8proc does not provide a complete `cluster_type = keycap` API. THE still
needs a classifier on top of utf8proc and Unicode emoji data. The classifier
should be deterministic, small, and testable.

The classifier should prefer Unicode-defined sequence forms from UTS #51 and
related data files:

- keycap sequences.
- regional indicator flag sequences.
- tag flag sequences.
- emoji ZWJ sequences.
- emoji modifier sequences.
- emoji/text variation sequences.
- generic combining clusters.
- generic wide, ambiguous, private-use, ASCII, and unknown clusters.

## Cluster Classes

Cluster classes are ordered from specific to generic. First match wins.

```text
keycap
regional-flag
tag-flag
emoji-zwj
emoji-modifier
emoji-variation
text-variation
combining-stack
combining
emoji
wide
ambiguous
private-use
ascii
unknown
```

The existing names `short-zwj`, `heart-zwj`, and `family-zwj` can remain as
profile aliases or sub-classes for compatibility. New classifier code should
also expose a generic `emoji-zwj` family so terminal policy can be expressed
without overfitting to the current heuristic categories.

Initial class definitions:

```text
keycap
  [U+0030..U+0039 U+0023 U+002A] U+FE0F? U+20E3

regional-flag
  Regional_Indicator Regional_Indicator

tag-flag
  U+1F3F4 TagChar+ U+E007F

emoji-zwj
  Cluster containing U+200D and an Extended_Pictographic component.

emoji-modifier
  Emoji modifier base plus U+1F3FB..U+1F3FF.

emoji-variation
  Emoji/text-capable base plus U+FE0F.

text-variation
  Emoji/text-capable base plus U+FE0E.

combining
  One spacing base plus one combining/extend/spacing-mark component, with
  logical width less than or equal to one.

combining-stack
  Combining cluster with multiple combining/extend/spacing-mark components.

emoji
  Single-cluster emoji or pictographic cluster that is not a more specific
  sequence class.

wide
  Cluster whose logical width is two or more and is not a more specific emoji
  sequence class.

ambiguous
  Printable non-ASCII cluster whose width is terminal/font-sensitive.

private-use
  Cluster starting with a private-use codepoint.

ascii
  Single ASCII cluster.
```

## Profile Lookup Semantics

Profile lookup should be ordered:

1. Exact sequence or named pattern match, once implemented.
2. Class plus selected display mode.
3. Class normal display profile.
4. Generic fallback class.
5. Built-in unknown fallback.

Existing `SET UTF DISPLAY GROUPED|COMPONENTS|TOGGLE` remains the global display
preference. A display-specific profile is used only when the class has one.

Physical drivers consume the resolved entry as a terminal contract:

```text
class
display mode
output transform
substitute/base data
layout width
cursor width
paint width
repair strategy
visual mark
```

The logical layer may ask for the class and mapping metadata, but must not use
profile layout width as logical column width.

## Mapping Syntax

Stage one should extend the existing `SET UTF TERMINAL CLASS` syntax rather
than introduce a second profile language.

Current syntax remains valid:

```text
SET UTF TERMINAL CLASS class [DISPLAY display] LAYOUT n CURSOR n
SET UTF TERMINAL CLASS class [DISPLAY display] OUTPUT native
SET UTF TERMINAL CLASS class [DISPLAY display] OUTPUT expanded
SET UTF TERMINAL CLASS class [DISPLAY display] OUTPUT substitute U+25A1
SET UTF TERMINAL CLASS class [DISPLAY display] CURSORSTRATEGY strategy
SET UTF TERMINAL CLASS class [DISPLAY display] REPLACESTRATEGY strategy
```

Planned output transforms:

```text
native
  Emit the original cluster.

substitute U+codepoint
  Emit one configured substitute codepoint.

base
  Emit a class-specific stable base representation.

components
  Emit visible component characters, dropping joiners/selectors where the class
  definition says they are display mechanics rather than useful text.

hex
  Emit compact diagnostic codepoint text when a safe visual form is more useful
  than the original cluster.
```

`expanded` should remain accepted for compatibility. It can become an alias for
`components` once component output is fully class-aware.

Planned visual marker syntax:

```text
SET UTF TERMINAL CLASS class [DISPLAY display] MARK none
SET UTF TERMINAL CLASS class [DISPLAY display] MARK compressed
SET UTF TERMINAL CLASS class [DISPLAY display] MARK substituted
SET UTF TERMINAL CLASS class [DISPLAY display] MARK unsafe
```

`MARK` is a render attribute hint. It must not be the only way to understand the
display because monochrome terminals, themes, and accessibility settings may
hide colour.

Later, exact and wildcard cluster matching can be added if class-level policy
is not precise enough:

```text
SET UTF TERMINAL MATCH name PATTERN U+0030..U+0039 U+FE0F? U+20E3 OUTPUT base
SET UTF TERMINAL MATCH name PATTERN class:emoji-zwj OUTPUT components
```

This later `MATCH` form should use an ordered rule table and should not be
implemented until class-level mapping has proved insufficient.

## Output Transform Semantics

`native` preserves the original codepoint sequence.

`substitute` emits exactly the configured substitute codepoint. The output
width defaults to one unless an explicit layout width overrides it.

`base` is class-specific:

- keycap: emit the base ASCII digit, `#`, or `*`; drop U+FE0F and U+20E3.
- regional-flag: emit two ASCII region letters when available.
- tag-flag: emit a short ASCII tag label when decodable, otherwise substitute.
- emoji-variation: emit the base codepoint without U+FE0F.
- text-variation: emit the base codepoint without U+FE0E.
- emoji-modifier: emit the unmodified base when safe, otherwise substitute.
- other classes: fall back to substitute unless a class-specific base rule is
  defined.

`components` emits a stable component display:

- drop U+200D.
- drop U+FE0E/U+FE0F when they are only presentation selectors.
- keep visible spacing components.
- preserve component order.
- compute or configure the resulting physical width.

`hex` is diagnostic, not a normal editing display. It may exceed one cell and
should not be used as the default compressed mode.

## Width Semantics

Each rendered cluster has four width concepts:

- logical width: editor logical cell width from the logical text model.
- display width: physical terminal cells reserved for output.
- cursor width: physical terminal cells covered by hardware/software cursor
  handling.
- paint width: physical cells that must be blanked/repainted to avoid stale
  fragments.

For native output, display width is terminal-profile-specific.

For compressed output, display width should normally equal the safe replacement
width. Apple Terminal keycaps rendered as `base` should be one display cell and
one cursor cell.

For component output, display width should describe the visible component
string, not the original grouped cluster. It may differ from logical width.

Profile parsing should reject zero or negative display/cursor widths except for
explicit future `auto` support. `auto` should not be added until render output
has a reliable width calculator for transformed strings.

## Driver Semantics

The curses driver consumes all terminal contract fields:

- transformed output.
- display width.
- cursor width.
- paint width.
- visual marker.
- repaint strategy.

The headless/LLM driver should preserve enough render metadata for tests and
diagnostics, but semantic snapshots should stay logical:

- original cluster text.
- cluster index.
- logical cell column.
- optional class and physical mapping metadata.
- optional compressed/substituted marker.

LLM protocol clients should not need terminal repair strategies to reason about
text position.

Semantic LLM snapshots expose UTF cluster metadata only as row annotations.
Each annotation is keyed by logical cell and logical width, then names the
cluster class, output method, marker, and compressed/substituted booleans. It
does not expose terminal display width, cursor width, paint width, or repair
strategy as position authority. Compact snapshots use the same data in a short
`u` array; full snapshots use a `utf` array.

## Base Profile

The portable base profile should prefer native output for well-behaved clusters:

```text
ascii           native, width 1
combining       native, logical width 1, conservative repair
wide            native, width 2
emoji           native, width 2
regional-flag   native or profile-calibrated width 2/3
tag-flag        native, width 2
emoji-zwj       grouped native where safe, components display available
keycap          native by default unless terminal identity overrides it
unknown         native with conservative repair, or substitute in strict mode
```

Apple Terminal should override native keycap rendering once the transform is
implemented:

```text
SET UTF TERMINAL CLASS keycap OUTPUT base
SET UTF TERMINAL CLASS keycap MARK compressed
SET UTF TERMINAL CLASS keycap LAYOUT 1 CURSOR 1
SET UTF TERMINAL CLASS keycap CURSORSTRATEGY cells
SET UTF TERMINAL CLASS keycap REPLACESTRATEGY cells
```

This gives up the colourful keycap glyph in Apple Terminal, but keeps the
logical text intact and preserves readable identity.

The Apple Terminal change is not an Apple-only code path. `OUTPUT base`,
`OUTPUT components`, and `MARK` are general terminal-profile capabilities. The
Apple identity and `system-osx.the` use them as a policy choice for the known
Apple keycap repaint/cursor defect; another terminal profile can opt into the
same mapping, or keep native keycaps, without changing the logical editor
model.

## Implementation Plan

1. Add cluster facts.
   Create a shared classifier module, likely `src/utfcluster.c` and
   `src/utfcluster.h`, that accepts a `TextCluster` and exposes decoded facts:
   codepoints, class, sub-class flags, logical width, and transform helpers.
   Keep terminal policy out of this module.

2. Move classification out of terminal profile code.
   Replace the heuristic classifier in `src/utfterm.c` with calls into the new
   shared classifier. Preserve existing class names and behavior with tests
   before adding new classes.

3. Tighten logical terminology.
   Add comments and tests clarifying that `TextPos.cell_column` is logical
   editor cell column, while terminal profile widths are physical render
   contract. Do not rename fields in the first implementation slice unless the
   change is mechanical and low-risk.

4. Extend profile data.
   Add output modes `base` and `components`, plus a `MARK` field. Preserve
   `expanded` and `substitute` compatibility.

5. Add render transforms.
   Extend `TheRenderCluster` or the render conversion path so transformed output
   can be represented without losing the original cluster. `rendercell.c` should
   remain the common place that turns a logical cluster plus profile entry into
   driver-neutral render metadata.

6. Add Apple Terminal keycap override.
   Change only terminal identity/profile defaults after unit tests prove
   `OUTPUT base` renders keycaps as ASCII bases with physical width one. This
   step must remain a terminal-profile policy that uses the general mapping
   capability, not a special Apple-only renderer branch.

7. Update LLM metadata carefully.
   If useful, expose cluster class and compressed/substituted flags in semantic
   snapshots. Do not expose terminal display width as the position authority.
   Implemented metadata is row-local and logical-cell based: `cell`, logical
   `width`, `class`, `output`, `mark`, `compressed`, and `substituted`.

8. Add tests.
   Cover classifier behavior, profile parsing, render transforms, logical
   invariance, headless render metadata, LLM semantic stability, and the curses
   boundary inventory. Keep real Apple Terminal visual verification in the
   terminal probe/manual-test lane. The LLM coverage checks both full and
   compact semantic metadata and asserts that physical display widths are not
   exported into the semantic position contract.

## Test Plan

Focused unit tests:

- `test_utfcluster`: classifies keycap, flags, tag flags, ZWJ, variation,
  combining, wide, ambiguous, ASCII, private-use, and unknown clusters.
- `test_utfterm`: parses new output modes and marker settings, and preserves
  existing profile behavior.
- `test_rendercell`: verifies native, substitute, base, and components output
  conversion while preserving original cluster metadata.
- `test_utflayout`: proves terminal mapping changes physical display columns
  without changing logical cell columns.
- `test_textpos`: proves cluster indices and logical cell columns remain stable
  for keycaps, maps, ZWJ, combining, and virtual EOL positions.
- `test_headlessdriver` and `test_virtual_screen`: verify transformed render
  metadata and logical cursor preservation.
- LLM full-runtime tests: ensure snapshots continue to report logical cursor
  positions independent of terminal profile overrides.

Manual/probe tests:

- Apple Terminal keycap row with native output.
- Apple Terminal keycap row with `OUTPUT base`.
- iTerm2/WezTerm/Ghostty comparison for native keycaps and ZWJ classes.
- Curses diagnostic and raw diagnostic probes for any attempted native repair.

Guardrails:

```sh
git diff --check
bash tests/inventory_direct_curses.sh --summary /Users/adrian/CLionProjects/THE
ctest --test-dir cmake-build-debug \
  -R 'test_textpos|test_utflayout|test_utfterm|test_rendercell|test_headlessdriver|test_virtual_screen|test_llmdriver|test_llmruntime|test_driver_modules|test_curses_boundary' \
  --output-on-failure
```

## References

- utf8proc API documentation:
  `https://juliastrings.github.io/utf8proc/doc/utf8proc_8h.html`
- utf8proc property struct documentation:
  `https://juliastrings.github.io/utf8proc/doc/structutf8proc__property__struct.html`
- Unicode UAX #29, Text Segmentation:
  `https://unicode.org/reports/tr29/`
- Unicode UTS #51, Unicode Emoji:
  `https://unicode.org/reports/tr51/`
- Unicode UAX #11, East Asian Width:
  `https://unicode.org/reports/tr11/`

## Open Questions

- Should utf8proc become a required dependency for maintained UTF builds, or
  should the current fallback classifier remain supported indefinitely?
- Should `regional-flag` keep the current three-cell default until terminal
  baselines are refreshed, or should the base profile move to two cells?
- How much physical mapping metadata should LLM snapshots expose without making
  agents reason about terminal repair details?
- Should exact `MATCH` rules be implemented, or are class-level mappings enough
  for the known problem clusters?
- Should `MARK compressed` map to a dedicated logical style, a physical driver
  attribute hint, or both?
