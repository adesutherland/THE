# UTF-8 Enablement Handover

Last updated: 2026-05-21.

## Current State

THE's UTF-8 work is split into two models:

- Logical editor model: grapheme-cluster based, shared across platforms, using
  `TextPos` and utf8proc.
- Physical terminal model: terminal-specific cell widths, cursor widths, output
  methods, cursor repaint strategies, and replacement repaint strategies.

Do not fix terminal paint problems by changing logical cluster segmentation.
The logical model stays stable; terminal quirks belong in terminal profiles.

THE now has a shared UTF-8 repair planner in `src/utfrepair.c`. Cursor
movement and replacement/full-line redraw ask this planner how far left the
physical repair must start, then `show.c` executes the plan. Strategy meaning
should be added or changed there first, not duplicated in renderer branches.
All feature classes, including ASCII, can use the same strategy machinery; the
ASCII fast path is only an optimization when the active ASCII profile is the
native one-cell `cells` default.

The probe is expected to mirror that model. Its calibration UI presents output
and strategy choices consistently across classes; ZWJ working and keycaps
failing should be treated as profile/strategy evidence, not as permission to add
class-specific renderer behavior.

Cursor investigation now records both coordinates: the logical editor cell used
to select the `TextCluster`, and the physical terminal display column used to
paint the software cursor. This keeps the keycap case visible without changing
logical text positioning: a keycap may remain one logical cell while the terminal
profile gives it a two-cell layout and cursor footprint.

The file-area cursor still needs a fuller logical cursor layer. The current UTF
left/right path now carries the target logical cell through repaint and uses the
physical curses cursor only as a parked implementation detail, but other cursor
commands may still derive their live position from curses. Long term, file-area
cursor state should be `{row, logical_cell}` first, with physical display columns
computed only by the renderer/driver.

The first driver-boundary slice is now present. `src/utflayout.c` owns pure
logical-to-physical UTF cell mapping without curses calls. `src/cursesdriver.c`
wraps file-area curses cursor target calculation, movement, cursor repaint
transitions, and refresh. `src/show.c` keeps its existing public helpers but
delegates layout mapping to `utflayout`. `src/llmdriver.c` adds a passive
LLM-friendly screen snapshot plus normalized text/key/command input structures;
it is not yet wired into the live input loop.

The generic suffix-style cursor repair now follows the probe order: clear the
selected suffix, flush that blank state when requested, repaint the suffix in
normal attributes, then overlay the new software cursor target. This avoids
mixing keycap/flag/ZWJ glyph repaint with cursor styling during the suffix pass.

Cursor strategy selection uses the shared strategy ranking for the affected
visible line prefix, not only the old and new cursor cells. This matters for
ASCII-to-ASCII motion immediately after a troublesome cluster: moving from `B`
to the following space in `A keycap B space` still inherits the keycap repair
strategy because the terminal state to the left can affect the physical cells
being touched. The same rule applies after line end: synthetic visible cells
past record end still use the line prefix, so a keycap/flag/ZWJ earlier on the
visible line can keep the conservative repair strategy active.

UTF file-area cursor movement now distinguishes logical viewport start from
physical display visibility. `verify_col` remains a logical editor column, but
the UTF branch of `execute_move_cursor()` asks the curses driver whether the
target logical column is physically visible on the current line. That decision
uses `src/utflayout.c`, so a line containing keycaps can shift the logical
viewport before the old logical-only `column_in_view()` test would have done so.
This is intentionally line-specific: the same logical cursor column may have a
different physical display column on a plain ASCII line, a keycap line, a flag
line, or a ZWJ/substitute-output line.

Temporary cursor tracing has been removed from the baseline, but the important
finding should be kept. The macOS build uses ncurses (`USE_NCURSES=1`, linked
to `/usr/lib/libncurses.5.4.dylib`), not PDCurses. The focused trace across the
keycap fixture showed THE advancing the logical target and curses-driver display
target correctly through end-of-line. For example, a move to logical cell 25 on
the three-keycap line mapped to display column 28, the virtual after-EOL span
was selected, and the driver then moved to display column 28. The visible jump
therefore does not look like a logical cursor or viewport calculation failure.

The trace also showed why the diagnostic dot experiment was useful but not a
real fix. Painting after-EOL cells as `.` suppressed the symptom because the
terminal had visible non-space glyphs to materialize, while repainting normal
spaces still left the issue. Span-clearing and per-cell blank trials did not
produce a durable repair, so the remaining bug appears to sit below logical
mapping in THE's curses refresh/materialization path or in terminal treatment of
blank cells after keycap glyphs.

The latest file-area repair path bounds suffix-strategy clearing to the visible
span that can actually be affected by the old cursor, the new cursor, and the
current logical line end. That keeps THE closer to the working probe sequence
and avoids asking ncurses to clear long real-trailing-space runs when a short
cursor repair is enough. File-area `SOS DELBACK` and `SOS DELCHAR` now use the
logical UTF cursor and `TextPos` cluster byte ranges before mutating `rec`,
instead of deriving edit offsets from the curses physical column.

The next useful keycap step is a minimal one-line demonstrator, ideally with a
single carefully chosen line containing keycaps followed by real spaces and then
after-EOL cursor movement. Compare that against the probe's working
`first`/`whole` paths by recording the logical cursor request and the physical
curses operations emitted by the driver. THE must produce the same physical
sequence through the curses driver, without changing or depending on ncurses
internals and without putting class-specific keycap behavior into the logical
layer.

## Important Artifacts

- `doc/utf-design.md`: detailed design, findings, and historical log.
- `doc/cursor-driver-architecture.md`: approved logical UI/physical driver
  separation plan and ownership rules.
- `tools/utf_terminal_probe.c`: interactive terminal calibration/probe tool.
- `src/utfterm_defaults.h`: shared THE/probe coded default physical terminal
  table.
- `src/utflayout.c`, `src/cursesdriver.c`, `src/llmdriver.c`: first driver
  boundary modules for UTF layout, curses materialization, and an LLM-oriented
  screen/input surface.
- `system-osx.the`: macOS system UTF-8 profile consumed by THE and generated
  by the probe.
- `tests/fixtures/utf-render.txt`: manual editor fixture for UTF-8 rendering.
- `tests/test_utfrepair.c`, `tests/test_utfterm.c`, `tests/test_utf_fixture.c`,
  `tests/test_utflayout.c`, `tests/test_llmdriver.c`, and `tests/test_textpos.c`:
  repair planning, terminal-profile, fixture, layout, driver, and text-position
  regression coverage.

## macOS Apple Terminal Baseline

The current macOS baseline is `system-osx.the`. It is a complete system
profile, not a defaults-plus-overrides pair. Important observed choices include:

- `regional-flag`: default `L3 C3`, cursor `cells`, replacement
  `suffix`.
- `keycap`: `L2 C2`, cursor `first`, replacement
  `first`.
- `modifier`: `L4 C4`.
- ZWJ grouped display: use `substitute` for `short-zwj`, `heart-zwj`, and
  `family-zwj`.
- ZWJ component display: `short-zwj` uses `native L4 C4`; `heart-zwj` uses
  `expanded L6 C6`; `family-zwj` uses `expanded L8 C8`.

## Probe Usage

Build the probe:

```sh
cmake --build cmake-build-debug --target utf_terminal_probe -j2
```

Open interactive calibration:

```sh
./cmake-build-debug/utf_terminal_probe calibrate all \
  --profile-dir ./cmake-build-debug/release
```

On macOS this reads and writes `system-osx.the` in the selected directory. THE
loads that system profile before the user profile; `-n` suppresses only the
user profile. The install rule copies the build release copy, so calibrating
`cmake-build-debug/release/system-osx.the` before install carries that generated
profile into the installed `share/the` directory.

Validate the saved macOS profile non-visually:

```sh
./cmake-build-debug/utf_terminal_probe calibrate all --no-visual \
  --profile system-osx.the
```

Add `--write-profile` to make a scripted non-visual run rewrite the profile
after validation.

## Next Work

The active refactor is the logical UI/physical driver split. Treat this section
and `doc/cursor-driver-architecture.md` as the stored implementation plan when
resuming after context compression.

The target architecture is:

```text
editor command -> logical UI model -> physical driver
physical input -> normalized input event -> editor command
```

The logical layer owns focus, file line, logical screen row, row role, logical
`TextPos`, desired horizontal cell, and logical viewport. Editor commands should
not read or move curses windows. The curses driver owns physical columns,
software cursor painting, UTF repair execution, refresh ordering, hardware
cursor parking, and all curses calls. The LLM driver must use the same logical
screen and normalized input model.

Execution is intentionally stepwise, with a build, CTest run, and commit after
each meaningful step:

1. Document the architecture and add guardrails.
2. Add logical UI frame and fake-driver operation types with unit coverage.
3. Route file-area cursor movement through logical requests.
4. Route file-area editing through logical `TextPos` positions.
5. Consolidate software cursor painting into one driver-owned path.
6. Bring prefix and command-line cursor/editing behavior under the same model.
7. Normalize curses, mouse, and LLM input through a shared event type.
8. Tighten the guardrails so editor logic cannot call curses directly.

Current progress before this sequence: virtual `TextPos`, passive
`LogicalCursorState`, `utflayout`, an initial `cursesdriver` adapter, and passive
`llmdriver` structures exist with unit coverage. They are foundations, not the
finished architecture. Runtime cursor code still has multiple physical paths and
must be migrated.

After each step run:

```sh
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
```

## Sequencing Advice

Finish the macOS Apple Terminal path in THE first, then probe Linux and Windows.
The macOS baseline is currently the best-evidenced terminal profile, and it has
a known manual fixture that reproduces the hard keycap/ZWJ behavior. Proving the
complete loop once is more valuable than collecting more platform data before
THE can consume a profile.

The macOS pass must still be profile-driven. Do not hard-code Apple Terminal
rules directly into rendering paths. Treat macOS as the first proof of the
configuration architecture:

1. Load coded defaults.
2. Apply the macOS override fragment.
3. Render from the resulting physical profile table.
4. Validate view, cursor walking, and replacement separately.
5. Only then add Linux, Windows Terminal, iTerm2, or other terminal baselines.

## Known Cautions

- The baseline is visual and terminal-specific. It should not be generalized to
  Linux, Windows Terminal, iTerm2, or other curses stacks without calibration.
- Cursor movement success does not prove replacement safety. Replacement has a
  separate strategy field for that reason.
- `substitute` is a physical display choice for any class/display. The file
  bytes and logical grapheme cluster remain unchanged.
