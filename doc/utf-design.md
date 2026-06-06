# UTF-8 Design and Status

Last updated: 2026-06-06.

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
class, display mode, output method, metric method, mark,
WIDTH, ADVANCE, CURSOR, REPAINT,
CURSORSTRATEGY, REPLACESTRATEGY
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
- `CURSORSTRATEGY` and `REPLACESTRATEGY`: separate because cursor movement can
  be safe when text replacement still needs stronger repaint.

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

When adding probe results, keep the instruction order conceptual rather than
terminal-specific: choose the display mode, choose the output transform, then
choose the metric model. A platform profile should override only the part it has
measured. For example, macOS can set NORMAL keycap, regional-flag, ZWJ, or tag
classes to `OUTPUT sanitize` with metrics based on the sanitized output, while
another terminal leaves those classes at native/profile metrics or uses explicit
two-cell profile widths. This keeps new Apple workarounds from changing
platforms whose behavior has already been calibrated.

THE loads generic built-in defaults, any optional terminal identity hook, the
platform system profile, and then the user profile. Terminal identity hooks must
not hide platform policy that can be expressed in the profile language. On macOS
the generated platform profile is `system-osx.the`, so Apple-specific settings
remain visible and replaceable without rebuilding THE.

## Next Profile Grammar

The current profile grammar is useful but confusing because display mode,
cluster selection, output transform, metrics, and repair strategy can appear as
separate commands that look equally important. The next revision should make the
concern order explicit and mode-scoped.

Keep the global mode command:

```text
SET UTF DISPLAY NORMAL|DECOMPOSED|SINGLE|TOGGLE
```

Use a separate rule command whose first operand is the display mode being
configured:

```text
SET UTF DISPLAY NORMAL     CLASS selector OUTPUT method [parameters...]
SET UTF DISPLAY DECOMPOSED CLASS selector OUTPUT method [parameters...]
SET UTF DISPLAY SINGLE     CLASS selector OUTPUT method [parameters...]
```

This gives every rule the same shape:

```text
mode -> selector -> output plan -> physical parameters
```

The implementation should mirror that shape:

```c
typedef struct UtfDisplayRule {
   UtfDisplayMode mode;
   UtfClusterSelector selector;
   UtfOutputPlan output;
   UtfMetricPlan metrics;
   UtfRepairPlan repair;
   UtfDisplayMark mark;
} UtfDisplayRule;
```

Display modes must be independent namespaces. Lookup should search only the
active mode, in this order:

```text
exact sequence -> predicate selector -> class selector -> ANY/default
```

Avoid implicit fallback from `DECOMPOSED` or `SINGLE` to `NORMAL`; if a mode
needs a default, it should have an explicit `CLASS ANY` rule in that mode. This
prevents NORMAL platform policy from leaking into SINGLE replacement display or
DECOMPOSED component display.

Selectors should start with the current class names and then grow into
structured predicates when class-level policy is too coarse:

```text
CLASS keycap
CLASS regional-flag
CLASS zwj
CLASS zwj COMPONENTS 2
CLASS zwj COMPONENTS *
CLASS rgi
CLASS rgi-zwj
CLASS non-rgi-zwj
SEQUENCE U+1F469 U+200D U+1F4BB
CLASS ANY
```

The selector is where cluster searching belongs. It can match broad class,
exact sequence, component count, ZWJ presence, variation selector presence,
RGI/non-RGI status, or other cluster facts. The renderer should receive the
chosen rule, not re-decide selector policy.

Output methods should be reduced to four concepts:

```text
OUTPUT NATIVE
OUTPUT SANITIZE [sanitize-policy parameters...]
OUTPUT REPLACEMENT DEFAULT|BASE|U+nnnn
OUTPUT CHARACTERS [character-output parameters...]
```

`NATIVE` writes the stored cluster. `SANITIZE` writes a safe representation of
the stored cluster for the active profile without changing the file bytes or
logical cluster identity. `REPLACEMENT` writes a substitute or a class-specific
base form. `CHARACTERS` writes a controlled component view such as
decomposed/status display. Existing names such as `base`, `substitute`,
`components`, and `expanded` can map onto this model during migration, but the
new design should not treat them as separate display modes.

`SANITIZE` is the preferred NORMAL-mode escape hatch for terminals whose native
emoji shaping and grid accounting disagree. It should be class-aware rather
than a global strip pass. Typical macOS policies are:

```text
keycap        -> base ASCII key (#, *, 0-9)
regional-flag -> stable region letters or another fixed two-cell label
tag-flag      -> stable tag label or class replacement
zwj           -> safe components with ZWJ/variation mechanics removed, or a
                 class replacement when component output would be misleading
variation     -> profile-selected text/base presentation
modifier      -> base emoji or class replacement
```

When NORMAL uses `SANITIZE`, its default metrics should come from the sanitized
output. The aim is to make `WIDTH`, `ADVANCE`, `CURSOR`, and `REPAINT` agree as
often as the chosen replacement allows, so normal editing does not depend on
unobservable renderer overhang. Reverse video is not implied by `SANITIZE`.

When `DECOMPOSED` writes a cluster character by character, each emitted
component should itself be rendered through the appropriate simple/single rule.
That keeps troublesome component code points, such as presentation selectors,
joiners, tag characters, keycap marks, or other zero-width mechanics, from
being reintroduced by the decomposed view. Decomposition is therefore not just
raw code point output; it is a mode-specific character view with per-component
policy.

Visual highlighting is also mode-specific. Reverse video for replacements or
component characters is a way to show UTF display facts in `DECOMPOSED` and
`SINGLE` modes. It should not be an automatic consequence of using replacement
output in `NORMAL` mode. Normal mode may need replacement output for terminal
safety, but it should remain visually natural unless a separate mark explicitly
asks for visible annotation.

Everything else is an output parameter:

```text
WIDTH n ADVANCE n CURSOR n REPAINT n
METRICS FIXED|OUTPUT|NATIVE
CURSORSTRATEGY cells|line|suffix|prev|first|whole
REPLACESTRATEGY cells|line|suffix|prev|first|whole
MARK none|compressed|substituted|unsafe
```

Example profile policy:

```text
SET UTF DISPLAY NORMAL CLASS regional-flag OUTPUT SANITIZE REGION WIDTH 2 ADVANCE 2 CURSOR 2 REPAINT 2
SET UTF DISPLAY NORMAL CLASS zwj OUTPUT SANITIZE COMPONENTS METRICS OUTPUT
SET UTF DISPLAY DECOMPOSED CLASS zwj OUTPUT CHARACTERS METRICS OUTPUT
SET UTF DISPLAY SINGLE CLASS ANY OUTPUT REPLACEMENT DEFAULT WIDTH 1 ADVANCE 1 CURSOR 1 REPAINT 1
```

The important separation is that `NORMAL`, `DECOMPOSED`, and `SINGLE` are not
variants of one entry. They are separate policies that happen to match the same
logical cluster.

## Simplified Normal Policy

The Apple Terminal work has shown that THE should stop trying to make every
native emoji sequence work through redraw tricks. NORMAL mode is the stable
editing view. It may use native output where native output is reliable, and it
may use `OUTPUT SANITIZE` where the terminal's glyph rendering and grid
accounting disagree.

This should simplify the curses driver. The driver should receive a resolved
rule and execute it:

```text
selector matched -> output transform -> metrics -> cursor/repaint strategy
```

The driver should not contain scattered Apple-specific tests for keycaps,
regional flags, tag flags, or ZWJ sequences. Apple policy belongs in
`system-osx.the` through mode-scoped rules. If a cluster class is safe, leave it
native. If it is unsafe, sanitize that class and stop there. DECOMPOSED and the
UTF status display provide the explanatory component view; SINGLE provides the
one-cell safety view.

`SANITIZE` does not mean "strip all Unicode features." It is a narrow
class-aware terminal policy. For example, the macOS profile may sanitize
keycaps, regional flags, tag flags, and selected ZWJ classes while leaving
ordinary wide/CJK characters, combining letters, and simple emoji native.

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

## Current Status

Implemented:

- UTF-8 and wide-character support are on by default.
- Logical positions use UTF-8 bytes, code points, grapheme cluster indexes, and
  logical cell columns through `TextPos`.
- `utf8proc` supplies maintained grapheme breaks and character widths when
  `USE_UTF8PROC=ON`.
- Cluster classification is shared in `src/utfcluster.c`.
- Terminal profiles support mode-scoped `SET UTF DISPLAY <mode> CLASS ...`
  rules with `WIDTH`, `ADVANCE`, `CURSOR`, `REPAINT`, output transforms,
  class-aware `OUTPUT SANITIZE`, marks, cursor strategies, and replacement
  strategies.
- Apple Terminal overrides are represented as profile policy, not renderer
  special cases or compiled fallback tables.
- Rendering carries logical width plus profile width, advance, cursor, repaint,
  configured output method, resolved output method, display mode, class, and
  mark through `TheRenderCluster`.
- `EXTRACT /UTF/` and `debug utf-display` expose UTF support, active display
  mode, and canonical replayable display rules.
- The LLM driver exposes row-level UTF annotations with logical width, `WIDTH`,
  `ADVANCE`, `CURSOR`, `REPAINT`, display mode, class/selector, configured and
  resolved output, metrics source, mark, safe emitted display text, and
  compressed/substituted flags.
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
  `OUTPUT SANITIZE`, `MARK compressed`, and one-cell physical widths for
  keycaps. The stored text remains the original keycap sequence.
- Regional flags are separate from keycaps, but Apple Terminal shows a similar
  native overhang/closer problem in no-curses probes. NORMAL can use
  `OUTPUT SANITIZE` for these classes rather than preserving native glyphs with
  sacrificial physical cells.
- ZWJ sequences must remain one logical grapheme cluster whether NORMAL renders
  a sanitized view, DECOMPOSED renders the status-style component preview, or
  SINGLE uses a substitute. Native ZWJ output is an optional platform policy,
  not the definition of NORMAL mode.
- A successful cursor-walk probe does not prove replacement behavior. Cursor
  and replacement strategies must be calibrated independently.

## Probe Retirement

The probe executables were useful to discover Apple Terminal behavior, but they
do not define the architecture. They have been decommissioned now that profile
grammar and LLM-driver diagnostics express the same policy.

Keep these lessons from the probes:

- Keycap and regional-flag samples report a two-cell cluster and a three-cell
  `cluster+B` span, but the following `B` is visibly drawn one cell too early.
  Adding one sacrificial space before the real `B` fixes the visible output
  while merely increasing the raw span by one. Treat this as evidence for an
  Apple-specific extra advance/repaint cell, not as a different Unicode width.
- ZWJ and tag-flag samples can look visually composed while CPR/DSR reports
  component-like advances such as 5, 7, 8, or 11 cells. That means a successful
  visual glyph and the terminal's reported cursor position are separate facts.
- Cursor-position reports cannot be used as an oracle for visual correctness.
  They are still useful for terminal grid accounting, but manual visual reports
  or screenshot-based probes are needed to detect overhang, overlap, and
  composed-glyph success.

Do not spend more implementation time chasing Apple Terminal native redraw
behavior. Convert proven unsafe Apple classes to profile-visible
`OUTPUT SANITIZE` rules, and use `EXTRACT /UTF/`, `debug utf-display`, and
LLM-driver snapshots for automated testing.

## Validation Snapshot

Focused validation for this architecture uses the query and LLM diagnostic path
instead of probe executables:

```sh
cmake --build cmake-build-debug --target test_utfterm test_utflayout test_llmdriver test_llmruntime the
ctest --test-dir cmake-build-debug \
  -R 'test_utfterm|test_utflayout|test_llmdriver|test_llmruntime|test_the_llm_full_runtime' \
  --output-on-failure
```

A manual `the --driver llm` probe confirmed:

- capabilities report `driver=llm`, `curses=false`, full THE dispatcher,
  CREXX available in this build, syntax/style spans, parser diagnostics, and
  the supported protocol verbs.
- UTF row annotations report CJK as `wide/native` with width metadata.
- macOS profile policy reports keycaps as `keycap/sanitize/resolved=base`
  with one-cell physical metadata.
- The normalized `text`/`type` protocol path still feeds bytes to
  `process_key()`, so UTF text entry remains an outstanding item.

## Outstanding Items

1. UTF display profile replay hardening.
   Keep expanding round-trip tests that set mode-scoped display rules, extract
   canonical `EXTRACT /UTF/` output, replay it into a fresh profile, and verify
   the same LLM-visible resolved rule facts.

2. Windows and Linux validation.
   Prove runtime module loading, wide curses/PDCurses behavior, UTF profiles,
   and `the --driver llm` on Linux and native Windows. Add system profiles only
   after profile/LLM evidence exists for those terminals.

3. Probe decommissioning.
   Probe executables, CMake targets, and generated-profile probe tests are
   retired. Keep new profile-policy tests on `EXTRACT /UTF/`, `debug
   utf-display`, and LLM render metadata.

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

## Next Session Prompt

Use this prompt to start the implementation slice:

```text
We are working in THE on the UTF display architecture. Read doc/utf-design.md
and doc/commands/SET_UTF.md first.

Implement the new mode-scoped UTF display profile model:

1. Keep SET UTF DISPLAY NORMAL|DECOMPOSED|SINGLE|TOGGLE as the global active
   display mode. TOGGLE must cycle NORMAL -> DECOMPOSED -> SINGLE -> NORMAL.
2. Add the rule grammar:
   SET UTF DISPLAY NORMAL|DECOMPOSED|SINGLE CLASS selector OUTPUT method [parameters...]
   The rule lookup namespace is per mode: exact/predicate selectors first,
   class selector next, explicit CLASS ANY/default last. Do not fall back from
   DECOMPOSED or SINGLE to NORMAL.
3. Add OUTPUT SANITIZE as a first-class output method. It is mainly for NORMAL
   mode and should be class-aware, not a global Unicode strip pass. Sanitize
   only proven unsafe classes. On macOS, start with keycap, regional-flag,
   tag-flag, and ZWJ classes that are unsafe in Apple Terminal. Leave safe
   classes native.
4. Keep WIDTH, ADVANCE, CURSOR, and REPAINT as independent fields, but make
   sanitized NORMAL rules default to METRICS OUTPUT so common Apple cases become
   1/1/1/1 or 2/2/2/2 rather than bespoke redraw hacks.
5. Make the curses driver execute resolved profile rules. Remove scattered
   Apple-specific C rules where the profile can express the behavior. Apple
   policy belongs in system-osx.the.
6. Extend EXTRACT /UTF/ so tests can query active UTF support, active display
   mode, and canonical SET UTF DISPLAY ... CLASS ... rules that can be replayed
   into a fresh profile.
7. Add an LLM protocol diagnostic, preferably debug utf-display, and include
   active UTF display mode in capabilities. The diagnostic should expose the
   effective mode-scoped rules in machine-readable form for automated tests.
8. Update LLM UTF annotations so snapshots report the resolved rule facts:
   active mode, class/selector, output method, mark, WIDTH, ADVANCE, CURSOR,
   REPAINT, and emitted display text where safe.
9. Update tests to drive SET UTF DISPLAY through the real command dispatcher and
   validate round-tripping through EXTRACT /UTF/ and debug utf-display. Prefer
   test_utfterm, test_utflayout, test_llmdriver, test_llmruntime, and
   test_the_llm_full_runtime over terminal probes.
10. Decommission probe executables and related generated-profile tests after
    the query/LLM path covers the same profile policy. Do not spend more time
    fixing Apple Terminal native redraw behavior; use NORMAL sanitize for unsafe
    classes, DECOMPOSED/status for explanation, and SINGLE for one-cell safety.

Run git diff --check and the focused UTF/LLM test slice before reporting.
```

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
