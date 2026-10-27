# UTF-8 architecture and terminal profiles

This guide describes UTF text identity, layout and terminal-profile behavior.
See [known issues](known-issues.md) for limitations and the
[roadmap](roadmap.md) for planned work and qualification.

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

UTF text entry is separate from display policy. Literal `TEXT`, `INPUT`, LLM
`text`, and LLM `insert after` accept UTF-8 text. `UTFTEXT`, `UTFINPUT`,
LLM `text-utf`, and LLM `insert-utf` provide deterministic
code-point entry using `U+nnnn` notation. These routes store the resulting
UTF-8 bytes without normalization.

## utf8proc Decision

`utf8proc` is already an enabled-by-default dependency for maintained UTF
builds:

- `USE_UTF8=ON` and `USE_UTF8PROC=ON` are the CMake defaults.
- CMake first tries `find_package(utf8proc)`.
- If no package is available, CMake fetches `JuliaStrings/utf8proc` at
  `v2.11.3` with `FetchContent`.

Use `utf8proc` for supported UTF behavior. The
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
- `src/utfinput.c`: deterministic `U+...` code-point parsing for UTF text
  entry commands and LLM replay.
- `src/drivers/curses/cursesdriver.c`: terminal lowering, refresh ordering,
  software cursor painting, and UTF repair execution.
- `src/llm/llmdriver.c` and `src/llm/llmsession.c`: semantic snapshots and
  normalized no-curses protocol for `the --driver llm`.

The public driver boundary must remain curses-free. Editor code should use
logical positions and driver operations, not curses cursor state.

## Terminal Profile Model

`SET UTF DISPLAY <mode> CLASS` configures physical rendering for each cluster
class in each display mode.
The active profile entry contains:

```text
class, display mode, output method, metric method, mark,
WIDTH, ADVANCE, CURSOR, REPAINT,
DISPLAYSTRATEGY, CURSORSTRATEGY, REPLACESTRATEGY
```

Meanings:

- `WIDTH`: user-visible width reported in metadata and user-facing column
  calculations.
- `ADVANCE`: physical terminal cells used to place following output.
- `CURSOR`: physical cells covered by cursor/background presentation.
- `REPAINT`: physical cleanup footprint for stale glyph fragments.
- `OUTPUT`: `native`, `sanitize`, `expanded`, `components`, `base`, or
  `substitute`.
- `METRICS`: `auto`, `profile`, `components`, or `expanded`.
- `MARK`: `none`, `compressed`, `substituted`, or `unsafe`.
- `DISPLAYSTRATEGY`: `inline` or `isolate`.
- `CURSORSTRATEGY` and `REPLACESTRATEGY`: separate because cursor movement can
  be safe when text replacement still needs stronger repaint.

`DISPLAYSTRATEGY isolate` is a component-boundary strategy. It is used when
the desired display is still `OUTPUT components`, but adjacent native
components can recombine if written next to one another. The renderer writes
the component cells normally, inserts separator cells between adjacent visible
components, then applies the UTF remapping hint as a post-render span overlay.
The first calibrated use is decomposed regional flags on macOS.

`SET UTF DISPLAY NORMAL|DECOMPOSED|SINGLE|TOGGLE` chooses the preferred display
mode. Display mode is a view preference; it does not change logical text
identity. `SINGLE` forces the profile `WIDTH` to one cell, while `ADVANCE`,
`CURSOR`, and `REPAINT` remain independent physical terminal behavior. `TOGGLE`
cycles `NORMAL -> DECOMPOSED -> SINGLE -> NORMAL`.

Metrics are deliberately independent from output. `OUTPUT` controls what is
drawn; `METRICS` controls how THE maps that logical cluster onto the terminal
grid. `METRICS auto` preserves existing behavior. `METRICS profile` uses the
explicit WIDTH/ADVANCE/CURSOR/REPAINT values. `METRICS components` measures the
decomposed preview, including its separator cells. `METRICS expanded` measures
adjacent visible components without preview separators.

This separation keeps platform profiles lockable. A terminal that shapes native
clusters correctly can keep NORMAL as native/profile metrics. A terminal that
draws native clusters outside its own grid accounting can choose NORMAL
`OUTPUT sanitize` for unsafe classes instead of trying to compensate with
sacrificial spaces and redraw timing. DECOMPOSED still uses component output and
component metrics, and SINGLE still forces one-cell substitute output.

Dynamic metric modes are calculated from the actual cluster rather than from a
fixed sample width. Longer ZWJ sequences therefore do not inherit one hard-coded
family width. The component widths come from the same platform profile used for
individual emoji, variation, modifier, and regional-indicator classes.

When adding platform rules, keep the instruction order conceptual rather than
terminal-specific: choose the display mode, choose the output transform, then
choose the metric model. A platform profile should override only the part it has
measured. For example, the current conservative Apple/Linux profile keeps
regional flags and modifier sequences native with calibrated profile metrics,
sanitizes keycaps, substitutes unsafe ZWJ classes, and uses `DISPLAYSTRATEGY
isolate` for decomposed flags. This keeps new terminal workarounds from
changing logical editing behavior and leaves room to relax Linux-native
terminal profiles after measurement.

THE loads generic built-in defaults, any optional terminal identity hook, the
platform or terminal system profile, and then the user profile. Terminal
identity hooks must not hide platform policy that can be expressed in the
profile language. On macOS the platform profile is `system-osx.the`; when
TERM_PROGRAM is visible as `Apple_Terminal`, THE prefers
`system-apple-terminal.the`; on Linux the current `system-linux.the` is a
conservative profile aligned with the Apple Terminal safety rules because SSH
sessions often hide the real terminal emulator. These settings remain visible
and replaceable without rebuilding THE.

## Profile grammar

Select the active display mode with:

```text
SET UTF DISPLAY NORMAL|DECOMPOSED|SINGLE|TOGGLE
```

Configure a class in a particular mode with:

```text
SET UTF DISPLAY NORMAL|DECOMPOSED|SINGLE CLASS class [settings...]
```

The class names are `ascii`, `combining`, `combining-stack`, `wide`,
`ambiguous`, `emoji`, `text-variation`, `emoji-variation`, `modifier`, `keycap`,
`regional-indicator`, `regional-flag`, `short-zwj`, `heart-zwj`, `family-zwj`,
`tag-flag` and `private-use`. `CLASS ANY` applies the settings to every class
in the chosen mode. The implementation uses a table of
`Utf8TerminalProfileEntry` values, with an entry for each class and mode.
Later commands update the selected entries; a broad rule should precede any
class-specific overrides. Modes keep independent entries.

Supported settings include:

```text
OUTPUT NATIVE|SANITIZE|EXPANDED|COMPONENTS|BASE|SUBSTITUTE [U+nnnn]
METRICS AUTO|PROFILE|COMPONENTS|EXPANDED|OUTPUT
WIDTH n ADVANCE n CURSOR n REPAINT n
DISPLAYSTRATEGY INLINE|ISOLATE
CURSORSTRATEGY cells|line|suffix|prev|first|whole
REPLACESTRATEGY cells|line|suffix|prev|first|whole
MARK none|compressed|substituted|unsafe
```

A code point after `SUBSTITUTE` selects its replacement character.
`OUTPUT REPLACEMENT DEFAULT|BASE|U+nnnn` and `OUTPUT CHARACTERS` are accepted
aliases for replacement/component output. `METRICS FIXED` and `METRICS NATIVE`
are aliases for `PROFILE`. Accepted output and metric combinations depend on
the mode; `SINGLE` keeps a one-cell `WIDTH`. See the
[SET UTF reference](commands/SET_UTF.md) and `src/utfterm.c` for the complete
validation rules and the compatible `UTF TERMINAL CLASS` form.

Example profile rules:

```text
SET UTF DISPLAY NORMAL CLASS keycap OUTPUT SANITIZE METRICS OUTPUT WIDTH 1 ADVANCE 1 CURSOR 1 REPAINT 1
SET UTF DISPLAY NORMAL CLASS regional-flag OUTPUT NATIVE METRICS PROFILE WIDTH 2 ADVANCE 3 CURSOR 3 REPAINT 3
SET UTF DISPLAY NORMAL CLASS short-zwj OUTPUT SUBSTITUTE U+25A1 METRICS OUTPUT
SET UTF DISPLAY DECOMPOSED CLASS regional-flag OUTPUT CHARACTERS METRICS OUTPUT DISPLAYSTRATEGY ISOLATE
SET UTF DISPLAY DECOMPOSED CLASS short-zwj OUTPUT CHARACTERS METRICS OUTPUT
SET UTF DISPLAY SINGLE CLASS ANY OUTPUT REPLACEMENT DEFAULT WIDTH 1 ADVANCE 1 CURSOR 1 REPAINT 1
```

Inspect and replay canonical `EXTRACT /UTF/` output to verify the resolved
profile. Exact-sequence and structured selectors are possible future work in
the [roadmap](roadmap.md).

## Simplified Normal Policy

The Apple Terminal work has shown that THE should stop trying to make every
native emoji sequence work through redraw tricks. NORMAL mode is the stable
editing view. It may use native output where native output is reliable, and it
may use `OUTPUT SANITIZE` where the terminal's glyph rendering and grid
accounting disagree.

This simplifies the curses driver. The driver receives a resolved
rule and executes it:

```text
selector matched -> output transform -> metrics -> cursor/repaint strategy
```

The driver should not contain scattered Apple-specific tests for keycaps,
regional flags, tag flags, or ZWJ sequences. Terminal policy belongs in system
profiles such as `system-osx.the`, `system-linux.the`, and
`system-apple-terminal.the` through mode-scoped rules. If a cluster class is
safe, leave it native. If it is unsafe, use the smallest profile-visible
transform that gives stable editing behavior. DECOMPOSED and the UTF status
display provide the explanatory component view; SINGLE provides the one-cell
safety view.

`SANITIZE` does not mean "strip all Unicode features." It is a narrow
class-aware terminal policy. In the macOS profile, keycaps sanitize to their
base key, ZWJ classes substitute to stable one-cell symbols, regional flags
remain native with a calibrated three-cell advance, modifiers remain native
with calibrated profile metrics, and ordinary wide/CJK characters, combining
letters, and simple emoji stay native through the generic defaults.

## Width Model

The simplified policy reduces the number of cases that need strange physical
metrics, but it does not remove the four width fields.

Keep:

```text
WIDTH n ADVANCE n CURSOR n REPAINT n
```

Their roles remain different:

- `WIDTH`: user-visible width exposed to metadata and logical display
  calculations.
- `ADVANCE`: physical cells used to place following terminal output.
- `CURSOR`: cells covered by cursor/background presentation.
- `REPAINT`: cells cleared or repainted to remove stale glyph fragments.

For sanitized Apple NORMAL rules, these will usually collapse to the same
boring values, often `1/1/1/1` or `2/2/2/2`. That is a good outcome. The fields
are still needed for:

- generic native wide/CJK and emoji defaults, where two-cell output is normal.
- terminals that correctly support native grapheme output but need explicit
  profile widths.
- DECOMPOSED output, whose width is calculated from the emitted component view.
- future platforms where cursor or repaint behavior differs from advance even
  though Apple uses sanitize.

The implementation should therefore keep the model but make the common path
boring. `OUTPUT SANITIZE` should default `METRICS OUTPUT`, so width values come
from the sanitized representation unless a profile explicitly overrides them.

Profiles should still have an explicit width-setting instruction:

```text
SET UTF DISPLAY <mode> CLASS <selector> WIDTH n ADVANCE n CURSOR n REPAINT n
```

or the same width parameters appended to an `OUTPUT` rule. This instruction is
expected to be rare in the simplified model. Use it when a terminal/platform has
a real physical exception that cannot be represented by the output method and
`METRICS OUTPUT`; do not use it as the default way to describe sanitized Apple
classes.

## Settings Query and Round Trip

The architecture is testable without a terminal probe. `SET UTF DISPLAY` and
all mode-scoped display rules have a canonical query form so tests can set a
profile, query it back, and compare the effective rules.

The existing `EXTRACT /UTF/` surface exposes the query data rather than adding a
separate probe-only mechanism:

```text
utf.1 = ON|OFF
utf.2 = DISPLAY NORMAL|DECOMPOSED|SINGLE
utf.3 = <number of effective display rules>
utf.4... = canonical SET UTF DISPLAY rule strings
```

Canonical rule strings should be accepted by the command parser unchanged. A
round-trip test should be able to:

```text
SET UTF DISPLAY NORMAL
SET UTF DISPLAY NORMAL CLASS keycap OUTPUT SANITIZE KEYCAP WIDTH 1 ADVANCE 1 CURSOR 1 REPAINT 1
EXTRACT /UTF/
```

and then replay the returned `SET UTF DISPLAY ... CLASS ...` lines into a fresh
profile to get the same effective table.

The LLM driver exposes the same effective UTF display profile through `debug
utf-display`, and includes the active display mode in `capabilities`. This makes
automated tests possible without curses:

```text
capabilities
command set utf display decomposed
debug utf-display
look filearea compact max=120
```

Snapshots expose row-level UTF annotations driven by the resolved display rule:
active mode, selector/class, configured and resolved output method, metrics
source, mark, `WIDTH`, `ADVANCE`, `CURSOR`, `REPAINT`, and the text actually
emitted for sanitized/decomposed/single views where that is safe to serialize.

## RGI Recognition Hypothesis

Current Apple Terminal symptoms may be explained by Recommended for General
Interchange emoji recognition. Apple may sometimes recognize a cluster as a
single emoji sequence from the RGI list and sometimes fall back to drawing the
component code points. A local redraw can then change the apparent width or
shape even though THE wrote the same bytes.

This hypothesis fits several observed symptoms:

- Keycaps, paired regional flags, tag flags, emoji modifiers, and ZWJ sequences
  are all sequence-shaped emoji features where RGI recognition matters.
- Some clusters look stable on initial paint but change after cursor movement or
  partial repaint, which suggests composition state in the terminal rather than
  a changed logical cluster.
- The two-face ZWJ behaving differently from short and larger family ZWJs may
  mean one sequence is recognized as RGI while another is drawn as components,
  or that the terminal switches between those states after a redraw.
- Different cursor and replacement strategies can be needed because repainting
  part of a recognized sequence may damage or decompose the composed glyph,
  while repainting a fallback component run has different physical behavior.

This opens more selector classes. We should not only distinguish `short-zwj`,
`heart-zwj`, and `family-zwj`; we may need selectors such as:

```text
CLASS rgi
CLASS non-rgi
CLASS rgi-zwj
CLASS non-rgi-zwj
CLASS rgi-keycap
CLASS rgi-regional-flag
CLASS rgi-tag-flag
CLASS emoji-modifier
```

Do not rush a large Unicode table into the driver. Profile rules can use exact
sequence or narrow selector matches when a broad class proves wrong. If
automatic RGI classification becomes necessary, it should come from generated
Unicode emoji data rather than hand-maintained C conditionals.

If more evidence is needed before a profile rule is locked down, use temporary
manual experiments or LLM-driver-visible render metadata. The old probe
executables should not remain part of the architecture. Useful sample families
for any temporary experiment are:

- valid RGI keycap sequences and nearby keycap-like non-RGI sequences.
- valid country regional flags and unsupported regional-indicator pairs.
- RGI tag flags such as England/Scotland/Wales and non-RGI tag sequences.
- known RGI ZWJ sequences and syntactically similar non-RGI ZWJ sequences.
- emoji modifier sequences and unsupported modifier-like pairs.
- text-presentation and emoji-presentation variation sequences.

For each sample, inspect all three display modes and the repaint strategies that
matter to the curses driver:

```text
initial native paint
same-cell repaint
cursor enter from left and right
cursor leave to left and right
suffix clear and repaint
whole-line clear and repaint
replacement/overlay repaint
```

Curses cannot directly ask the terminal whether a sequence was recognized as
RGI. The useful output of any temporary experiment is a profile rule or a
selector-specific observation, not a claim about Unicode semantics.

## Editing behavior

Literal and deterministic code-point entry store UTF-8 without normalization.
The main file-area cursor, selection, block, shift, case, overlay and SOS
mutation paths use logical clusters. Delete/Backspace commit their edited
record before cursor movement or refresh, so subsequent snapshots and SAVE
use the same text. Terminal profile widths do not change the deletion unit.
See [test coverage](testing.md) for the regression routes.

## Apple Terminal rendering policy

- Apple Terminal can retain composition state across local repaint. Repainting
  only the cell after a keycap can visually damage the keycap or the following
  text even when logical cursor state is correct.
- Native keycap glyphs were unreliable enough that the macOS profile now uses
  `OUTPUT SANITIZE`, `MARK compressed`, and one-cell physical widths for
  keycaps. The stored text remains the original keycap sequence.
- Regional flags are separate from keycaps. The working macOS baseline keeps
  them native in NORMAL mode with a two-cell `WIDTH` and three-cell
  `ADVANCE`/`CURSOR`/`REPAINT`. In DECOMPOSED mode, `DISPLAYSTRATEGY isolate`
  reproduces the status-line component spacing and then applies the UTF
  remapping hint over the whole rendered component span.
- Emoji modifier sequences are native on the current macOS baseline, but use
  calibrated profile metrics because Apple Terminal's terminal-cell accounting
  does not match the simple two-cell default.
- ZWJ sequences must remain one logical grapheme cluster whether NORMAL renders
  a substitute view, DECOMPOSED renders the status-style component preview, or
  SINGLE uses a substitute. Native ZWJ output is an optional platform policy,
  not the definition of NORMAL mode.
- A successful cursor-walk probe does not prove replacement behavior. Cursor
  and replacement strategies must be calibrated independently.

## Validation

Use canonical `EXTRACT /UTF/` output, `debug utf-display` and row-level LLM
metadata to inspect and replay profile rules. The metadata includes logical
width, physical width/advance/cursor/repaint, class, mode, output transform and
resolved safe text. Terminal cursor-position reports measure grid accounting;
they do not establish whether a composed glyph looks correct.

```sh
ctest --test-dir cmake-build-debug \
  -R 'test_utfterm|test_utflayout|test_llmdriver|test_llmruntime|test_the_llm_full_runtime|test_delete_persistence_' \
  --output-on-failure
```

Calibrate cursor entry/exit, suffix/line repaint and replacement independently.
A successful cursor walk does not prove replacement behavior. Use temporary
visual experiments when needed, then express a proven correction in a profile
rule or physical driver strategy. Probe executables are not part of the product
architecture. Physical terminal checks remain necessary alongside automated
logical-state and saved-byte assertions.

## Guardrails

Before closing a UTF slice:

- Show the behavior through focused unit tests or `the --driver llm`.
- Keep logical behavior independent of terminal profile widths.
- Keep terminal mechanics inside the physical driver or profile layer.
- Run `git diff --check`.
- Run the relevant UTF and LLM CTest slice.
- Use canonical `EXTRACT /UTF/` output and LLM-driver diagnostics for
  terminal-profile tests. Temporary manual terminal experiments are evidence
  only; do not make probe executables part of the architecture.
