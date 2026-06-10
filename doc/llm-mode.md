# LLM Mode Agent Guide

Last updated: 2026-06-02.

This document describes THE's LLM-facing mode for agents and tool authors that
need to inspect editor state and drive editor input without depending on curses
escape sequences, physical terminal columns, or screen scraping.

For UTF design, status, and outstanding items, read `doc/utf-design.md`.

## Status

Implemented:

- `the --driver llm` is the strategic no-curses agent/editor surface. It boots
  real THE with the LLM driver module selected, skips curses initialization,
  opens files through real file/view state, and routes `command ...` through
  THE's full command dispatcher.
- `the_driver_llm` is the runtime-loaded driver module behind `--driver llm`.
  It owns the headless physical driver behavior and must not link curses.
- `src/llm/llmdriver.c` and `src/llm/llmdriver.h` format semantic snapshots,
  token-saving view modes, normalized input wrappers, and debug diagnostics.
- `src/llm/llmsession.c` owns the full-runtime stdin/stdout protocol loop.
- `src/llm/llmruntime.c` adapts real `screenframe`/`SHOW_LINE` state into LLM
  snapshots.
- `src/inputevent.c` owns the shared text/key/command/logical-hit/debug event
  model.
- The normal live curses mouse path converts terminal packets into the same
  `TheInputEvent` logical-hit targets used by the LLM protocol.
- `src/uidriver.c` and `src/screenframe.c` provide the logical frame surface
  used by the LLM formatter and virtual/fake-driver tests.
- `src/transientui.c` provides no-curses readv/dialog/popup snapshots with
  geometry, row roles, focus, selected/active button or item, edit text, popup
  viewport offsets, and logical hit targets.
- Drivers are runtime-loaded modules. `the` no longer links curses directly;
  `the_driver_curses` owns curses startup/shutdown and terminal mechanics.

Retired:

- The lightweight fake editor harness and headless mini-session executable have
  been removed. Formatter/input-only coverage now belongs in `test_llmdriver`,
  `test_llmruntime`, `test_transientui`, and `test_inputevent`; full editor
  behavior belongs in `test_the_llm_full_runtime` through `the --driver llm`.

## Design Intent

LLM mode is a driver, not a terminal emulator and not a second editor.

The curses driver materializes logical editor state onto a terminal. The LLM
driver exposes the same logical editor state as structured text and accepts
normalized editor input. An agent should not infer editor state from ANSI
escapes, hardware cursor position, terminal color attributes, or terminal width
quirks.

The split is:

- Logical editor model: file text, prefix text, command-line text, focus,
  logical cursor position, grapheme-aware `TextPos`, logical syntax/style
  categories, real buffer/file-ring state, and normalized input events.
- Curses driver: curses windows, refreshes, hardware cursor movement, physical
  display columns, terminal mouse packets, and terminal-specific UTF repair.
- LLM driver: logical screen snapshot, logical cursor/focus information,
  status text, command-line text, logical syntax/style spans, diagnostics,
  and normalized text/key/command/logical-hit/debug input.

The LLM driver must remain independent of terminal profiles. Terminal profiles
describe physical display behavior only. They must not change the logical text,
logical cursor position, or input event that an agent sees.

## Surface

Start with:

```sh
./cmake-build-debug/the --driver llm -n path/to/file
```

Supported protocol commands:

- `look [full|filearea|reserved|prefix|focus] [compact] [max=N]`
- `delta [full|filearea|reserved|prefix|focus] [compact] [max=N]`
- `look delta ...`
- `look ... [prefix=0|1] [command=0|1] [status=0|1] [cursor=0|1]`
- `capabilities`
- `focus command`, `focus filearea`, or `focus prefix`
- `hit TARGET LINE ROW CELL [SCREEN WINDOW]`
- `key NAME`
- `text TEXT`
- `type TEXT`
- `insert after LINE TEXT`
- `command COMMAND`
- `transient readv [TEXT]`, `transient dialog [TEXT]`, or `transient popup`
- `transient look`, `transient key NAME`, `transient text TEXT`,
  `transient hit ROW COL`, `transient result`, `transient close`,
  `transient cancel`
- `debug NAME`
- `quit` or `exit`

`capabilities` is the authoritative declaration for supported and
build-dependent behavior. It reports the full dispatcher, real buffers,
profile/CREXX availability, syntax/style spans, parser diagnostics, transient
support, and host-owned build/test hooks.

ACK responses include `message_changed`; `last_message` appears only when THE
has produced a fresh human-facing message since the previous ACK. ACKs also
include compact focus, buffer, selection, and pending-prefix state.

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
- `buffers`: real file-ring metadata.
- `selection`: real block/selection state.
- `diagnostics`: parser diagnostics when available.

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

- `full`: all visible semantic rows, command line, status, focus, prefixes,
  and text.
- `filearea`: editable file rows only. Use this for normal scrolling.
- `reserved`: non-file informational rows such as TOF/EOF, scale, bounds,
  tab lines, status, prompt, and reserved lines.
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

The curses bridge can convert text and key inputs back to legacy THE key codes.
The normal live mouse path converts terminal mouse packets into logical-hit
events before legacy mouse-definition dispatch. Command, logical-hit, and debug
inputs are preserved as structured events and should be routed by migrated
dispatch groups.

## Agent Usage Rules

1. Read the latest screen snapshot before acting.
2. Prefer `command ...` for explicit editor commands that should go through
   THE's real command dispatcher.
3. Prefer `key ...` for navigation that should behave like user cursor motion.
4. Prefer `text ...` or `type ...` for literal text insertion.
5. After sending input, wait for the next snapshot before deciding on another
   action.
6. Use logical cursor fields for reasoning about position. Treat physical
   screen fields as diagnostic context only.
7. Do not infer UTF layout from visible glyph width. A keycap, flag, ZWJ emoji,
   or combining sequence may have terminal-specific physical width while
   remaining one logical editor cluster.

## Development Rules

- Do not add curses includes, `WINDOW *`, `getyx()`, `wmove()`, `wgetch()`, or
  terminal escape handling to `llmdriver.c`, `llmruntime.c`, or
  `llmsession.c`.
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
  the the_driver_llm test_inputevent test_llmdriver test_llmruntime \
  test_transientui test_keycodes test_curses_keymap -j2
ctest --test-dir cmake-build-debug \
  -R 'test_inputevent|test_llmdriver|test_llmruntime|test_transientui|test_the_llm_full_runtime|test_the_llm_parser_diagnostics|test_the_llm_profile_crexx|test_driver_modules|test_curses_boundary|test_curses_boundary_inventory' \
  --output-on-failure
```

Coverage includes semantic formatting, compact views, buffer metadata, input
conversion and queues, debug snapshots, virtual frames/fake-driver logs,
logical hits, no-curses transient UI state transitions, real-runtime syntax and
style spans, parser diagnostics, profile/CREXX paths, prefix/block/file-ring
state, command-triggered modal continuations, and link isolation for `the` and
`the_driver_llm.so`.

CREXX/pty tests remain the full-editor integration surface. A skipped CREXX
test means that surface was unavailable; it does not weaken the no-curses
agent boundary proof.
