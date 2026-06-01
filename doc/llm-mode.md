# LLM Mode Agent Guide

Last updated: 2026-05-29.

This document describes the intended LLM-facing mode for THE. It is written for
agents and tool authors that need to inspect editor state and drive editor input
without depending on curses escape sequences, physical terminal columns, or
screen scraping.

For active migration status and next closable tasks, read
`doc/utf-handover.md`.

## Status

Implemented:

- `src/llmdriver.c` and `src/llmdriver.h` format semantic snapshots,
  token-saving view modes, normalized input wrappers, and debug diagnostics.
- `src/inputevent.c` owns the shared text/key/command/logical-hit/debug event
  model.
- The normal live curses mouse path converts terminal packets into the same
  `TheInputEvent` logical-hit targets used by the LLM/agent surfaces for
  file-area, prefix, command, status, tabline/filetabs, divider, and window
  hits.
- `src/uidriver.c` and `src/screenframe.c` provide the logical frame surface
  used by the LLM formatter and virtual/fake-driver tests.
- `src/transientui.c` provides no-curses readv/dialog/popup snapshots with
  geometry, row roles, focus, selected/active button or item, edit text, popup
  viewport offsets, and logical hit targets.
- `the_agent` is the first live no-curses proof target. It is interactive over
  stdin/stdout, links no curses library or curses driver, and uses the same
  `LlmDriverScreenView` and `TheInputEvent` contracts.
- `the_llm_headless` is a no-curses executable skeleton for the broader
  headless direction. It links the transient UI model and is checked for curses
  dependencies and curses-driver symbols. `--mini-session` performs a
  no-curses edit/save session against a file and reports the resulting
  semantic state.
- `doc/llm-headless-capabilities.md` is the concise capability inventory for
  the current agent/headless editor surface.

Agent surface:

`the_agent` is an agent subset, not a runtime switch inside the full curses
editor and not THE's full command dispatcher. It supports file-area, prefix,
and command-line focus, logical hits, normalized key/text input, file
open/save/write, search/find navigation, replace, line operations, prefix
command subset execution, selection/range operations, bounded agent-side
undo/redo, buffer open/switch/list/close, flat project listing, retained-frame
deltas, a focused command set, and this SOS navigation/edit subset:

```text
TOPEDGE BOTTOMEDGE LEFTEDGE RIGHTEDGE FIRSTCOL LASTCOL ENDCHAR FIRSTCHAR
DELCHAR CUADELCHAR DELBACK CUADELBACK DELEND DELWORD PREFIX TABFIELDF
TABFIELDB QCMND EXECUTE
```

Other THE/SOS commands return an explicit unsupported-command response. Use the
`capabilities` protocol command to inspect the exact supported surface.

Full THE dispatcher behavior and CREXX macros are outside the LLM/headless
target because they require the full editor command/profile/runtime stack.
Build/test execution is also outside the target; agents should run shell,
CMake, and CTest directly and use THE for buffer editing.

Transient UI snapshots are proved in `test_transientui`, available through
`the_llm_headless --transient-demo`, and exposed in `the_agent` through
`transient readv`, `transient dialog`, `transient popup`, `transient look`,
`transient key`, `transient text`, `transient hit`, and `transient result`.

The next roadmap item is selectable-driver startup: making a no-curses driver
choice available through a real editor startup/profile path instead of only
the proof executables. The modal/standard-screen contraction, raw input
compatibility wrapper retirement, and role/window/cursor presentation
contraction are closed. `doc/utf-handover.md` is the source of truth for that
work.

## Design Intent

LLM mode is a driver, not a terminal emulator.

The curses driver materializes logical editor state onto a terminal. The LLM
driver exposes the same logical editor state as structured text and accepts
normalized editor input. An agent should not infer editor state from ANSI
escapes, hardware cursor position, terminal color attributes, or terminal width
quirks.

The split is:

- Logical editor model: file text, prefix text, command-line text, focus,
  logical cursor position, grapheme-aware `TextPos`, logical syntax/style
  categories, and normalized input events.
- Curses driver: curses windows, refreshes, hardware cursor movement, physical
  display columns, terminal mouse packets, and terminal-specific UTF repair.
- LLM driver: logical screen snapshot, logical cursor/focus information, status
  text, command-line text, logical syntax/style spans, and normalized
  text/key/command/logical-hit/debug input.

The LLM driver must remain independent of terminal profiles. Terminal profiles
describe physical display behavior only. They must not change the logical text,
logical cursor position, or input event that an agent sees.

## Screen View Contract

`LlmDriverScreenView` is the current snapshot structure. It contains:

- `rows` and `cols`: the logical visible screen size reported to the agent.
- `cursor`: the logical cursor, including zone, line number, zone row, and
  logical cell position.
- `cursor_screen_row` and `cursor_screen_col`: diagnostic screen coordinates.
  These are not a substitute for the logical cursor.
- `lines`: visible rows with row role, line number, logical row, prefix text,
  semantic prefix command text, line text, syntax/style spans, current-line
  marker, and cursor marker.
- `command_line`: command area text when available.
- `status`: status text when available.
- `buffer_path`, `buffer_dirty`, and `buffer_line_count`: current buffer
  metadata.
- `undo_available` and `redo_available`: bounded agent-side history state.
- `selection`: active range endpoints plus clipboard text.
- `buffers`: open agent buffers with path/dirty/line-count/current metadata.
- `project`: flat project listing metadata from `project-list [DIR]`.

Style spans describe parser/editor categories, not terminal colors. They are
derived from THE's existing `ECOLOUR_*`/parser state and are exposed as logical
names such as `keyword`, `string`, `comment`, `function`, or `operator`.

Parser diagnostics are editor state too. In the full editor, macros and agents
can use `SDSLHWAIT` followed by `EXTRACT /PMSGS/` to list all SDSLH messages in
the current file without relying on status-line color or cursor placement.

Agents should use the cursor fields and visible rows to decide the next editor
action. Do not parse terminal escape output or rely on physical cursor parking.

## View Modes

Use the smallest view that can answer the immediate question:

- `full`: all visible semantic rows, command line, status, focus, prefixes, and
  text.
- `filearea`: editable file rows only. Use this for normal scrolling.
- `reserved`: non-file informational rows such as TOF/EOF, scale, bounds, tab
  lines, status, prompt, and reserved lines.
- `prefix`: prefix command text only.
- `focus`: the row containing the logical cursor.

Formatting options include `first_row`, `row_count`, `max_text_cols`,
`include_prefix`, `include_command`, `include_status`, `include_cursor`, and
`compact`.

## Input Contract

`LlmDriverInput` and `TheInputEvent` represent input at the editor boundary.

The current input kinds are:

- `text`: a Unicode code point intended to be inserted or processed as text.
- `key`: a named non-text key such as `left`, `right`, `up`, `down`, `home`,
  `end`, `pageup`, `pagedown`, `enter`, `esc`, `tab`, `backtab`, `backspace`,
  `delete`, `insert`, or function keys `f1` through `f64`.
- `command`: a command-line command such as `next`, `save`, or `set ...`.
- `logical-hit`: a mouse-like logical target for file area, prefix, command,
  prompt, status, tabline, divider, window selection, or transient UI hit
  targets.
- `debug`: a diagnostic request such as cursor mapping or driver ops.
- `none`: no input.

The current curses bridge can convert text and key inputs back to legacy THE
key codes. The normal live mouse path converts terminal mouse packets into
logical-hit events before legacy mouse-definition dispatch. Command,
logical-hit, and debug inputs are preserved as structured events and should be
routed by migrated dispatch groups.

`the_agent` accepts these protocol commands:

- `look [full|filearea|reserved|prefix|focus] [compact] [max=N]`
- `delta [full|filearea|reserved|prefix|focus] [compact] [max=N]`
- `look delta ...`
- `look ... [prefix=0|1] [command=0|1] [status=0|1] [cursor=0|1]`
- `capabilities`
- `focus command` or `focus filearea`
- `hit TARGET LINE ROW CELL [SCREEN WINDOW]`
- `key NAME`
- `text TEXT`
- `command COMMAND`
- `transient readv [TEXT]`, `transient dialog [TEXT]`, or `transient popup`
- `transient look`, `transient key NAME`, `transient text TEXT`,
  `transient hit ROW COL`, `transient result`, `transient close`
- `debug NAME`
- `quit` or `exit`

Current supported editor commands include `open`, `open!`, `new`, `new!`,
`save`, `write`, `goto`, `top`, `bottom`, `pageup`, `pagedown`, `find`,
`find-next`, `find-prev`, `replace`, `replace-all`, `insert`, `type`,
`setline`, `insertline`, `appendline`, `deleteline`, `duplicateline`,
`prefix`, `prefix-clear`, `prefix-execute`, `select`, `selection-copy`,
`selection-delete`, `selection-replace`, `undo`, `redo`, `buffer-open`,
`buffer-switch`, `buffer-list`, `buffer-close`, and `project-list`.
Snapshots include buffer, history, selection, project, and semantic prefix
metadata.

## Agent Usage Rules

1. Read the latest screen snapshot before acting.
2. Prefer command input for explicit editor commands that the active surface
   supports.
3. Prefer key input for navigation that should behave like user cursor motion.
4. Prefer text input for literal text insertion.
5. After sending input, wait for the next snapshot before deciding on another
   action.
6. Use logical cursor fields for reasoning about position. Treat physical
   screen fields as diagnostic context only.
7. Do not infer UTF layout from visible glyph width. A keycap, flag, ZWJ emoji,
   or combining sequence may have terminal-specific physical width while
   remaining one logical editor cluster.

## Development Rules

- Do not add curses includes, `WINDOW *`, `getyx()`, `wmove()`, `wgetch()`, or
  terminal escape handling to `llmdriver.c`.
- Do not add curses includes, `WINDOW *`, or terminal APIs to `transientui.c`;
  transient UI state must remain logical and reusable by curses, LLM, and tests.
- Do not expose terminal-profile layout widths as logical text position.
- Do not make LLM input a separate command language. It should produce the same
  normalized editor events that curses input eventually produces.
- Do not make the LLM driver depend on the current terminal.
- Keep formatted output stable enough for agents, but prefer structured fields
  in code over string parsing when possible.

## Test Coverage

Focused no-curses/LLM tests:

```sh
cmake --build cmake-build-debug --target \
  test_llmdriver test_virtual_screen test_agentdriver test_transientui \
  the_agent the_llm_headless -j2
ctest --test-dir cmake-build-debug \
  -R 'test_llmdriver|test_virtual_screen|test_agentdriver|test_transientui|test_the_agent|test_the_llm_headless_no_curses' \
  --output-on-failure
```

Coverage includes semantic formatting, compact views, buffer metadata, input
conversion and queues, debug snapshots, virtual frames/fake-driver logs,
logical hits, agent file loading/open/save/write, file-area, prefix, and
command-line focus, command cursor movement, Enter submission, search/find,
replace, line operations, prefix command execution, UTF-aware selections,
undo/redo availability, buffer switching, project listing, retained-frame
deltas, capability output, unsupported-command diagnostics, the closed SOS
subset, the `the_llm_headless --mini-session` editing proof, no-curses
transient UI state transitions, and live `the_agent` transient protocol flow
for readv, dialog, and popup.

CREXX/pty tests remain the full-editor integration surface. A skipped CREXX
test means that surface was unavailable; it does not weaken the no-curses
agent boundary proof.
