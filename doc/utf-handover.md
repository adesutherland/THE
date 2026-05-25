# UTF-8 Enablement Handover

Last updated: 2026-05-25.

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

The file-area cursor now has a logical-first layer. UTF left/right movement,
logical cursor repaint, text insertion, `SOS DELBACK`, and `SOS DELCHAR` prefer
`VIEW_DETAILS.logical_cursor` and convert to byte offsets through `TextPos`.
The physical curses cursor is still parked by the curses driver, but the logical
row/cell is the authority for the migrated UTF paths. `execute_move_cursor()`
now derives the file-area row from logical cursor/focus state in both UTF and
no-UTF builds, and materializes the cursor through `cursesdriver.c`.
`execute_makecurr()` and `rearrange_line_blocks()` preserve cursor position by
logical file-area/prefix cells instead of capturing and restoring physical
curses coordinates. `insert_new_line()` now derives its target row/cell from
logical focus state and materializes file-area or prefix focus through the
driver. `selective_change()` now moves its confirmation prompt cursor from the
match's logical `TextPos` cell and driver viewport visibility. The remaining
`execute.c` OS shell bridge, `EDITV LIST` screen, popup placement, and
popup/dialog transient-window mechanics now route through driver-owned physical
wrappers rather than direct curses calls. Other legacy cursor commands still
need migration before the boundary can be made strict.

The first driver-boundary slices are now present. `src/utflayout.c` owns pure
logical-to-physical UTF cell mapping without curses calls. `src/uidriver.c`
defines logical row roles, frames, cursor overlays, and fake-driver operation
logs. `src/screenframe.c` builds live `UiFrame` snapshots from THE's current
file-area rows. `src/cursesdriver.c` wraps file-area curses cursor target
calculation, movement, software cursor cell painting, UTF/ascii cell write/fill
primitives, render-entry cursor save/restore, renderer attribute/touch/refresh
mechanics, cursor repaint transitions, refresh, window cursor capture/restore,
window origin/size reads, and input timeouts. `src/show.c` keeps its existing
public helpers but now builds a live frame during full file-area redraw and
uses it to select file-area and prefix software cursor overlays. The renderer
no longer falls back to the old `cursor_focus_*` physical snapshot group when
painting software cursor cells; frame-backed cursor target selection is covered
by `test_virtual_screen`. Targeted command-line redraw uses the live logical
command cursor directly; targeted prefix redraw builds a `UiFrame` when
possible; SDSLH bracket highlighting uses logical file focus instead of
`get_cursor_position()`; and UTF whole-line cursor-repair repaint can reuse a
frame-backed overlay. Status/HEXDISPLAY now inspects the live logical
file-area, prefix, or command cursor when available, and
`prepare_view()`/`advance_view()` now preserve and restore view-switch cursor
positions from logical cursor targets before falling back to old window
snapshots. Narrow legacy fallbacks remain for status/view-switch paths that
still use physical cursor snapshots instead of a logical area model.
`src/inputevent.c` owns normalized text/key/command/logical-hit/debug events
and legacy key conversion. The live curses loop now reads through
`cursesdriver.c` and normalizes collected key codes through `TheInputEvent`
before passing the equivalent legacy key to the existing dispatcher. Mouse and
resize key behavior is preserved at the compatibility-key level; logical mouse
hit events remain later work. `src/llmdriver.c` now exposes role-aware semantic
snapshots, compact token-saving view modes, shared normalized input wrappers,
cursor mapping diagnostics, and driver operation log formatting.

The virtual UI harness baseline is now in place as `test_virtual_screen`.
`src/uidriver.c` has logical `divider` and `window` row roles, and
`src/inputevent.c` has target-kind metadata for file-area, prefix, command,
status, tabline, divider, and window logical hits. The harness builds
driver-neutral `UiFrame` states for file rows, prefixes, command/status rows,
tabline, divider/window rows, and UTF keycap/ZWJ fixture rows; then it verifies
semantic LLM views, compact modes, cursor overlays, logical-hit payloads, and
fake-driver/debug operation logs without curses. This is the new accelerator:
renderer migration should use this and related no-curses tests to remove whole
legacy fallback groups instead of keeping both logical and physical authority
alive in the dangerous middle ground.

`the_agent` is useful as a no-curses proof target, but it is not yet a complete
replacement for the full editor integration path. It can exercise logical file
and command focus, normalized key/text input, and a small command subset, but it
does not yet route arbitrary THE commands or SOS commands such as
`SOS TOPEDGE` through the real command dispatcher. The agent now exposes this
boundary directly: `capabilities` reports the supported agent subset and an
unsupported command returns a stable diagnostic with a capabilities hint. Use it
as an extra LLM-driver smoke layer, not as the only proof for command behavior.

The macro/agent visibility layer has two distinct message surfaces. THE message
history remains available through `EXTRACT /MESSAGES/` and `QUERY MESSAGES`.
SDSLH syntax diagnostics are available through `SDSLHWAIT`, `EXTRACT /PMSGS/`,
and `QUERY PMSGS`; `pmsgs.n` records include line, column, severity, code, and
message text. The SDSLH diagnostic collector walks the parse tree, so scripts
can see zero-length parser diagnostics as well as messages attached to visible
tokens.

CREXX/pty tests remain the stronger full-editor integration surface for command
and SOS behavior because they run through the live command processor. Their
limitations should be made explicit in tests and docs when they matter: they
require a CREXX-enabled build, a working CREXX compiler/import directory, and a
pty-capable host; failures can be compiler/interface errors rather than editor
regressions; and they do not prove no-curses behavior. Skip messages should
name the missing prerequisite rather than collapsing all failures into a generic
test skip. Prefer CREXX for full editor command coverage and `the_agent` for
driver-boundary/no-curses coverage.

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
- `doc/llm-driver-agent-guide.md`: agent-facing LLM driver requirements,
  semantic snapshot contract, input model, and debugging commands.
- `tools/utf_terminal_probe.c`: interactive terminal calibration/probe tool.
- `src/utfterm_defaults.h`: shared THE/probe coded default physical terminal
  table.
- `src/utflayout.c`, `src/uidriver.c`, `src/screenframe.c`,
  `src/cursesdriver.c`, `src/inputevent.c`, `src/llmdriver.c`: first driver
  boundary modules for UTF layout, logical UI frames, live frame capture,
  curses materialization, normalized input, and an LLM-oriented semantic
  screen/debug surface.
- `system-osx.the`: macOS system UTF-8 profile consumed by THE and generated
  by the probe.
- `tests/fixtures/utf-render.txt`: manual editor fixture for UTF-8 rendering.
- `tests/test_utfrepair.c`, `tests/test_utfterm.c`, `tests/test_utf_fixture.c`,
  `tests/test_utflayout.c`, `tests/test_inputevent.c`,
  `tests/test_llmdriver.c`, `tests/test_virtual_screen.c`,
  `tests/test_agentdriver.c`,
  `tests/test_the_agent_capabilities.sh`, and `tests/test_textpos.c`: repair
  planning, terminal-profile, fixture, layout, normalized input, virtual
  screen/fake-driver, agent-surface, and text-position regression coverage.

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

The active refactor is the logical UI/physical driver split. Treat this section,
`doc/cursor-driver-architecture.md`, and `doc/llm-driver-agent-guide.md` as the
stored implementation plan when resuming after context compression.

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
screen and normalized input model. It is a first-class UI driver: it should
return semantic, deterministic, token-aware screen snapshots; accept normalized
logical input; and expose debug/introspection commands such as cursor mapping,
visible row listing, pending driver operations, and last-render explanation.
Normal LLM scrolling should use compact `filearea` views with prefixes,
command/status chrome, and long text omitted unless requested.

Execution is intentionally stepwise, with a build, CTest run, and commit after
each meaningful step. Current checkpoint status:

1. Document the architecture and add guardrails: done.
2. Add logical UI frame and fake-driver operation types with unit coverage:
   done.
3. Route file-area cursor movement through logical requests: initial UTF
   left/right path done.
4. Route file-area editing through logical `TextPos` positions: initial text,
   delete-back, and delete-char path done.
5. Consolidate software cursor painting into one driver-owned path: partial.
   Full file-area redraw now builds a live `UiFrame` and uses it for file-area
   and prefix overlay ownership; software-cursor attribute, cell painting,
   UTF/ascii cell write/fill primitives, and render-entry cursor save/restore
   helpers now live in `cursesdriver.c`. Renderer attribute/touch/refresh calls
   also go through driver helpers. Targeted command-line redraw, targeted
   prefix redraw, SDSLH bracket matching, and UTF whole-line cursor repair now
   use logical or frame-backed cursor data. Broader targeted redraws still need
   to become driver-level logical render requests instead of relying on legacy
   snapshots.
6. Bring prefix and command-line cursor/editing behavior under the same model:
   done for the normal text-entry surface. Focus has logical cursor state,
   `TEXT` edits for command-line and prefix areas now mutate the logical
   command/prefix buffers and redraw through the area display helpers, and
   `FIELD`/`FIELDWORD`/line-column reporting prefer logical cells. Remaining
   command and prefix work is now mostly SOS/key-navigation cleanup, not the
   core area ownership model.
7. Normalize curses, mouse, and LLM input through a shared event type: partial.
   `src/inputevent.c` owns the shared event type, legacy key conversion, and
   queue; LLM wrappers use it. The main curses key path now normalizes through
   `TheInputEvent` before legacy dispatch. Remaining work is direct normalized
   dispatch where safe and logical mouse-hit event routing.
8. Tighten the guardrails so editor logic cannot call curses directly: pending.
9. Add a no-curses agent proof target: done. `the_agent` opens files, accepts
   normalized agent input, emits semantic LLM snapshots, and links no curses
   library or curses driver source. It covers file-area and command-line focus,
   including command cursor movement and Enter submission. It proves the logical
   editor/LLM surface can function independently while the full curses editor is
   still being migrated.
10. Migrate ordinary `execute.c` cursor effects: partial. Six safe slices are
   complete and committed:
   - `76a5425 Route execute cursor moves through logical row`
   - `7208f7e Preserve makecurr cursor via logical cells`
   - `41cc276 Preserve block cursor via logical cells`
   - `91ba101 Route insert line cursor through logical state`
   - `d88abf7 Route selective change prompt cursor logically`
   - `9101f5b Route execute transient windows through driver`
   These moved `execute_move_cursor()`, `execute_makecurr()`, and the ordinary
   block copy/move/delete cursor-preservation path, `insert_new_line()`, and
   `selective_change()` prompt placement away from physical cursor state.
   CREXX/pty coverage was extended in `tests/test_normal_area_queries.sh`,
   `tests/test_sos_navigation_queries.sh`, and
   `tests/test_selective_change_prompt.sh`. The OS suspend/resume bridge,
   `EDITV LIST`, popup placement, and popup/dialog mechanics are still physical
   behavior, but they now go through `cursesdriver.c` wrappers rather than
   direct curses calls from `execute.c`.

Runtime cursor code still has multiple physical paths and must be migrated.
`src/cursor.c`, `src/comm5.c`, `src/query1.c`, `src/query2.c`, and `src/edit.c`
no longer contain direct `getyx`, `wmove`, `getbegyx`, `getmaxx`, `getmaxy`,
or `wtimeout` calls, but many other legacy command, SOS, utility, mouse, and
renderer paths still do. The guardrail test is intentionally permissive while
the live renderer is still being split; tighten it only after the remaining
command, prefix, mouse, and renderer paths have driver-owned equivalents.

Aggressive migration sequence:

The project is now far enough into the split that small helper-by-helper slices
are more dangerous than larger test-visible moves. New slices should migrate a
whole behavior group when that group can be observed through one of these
surfaces in the same commit:

- `the_agent` or `llmdriver` semantic snapshots for no-curses behavior.
- a fake/virtual screen CTest that builds a `UiFrame`, drives normalized input,
  and compares cursor/focus/output or fake-driver operation logs.
- CREXX/pty CTests for full-editor command dispatch that cannot yet run through
  `the_agent`.
- focused unit CTests for `inputevent`, `uidriver`, `screenframe`, `utflayout`,
  and driver adapters.

Preferred order, with larger slices:

1. Use the virtual UI harness as the renderer migration accelerator.
   `test_virtual_screen` now proves file-area, prefix, command, status,
   tabline, divider/window, UTF fixture rows, compact LLM views, cursor
   overlays, logical hits, and fake-driver operation logs without curses. The
   next renderer slices should extend this harness for the target behavior and
   remove the matching legacy fallback group in the same commit.
2. Expand `the_agent` from an agent subset toward a real command/input bridge.
   Route normalized text/key/command events through shared editor input paths
   where possible, and keep explicit capability output for anything still
   unsupported. Each newly supported command or focus behavior needs an agent
   script CTest plus, where relevant, a CREXX full-editor CTest.
3. Move curses input from compatibility-key normalization to normalized-event
   dispatch in groups: text, named navigation keys, command submission, prefix
   editing, then mouse logical hits. Preserve legacy key definitions at the
   boundary, but make dispatch decisions consume `TheInputEvent` when the
   behavior is covered by agent/inputevent CTests.
4. Convert `show.c` fallbacks by render group, not by isolated helper:
   status/view-switch fallbacks, full-screen render entry/exit, and UTF repair
   repaint should all become frame-backed or driver-request-backed. Add virtual
   renderer/fake driver CTests before tightening each fallback.
5. Retire active-driver query/materialization fallbacks in non-renderer command
   code once the same row/cell behavior is visible through logical queries,
   agent snapshots, or CREXX query tests. Tighten
   `tests/check_curses_boundary.sh` immediately for each fully migrated module.
6. Introduce logical popup/dialog/window-lifecycle models as a single larger
   slice only when they can be exposed in LLM snapshots and virtual screen
   tests. Until then, keep those mechanics driver-owned physical behavior.
7. Keep manual terminal smoke as the final paint check, not the primary safety
   net. Any manual regression found during this phase should produce a CTest or
   explicit agent capability disclosure before the area is considered closed.

After each step run:

```sh
cmake --build cmake-build-codex-debug -j2
ctest --test-dir cmake-build-codex-debug --output-on-failure
cmake --build cmake-build-noutf8 -j2
ctest --test-dir cmake-build-noutf8 --output-on-failure
```

Latest verification after the virtual UI/fake-driver harness slice: UTF
build/CTest was green, 32/32. no-UTF build/CTest was green, 17/17 with
SDSLH-dependent tests skipped as intended. Focused LLM/`the_agent` smoke passed
as part of both full CTest suites.

## Suggested Next Slices

Use bigger, CTest-backed slices. A good slice now has three parts: expose the
behavior through agent/LLM or a virtual screen, migrate the real path, then
tighten a guardrail or capability declaration.

High-value next slices:

- Renderer fallback purge: extend `test_virtual_screen` for each render group,
  then remove the corresponding `show.c` non-frame fallback path in the same
  commit. The non-frame `cursor_focus_*` software-cursor overlay fallbacks have
  been retired. Good next targets are status/view-switch physical snapshot
  fallbacks and render entry/exit logical decisions. Keep physical cursor
  save/restore, refresh, touch/update, UTF/ascii cell writes, software cursor
  painting, and cursor parking in `cursesdriver.c`.
- Agent command bridge: support a first real group of full-editor commands or
  SOS commands through normalized agent input, with `the_agent` CTests proving
  no-curses behavior and CREXX CTests proving full-editor parity where needed.
- Normalized mouse/input group: model curses mouse clicks as logical-hit events
  for file area, prefix, command line, status, file tabs, divider, and window
  selection. Add focused `inputevent`/adapter tests before connecting
  `process_key()` to consume those events.
- Guardrail ratchet: after each group lands, extend
  `tests/check_curses_boundary.sh` for the fully cleaned module or behavior
  class instead of waiting for the entire migration.

Suggested verification remains both full build trees, both full CTest suites,
and the focused LLM/`the_agent` smoke in UTF and no-UTF builds.

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
