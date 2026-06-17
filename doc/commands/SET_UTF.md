# SET UTF
**configures physical UTF terminal behaviour**

## Syntax
```text
[SET] UTF DISPLAY NORMAL|DECOMPOSED|SINGLE|TOGGLE
[SET] UTF DISPLAY NORMAL|DECOMPOSED|SINGLE CLASS selector OUTPUT method [parameters...]
[SET] UTF TERMINAL CLASS class [DISPLAY display] WIDTH width ADVANCE advance CURSOR cursor REPAINT repaint
[SET] UTF TERMINAL CLASS class [DISPLAY display] WIDTH width
[SET] UTF TERMINAL CLASS class [DISPLAY display] ADVANCE advance
[SET] UTF TERMINAL CLASS class [DISPLAY display] CURSOR cursor
[SET] UTF TERMINAL CLASS class [DISPLAY display] REPAINT repaint
[SET] UTF TERMINAL CLASS class [DISPLAY display] OUTPUT method [U+codepoint]
[SET] UTF TERMINAL CLASS class [DISPLAY display] METRICS method
[SET] UTF TERMINAL CLASS class [DISPLAY display] MARK mark
[SET] UTF TERMINAL CLASS class [DISPLAY display] DISPLAYSTRATEGY strategy
[SET] UTF TERMINAL CLASS class [DISPLAY display] CURSORSTRATEGY strategy
[SET] UTF TERMINAL CLASS class [DISPLAY display] REPLACESTRATEGY strategy
```

## Description
The SET UTF command configures the physical terminal profile used by UTF-8 rendering.
These settings describe how the terminal displays, covers, clears, and repaints UTF-8
grapheme clusters. They do not change the bytes stored in the file, the internal logical
UTF-8 text model, or how editing commands such as CHANGE, CINSERT, COVERLAY, CREPLACE,
and REPLACE select or replace text. Editing commands continue to operate on logical UTF-8
cluster positions; SET UTF only adjusts the physical terminal driver profile used to draw
those clusters.

The UTF DISPLAY form selects the global display mode THE prefers when rendering
UTF clusters. It does not affect the logical text model.

- NORMAL uses each class's normal profile. This is the default and generally
  provides a stable readable editing display. It writes stored UTF-8 clusters
  natively where the terminal profile trusts native output, and may sanitize
  unsafe classes where native terminal rendering is unreliable.
- DECOMPOSED uses the decomposed profile. For classes such as ZWJ sequences
  this usually means the same visible component preview used by the UTF status
  field: joiners and tag characters are suppressed, standalone variation
  selectors are shown as markers, and keycaps use a safe outline preview.
- SINGLE uses the single-cell profile. The profile `WIDTH` is always one cell;
  `ADVANCE`, `CURSOR`, and `REPAINT` remain independent terminal facts.
- TOGGLE cycles through NORMAL, DECOMPOSED, SINGLE, and back to NORMAL.

For example, a ZWJ sequence such as a family emoji may have a NORMAL profile
that sanitizes the cluster for a stable terminal grid and a DECOMPOSED profile
that writes its visible parts. SET UTF DISPLAY selects the active mode.

The target rule grammar is mode-scoped:

```text
SET UTF DISPLAY NORMAL CLASS keycap OUTPUT SANITIZE KEYCAP WIDTH 1 ADVANCE 1 CURSOR 1 REPAINT 1
SET UTF DISPLAY DECOMPOSED CLASS keycap OUTPUT CHARACTERS METRICS OUTPUT
SET UTF DISPLAY DECOMPOSED CLASS regional-flag OUTPUT COMPONENTS DISPLAYSTRATEGY ISOLATE
SET UTF DISPLAY SINGLE CLASS ANY OUTPUT REPLACEMENT DEFAULT WIDTH 1 ADVANCE 1 CURSOR 1 REPAINT 1
```

Each mode is an independent namespace. A missing DECOMPOSED or SINGLE rule must
be supplied by an explicit rule for that mode, such as `CLASS ANY`; it should
not inherit NORMAL platform policy implicitly.

The CLASS operand identifies the UTF-8 feature class to configure. Supported classes are:

- ascii
- combining
- combining-stack
- wide
- ambiguous
- emoji
- text-variation
- emoji-variation
- modifier
- keycap
- regional-indicator
- regional-flag
- short-zwj
- heart-zwj
- family-zwj
- tag-flag
- private-use

The optional DISPLAY operand in the TERMINAL CLASS form selects which physical
profile entry is being configured for that class. The normal display is used
when DISPLAY is omitted. Supported display entries are `normal`, `decomposed`,
and `single`. The current default table provides all three entries for every
UTF terminal class.

WIDTH specifies the human/user-visible width reported for the cluster. ADVANCE
specifies the terminal grid advance used to place the next rendered cluster.
CURSOR specifies the number of terminal cells the software cursor or cursor
background must cover. REPAINT specifies the cleanup footprint to blank or
repaint when stale glyph fragments may remain. These four values are recorded
independently because user-visible width, terminal advance, cursor presentation,
and repaint cleanup can differ on real terminals. For example, Apple Terminal
or another terminal may need an explicit width override for a native cluster
that is left unsanitized. The regional-indicator class represents a single
Regional Indicator codepoint; regional-flag represents the normal two-codepoint
flag sequence, so terminal profiles can describe decomposed flag components
separately from normal pairs.

The mode-scoped display grammar should also accept explicit width-setting rules
when a platform really needs them:

```text
SET UTF DISPLAY NORMAL CLASS regional-flag WIDTH 2 ADVANCE 3 CURSOR 3 REPAINT 3
```

These rules are expected to be rare after NORMAL sanitize is available. Prefer
`OUTPUT SANITIZE ... METRICS OUTPUT` for unsafe Apple classes, and use explicit
widths only for real physical exceptions.

METRICS specifies how effective physical metrics are calculated. Supported
methods are:

- auto - use the default for the output method
- profile - use the WIDTH/ADVANCE/CURSOR/REPAINT values directly
- components - measure the decomposed component preview that THE emits
- expanded - measure adjacent visible components without decomposed-preview
  separator cells

`auto` preserves the normal contract: native output uses profile metrics, while
components/expanded/sanitize output uses dynamic output metrics where possible.

DISPLAYSTRATEGY controls how transformed output is lowered after OUTPUT has
selected the visible representation. Supported strategies are:

- inline - emit the transformed output in the normal component sequence
- isolate - emit visible components as separate cells, insert separator cells
  between adjacent visible components, then apply the UTF remapping hint across
  the whole rendered span

`isolate` is intended for classes whose native visible components can recombine
when adjacent. Regional flags are the first calibrated use: decomposed
regional indicators are written normally with one separator cell between them,
and reverse video is applied afterward as a span overlay so the glyph sequence
stays stable.
A platform profile can override this without changing what is drawn. For
example, Apple Terminal should normally use `OUTPUT SANITIZE` for unsafe native
classes rather than preserving native ZWJ/keycap/flag output with bespoke
redraw metrics. Another platform can leave the rule native, or set
`METRICS profile` and explicit widths, if its terminal shapes the native
cluster as a stable two-cell glyph.

Profile authors should treat display, output, and metrics as separate decisions:
first select the display entry (`normal`, `decomposed`, or `single`), then select
what THE writes (`OUTPUT`), then select how that output occupies the terminal
grid (`METRICS` plus any explicit WIDTH/ADVANCE/CURSOR/REPAINT values). Platform
quirks should be scoped to the platform system profile, not copied into common
defaults. This lets a locked-down platform keep a calibrated native two-cell ZWJ
rule while macOS uses `OUTPUT SANITIZE` for the same unsafe class.

OUTPUT specifies how the class is written to the terminal. Supported methods are:

- native - write the stored UTF-8 sequence
- sanitize - write a class-aware safe representation without changing stored
  bytes or logical cluster identity
- expanded - write visible component characters through the expanded path
- components - write the class-aware component preview used by decomposed
  display
- base - write a class-specific stable base form
- substitute - write a substitute display character

When method is substitute, an optional Unicode codepoint may follow the method.
The codepoint is scoped to that class and display. Substitute output can be used
for any class, not only ZWJ profiles. For example:

```text
SET UTF TERMINAL CLASS short-zwj DISPLAY normal OUTPUT substitute U+0040
SET UTF TERMINAL CLASS keycap OUTPUT substitute U+25A1
SET UTF TERMINAL CLASS regional-indicator OUTPUT substitute U+25A1
SET UTF TERMINAL CLASS regional-flag OUTPUT substitute U+25A1
```

Base output is a general physical-display transform, not an Apple-specific
command. Current base mappings include keycaps to their ASCII digit, `#`, or
`*`; regional flags to their ASCII region letters; and variation/modifier
clusters to their unmodified base codepoint where safe. Apple Terminal uses
this general capability for keycaps because its native keycap glyph rendering
can damage visible cursor/repaint state.

Sanitize output is the preferred NORMAL-mode policy for unsafe terminal
classes. It should be narrower than a global Unicode-stripping rule. A platform
profile should sanitize only classes that are proven unsafe for that platform,
such as Apple Terminal keycaps, regional flags, tag flags, or selected ZWJ
classes. Safe classes should remain native.

Components output is the class-aware form of decomposed display. Expanded output
uses the same visible-component transform through the expanded path. The
component preview suppresses U+200D joiners and tag characters, suppresses
FE0E/FE0F inside ZWJ clusters, shows standalone FE0E/FE0F as `T`/`E`, and
renders keycaps as base character, blank, and a safe outline marker. Normal
profiles may also use components as an explicit fallback when a terminal cannot
reliably shape native clusters. For single profiles, expanded/component
requests are treated as substitute output.
In DECOMPOSED and SINGLE modes, replacement-style output may be shown with
reverse video so transformed cells are visible as UTF display facts. NORMAL
sanitize/replacement output should not be reversed automatically; NORMAL is the
stable editing view.

MARK records a visual hint for transformed clusters. Supported marks are:

- `none`: no special marker.
- `compressed`: the physical display is intentionally shorter or simpler than
  the stored cluster.
- `substituted`: the physical display is a substitute character.
- `unsafe`: the class is known to be unreliable on the current terminal.

MARK is physical render metadata only. It does not change the stored text,
logical cursor position, or command semantics. For example:

```text
SET UTF TERMINAL CLASS keycap OUTPUT base
SET UTF TERMINAL CLASS keycap MARK compressed
SET UTF TERMINAL CLASS keycap WIDTH 1 ADVANCE 1 CURSOR 1 REPAINT 1
SET UTF TERMINAL CLASS regional-indicator WIDTH 2 ADVANCE 2 CURSOR 2 REPAINT 2
```

CURSORSTRATEGY specifies the repaint strategy used when the cursor moves across the class.
REPLACESTRATEGY specifies the repaint strategy used after text replacement or overlay.
These are separate because a terminal can handle cursor movement correctly while still
needing a stronger repaint after replacement.

Supported strategies are:

- `cells`: repaint only the changed cells.
- `line`: repaint the visible line/run without a pre-clear.
- `suffix`: clear from the changed cluster to the end, flush, then repaint.
- `prev`: clear from one cluster before the changed cluster, flush, then repaint.
- `first`: clear from the first matching cluster in the visible run, flush, then repaint.
- `whole`: clear the whole visible line/run, flush, then repaint.

At startup, THE loads the built-in UTF terminal defaults and then applies the
file named by the THE_UTF_TERMINAL_PROFILE environment variable if it is set.
During normal profile processing, THE also runs a system profile before the
user profile. The default profile is `system-<platform>.the` in THE_HOME_DIR,
for example `system-linux.the` or `system-osx.the`. If TERM_PROGRAM is visible
as `Apple_Terminal`, THE prefers `system-apple-terminal.the` when that file is
present. This is best-effort only: SSH sessions often expose only a generic
TERM such as `xterm-256color`, so THE cannot always infer the terminal
emulator. Platform and terminal quirks, including Apple Terminal behaviour,
should be visible as profile settings rather than hidden in compiled fallback
tables.

The user profile runs after the system profile, so user settings can override
platform calibration deliberately. The `-n` command-line switch skips only the
user profile; the system profile still runs.

A UTF terminal profile file contains the same SET UTF commands documented
here. REXX-style profile fragments are accepted, so `/* ... */` comments,
`address the`, and quoted instructions such as
`'SET UTF TERMINAL CLASS keycap OUTPUT base'` may be used. Blank lines
and lines beginning with `*` or `#` are ignored.

The historical terminal-profile generation path has been retired. The system
profile should be readable and editable directly, and automated tests verify
effective settings through query/LLM diagnostics rather than through a terminal
probe. The older `LAYOUT` and `PAINT` syntax is intentionally not accepted, to
avoid mixing the previous three-width model with the current four-width model.

## Query And Round-Trip Testing

`EXTRACT /UTF/` reports whether UTF support is built in, the active display
mode, and the canonical replayable display rules so profiles can be tested
without a terminal:

```text
utf.1 = ON|OFF
utf.2 = DISPLAY NORMAL|DECOMPOSED|SINGLE
utf.3 = <number of effective display rules>
utf.4... = canonical SET UTF DISPLAY rule strings
```

The canonical rule strings must be accepted by SET UTF unchanged. A test should
be able to set UTF display rules, extract them, replay the returned rules into a
fresh profile, and observe the same effective UTF display table.

The LLM driver exposes the same settings through `debug utf-display` and
includes the active UTF display mode in `capabilities`, so automated tests can
drive `command set utf display ...` through the real command dispatcher and
then validate the effective settings without curses.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## Default
SET UTF DISPLAY NORMAL.

The built-in table is deliberately generic: it describes a sane fixed-width
curses environment before platform calibration is applied. Normal display
defaults are:

| class | output | metrics | W/A/C/R | cursor | replace |
| --- | --- | --- | --- | --- | --- |
| ascii | native | auto | 1/1/1/1 | cells | cells |
| combining | native | auto | 1/1/1/1 | cells | cells |
| combining-stack | native | auto | 1/1/1/1 | cells | cells |
| wide | native | auto | 2/2/2/2 | cells | cells |
| ambiguous | native | auto | 1/1/1/1 | cells | cells |
| emoji | native | auto | 2/2/2/2 | cells | cells |
| text-variation | native | auto | 1/1/1/1 | cells | cells |
| emoji-variation | native | auto | 2/2/2/2 | cells | cells |
| modifier | native | auto | 2/2/2/2 | cells | cells |
| keycap | native | auto | 2/2/2/2 | cells | cells |
| regional-indicator | native | auto | 2/2/2/2 | cells | cells |
| regional-flag | native | auto | 2/2/2/2 | cells | cells |
| short-zwj | native | auto | 2/2/2/2 | cells | cells |
| heart-zwj | native | auto | 2/2/2/2 | cells | cells |
| family-zwj | native | auto | 2/2/2/2 | cells | cells |
| tag-flag | native | auto | 2/2/2/2 | cells | cells |
| private-use | native | auto | 1/1/1/1 | cells | cells |

The DECOMPOSED defaults use native output for simple classes and component
preview output for variation, modifier, keycap, regional-flag, ZWJ, and tag-flag
classes. The SINGLE defaults keep `WIDTH` at one cell and use native, base, or
substitute output according to the class. Platform system profiles, such as
`system-osx.the`, `system-linux.the`, and `system-apple-terminal.the`, override
this generic table with measured or deliberately conservative terminal
behaviour. The optional THE_UTF_TERMINAL_PROFILE file is applied at startup
when present.

## See Also
CHANGE, CINSERT, COVERLAY, CREPLACE, REPLACE

Design and status notes for the UTF model live in `../utf-design.md`.

## Status
Implemented. Terminal calibration and platform profile validation remain active
work items.
