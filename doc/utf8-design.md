# UTF-8 Support Design

This document is the working design record for making THE Unicode-first across
macOS, Linux, and Windows. Keep it updated as implementation choices change.

## Goals

- Build with UTF-8 and wide-character support enabled by default.
- Store file contents as UTF-8 bytes without lossy conversion.
- Treat editor-facing character positions as Unicode extended grapheme cluster
  positions.
- Track terminal display positions as cell columns, not bytes or code points.
- Avoid APIs whose meaning changes under `#ifdef USE_UTF8`.

## Non-Goals For This Slice

- Full migration of every historical command and macro API to cluster-aware
  names in one change.
- Perfect terminal display-cell width for every multi-codepoint emoji sequence.
  Grapheme clustering is now correct; terminal sequence width remains a
  separate compatibility problem because terminal emulators vary.
- Rewriting all historical byte-oriented internals at once.

## Current Approach Summary

This section is the current source of truth. The later historical log records
the path taken to get here, including several probe results and THE-side
experiments that were useful but have since been superseded.

For a concise thread-to-thread handover, see `doc/utf8-handover.md`. The
definitive `2026-05-13-poc34` coded default table is saved at
`tools/utf8_terminal_profiles/defaults-poc34.the`. The current macOS Apple
Terminal calibration baseline is saved at
`tools/utf8_terminal_profiles/macos-apple-terminal-poc34-overrides.the`.

THE has two deliberately separate UTF-8 models:

- The **logical model** is editor behavior. It is shared across platforms and
  is derived from UTF-8 bytes plus utf8proc grapheme-cluster segmentation. Text
  movement, status display, insertion, replacement, deletion, mouse snapping,
  command APIs, and tests should use `TextPos` and grapheme clusters.
- The **physical terminal model** is display behavior. It is platform,
  terminal, font, and curses dependent. It maps a logical grapheme cluster to
  the cells and repaint strategy needed by the active terminal.

Do not repair terminal paint bugs by changing logical cluster boundaries. If
Apple Terminal, xterm, Windows Terminal, or a curses stack paints a cluster
differently, that belongs in a terminal profile.

Column-oriented edit commands are part of the logical model. `CINSERT`,
`CREPLACE`, and `COVERLAY` convert any file-area display coordinate back to a
logical cell column, snap to the start of the containing grapheme cluster, and
edit whole clusters. They must not use terminal-profile layout widths to decide
what bytes to insert, delete, or replace.

The terminal profile is keyed by feature class, display intent, and terminal
identity. Each entry describes how one logical cluster class is physically
written and repaired on the active terminal:

```text
feature_class
display_intent
output_method
substitute_codepoint
layout_width
cursor_background_width
cursor_strategy
replacement_strategy
```

`layout_width` and `cursor_background_width` are the physical widths used by
the renderer and software cursor. They are not byte counts, code point counts,
or necessarily utf8proc's logical cell width. `output_method` is `native`,
`expanded`, or `substitute`; substitute output is available to any class and
intent and is still only a display choice. `cursor_strategy` records how far
left THE must reset terminal composition state before repainting during cursor
movement. `replacement_strategy` is intentionally separate because editing text
can invalidate more terminal state than cursor movement.

The approved renderer refactor is to route every physical repair through one
shared plan:

```text
profile entry + purpose + logical line slice -> repair plan
```

The plan names the selected strategy, the logical start `TextPos`, whether the
repair covers changed cells, a suffix, or the whole line, and whether a clear
must be flushed or paused before repaint. `show.c` should execute this plan; it
should not reinterpret the strategy independently for cursor movement and text
replacement. Keycaps, ZWJ sequences, flags, combining marks, and even ASCII are
all ordinary profile entries. ASCII may keep a fast path only when its active
profile is the native one-cell `changed_cells` default and there is no old-line
repair hint.

The terminal probe must follow the same rule. Calibration output methods and
cursor/replacement strategies are not ZWJ-only or keycap-only menus: every class
can choose from the same physical output and repair vocabulary. A class may
still have only a `normal` intent entry, but that entry can select native or
substitute output and any generic strategy.

Current Apple Terminal findings from `utf8_terminal_probe`:

- Regional flags are the control success case. `--testcursor flag 3 3 line`
  walks cleanly in Apple Terminal.
- Keycaps are physically `layout_width=2` and `cursor_background_width=2`.
  Wider static layouts can make isolated rows look tempting, but they shift the
  following `B`, space, and next `A`.
- Keycaps corrupt when THE/curses locally repaints after the keycap. The
  terminal appears to keep composition state for the preceding keycap, so
  repainting only the following `B`/space can make the `B` visually join or
  collapse into the keycap.
- For multi-keycap rows, `flashfirstcluster` and `flashwhole` work. The fastest
  known working keycap reset is `flashfirstfast`: clear from the first keycap
  in the rendered run, flush that blank, then repaint immediately with no
  delay.
- Local `flashcell`, `flashpair`, and `flashbackcluster*` repairs are not
  reliable for keycaps on Apple Terminal. They can collapse clusters going
  right and recover them going left, which points to terminal composition state
  rather than a THE logical cursor bug.
- Replacement behavior has not yet been proven. Do not promote the cursor
  movement strategy into text replacement until a replacement probe shows it is
  correct.
- For ZWJ classes, the probe must distinguish user display intent from terminal
  output method. `group` intent can be satisfied by good native shaping or by a
  configured substitute. `components` intent can be satisfied by good native
  fallback or by expanded output with U+200D removed. These are different
  physical terminal samples even though they are the same logical editor
  cluster.
- Apple Terminal's literal `woman-heart-man` fallback is acceptably represented
  by integer width `6`, but the glyphs visually overlap: the heart and trailing
  face can look more like a fractional `2.5`-cell sub-run. THE stores integer
  cell footprints only, so the profile should record the closest stable integer
  plus the repaint strategy that keeps neighbouring cells clean.
- The four-face family ZWJ literal fallback has not yet produced a good Apple
  Terminal profile. The calibration tool therefore offers wider integer
  candidates so this can be tested as a physical terminal footprint issue
  rather than a logical-cluster issue.

For mixed lines, the renderer should use the strongest repair plan needed by any
cluster in the affected render run. A plain ASCII suffix can remain fast, but
once a keycap-like cluster to the left requires a composition reset, the safe
repaint boundary may be that first matching cluster rather than the changed
cell.

The current implementation order is:

1. Keep proof tools and profile fragments as physical terminal evidence only.
2. Maintain one shared repair planner with focused unit coverage for cursor and
   replacement purposes.
3. Make `show.c` execute the shared plan for cursor transitions and full-line
   redraw/replacement.
4. Let all classes, including ASCII, use the same strategy and substitute-output
   machinery; keep fast paths as optimizations only when the active profile
   makes them equivalent.
5. Promote settings to built-in defaults or documented user overrides only after
   probe output and THE-side fixture testing agree.

## Units

THE must keep these units distinct:

- **Byte offset**: Offset into the UTF-8 line buffer. This is the storage and
  file I/O unit.
- **Code point index**: Index of a Unicode scalar value in a line. This is
  retained for compatibility and low-level text operations.
- **Cluster index**: Index of an extended grapheme cluster. This is the
  editor-facing character unit.
- **Cell column**: Terminal display column. This is the rendering, cursor, and
  mouse hit-testing unit.

## Shared Position Types

The shared position model is intentionally explicit:

```c
typedef struct {
    size_t byte_offset;
    size_t codepoint_index;
    size_t cluster_index;
    int cell_column;
} TextPos;

typedef struct {
    LINETYPE line_number;
    TextPos text;
} FilePos;

typedef struct {
    short row;
    short col;
} ScreenPos;

typedef struct {
    FilePos file;
    ScreenPos screen;
} EditorPos;

typedef struct {
    TextPos start;
    TextPos end;
    int leading_cells;
    int content_cells;
    int trailing_cells;
} TextCellSlice;
```

`TextPos` represents a caret/insertion boundary in one logical line. It does
not represent a character object. Character metadata is derived from the line
at a `TextPos` when needed.

`cluster_index` is the number of complete extended grapheme clusters before the
position. A code point boundary inside a cluster can therefore have the same
`cluster_index` as the cluster start. For example, after the `e` in `e` plus
U+0301 COMBINING ACUTE ACCENT, `codepoint_index` has advanced but
`cluster_index` has not.

## Unicode Dependency

THE uses `utf8proc` for Unicode segmentation and character width data.

- CMake first tries `find_package(utf8proc)`.
- If no installed package is available, CMake fetches
  `https://github.com/JuliaStrings/utf8proc.git` at `v2.11.3`.
- `textpos.c` uses `utf8proc_grapheme_break_stateful()` for UAX #29 extended
  grapheme cluster boundaries. Calls are made in string order, as required by
  utf8proc's stateful API.
- `textpos.c` uses `utf8proc_charwidth()` for code point cell widths.
- The fallback without `USE_UTF8PROC` remains codepoint-cluster compatible only
  for build isolation; robust UTF-8 support requires utf8proc.

## Canonical Construction

Callers should not hand-fill individual `TextPos` fields. Positions are
canonical only when produced by the shared helpers:

```c
TextPos textpos_begin(void);
FilePos filepos_make(LINETYPE line_number, TextPos text);
ScreenPos screenpos_make(short row, short col);
EditorPos editorpos_make(LINETYPE line_number, TextPos text, short screen_row, short screen_col);
TextPos textpos_from_byte(const CHARTYPE *line, size_t len, size_t byte_offset);
TextPos textpos_from_codepoint(const CHARTYPE *line, size_t len, size_t codepoint_index);
TextPos textpos_from_codepoint_virtual(const CHARTYPE *line, size_t len, size_t codepoint_index);
TextPos textpos_from_cluster(const CHARTYPE *line, size_t len, size_t cluster_index);
TextPos textpos_from_cluster_virtual(const CHARTYPE *line, size_t len, size_t cluster_index);
TextPos textpos_from_cell(const CHARTYPE *line, size_t len, int cell_column, TextSnap snap);
TextPos textpos_from_cell_virtual(const CHARTYPE *line, size_t len, int cell_column, TextSnap snap);
TextPos textpos_next_codepoint(const CHARTYPE *line, size_t len, TextPos pos);
TextPos textpos_prev_codepoint(const CHARTYPE *line, size_t len, TextPos pos);
TextPos textpos_next_cluster(const CHARTYPE *line, size_t len, TextPos pos);
TextPos textpos_prev_cluster(const CHARTYPE *line, size_t len, TextPos pos);
TextPos textpos_prev_cell_boundary(const CHARTYPE *line, size_t len, TextPos pos);
TextCellSlice textpos_slice_cells(const CHARTYPE *line, size_t len, int start_cell, int width_cells);
```

Rules:

- Byte offsets are normalized to UTF-8 code point boundaries.
- Out-of-range indexes clamp to the end of the line.
- Virtual variants preserve existing editor behavior past end-of-line by
  extending code point, cluster, and cell positions one cell per virtual
  character after the physical line end. They must not create positions inside
  a physical wide character.
- Invalid UTF-8 bytes decode as U+FFFD and consume one byte, so scanning always
  progresses.
- Cluster boundaries are extended grapheme cluster boundaries from utf8proc.
- Display slices are expressed in cells. If a slice starts or ends inside a
  wide character, the partial character is omitted and represented as leading
  or trailing padding cells.

## Mouse And Cell Snapping

A mouse click begins as a screen row/column, which maps to a cell column in a
logical line. `textpos_from_cell()` resolves that cell to a canonical `TextPos`.

The cell mapper supports these snap modes:

- `TEXT_SNAP_BACKWARD`: choose the cluster boundary at or before the cell.
- `TEXT_SNAP_FORWARD`: choose the cluster boundary after the cell when the
  cell lands inside a multi-cell cluster.
- `TEXT_SNAP_NEAREST`: choose the nearest insertion boundary.

For a double-width character such as U+1F600, or a multi-codepoint cluster such
as a regional-indicator flag pair, occupied cells map to valid insertion
boundaries according to the selected snap rule; no API receives a bare
ambiguous `column`.

Clicks or moves past end-of-line use the virtual constructors so existing
virtual-space behavior remains available while still carrying byte, code point,
cluster, and cell fields coherently.

Left/right cursor movement in the file area moves by grapheme cluster
boundaries. This matters for combining marks and emoji sequences: a user-visible
character can contain multiple code points, and some code points can have zero
display width.

All successful up/down cursor movement in the file area snaps the destination
cell backward to the start of the containing cluster after changing rows. This
includes the normal physical arrow-key path (`CURSOR ESCREEN UP/DOWN`) as well
as CUA movement. This prevents vertical movement from leaving the cursor in the
middle of a double-width emoji, flag pair, or combining sequence, and avoids
the surprising "nearest edge" behavior when the preserved column lands on the
second half of a cluster.

Note that Unicode extended grapheme clusters do not always match a terminal's
visual presentation. A ZWJ emoji sequence is one editor character even if a
terminal renders the sequence as several visible emoji glyphs. THE's editing
unit remains the grapheme cluster; terminal-specific display mismatches should
be treated as rendering compatibility issues, not as cursor-position model
changes.

POC cursor decision: in UTF-8 mode, THE should own the visible cursor rather
than relying on the terminal hardware cursor in editor-controlled windows. The
hardware cursor is an independent terminal drawing primitive; on terminals with
color emoji overhang, especially Apple Terminal regional flags, it can visually
appear one cell away from the logical cluster even when THE's model and status
line are correct. Drawing the cursor through the same renderer as the text keeps
the visible cursor, status field, and movement model on the same cluster.

The POC uses 3270-style cursor semantics by default: insert mode is a software
block cursor and overwrite/replace mode is a software underline cursor. This is
configurable through `SET CURSORSTYLE`: `MAINFRAME`/`3270` maps insert to block
and overwrite to underline, `MODERN` reverses that mapping, `BLOCK` uses block
for both modes, and `UNDERLINE` uses underline for both modes. The status line
also shows `INS` when insert mode is active so cursor shape is not the only mode
indicator.

Cursor moves that use the UTF-8 software-cursor path refresh the screen so the
cursor tracks the current cluster. The renderer captures the file-area cursor
position before repainting any lines and uses that saved display snapshot when
deciding which cluster to paint as the cursor. It must not call `getyx()` during
line painting to discover the editor cursor, because the renderer itself moves
the curses cursor as it writes each row.

The final software-cursor implementation must handle empty visual cursor
targets explicitly. End-of-line and virtual-space cursor positions do not have a
grapheme cluster to recolour, so the renderer must draw a styled blank cell at
the saved screen column. Command-line entry must also repaint the command-line
software cursor immediately when focus moves there; it must not rely on the next
typed character or cursor movement to make the cursor appear.

Focus transitions are cursor-rendering events. The current codebase has several
historical paths that change `current_window` directly, including
`THEcursor_cmdline()`, `THEcursor_home()`, and `go_to_new_field()`. The software
cursor POC exposed that this is not maintainable: entering or leaving a window
must repaint both the old cursor owner and the new cursor owner immediately. The
clean implementation should introduce a small focus-transition component, for
example `cursor_focus_enter_command()`, `cursor_focus_enter_filearea()`, and a
shared refresh hook. Those helpers should own `previous_window`,
`current_window`, `pre_process_line()`, `post_process_line()`, cursor column
state, stale software-cursor cleanup, new software-cursor drawing, and the final
`doupdate()`. Direct assignments to `current_window` should be audited and
replaced where they represent user-visible focus changes.

The POC also exposed two remaining cursor-zone gaps that should not be fixed by
one-off refresh calls in the clean implementation:

- Backspace/delete paths can hide the software cursor until another repaint
  happens. Command-line mutations must be cursor-rendering events in the same
  way as cursor motion and focus transitions.
- Prefix-area entry still appears to use the hardware cursor. Prefix should be
  treated as another editor-controlled zone with the same software-cursor
  ownership rules as the file area and command line.

Because robust editing of complex UTF-8 clusters requires THE to draw cursor
coverage over one or more terminal cells, UTF-8 mode should be designed as a
software-cursor architecture from the start. Hardware cursors may still exist as
an implementation detail for non-UTF-8 builds, prompts, dialogs, or platforms
where a terminal-native cursor is explicitly chosen, but the UTF-8 editor path
should not depend on hardware cursor placement for correctness.

The cursor presentation decision is centralized in
`current_cursor_presentation()`. The POC modes are hardware cursor and
software cursor. New platform workarounds must be added there rather than as
local checks in the renderer, command-line transition, or `draw_cursor()`.

All UTF-8 file-area cursor consumers must go through the shared cursor mapping
helpers rather than deriving file positions directly from curses `getyx()`
columns. Curses/window positions are physical terminal cells; editor positions
are logical `TextPos.cell_column` values. `show_utf8_logical_col_from_display()`
maps mouse and curses coordinates back to logical cells, while
`show_utf8_display_col_from_logical()` maps a logical cell to the physical
screen column after terminal-specific width policy has been applied. This keeps
movement, status reporting, rendering, and editing on one position model even
when a keycap, flag, or ZWJ cluster occupies a different physical footprint
from its logical width.

Simple UTF-8 file-area left/right movement must not rebuild and repaint the
whole screen just to move the software cursor. Re-emitting unchanged clusters to
the left of the cursor can expose terminal shaping differences and make stable
text appear to slide. The fast path restores only the old cursor target and
paints only the new cursor target when the row and horizontal viewport do not
change; it falls back to the full redraw only when scrolling or changing rows
requires it.

Typed text in the UTF-8 file area edits by resolved `TextPos`. Insert mode
inserts at `TextPos.byte_offset`. Overwrite mode replaces the whole grapheme
cluster under the cursor, then places the cursor after the inserted text. It
must not overwrite a single byte or use `verify_col + getyx().x` as a byte
offset.

## API Direction

- New semantic APIs should accept or return `TextPos`, `FilePos`, `ScreenPos`,
  or `EditorPos` as appropriate.
- Low-level byte work is allowed only in explicitly byte-named helpers, e.g.
  `byte_offset`, `_bytes`, or `_raw`.
- Existing macro-visible APIs that naturally mean characters should become
  cluster-oriented.
- Truly byte-oriented legacy APIs should be renamed, isolated, or documented as
  byte APIs before they are exposed further.

## Rendering Direction

Rendering must use terminal cell widths. It must not assume that one code point
is one cell. Wide characters, combining marks, and invalid bytes must all pass
through the shared layout helpers.

Cell width is a grapheme-cluster property, not just a sum of code point
widths. THE deliberately keeps two related but distinct width models:

- `TextPos.cell_column` is the editor/logical terminal-cell position. Cursor
  movement, status, commands, and API-facing positions use this model.
- Display cells are physical columns used to paint the active terminal. The
  file-area renderer maps logical cells to display cells so terminal-specific
  paint overhangs do not change the editor/API position model.

Combining sequences stay one logical cell when the base is one cell, and
regional-indicator flag pairs stay two logical cells. The root regional-flag
failure is not that Unicode, utf8proc, or curses reports the wrong logical
cluster width: the flag is one grapheme cluster and two terminal cells. The
failure observed on Apple Terminal is that the terminal reports a normal
two-cell cursor advance for the flag while the color flag glyph visibly paints
into the following cell. A cursor-position probe can therefore report the
correct column while the screen still shows the next character visually covered
by the flag artwork. That is a terminal paint/compositing mismatch, not a
Unicode segmentation failure.

The clean implementation keeps flags literal and logical-width-correct. Visual
overhang compensation is applied as a separate display-cell policy, not by
changing `TextPos.cell_column`. The file-area renderer now derives physical
layout width, cursor width, and ZWJ output method from the active terminal
profile. The older ad hoc `THE_UTF8_*_WIDTH` and flag-overhang diagnostic
switches have been retired from the renderer path; equivalent terminal
differences should be represented with `SET UTF8 TERMINAL CLASS ...` profile
entries.

Display slicing is cluster-aware. If a viewport starts or ends inside a
multi-cell cluster, the partial cluster is omitted and replaced with padding
cells. Padding is capped to the visible width so a clipped large cluster cannot
spill into following output.

The file-area renderer writes each grapheme cluster atomically on the slow
wide-curses path when the cluster fits in a curses `cchar_t`. Rendering a
cluster codepoint-by-codepoint can expose intermediate terminal shaping states,
especially for regional-indicator flags, where a terminal may paint an
individual regional indicator as an emoji-width glyph before seeing the pair.
Atomic cluster output reduces codepoint-boundary paint drift, but it does not
by itself guarantee that a terminal emulator's or curses' actual cursor advance
matches THE's cluster/cell model. Clusters too large for `cchar_t` currently
fall back to codepoint output; this is why long ZWJ emoji remain an important
manual-test case.

When a cluster's display width differs from the sum of its component code point
widths, the file-area renderer may reserve a wider physical paint footprint
while preserving the logical position. Current compatibility cases are macOS
regional-flag overhang and one-cell emoji-presentation/keycap clusters that
terminals commonly render as two physical cells.

Terminal policy must keep cursor advance and paint footprint separate:

- **Logical width** comes from the editor text model. It determines grapheme
  movement, status lookup, text editing, and API-facing positions.
- **Cursor/display advance width** is the terminal column distance between
  logical cursor stops after terminal-specific shaping. It is used to map
  logical columns to physical screen columns and back.
- **Paint footprint width** is the number of physical cells that must be
  cleared/redrawn to avoid stale or overpainted glyph pixels. It may be wider
  than cursor/display advance.
- **Cursor styling policy** is separate again: some terminal/glyph combinations
  cannot be safely reverse-video styled and may need marker/underline-style
  cursor presentation instead of recolouring the glyph itself.

The Apple Terminal keycap probe result is the current motivating example. The
early raw-only POC made `raw_paint3_cursor2_marker` look promising, but later
curses/THE-style probes superseded that result. Keycaps are a two-cell physical
layout with a two-cell software cursor on Apple Terminal. The bug is not that
the following `B` should move to a third cell; it is that local repaint after a
keycap can reuse stale terminal composition state. The current proven reset is
to clear from the first keycap in the rendered run, flush the blank state, and
then repaint immediately (`flashfirstfast` in `utf8_terminal_probe`).

Regional-indicator flags are deliberately separate from the ZWJ case. On some
terminals the flag glyph paints correctly but either the cursor advance or the
glyph's visible overhang does not match the cells needed by following text. THE
leaves literal flag rendering as the default and writes each regional-indicator
pair atomically when possible. The renderer then maps following logical cells to
their physical display cells while keeping the status line and editing model
unchanged.

The problematic classes are not "all emoji"; they are sequences where the
terminal grid width is context-sensitive or implementation-specific:

- Regional-indicator flag pairs.
- Emoji ZWJ sequences.
- Emoji modifier and presentation-selector sequences.
- Emoji tag sequences, including subdivision flags.
- Keycap sequences.
- East Asian ambiguous-width characters and private-use icon fonts.

## Joiner Policy

ZWJ sequences remain one logical grapheme cluster in THE. That rule should not
change when a terminal displays a family emoji as one combined glyph, four
visible people, or a mix of components. The difference is physical rendering,
not editor text identity.

There are two user-facing display intents for joined clusters:

- `group`: show the logical cluster as one grouped icon or replacement
  character. The replacement character/string is user-configurable and may later
  be keyed by exact sequence or feature class.
- `components`: show the component code points separately in the file area,
  while keeping one logical "fat" cursor/editing position spanning all displayed
  component cells.

Those intents are separate from the terminal output method used to realize
them:

- `native`: emit the stored UTF-8 sequence, including U+200D joiners, and let
  the terminal/font decide whether it shapes the sequence into one glyph or
  falls back to visible components. Native output may satisfy either user
  intent on a particular terminal: it can produce a good grouped glyph, or it
  can produce a good component fallback.
- `expanded`: emit the component code points with U+200D suppressed for
  file-area rendering only. This is the explicit implementation of the
  `components` intent when native output collapses the sequence or paints it
  poorly.
- `substitute`: emit a configured replacement code point/string for the
  `group` intent. For the ZWJ probe itself, a simple one-character substitute
  does not need special ZWJ terminal measurements; it follows the normal
  physical rules for whatever substitute character the user configured.

THE cannot reliably force a terminal to collapse an arbitrary ZWJ sequence into
a single glyph. There is no separate Unicode scalar for most family/couple
sequences; the standard representation is the sequence itself, and shaping is
owned by the terminal, font, and emoji implementation. THE can ask for native
grouping only by emitting the literal sequence. If native output falls back to
components, THE must either reserve the observed physical component width or use
a configured substitute for the grouped intent.

A configured substitute is a valid logical-to-physical display mapping, but it
is not the same as terminal shaping. It may be semantically approximate: for
example, a family ZWJ sequence might be displayed as a generic family glyph or
private-use marker, losing gender, skin tone, or member-count detail. Therefore
substitution must be display-only. THE must preserve the original bytes for
save/copy, report the original code points in status, keep the original cluster
as the editing unit, and map mouse/cursor hits on the substitute back to that
one logical cluster.

THE can force expansion more reliably than collapse by rendering the components
without joiners. That should be a terminal-profile display option, not a change
to the buffer or logical cursor model. It also needs its own cursor movement and
replacement probes because one logical cluster may cover many physical cells.

## Terminal Calibration Utility

THE should ship with conservative built-in terminal policy defaults for known
platform/terminal combinations, but those defaults must be overrideable without
rebuilding. The proposed calibration utility is an interactive probe built from
`utf8_terminal_probe`:

- It presents one feature class at a time, for example `1/N keycaps`,
  `2/N regional flags`, `3/N emoji presentation`, `4/N ZWJ fallback`, and so on.
- For each class it first runs an automatic cursor-advance probe using raw
  terminal output and `CSI 6 n`, and where useful compares that with curses
  accounting.
- It then shows a small set of visual choices for paint footprint and cursor
  styling. The user steps through options and selects the first row that looks
  correct.
- The saved result records terminal policy, not Unicode semantics. Logical
  cluster boundaries and logical editor widths continue to come from utf8proc.
- The output is a profile/config fragment that THE can load at startup, while
  simple environment-variable overrides remain available for diagnostics.

The calibration result should be a table keyed by feature class and terminal
identity. Each entry should contain at least:

```text
feature_class
display_intent
output_method
layout_width
cursor_background_width
paint_footprint_width
repaint_strategy
replacement_strategy
cursor_style_strategy
probe_source
```

For non-ZWJ classes, `display_intent` and `output_method` can be omitted or
treated as `normal/native`. For ZWJ classes, the profile should keep separate
physical entries for `group` and `components` intent whenever native output is
being evaluated for either mode. A substitute-only `group` entry may not need
ZWJ-specific probe measurements because THE will render the configured
substitute as an ordinary character/string.

`layout_width` can often be approximated automatically with cursor-position
queries. `cursor_background_width`, `paint_footprint_width`,
`repaint_strategy`, `replacement_strategy`, and the ZWJ intent/output mapping
usually require visual confirmation because common terminal protocols do not
report which neighbouring cells a color glyph painted over or which composition
state the terminal retained. The utility should therefore combine automatic
measurement with explicit user selection instead of pretending this can be
fully inferred.

The current Apple Terminal keycap finding would save roughly:

```text
feature_class=keycap
layout_width=2
cursor_background_width=2
paint_footprint_width=2
repaint_strategy=clear_from_first_cluster_fast
replacement_strategy=unproven
cursor_style_strategy=background_cells
probe_source=testchain_flashfirstfast
```

The intended workflow is:

1. Prove a terminal-policy result in the standalone probe.
2. Apply the smallest THE-side experiment for that feature class.
3. If THE behaves correctly, promote the result to the built-in default table
   for that platform/terminal.
4. Extend the calibration utility so other platforms and terminals can generate
   an override table without source changes.

Reference notes:

- Unicode UTS #51 defines flag emoji as two Regional Indicator characters and
  ZWJ emoji as sequences joined by U+200D. It explicitly notes that ZWJ
  sequences may fall back to separate emoji when a single glyph is unavailable.
- WezTerm documents that mismatched Unicode width expectations can make text
  columns or editor cursors appear in the wrong place.
- Mitchell Hashimoto's terminal grapheme write-up documents exactly why ZWJ
  emoji can be width 2, 4, 5, or 6 depending on terminal behavior, and suggests
  querying terminal cursor position with `CSI 6 n` when exact runtime width is
  needed.
- A long-standing ncurses/macOS flag report describes the same flag-overlap
  failure mode: curses follows the system width model while Terminal.app/iTerm
  may render regional indicators differently.

Curses output should use wide-character APIs (`setcchar`, `wadd_wch`,
`wadd_wchnstr`) rather than writing into `cchar_t` internals or passing Unicode
code points to narrow `waddch` paths.

The status-line HEXDISPLAY path owns a fixed-width UTF-8 field before the
right-hand status indicators. It displays the focused cluster in brackets plus
its code points, for example `[a] U+61`, `[é] U+65+301`, or
`[🇺🇸] U+1F1FA+1F1F8`. Code point numbers use no leading zeroes. If the
field does not fit, the textual code list is truncated with `...`.

The status path must not draw isolated combining marks or control characters
directly. Combining marks may be drawn only as part of their full focused
cluster; controls are reported through code points only so they cannot move or
corrupt the status line.

## Platform Direction

- macOS: use Apple SDK curses wide APIs exposed through `<ncurses.h>`.
  CMake should not request a separate `ncursesw` include path on macOS because
  Apple's SDK does not ship one.
- Linux: prefer/link wide curses (`ncursesw`) through CMake's
  `CURSES_NEED_WIDE` path.
- Windows: build/use PDCurses with wide support and forced UTF-8 where needed.
- Locale setup should use the user's environment via `setlocale(LC_ALL, "")`,
  with UTF-8 fallbacks where the platform has them.

## Testing Strategy

Testing should land with each implementation slice.

Foundation tests:

- ASCII byte/codepoint/cell identity.
- Accented BMP characters.
- CJK wide characters.
- Single-codepoint emoji such as U+1F600.
- Combining marks and their base characters as one cluster.
- Regional-indicator flag pairs as one cluster.
- Zero-width-joiner emoji sequences as one cluster.
- Invalid UTF-8 progress and U+FFFD handling.

Editor integration tests:

- Batch read/save preserves UTF-8 bytes.
- `QUERY UTF8` reports enabled by default.
- Cursor movement and mouse hit-testing use `TextPos`.
- Rendering clips and pads by cells, not bytes or code points.
- Syntax highlighting offsets map through canonical positions.

Current automated coverage:

- `tests/test_textpos.c` covers canonical position construction,
  byte-boundary normalization, code point counting, grapheme cluster counting,
  cell-width mapping, cell-based display slicing, invalid UTF-8 progress,
  UTF-8 encoding, and representative UAX #29 grapheme break cases. It also
  pins cluster-aware cell snapping and cluster widths for combining sequences,
  regional-indicator flags, and ZWJ emoji.
- `tests/test_utf8_fixture.c` is wired into CTest and validates the manual
  UTF-8 fixture is valid UTF-8. It also asserts the fixture still contains the
  required combining, regional-flag, two-face ZWJ, and four-face ZWJ samples,
  and that each `A<cluster>B` sample is three editor grapheme clusters.
- A fresh macOS CMake build with default options verifies that UTF-8/wide
  curses configuration no longer requires `<ncursesw/ncurses.h>`.

Manual renderer fixture:

- `tests/fixtures/utf8-render.txt` is a valid UTF-8 file for visual/manual
  checks in THE. It includes inline manual test notes.
- Use it to inspect ASCII, accented text, combining marks, CJK double-width
  characters, single-codepoint emoji, emoji modifiers, variation selectors,
  keycaps, regional-indicator flags, short ZWJ emoji, two-face ZWJ emoji,
  four-face ZWJ emoji, mixed lines, and horizontal viewport clipping.
- Set `THE_UTF8_RENDER_TRACE=/tmp/the-utf8-render.log` while running THE to
  capture the live file-area renderer's cluster decisions. The trace records
  each drawn cluster's code points, utf8proc logical cell width, screen column,
  clear width, cursor-hit decision, curses cursor column after writing, and the
  forced post-write column. This is intended to distinguish a wrong logical
  width from a terminal paint/compositing mismatch.
- Invalid UTF-8 is intentionally excluded from this manual text fixture because
  editors and source tools may silently normalize or repair invalid byte
  sequences. Keep invalid-byte coverage in byte-oriented automated tests or
  generated binary fixtures.

Terminal probe tool:

- Build `utf8_terminal_probe` to investigate terminal and curses behavior
  outside THE's editor state:

  ```sh
  cmake --build cmake-build-debug --target utf8_terminal_probe
  ./cmake-build-debug/utf8_terminal_probe --cases tools/utf8_terminal_probe_cases.tsv --report /tmp/the-utf8-terminal-probe.txt --pause
  ```

  To remove curses from the experiment entirely, run the raw ANSI-only POC:

  ```sh
  ./cmake-build-debug/utf8_terminal_probe --raw-poc --report /tmp/the-utf8-raw-poc.txt --pause
  ```

  To keep curses in the loop while still isolating THE, run the focused visual
  probe. `--curses-poc` is now a compatibility alias for the keycap/flag focus
  set:

  ```sh
  ./cmake-build-debug/utf8_terminal_probe --curses-poc --report /tmp/the-utf8-curses-poc.txt --pause
  ```

  To inspect one UTF family, use `--utfvis`. Selectors can be sample names
  such as `keycap-1` or `flag-us`, classes such as `keycap`, `variation`, or
  `zwj`, aliases such as `flag`, or `all`:

  ```sh
  ./cmake-build-debug/utf8_terminal_probe --utfvis flag --report /tmp/the-utf8-flag.txt --pause
  ```

  To animate the THE-style background cursor across `A-cluster-B-space-A-cluster-B`,
  pass the sample selector, layout width, cursor-background width, and optionally
  a repaint mode:

  ```sh
  ./cmake-build-debug/utf8_terminal_probe --testcursor flag 3 3 line --report /tmp/the-utf8-cursor.txt --timeout-ms 200
  ```

  To create or edit a terminal-profile fragment interactively, run calibration
  mode. The selector may be `focus`, `keycap`, `flag`, `zwj`, a sample name, a
  class name, or `all`; the default is `all`:

  ```sh
  ./cmake-build-debug/utf8_terminal_probe calibrate --profile /tmp/the-utf8-terminal-profile.the --report /tmp/the-utf8-calibrate.txt --timeout-ms 200
  ```

  Bare `utf8_terminal_probe` is equivalent to `utf8_terminal_probe calibrate`.
  The simplified user-facing commands are:

  ```text
  utf8_terminal_probe calibrate [selector]
  utf8_terminal_probe list
  utf8_terminal_probe view [selector]
  utf8_terminal_probe cursor selector layout_width cursor_width [mode]
  utf8_terminal_probe chain selector layout_width cursor_width [mode]
  ```

  The old `--utfvis`, `--testcursor`, `--testchain`, `--raw-poc`, and
  `--curses-poc` flags remain as diagnostic compatibility aliases, but normal
  calibration should use the simplified commands above.

  Calibration mode reads the existing profile when present, then opens a main
  screen listing each selected feature class and its current view, cursor,
  replacement, and ZWJ intent/output settings. `Enter` configures the selected
  class, `s` saves the profile, and `q` quits without saving.

  For complex joined clusters, the UI should be organized as two intent rows
  rather than one flat ZWJ-policy choice:

  ```text
  family-zwj group       native literal     L? C? cursor=? replace=?   or substitute
  family-zwj components  native fallback    L? C? cursor=? replace=?   or expanded
  ```

  The user is choosing what THE should display: a grouped icon/replacement, or
  the separate component code points under one fat logical cursor. The probe is
  choosing how the terminal can physically realize that display: native literal
  output if it happens to produce the desired appearance, expanded output with
  U+200D removed for component mode, or a configured substitute for group mode.
  Substitute mode is mostly outside the terminal probe unless the substitute
  itself is a special-width character; the ZWJ-specific measurements are needed
  for native and expanded output. Native and expanded rows each have their own
  physical `layout_width`, `cursor_background_width`, cursor strategy, and
  replacement strategy. They may end up the same on one terminal, but the probe
  must treat them as independent until view, cursor walking, and replacement
  all pass. The output-method screen should show both the original file/logical
  code points and, per candidate row, the code points that will be emitted to
  the terminal plus that row's current/default L/C and repaint settings, a plain
  editor preview, and a cursor editor preview. This keeps visually-identical
  native fallback and expanded output distinguishable when the only difference
  is that expanded suppresses U+200D.

  When more than one output method works, prefer the lowest-scored row. In
  general that means the least-transforming working method wins: native grouped
  output before substitute for `group` intent, and native component fallback
  before explicit expanded output for `components` intent. Expanded output is
  still the right choice when native paints incorrectly or leaves unstable
  cursor/replacement behavior.

  After selecting a ZWJ intent row and output method, the following display/view,
  cursor-walk, and replacement screens use that exact physical sample. The view
  screen shows the current/default/policy choices plus integer `1..12`
  layout/cursor candidates and common layout/cursor hybrids. Cursor and
  replacement strategy screens show a preference score; lower scores are
  faster/preferred when multiple strategies look correct. Changing strategy
  resets the sample to a clean baseline before replaying the animation.
  Built-in defaults are applied before reading the profile, and saved profiles
  contain only classes that differ from those coded defaults. The generated
  profile is a proposed THE instruction fragment, using commands such as
  `SET UTF8 TERMINAL CLASS keycap LAYOUT 2 CURSOR 2` and
  `SET UTF8 TERMINAL CLASS keycap REPLACESTRATEGY clear_from_first_cluster_fast`.
  The eventual ZWJ shape may need intent-qualified commands, for example:

  ```text
  SET UTF8 INTENT components
  SET UTF8 TERMINAL CLASS family-zwj INTENT group OUTPUT native
  SET UTF8 TERMINAL CLASS family-zwj INTENT group LAYOUT 2 CURSOR 2
  SET UTF8 TERMINAL CLASS family-zwj INTENT components OUTPUT expanded
  SET UTF8 TERMINAL CLASS family-zwj INTENT components LAYOUT 8 CURSOR 8
  ```

  `SET UTF8 INTENT group|components|toggle` selects the global display intent
  used at render time for intent-aware classes. Classes with only normal
  profiles, such as `keycap`, still use their normal class profile regardless
  of the global intent.

  Non-visual calibration runs report the loaded settings but do not rewrite the
  profile.

  `testcursor` modes are `frame` (redraw the sample each step and let curses
  optimize), `cell` (redraw only the old and new cursor targets), and `line`
  (redraw the complete sample and force the row dirty with `touchline()`). Use
  `line` as the clean baseline when checking whether a terminal can redraw the
  whole `A-cluster-B-space-A-cluster-B` line without incremental-repaint damage.
  The flash modes force a physical blank before repainting: `flashline` blanks
  and refreshes the complete sample line, `flashcell` blanks and refreshes the
  old/new cursor targets, and `flashpair` blanks and refreshes the old/new
  target span plus one following target. `flashfrom0` through `flashfrom6`
  blank and refresh a suffix of the sample before repainting, where the index
  maps to `A1`, `cluster1`, `B1`, `space`, `A2`, `cluster2`, and `B2`.
  `--testchain selector layout_width cursor_width flashbackclusterN` repeats a
  selected grapheme cluster seven times in
  `XX-A-cluster-B-A-cluster-B-A-cluster-cluster-cluster-B-A-cluster-B-A-cluster-B-XX`
  and starts each blank/repaint suffix at the nearest `N` prior grapheme
  clusters. `flashfirstcluster` always starts at the first repeated cluster, and
  `flashwhole` starts at the leading `XX`. These modes test the terminal as a
  grapheme-composition state machine rather than as a byte or single-cell
  repaint problem, including a deliberate three-cluster run with no ASCII
  separator. Probe version `2026-05-12-poc25` adds first-cluster reset strategy
  variants: `paintfirstcluster` repaints from the first cluster without a clear,
  `touchfirstcluster` additionally marks the row dirty, `clearfirstcluster`
  clears and repaints in one refresh, `flashfirstfast` clears and flushes before
  repainting without a delay, and `flashfirstcluster` keeps the known-working
  clear/flush/repaint path with a small pause.

- The probe writes representative ASCII, combining, CJK, emoji, keycap,
  regional-flag, and ZWJ samples through several output paths: raw terminal
  UTF-8, `waddwstr`, one `wadd_wch` per code point, `wadd_wchnstr`, and
  whole-cluster `cchar_t` where curses can represent the cluster.
- `tools/utf8_terminal_probe_cases.tsv` is the extensible probe fixture. Add
  new rows as `name<TAB>class<TAB>policy_width<TAB>U+...`; the codepoint list
  accepts spaces, commas, or pluses between `U+XXXX` values.
- The report records curses' virtual cursor position and the terminal's
  physical cursor position from `CSI 6 n` so we can separate curses accounting
  from terminal cursor advance. The visual keycap-motion panel reproduces the
  `A1️⃣B A` cursor path without changing THE, which lets platform probes and
  POC fixes be compared before renderer changes are attempted.
- The terminal-absolute POC section compares raw absolute repaint strategies.
  Cell rows redraw only the old and new cursor targets. Span rows clear and
  redraw the whole `A`/keycap/`B`/space/`A` neighborhood, with an optional guard
  cell. It runs both a pure-raw base case and mixed cases where curses draws the
  base row before raw absolute overlay repair. Curses is flushed before the raw
  overlay, and the terminal is queried before any later curses refresh can
  reassert curses' keycap-width state. Because reverse video over color emoji
  is not reliably visible in Apple Terminal, the POC also draws a raw ASCII `^`
  marker below the target cell. This is the fixture for proving an output/resync
  strategy before it is adapted to THE.
- The `--raw-poc` mode never calls `initscr()`. It writes only ANSI escape
  sequences and UTF-8 bytes, and compares keycap reservations of two, three, and
  four terminal cells. It also compares reverse-video cursor styling with a
  marker-only row so we can separate curses effects, paint-footprint width, and
  reverse-video emoji repaint artifacts. Probe version `2026-05-12-poc25` hides
  the terminal hardware cursor and includes hybrid rows where keycap paint
  reservation is wider than the terminal's two-cell cursor advance.
- The `--utfvis` mode calls `initscr()` and lets curses own the screen. It
  renders a static matrix of independent samples selected by name, class, alias,
  or `all`. Each cell is freshly painted with a THE-style background cursor on
  `A`, the cluster, `B`, the space, or the final `A`; there is no animation, DSR
  query, raw repair, or repeated repaint of the same sample. Probe version
  `2026-05-12-poc25` compares layout-cell width (`L`) with cursor-background
  width (`C`) for the selected UTF family. The old `--curses-poc` mode remains
  as a compatibility alias for `--utfvis focus`, meaning keycaps plus flags.
- The `--testcursor` mode animates the same THE-style background cursor across
  `A-cluster-B-space-A-cluster-B` using explicit layout and cursor-background
  widths. This is the probe for movement bugs: once a static `utfvis` row looks
  right, `testcursor` shows whether repeated repainting can move left and right
  without corrupting neighboring cells. Its `cell`, `frame`, `line`, and
  `flash*` repaint modes separate logical old/new target repainting from
  complete-line repainting, explicit physical blanking, and curses' physical
  terminal update optimization. Probe version `2026-05-12-poc25` keeps `line`
  as the complete-line repaint baseline and adds flash modes to test whether
  Apple Terminal needs a real blank/refresh boundary before keycap repaint.
  The `flashfrom*` modes find the minimum safe left repaint boundary.
  `--testchain` generalizes that result across repeated clusters so we can
  compare repainting from the changed cluster, the nearest prior cluster, or an
  earlier prior cluster. Its `flashfirstcluster` and `flashwhole` modes test
  whether a row containing multiple keycaps must be repainted from the first
  keycap in the run or from the leading non-keycap anchor.

## Current Work Plan

The next UTF-8 work should prioritize proof tools over more renderer changes:

- Continue extending `utf8_terminal_probe --calibrate` into a user-friendly
  calibration tool with sections for display, cursor walking, and replacement.
- Cover at least these feature classes: ASCII/simple BMP, combining marks, CJK
  wide characters, emoji presentation selectors, emoji modifiers, keycaps,
  regional flags, ZWJ sequences, tag/subdivision flags, and ambiguous/private
  width glyphs. The current built-in calibration taxonomy is: `ascii`,
  `combining`, `combining-stack`, `wide`, `ambiguous`, `emoji`,
  `text-variation`, `emoji-variation`, `modifier`, `keycap`, `regional-flag`,
  `short-zwj`, `heart-zwj`, `family-zwj`, `tag-flag`, and `private-use`.

  Current coded defaults are mirrored in
  `tools/utf8_terminal_profiles/defaults-poc34.the`:

  ```text
  ascii normal/native:          L1 C1 cursor=changed_cells replace=changed_cells
  combining normal/native:      L1 C1 cursor=line replace=line
  combining-stack normal/native:L1 C1 cursor=line replace=line
  wide normal/native:           L2 C2 cursor=changed_cells replace=changed_cells
  ambiguous normal/native:      L1 C1 cursor=changed_cells replace=changed_cells
  emoji normal/native:          L2 C2 cursor=line replace=line
  text-variation normal/native: L1 C1 cursor=line replace=line
  emoji-variation normal/native:L2 C2 cursor=line replace=line
  modifier normal/native:       L2 C2 cursor=line replace=line
  keycap normal/native:         L2 C2 cursor=clear_from_first_cluster_fast replace=clear_whole_fast
  regional-flag normal/native:  L3 C3 cursor=changed_cells replace=clear_changed_suffix_fast
  short-zwj group/native:       L2 C2 cursor=line replace=line
  short-zwj components/expanded:L4 C4 cursor=line replace=line
  heart-zwj group/native:       L6 C6 cursor=line replace=line
  heart-zwj components/expanded:L6 C6 cursor=line replace=line
  family-zwj group/native:      L6 C6 cursor=line replace=line
  family-zwj components/expanded:L8 C8 cursor=line replace=line
  tag-flag normal/native:       L2 C2 cursor=line replace=line
  private-use normal/native:    L1 C1 cursor=line replace=line
  ```
- Let the user step through strategy choices and save the selected profile as
  THE configuration commands or an equivalent profile file.
- Treat the ZWJ user intent and output method as inputs to the rest of
  calibration. A terminal may use native literal output for grouped display of
  one ZWJ family, native fallback for component display of another, and
  explicitly expanded output for a third.
- Add replacement probes before changing THE replacement behavior for keycaps
  or ZWJ clusters. Cursor movement success is not enough evidence for editing.
- After the probe proves a profile entry, validate one THE-side change at a
  time against `tests/fixtures/utf8-render.txt`.

For Apple Terminal today, the saved baseline profile is
`tools/utf8_terminal_profiles/macos-apple-terminal-poc34-overrides.the`.
It was captured visually on 2026-05-14 with
`utf8_terminal_probe 2026-05-13-poc34`, `TERM=xterm-256color`, and
`TERM_PROGRAM=Apple_Terminal`. It is an override fragment to apply on top of
the probe's coded defaults. Key rows are:

```text
regional-flag normal/native: default L3 C3 cursor=changed_cells replace=clear_changed_suffix_fast
keycap normal/native:        L2 C2 cursor=clear_from_first_cluster_fast replace=clear_from_first_cluster_fast
modifier normal/native:      L4 C4 cursor=changed_cells replace=line
short-zwj group:             substitute
short-zwj components/native: L4 C4 cursor=changed_cells replace=line
heart-zwj group:             substitute
heart-zwj components/expanded:L6 C6 cursor=changed_cells replace=line
family-zwj group:            substitute
family-zwj components/expanded:L8 C8 cursor=changed_cells replace=line
```

The ZWJ rows remain intent/output specific. Apple Terminal can display some ZWJ
families as separate visible components, while other terminals may shape the
same stored bytes into one glyph. The calibration tool records the physical
result without changing THE's logical model.

## Appendix: Historical Implementation Log

This log is retained so old experiments can be understood or reverted without
guesswork. It is not the current design source of truth; use the "Current
Approach Summary", "Rendering Direction", "Joiner Policy", and "Terminal
Calibration Utility" sections above for the active strategy.

- 2026-05-07: Design agreed. Phase 1 starts with default UTF-8 build,
  portable wide-curses selection, locale initialization, and canonical
  `TextPos` helpers plus tests.
- 2026-05-07: Added `src/textpos.h` and `src/textpos.c` as the shared
  position/UTF-8 foundation. Added `tests/test_textpos.c` and wired it into
  CTest. Verified `cmake --build /tmp/the-utf8-phase1 --target the
  test_textpos -j2` and `ctest --test-dir /tmp/the-utf8-phase1
  --output-on-failure` pass on macOS.
- 2026-05-07: Added an include guard to `src/thedefs.h` so shared UTF-8
  headers can safely include the foundational type definitions without
  duplicate typedef warnings.
- 2026-05-07: Corrected the wide-character header gate in `src/the.h` to use
  `USE_WIDE_CHAR`, matching the new default wide build path.
- 2026-05-07: Added `TextCellSlice` and migrated the UTF-8 file-line renderer
  in `src/show.c` to clip and pad by terminal cells. The renderer now emits
  code points through `setcchar`/wide curses on both fast and slow paths, and
  target highlighting uses byte-range overlap instead of code point counts.
- 2026-05-07: Added `tests/fixtures/utf8-render.txt` as a manual renderer
  fixture for visual checks while migrating cursor, mouse, and syntax paths.
- 2026-05-07: Added virtual `TextPos` constructors and shared `FilePos`,
  `ScreenPos`, and `EditorPos` constructors. Migrated UTF-8 file-area cursor
  placement, left/right movement, vertical CUA end-of-line clamping,
  `execute_move_cursor()`, mouse/filearea snapping through `THEcursor_move()`,
  and current-position reporting to map between cluster indexes and terminal
  cells through the shared position model.
- 2026-05-07: Added `textpos_prev_cell_boundary()` as a low-level helper after
  identifying the zero-width combining-mark cursor trap. File-area movement is
  now handled at cluster boundaries once utf8proc is available.
- 2026-05-07: Adopted utf8proc as the Unicode segmentation dependency. CMake
  now prefers installed `utf8proc` and otherwise fetches `v2.11.3`. `TextPos`
  now carries real extended grapheme cluster indexes, and UTF-8 file-area cursor
  placement, left/right movement, line-end movement, and position reporting are
  cluster-oriented.
- 2026-05-07: Tightened cell-to-text snapping and display slicing to work at
  grapheme cluster boundaries rather than code point boundaries. Vertical CUA
  cursor movement now snaps the destination row to a canonical cluster
  boundary. Status-line HEXDISPLAY now reports the full focused cluster as
  textual code points and no longer draws the focused Unicode character into
  the status bar.
- 2026-05-07: Changed UTF-8 status HEXDISPLAY to a fixed 20-cell field that
  shows the focused cluster and no-leading-zero code points with `...`
  truncation. ASCII characters now use the same UTF-8 status path as emoji,
  flags, and combining clusters.
- 2026-05-07: Adjusted all successful file-area vertical cursor movement,
  including physical arrow-key `CURSOR ESCREEN UP/DOWN`, to snap backward to
  the start of a containing grapheme cluster instead of to the nearest edge.
  Mouse clicks remain nearest-edge snaps.
- 2026-05-07: Changed the slow UTF-8 file-area renderer to output whole
  grapheme clusters atomically with wide-curses string output. This replaces
  the regional-flag country-code fallback and keeps emoji shaping decisions
  with the terminal while reducing codepoint-boundary paint drift.
- 2026-05-07: Moved cell width accounting to grapheme-cluster boundaries.
  `TextPos.cell_column` now advances by cluster width, not by each component
  code point, so combining marks, regional-indicator flags, and emoji ZWJ
  sequences have stable shared cell positions. Added a skipped-by-default
  terminal cursor-advance probe for the flag/emoji overprint class of bugs.
- 2026-05-07: Adjusted ZWJ emoji width back to the terminal fallback width on
  macOS Terminal.app. ZWJ sequences remain one editing cluster, but their cell
  width follows what the terminal actually paints unless a runtime probe proves
  two-cell shaping is available.
- 2026-05-07: Added `THE_UTF8_FLAG_FALLBACK=1` as an opt-in diagnostic
  mitigation for terminals that paint regional-indicator flags as two-cell
  glyphs but only advance the cursor by one cell.
- 2026-05-07: Added per-cluster output-position reconciliation after UTF-8
  file-area writes. The renderer now explicitly moves the curses cursor to the
  next expected screen cell before writing following text, which prevents a
  later character from landing in the second cell of a correctly painted flag.
- 2026-05-07: Added per-cluster target-cell pre-clearing before file-area UTF-8
  writes. This addresses the visual case where a color emoji flag paints
  correctly but leaves pixels visible underneath the following character.
- 2026-05-07: Added a UTF-8 software block cursor for physical file-area
  clusters. The terminal hardware cursor is hidden for those clusters to avoid
  cursor compositing artifacts over and around emoji/flag glyphs.
- 2026-05-07: Added regional-flag overhang policy. Apple Terminal reports a
  two-cell cursor advance for flags but paints the flag artwork into the
  following cell, so THE treats literal regional flags as three display cells on
  Apple Terminal unless `THE_UTF8_FLAG_OVERHANG=0` or
  `THE_UTF8_FLAG_FALLBACK=1` is set.
- 2026-05-07: Split logical `TextPos.cell_column` from terminal display cells.
  Regional flags remain two logical cells for cursor movement, status, and
  API-facing positions; Apple Terminal's extra flag paint cell is now applied
  only when mapping logical positions to physical screen columns and back.
- 2026-05-09: Generalized terminal display-cell quirks behind build-time
  defaults and runtime overrides. macOS currently enables extra flag paint cells,
  two-cell emoji-presentation/keycap display, and two-cell couple/heart ZWJ
  display by default; other platforms default to the logical utf8proc model
  until validated.
- 2026-05-09: Updated the UTF-8 cluster writer to force the curses cursor to the
  physical display-policy width after each cluster, not the logical editor
  width. This keeps regional flags, keycaps, and shaped ZWJ clusters from
  leaking old logical-width assumptions back into the render path.
- 2026-05-08: Made the UTF-8 software cursor mode-aware. It is used only when
  the active cursor style is a block, so overwrite-mode block cursors still avoid
  terminal emoji cursor-compositing artifacts while insert-mode I-beam and
  command-line cursors use the terminal hardware cursor. Moving from the file
  area to the command line repaints if a software block cursor was visible, so
  the fake file-area cursor is cleared and the command-line hardware cursor is
  shown immediately.
- 2026-05-08: Introduced shared UTF-8 file-area cursor mapping helpers and moved
  `TEXT` command editing onto them. UTF-8 insert now writes at the resolved byte
  offset, and overwrite replaces the focused grapheme cluster instead of a raw
  byte or display cell. Command-line transitions now force a curses update after
  restoring the hardware cursor.
- 2026-05-08: Centralized cursor presentation selection in
  `current_cursor_presentation()`, so future terminal-specific policies can be
  added without duplicating mode checks across renderer and cursor visibility
  code.
- 2026-05-08: Clarified the regional-flag root cause: Apple Terminal reports
  normal two-cell cursor advance while visibly painting the color flag glyph into
  the following cell. The overhang policy is therefore a visual-display mapping,
  not a Unicode cluster-width change. Expanded the pseudo-terminal editing test
  to run with flag overhang enabled and cover overwriting the character after a
  flag.
- 2026-05-08: Fixed the Shift-Tab/TABFIELD command-line transition path, which
  bypasses `THEcursor_cmdline()`. It now clears any UTF-8 software block cursor
  and explicitly restores the command-line hardware cursor after moving fields.
- 2026-05-08: Tightened physical cursor placement after UTF-8 edits. The edit
  path now rebuilds the screen model before translating the post-edit `TextPos`
  to a terminal display column, which avoids stale pre-edit flag overhang data
  leaving insert-mode cursors in the wrong physical cell. The command-line
  handoff now forces a real command-window refresh after restoring visibility.
- 2026-05-08: Started a software-cursor POC. UTF-8 file-area and command-line
  cursor visibility are now THE-rendered rather than hardware-cursor rendered.
  The POC default follows 3270 convention: insert is block, overwrite is
  underline. `SET CURSORSTYLE MAINFRAME|MODERN|BLOCK|UNDERLINE` provides quick
  preset switching, and the status line displays `INS` while insert mode is on.
- 2026-05-08: Captured two POC requirements for the clean reimplementation:
  software cursors must draw a styled blank cell for EOL/virtual-space positions,
  and command-line focus changes must repaint the command-line cursor immediately.
- 2026-05-08: Fixed the `cursor home save` path used by the local Shift-Tab
  profile binding. That route entered the command line by changing
  `current_window` and rebuilding the screen model, but it did not repaint and
  flush the old file-area software cursor or the new command-line software
  cursor. The POC now has an explicit UTF-8 focus-transition repaint helper, and
  the design records the need for a first-class focus-transition component.
- 2026-05-08: Closed the software-cursor POC as good enough to guide the clean
  reimplementation. Remaining manual gaps are recorded: backspace/delete can
  hide the command-line cursor until a later repaint, and prefix-area entry still
  appears to use the hardware cursor. The clean implementation should treat all
  editor zones as software-cursor zones in UTF-8 mode.
- 2026-05-08: Started the clean software-cursor implementation. UTF-8 editor
  zones now route cursor presentation through `current_cursor_presentation()`;
  the hardware cursor is hidden for file area, prefix, and command line focus.
  `cursor_focus_capture()` snapshots the owning window before repaint, and the
  renderer draws software cursors for file-area cells, command-line cells,
  prefix cells, and EOL/empty file-area positions. Command-line text mutation,
  command-line focus transitions, TABFIELD focus transitions, and prefix edits
  now request cursor-aware repaints. `SET CURSORSTYLE
  MAINFRAME|3270|MODERN|BLOCK|UNDERLINE` was promoted from POC to documented
  configuration.
- 2026-05-09: Reapplied grapheme-cluster support to the clean implementation.
  `textpos.c` now exposes `TextCluster`, cluster navigation, cluster-aware
  cell snapping, and cluster-aware display slicing backed by utf8proc. The
  UTF-8 file renderer renders clusters atomically when they fit in a curses
  `cchar_t`, the status line uses a fixed 20-cell focused-cluster field with
  no-leading-zero `U+` code points and `...` truncation, F5 insert/overwrite
  toggles repaint the software cursor immediately, and file-area `TEXT`
  overwrite replaces the focused grapheme cluster instead of a raw byte.
- 2026-05-09: Rebuilt `tests/fixtures/utf8-render.txt` from scratch as a manual
  acceptance fixture with inline instructions and coverage for ASCII, accents,
  combining marks, CJK, emoji, modifiers, keycaps, flags, short ZWJ, two-face
  ZWJ, four-face ZWJ, mixed text, and horizontal clipping. Added
  `tests/test_utf8_fixture.c` so CTest validates the fixture remains valid
  UTF-8 and retains the required complex-cluster samples.
- 2026-05-09: Tightened the clean cursor invariant: file-area cursor movement,
  vertical entry, and mouse/screen entry should land only on grapheme-cluster
  starts. Codepoint-internal offsets remain valid internal positions for
  parsing, status, and editing calculations, but they are not legal interactive
  cursor stops. Also stopped packing clusters with multiple spacing code points
  into one curses `cchar_t`; those clusters are emitted as a sequence so curses
  and the terminal can shape flags and ZWJ-style sequences instead of exposing
  boxed component characters.
- 2026-05-09: Restored the status character field to show the focused cluster
  plus its no-leading-zero `U+` sequence inside the fixed 20-cell field, with
  textual truncation through `...`. The field is cleared before drawing the
  cluster so stale glyph pixels cannot survive a status refresh. The UTF-8
  file-area renderer also moved away from one buffered `wadd_wchnstr()` call for
  grapheme content: it now clears and writes each cluster at the cell column
  supplied by the text-position model, so following characters are placed from
  THE's utf8proc layout instead of inheriting curses' or the terminal's previous
  glyph state. A guarded `THE_UTF8_RENDER_TRACE` diagnostic records the live
  model/draw/cursor values for the remaining flag/keycap investigation.
- 2026-05-09: Reintroduced the POC's missing logical-to-physical file-area
  display mapping. `TextPos.cell_column` remains the utf8proc/editor model, but
  file rendering reserves wider physical paint footprints for macOS regional
  flags and one-cell emoji-presentation/keycap clusters. Software cursor hit
  testing remains logical and cursor painting uses the mapped physical column.
- 2026-05-09: Added brackets around the status-line focused cluster so manual
  testing can see both the cluster identity and how many status cells it uses.
- 2026-05-10: Added a UTF-8 redraw performance fast path. ASCII-only file-area
  lines now use THE's existing buffered line renderer and then overlay the
  software cursor, avoiding the grapheme-cluster renderer for ordinary source
  text. The UTF-8 cluster renderer also walks visible clusters sequentially and
  uses boundary-aware text-position helpers so it does not rescan from the start
  of the line for each visible cluster. One-codepoint clusters return their
  logical width immediately, and terminal policy environment overrides are
  cached after first use.
- 2026-05-10: Tuned the macOS heart/couple ZWJ display policy after Apple
  Terminal showed the two-face heart samples as separate visible face, heart,
  and face components while THE reserved only a shaped two-cell footprint.
  `TextPos.cell_column` still uses the utf8proc logical cluster width, but the
  macOS display policy now reserves six physical cells by default so following
  text and the status bracket do not overwrite the fallback components.
- 2026-05-10: Added the inverse file-area display mapping for mouse/screen
  entry. Mouse clicks arrive as physical terminal cells, so THE now translates
  them back through the same per-cluster display-width policy before snapping
  to a logical grapheme boundary. This keeps clicks around keycaps, regional
  flags, and other display-expanded clusters from landing one logical cell too
  far to the right.
- 2026-05-10: Stopped simple UTF-8 file-area left/right cursor movement from
  repainting the whole screen when horizontal scrolling is not involved. The
  path now repaints only the old and new cursor targets, using the same
  logical-to-physical display policy as full line rendering. This avoids
  re-emitting keycaps and other shaped/fallback clusters to the left of the
  cursor on every arrow-key step.
- 2026-05-10: Fixed a targeted software-cursor repaint regression where
  repainting a display-expanded cluster left the curses cursor at the end of
  the just-written cluster. The next arrow key then resolved the wrong logical
  cell, which could leave a stale cursor image behind and make right-to-left
  movement stick around keycaps. The fast path now restores the curses cursor
  to the physical cursor cell after repainting, and the manual fixture records
  the macOS Terminal keycap, flag, and two-face ZWJ regression lines.
- 2026-05-10: Fixed the status-line keycap regression. The file-area status
  lookup was still treating curses `x` as a logical cell, so after a keycap it
  reported the following logical character (`B` showed as blank, and the blank
  after `B` showed as the next `A`). Status now maps the physical display
  column back through `show_utf8_logical_col_from_display()` before selecting
  the focused cluster.
- 2026-05-10: Reverted attempted keycap fixes that changed the targeted repaint
  shape or forced whole-row redraw. Neither changed the macOS Terminal symptom,
  so the next step is movement/write-column instrumentation rather than another
  repaint guess. `THE_UTF8_RENDER_TRACE` now also records cursor-motion mapping
  and targeted cursor repaint columns.
- 2026-05-10: Added `utf8_terminal_probe`, a standalone curses/raw-terminal
  probe for keycaps, regional flags, ZWJ sequences, and related cursor advance
  cases. Future terminal-specific fixes should be proven in this harness before
  changing THE's renderer again.
- 2026-05-10: First Apple Terminal probe result: raw UTF-8 terminal output
  advances keycaps as two cells, while the tested ncurses wide-character paths
  (`waddwstr`, per-codepoint `wadd_wch`, and whole-cluster `cchar_t`) leave
  curses and Terminal.app using a one-cell keycap advance. In the keycap motion
  simulator, the curses path reports the cursor on `B` after `A1️⃣B`, but
  `CSI 6 n` reports Terminal.app two columns further right. The raw absolute
  UTF-8 path lands on the expected columns for `A`, keycap, `B`, blank, and the
  next `A`, so the remaining fix direction should be a terminal-absolute
  output/resync strategy, validated in the probe before touching THE again.
- 2026-05-10: Promoted the probe inputs into
  `tools/utf8_terminal_probe_cases.tsv` and added a terminal-absolute POC
  section. The POC uses raw absolute cursor-target redraws with reverse-video
  styling after either raw or curses base painting, so we can keep extending the
  fixture and compare platform behavior before changing THE's renderer.
- 2026-05-10: Tightened the terminal-absolute POC after the first visual run:
  raw overlay steps now move/query the terminal before any later curses refresh
  can reassert curses' one-cell keycap state, and the display includes a raw
  ASCII `^` marker below the target cell because Apple Terminal may not show
  reverse-video styling on color keycap emoji.
- 2026-05-10: Extended the POC after Apple Terminal showed correct raw cursor
  coordinates but damaged glyph repaint: selecting the keycap affected only one
  visual half, and later `B`, space, and `A` repaints could damage the preceding
  keycap cell even while the raw marker was in the right column. Probe version
  `2026-05-10-poc3` adds span-repaint rows to test whether clearing and
  redrawing a shaped neighborhood fixes the visual layer without changing the
  coordinate model.
- 2026-05-10: Added probe version `2026-05-10-poc4` with `--raw-poc`, a pure
  ANSI/UTF-8 mode that bypasses curses initialization. It tests whether Apple
  Terminal needs extra reserved cells after keycaps (`keycap_width=3` or `4`)
  and whether reverse-video emoji styling is itself the visual corruption
  trigger.
- 2026-05-10: Added probe version `2026-05-10-poc5` after the wider raw rows
  improved keycap painting but shifted the following cursor positions. This
  splits keycap paint reservation from cursor advance, hides the terminal
  hardware cursor during the raw POC, and adds `raw_paint3_cursor2_marker` and
  `raw_paint4_cursor2_marker` rows to test the hybrid policy directly.
- 2026-05-10: Applied the first THE-side validation of the split policy.
  `show_utf8_cluster_display_width()` remains the cursor/display-advance width,
  while file-area painting now uses a separate keycap paint footprint controlled
  by `THE_UTF8_KEYCAP_PAINT_WIDTH`/`THE_UTF8_KEYCAP_PAINT_WIDTH_DEFAULT`.
- 2026-05-10: Disabled the speculative keycap paint-footprint default after THE
  still failed the main fixture: the raw ANSI result did not prove that curses'
  virtual-screen model could replay the same layout. The split-policy code
  remains available behind `THE_UTF8_KEYCAP_PAINT_WIDTH`, but the default is
  neutral until the curses POC identifies a working model.
- 2026-05-11: Trimmed `--curses-poc` to the promising keycap rows and added
  THE-like block and underline software-cursor variants. Long scenario labels
  are now clipped before the sample area so the POC output itself does not
  pollute the glyph test. Probe version `2026-05-11-poc9` also clips the
  status line and adds keycap-backdrop rows that keep logical cursor advance at
  two cells while testing two- and three-cell visual cursor coverage for the
  keycap paint footprint.
- 2026-05-11: Rebuilt `--curses-poc` after the animated/raw-repair POC started
  corrupting its own screen. Probe version `2026-05-12-poc23` splits the
  tooling into `--utfvis selector` for static family comparison and
  `--testcursor selector layout_width cursor_width [mode]` for animated cursor
  motion. Keycaps and regional flags now use the same focused sample renderer,
  each target position is drawn in an independent cell, and the only cursor
  style is THE's normal background-cell cursor. The failed output-method
  selector and `keyspan` experiments were removed; `line` is the current
  complete-line redraw baseline. `flashline`, `flashcell`, `flashpair`, and
  `flashfrom0` through `flashfrom6` force a blank/refresh boundary before
  repainting the whole sample, the changed targets, a small changed-target span,
  or a suffix of the sample. Apple Terminal testing showed `flashfrom0` and
  `flashfrom1` work for keycaps, while `flashfrom2` fails, so the minimum safe
  repaint boundary appears to be the keycap cluster itself. Probe version
  `2026-05-11-poc21` also fixes the `flashfrom1` harness artifact where the
  leading `A` could keep stale cursor highlighting because it sits outside the
  repainted suffix.
- 2026-05-11: Added `--testchain`, which repeats the selected grapheme cluster
  in `A-cluster-B-A-cluster-B-A-cluster-B` and animates a THE-style cursor
  across the chain. Its `flashbackcluster0..6` modes blank/repaint a suffix
  starting at the changed cluster or at the nearest prior grapheme cluster(s),
  which tests the Apple Terminal keycap failure as a composition-state-machine
  boundary rather than a byte-position problem. Probe version
  `2026-05-12-poc23` adds `flashfirstcluster` and `flashwhole` after testing
  showed `flashbackcluster2` can still fail at the final `B`, apparently by
  collapsing the cluster before it. These modes check whether the safe boundary
  is the first poison/keycap cluster in the rendered run or the full run start.
- 2026-05-12: Extended `--testchain` in probe version `2026-05-12-poc24` to
  seven repeated clusters with leading/trailing `XX` and a deliberate
  three-cluster run with no ASCII separators. This should make the keycap
  repaint-boundary result definitive enough to guide THE's reset strategy.
- 2026-05-12: Added first-cluster reset strategy variants in probe version
  `2026-05-12-poc25` to find the cheapest working reset. The sequence to test
  is `paintfirstcluster`, `touchfirstcluster`, `clearfirstcluster`,
  `flashfirstfast`, and finally the known-working `flashfirstcluster`.
- 2026-05-12: Added probe version `2026-05-12-poc26` with `--calibrate`.
  Calibration mode walks selected feature classes through view/layout choice,
  cursor-walk repaint strategy, and replacement repaint strategy, then writes
  a proposed THE terminal-profile instruction fragment.
- 2026-05-12: Added probe version `2026-05-12-poc27` with cleaner calibration
  strategy switching. Changing cursor or replacement strategy now clears the
  stale sample and restarts the animation from the beginning. Strategy screens
  also show a lower-is-faster preference score so the user can choose the
  cheapest strategy when several look correct.
- 2026-05-12: Added probe version `2026-05-12-poc28` with a calibration main
  menu and profile loading. Calibration now reads the current profile, lists the
  selected feature classes with their active settings, lets the user configure
  one class at a time, and saves only on explicit request. The built-in sample
  taxonomy was expanded to sixteen initial classes including ambiguous width,
  tag flags, private-use glyphs, and separate short/heart/family ZWJ groups.
- 2026-05-12: Added probe version `2026-05-12-poc29` to fix profile-loaded
  ZWJ display policy strings so non-ZWJ classes keep `n/a` after a load/save
  cycle.
- 2026-05-12: Added probe version `2026-05-12-poc30` with coded defaults for
  every built-in calibration family. The main menu now shows whether each class
  is at its default or is overridden, and profile saves write only classes that
  differ from the coded defaults.
- 2026-05-12: Added probe version `2026-05-12-poc31` with a simplified
  command-line surface. Bare invocation now enters calibration, and the primary
  verbs are `calibrate`, `list`, `view`, `cursor`, and `chain`; the old
  dashed probe flags remain as compatibility aliases for investigation notes.
- 2026-05-12: Added probe version `2026-05-12-poc32`. ZWJ calibration now asks
  for the display policy before width/repaint screens and uses that policy's
  effective display bytes for the rest of the class. The view chooser includes
  wider integer candidates up to twelve cells for family/long-ZWJ fallbacks, and
  non-visual calibration reports loaded settings without rewriting the profile.
- 2026-05-13: Added probe version `2026-05-13-poc33`. ZWJ calibration entries
  are now intent-qualified: each built-in ZWJ class has separate `group/native`
  and `components/expanded` rows. Group rows can switch to `substitute` and then
  skip ZWJ-specific physical measurement; component rows can switch between
  native fallback and explicit expanded output.
- 2026-05-13: Added probe version `2026-05-13-poc34`. The ZWJ output-method
  screen now renders each candidate using that output method's own physical
  L/C and repaint settings, shows those settings in the row title, and displays
  a preference score so the user can choose the cheapest working method.

## Future Performance Roadmap

The immediate ASCII fast path has removed the worst visible redraw regression
for ordinary source files. The next performance step is a per-line UTF layout
cache for non-ASCII lines.

Proposed cached data per physical line:

- Line identity and revision/generation so cached layout is invalidated when
  the line text changes.
- A compact vector of grapheme clusters with byte range, code point range,
  cluster index, logical cell range, logical width, display-policy width, and
  small flags such as ASCII, regional flag, keycap, emoji presentation, ZWJ,
  and heart/couple ZWJ.
- Optional lookup accelerators from logical cell to cluster and byte offset to
  cluster for cursor movement, mouse hit-testing, status display, and editing.
- A fast ASCII marker so the renderer can bypass cache allocation entirely for
  pure ASCII lines.

The cache should be owned near the line model rather than by the renderer. The
renderer, cursor code, status line, and editing commands should all consume the
same cached layout view so they do not disagree about byte, cluster, logical
cell, and physical display positions.

Invalidation rules:

- Editing a line invalidates only that line's layout cache.
- Changing terminal display policies, cursor display policy, or platform
  compatibility knobs invalidates display-width fields, but not byte or cluster
  segmentation.
- Horizontal scrolling and redraw should not rebuild line layout; they should
  slice the cached cluster vector.
- Syntax highlighting and target highlighting may stay separate because they
  are colour overlays, not text segmentation.

Testing requirements:

- Unit tests should cover cache invalidation after insert, replace, delete, and
  line reload.
- Fixture tests should assert that cached and uncached layout produce the same
  byte, code point, cluster, logical cell, and display cell results for ASCII,
  combining marks, CJK, flags, keycaps, and ZWJ clusters.
- Manual testing should compare redraw responsiveness at top-of-file ASCII
  sections and mixed UTF sections before/after cache enablement.

## Appendix: Earlier Implementation Slice

This older slice is retained for context. The current near-term plan is the
probe/calibration work above, followed by the smallest validated THE renderer
change.

- Add the per-line UTF layout cache described above, initially behind a build or
  runtime switch if useful for comparison.
- Continue moving focus changes into explicit cursor/focus entry points for file
  area, prefix, command line, status/prompt contexts, and any dialog/editfield
  contexts that own cursor visibility.
- Make the cursor component fully responsible for software-cursor state, stale
  cursor cleanup, zone repaint, command-line mutation repaint, and final refresh.
  The current implementation has the shared capture/redraw path, but some legacy
  callers still assign `current_window` directly and should be audited.
- Finish auditing editing/input paths that still treat incoming UTF-8 text as
  individual bytes; the clean file-area `TEXT` path is now cluster-positioned,
  but multi-byte typed input should become a first-class unit.
- Add automated/integration coverage for cursor placement, command-line
  backspace/delete repaint, prefix entry, mouse hit-testing, wide characters,
  combining marks, regional flags, and virtual space.
- Broaden the terminal probe into platform-specific regression evidence for
  macOS, Linux, and Windows curses builds.
