# LLM Driver Agent Guide

Last updated: 2026-05-29.

This guide describes the agent-facing LLM driver surface. It intentionally
avoids migration planning detail; use `doc/utf-handover.md` for status and
next closable tasks.

## Purpose

The LLM driver is a first-class THE UI driver. It should let an agent edit,
navigate, inspect, and debug THE without scraping a terminal screen or
depending on curses cursor behavior.

The driver must minimize tokens by default. A useful agent loop should request
only the semantic slice needed for the next action, then expand specific areas
on demand.

## Core Principles

- Use logical coordinates: `zone`, `line_number`, `screen_row`, and logical
  `cell`.
- Return row roles, not terminal decoration. Important roles include `file`,
  `prefix`, `command`, `tof`, `eof`, `reserved`, `bounds`, `scale`, `tabline`,
  `status`, `prompt`, `divider`, and `window`.
- Keep file bytes and logical UTF clusters separate from physical display
  width, cursor width, repair strategy, and terminal profile class.
- Prefer deterministic compact JSON-like output over terminal text.
- Let the agent choose view mode and token budget.
- Never let the LLM driver mutate buffers directly. It should submit
  normalized input or editor commands through the shared input/command layer.

## Token-Saving View Modes

The LLM driver supports these formatting modes:

- `full`: all visible semantic rows, command line, status, focus, prefixes, and
  text. Use sparingly.
- `filearea`: editable file rows only. This is the default for scrolling
  through a file.
- `reserved`: non-file informational rows such as TOF/EOF, scale, bounds, tab
  lines, status, prompt, and reserved rows.
- `prefix`: prefix command text only.
- `focus`: only the row containing the logical cursor.

Formatting options:

- `first_row` and `row_count` restrict screen rows.
- `max_text_cols` truncates long lines.
- `include_prefix`, `include_command`, `include_status`, and
  `include_cursor` omit stable chrome while scrolling.
- `compact` uses short field names for high-frequency loops.

## Recommended Workflows

### Reading And Scrolling

Use `filearea`, compact output, hidden prefixes, hidden command/status, and a
line-length cap. Expand to `focus` or a small row range only when the next
action depends on nearby context.

Example shape:

```json
{
  "mode": "filearea",
  "rows": 24,
  "cols": 80,
  "screen_rows": [
    {"r": 3, "role": "file", "line": 42, "cur": 0, "t": "..."}
  ]
}
```

### Prefix Commands

Use `prefix` mode when the task involves line commands. Snapshots expose
semantic prefix command text as `pc` in compact output. The supported agent
subset is `d`, `del`, `delete`, `dup`, `copy`, `r TEXT`, `i TEXT`, and
`a TEXT`, entered with `prefix LINE COMMAND` and run with `prefix-execute`.

### Reserved Lines

Use `reserved` mode on demand to inspect scale, bounds, tabs, status, EOF/TOF,
prompts, and reserved application output. Do not include these rows in normal
file scrolling unless the task specifically needs them.

### Debugging THE

Use LLM debug commands rather than full screen dumps:

- `describe-focus`: current logical zone, line, row, cell, and desired cell.
- `describe-row`: role, line number, editable flag, and text for one row.
- `list-visible-rows`: compact table of row roles and file line numbers.
- `dump-cursor-mapping`: logical cell, viewport column, raw physical display
  column, clamped display column, and visibility.
- `dump-driver-ops`: fake/curses driver operation log.
- `explain-last-render`: concise renderer decision summary, including UTF class
  and repair strategy when applicable.

A keycap cursor bug should reduce to visible rows, logical focus, cursor
mapping, driver operations for the last movement, and last-render explanation.

## Skill Wrapper Shape

An agent skill can wrap the LLM driver with high-level actions:

- `look_file(rows=N, cols=M)`: compact file-only view.
- `look_focus(context=N)`: cursor row plus nearby file rows.
- `look_reserved()`: reserved/status/prompt rows.
- `look_prefix()`: prefix-command area only.
- `look_delta()`: changed semantic rows since the previous delta view.
- `send_key(name)`: normalized key input.
- `type_text(text)`: normalized text input.
- `run_command(command)`: THE command-line submission.
- `hit(target,line,row,cell)`: logical mouse-like hit target.
- `select(line1,cell1,line2,cell2)`: set a logical selection range.
- `undo()` / `redo()`: use the bounded agent mutation history.
- `list_buffers()` / `switch_buffer(target)`: inspect and switch agent
  buffers.
- `list_project(dir)`: ask THE for a flat visible project listing.
- `modal(kind)`: start and inspect a transient readv/dialog/popup demo flow.
- `debug_cursor()`: focus plus cursor mapping.
- `debug_render()`: driver ops plus last render explanation.

The wrapper should default to the smallest view that can answer the immediate
question and expand only when editor state is ambiguous.

## Implementation Status

Closed foundation:

- `src/uidriver.c`: frame rows, row roles, cursor overlay validation, cursor
  rebasing, and fake-driver operation logs.
- `src/screenframe.c`: live file-area snapshots for the current editor view.
- `src/inputevent.c`: shared normalized text/key/command/logical-hit/debug
  events, legacy key conversion, and input queues.
- `src/mousehit.c`: shared no-curses mapping from driver-edge mouse packet
  facts to logical-hit targets used by the normal live curses mouse path.
- `src/transientui.c`: no-curses readv/dialog/popup snapshot model with
  geometry, row roles, focus, title/prompt/edit text, buttons/items, selected
  state, popup viewport offsets, and logical hit targets.
- `src/llmdriver.c`: semantic screen view formatting, compact view modes,
  compatibility wrappers, and debug snapshot formatting.
- `test_virtual_screen`: no-curses virtual frame harness for file, prefix,
  command, status, tabline, divider, window, UTF fixture, compact-view, cursor,
  targeted-redraw row, logical-hit, and fake-driver operation coverage.
- `src/agentdriver.c` and `tools/the_agent.c`: no-curses proof target with
  file loading, LLM snapshots/deltas, normalized stdin commands, file-area,
  prefix, and command-line focus/editing, logical hits, file open/save/write,
  search/replace, line operations, prefix command subset execution, selection
  operations, bounded agent-side undo/redo, buffer list/switch/open/close, flat
  project listing, live transient modal demo protocol, buffer metadata, and
  explicit capability reporting.
- `tools/the_llm_headless.c`: no-curses executable skeleton for the broader
  headless direction. It links the transient UI model and exposes
  `--transient-demo` for inspecting transient snapshot formatting and
  `--mini-session` for a realistic no-curses edit/save proof.
- `doc/llm-headless-capabilities.md`: concise capability inventory and
  outside-target classification for the current no-curses agent/editor
  surface.

Current agent subset:

- Supported input commands: `look`, `delta`, `capabilities`, `focus`, `hit`,
  `key`, `text`, `type`, `command`, `debug`, `transient`, and `quit`.
- Supported editor commands include file/session commands (`open`, `open!`,
  `new`, `new!`, `save`, `write`), navigation commands (`goto`, `top`,
  `bottom`, `pageup`, `pagedown`, `tab`, `backtab`), search commands (`find`,
  `search`, `find-next`, `find-prev`), edit commands (`insert`, `type`,
  `replace`, `replace-all`), line commands (`setline`, `insertline`,
  `appendline`, `deleteline`, `duplicateline`), prefix commands (`prefix`,
  `prefix-clear`, `prefix-execute`), selection commands (`select`,
  `selection-copy`, `selection-delete`, `selection-replace`), history commands
  (`undo`, `redo`), buffer commands (`buffer-open`, `buffer-switch`,
  `buffer-list`, `buffer-close`), and `project-list`.
- Supported logical-hit targets: file-area, prefix, command, prompt, status,
  tabline/filetabs, divider, and window selection.
- Supported SOS commands: `TOPEDGE`, `BOTTOMEDGE`, `LEFTEDGE`, `RIGHTEDGE`,
  `FIRSTCOL`, `LASTCOL`, `ENDCHAR`, `FIRSTCHAR`, `DELCHAR`, `CUADELCHAR`,
  `DELBACK`, `CUADELBACK`, `DELEND`, `DELWORD`, `PREFIX`, `TABFIELDF`,
  `TABFIELDB`, `QCMND`, and `EXECUTE`.
- Unsupported full-editor commands return stable diagnostics and point callers
  to `capabilities`.

Outside the LLM/headless target:

- Full THE command dispatcher integration requires the full editor
  command/profile runtime. `the_agent` deliberately remains a bounded
  no-curses agent editing surface.
- CREXX macros require CREXX and full macro/profile integration.
- Terminal mouse packets remain physical input owned by the curses driver; the
  agent path uses logical `hit` commands.
- Build/test execution is a host automation concern. Run shell, CMake, and
  CTest outside THE, then use THE for buffer editing.
- Recursive project indexing is better handled by shell/agent tools. THE
  exposes a flat `project-list` snapshot for quick editor context.

## No-Curses Agent Executable

Build:

```sh
cmake --build cmake-build-debug --target the_agent -j2
```

Run against a file:

```sh
./cmake-build-debug/the_agent --rows 24 --cols 80 path/to/file.txt
```

Supported stdin commands:

- `look [full|filearea|reserved|prefix|focus] [compact] [max=N]`
- `delta [full|filearea|reserved|prefix|focus] [compact] [max=N]`
- `look delta ...`
- `look ... [prefix=0|1] [command=0|1] [status=0|1] [cursor=0|1]`
- `capabilities`
- `focus command`, `focus filearea`, or `focus prefix`
- `hit TARGET LINE ROW CELL [SCREEN WINDOW]`
- `key left|right|up|down|home|end|pageup|pagedown|tab|backtab|backspace|delete`
- `text TEXT`
- `command COMMAND`
- `transient readv [TEXT]`, `transient dialog [TEXT]`, `transient popup`
- `transient look`, `transient key NAME`, `transient text TEXT`,
  `transient hit ROW COL`, `transient result`, `transient close`
- `debug NAME`
- `quit` or `exit`

Example agent loop:

```sh
printf '%s\n' \
  'look filearea compact max=80' \
  'key right' \
  'look focus compact prefix=0' \
  'quit' \
  | ./cmake-build-debug/the_agent tests/fixtures/utf-render.txt
```

Command-line editing example:

```sh
printf '%s\n' \
  'focus command' \
  'text goto 2' \
  'key left' \
  'look focus compact prefix=0' \
  'key right' \
  'key enter' \
  'look focus compact prefix=0' \
  'quit' \
  | ./cmake-build-debug/the_agent tests/fixtures/utf-render.txt
```

Search, replace, line edit, and save example:

```sh
printf '%s\n' \
  'command find TODO' \
  'look focus compact prefix=0' \
  'command replace /TODO/DONE/' \
  'command appendline verified by agent' \
  'command save' \
  'look filearea compact max=120 prefix=0' \
  'quit' \
  | ./cmake-build-debug/the_agent --rows 24 --cols 100 path/to/file.txt
```

Prefix, selection, undo, and delta example:

```sh
printf '%s\n' \
  'look full compact max=120' \
  'command prefix 2 r rewritten by prefix' \
  'command prefix-execute' \
  'delta compact max=120' \
  'command select 1 0 1 5' \
  'command selection-copy' \
  'command selection-replace ALPHA' \
  'command undo' \
  'command redo' \
  'look full compact max=120' \
  'quit' \
  | ./cmake-build-debug/the_agent --rows 24 --cols 100 path/to/file.txt
```

Buffer, project, and modal example:

```sh
printf '%s\n' \
  'command buffer-open other.txt' \
  'command buffer-list' \
  'command project-list .' \
  'transient dialog confirm' \
  'transient look' \
  'transient key tab' \
  'transient key enter' \
  'transient result' \
  'quit' \
  | ./cmake-build-debug/the_agent --rows 24 --cols 100 path/to/file.txt
```

Guardrail test:

```sh
ctest --test-dir cmake-build-debug -R 'test_the_agent_no_curses' --output-on-failure
```

Focused agent tests:

```sh
ctest --test-dir cmake-build-debug \
  -R 'test_agentdriver|test_the_agent_script|test_the_agent_capabilities|test_the_agent_no_curses|test_llmdriver|test_transientui' \
  --output-on-failure
```

## Headless Boundary

Build:

```sh
cmake --build cmake-build-debug --target the_llm_headless test_transientui -j2
```

Inspect a transient snapshot:

```sh
./cmake-build-debug/the_llm_headless --transient-demo
```

Run the no-curses mini editing session:

```sh
./cmake-build-debug/the_llm_headless --mini-session path/to/file.txt
```

Guardrail tests:

```sh
ctest --test-dir cmake-build-debug \
  -R 'test_transientui|test_the_llm_headless_no_curses|test_curses_boundary' \
  --output-on-failure
```
