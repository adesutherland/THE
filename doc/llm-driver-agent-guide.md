# LLM Driver Agent Guide

Last updated: 2026-05-21.

## Purpose

The LLM driver is a first-class THE UI driver. It should let an agent edit,
navigate, inspect, and debug THE without scraping a terminal screen or depending
on curses cursor behavior.

The driver must minimize tokens by default. A useful agent loop should request
only the semantic slice needed for the next action, then expand specific areas
on demand.

## Core Principles

- Use logical coordinates: `zone`, `line_number`, `screen_row`, and logical
  `cell`.
- Return row roles, not terminal decoration. Important roles include `file`,
  `prefix`, `command`, `tof`, `eof`, `reserved`, `bounds`, `scale`, `tabline`,
  `status`, and `prompt`.
- Keep the file bytes and logical UTF clusters separate from physical display
  width, cursor width, and repair strategy.
- Prefer deterministic compact JSON-like output over terminal text.
- Let the agent choose the view mode and token budget.
- Never let the LLM driver mutate buffers directly. It should submit normalized
  input or editor commands through the same command layer as other drivers.

## Token-Saving View Modes

The LLM driver supports formatting options that should be exposed through the
agent-facing interface:

- `full`: all visible semantic rows, command line, status, focus, prefixes, and
  text. Use sparingly.
- `filearea`: only editable file rows. This is the default mode for scrolling
  through a file.
- `reserved`: only non-file informational rows such as TOF/EOF, scale, bounds,
  tab lines, status, prompt, and reserved lines.
- `prefix`: prefix command text only. Use when issuing or reviewing prefix
  commands.
- `focus`: only the row containing the logical cursor.

Formatting options include:

- `first_row` and `row_count` to restrict screen rows.
- `max_text_cols` to truncate long lines.
- `include_prefix` to omit prefix text while reading file content.
- `include_command` and `include_status` to omit stable chrome while scrolling.
- `include_cursor` to omit focus metadata for bulk content reads.
- `compact` to use short field names in high-frequency loops.

## Recommended Agent Workflows

### Reading and Scrolling

Use `filearea`, compact output, hidden prefixes, hidden command/status, and a
line-length cap. This gives the agent the text it needs without paying for
prompt/status/prefix chrome on every scroll.

Example shape:

```json
{"mode":"filearea","rows":24,"cols":80,"screen_rows":[{"r":3,"role":"file","line":42,"cur":0,"t":"..."}]}
```

When the agent needs context around the cursor, use `focus` first. Expand to a
small `filearea` row range only if the next command depends on surrounding text.

### Prefix Commands

Use `prefix` mode when the task involves line commands. The file text can stay
hidden until the command has been entered or verified.

### Reserved Lines

Use `reserved` mode on demand to inspect scale, bounds, tabs, status, EOF/TOF,
prompts, and reserved application output. Do not include these rows in normal
file scrolling unless the task specifically needs them.

### Debugging THE

Use LLM debug commands rather than asking for a full screen dump:

- `describe-focus`: current logical zone, line, row, cell, and desired cell.
- `describe-row`: role, line number, editable flag, and text for one row.
- `list-visible-rows`: compact table of row roles and file line numbers.
- `dump-cursor-mapping`: logical cell, viewport column, raw physical display
  column, clamped display column, and visibility.
- `dump-driver-ops`: fake/curses driver operation log.
- `explain-last-render`: concise renderer decision summary, including UTF class
  and repair strategy when applicable.

These commands are intended to make defects reproducible. A keycap cursor bug,
for example, should be reduced to:

1. visible rows in compact `filearea` mode;
2. logical focus;
3. cursor mapping;
4. driver ops for the last movement;
5. last render explanation.

## Skill Wrapper Shape

An agent skill can wrap the LLM driver with high-level actions:

- `look_file(rows=N, cols=M)`: compact file-only view.
- `look_focus(context=N)`: cursor row plus nearby file rows.
- `look_reserved()`: reserved/status/prompt rows.
- `look_prefix()`: prefix-command area only.
- `send_key(name)`: normalized key input.
- `type_text(text)`: normalized text input.
- `run_command(command)`: THE command-line submission.
- `hit(zone,line,row,cell)`: logical mouse-like hit target.
- `debug_cursor()`: focus plus cursor mapping.
- `debug_render()`: driver ops plus last render explanation.

The skill should default to the smallest view that can answer the immediate
question, and only expand when the editor state is ambiguous.

## Implementation Status

Implemented foundation:

- `src/uidriver.c` frame rows, row roles, cursor overlay validation, and driver
  operation logs.
- `src/screenframe.c` live frame snapshots for the current file-area rows.
- `src/inputevent.c` shared normalized text/key/command/logical-hit/debug
  events, legacy key conversion, and input queues.
- `src/llmdriver.c` semantic screen view formatting.
- compact view formatting with `full`, `filearea`, `reserved`, `prefix`, and
  `focus` modes.
- LLM compatibility wrappers around the shared input event layer.
- debug snapshot formatting for focus, cursor mapping, driver ops, and last
  render explanation.

Remaining work:

- extend live `UiFrame` creation beyond the file area so command, prompt,
  status, and reserved UI state all have one logical snapshot.
- expose the formatting options through the runtime LLM driver command/API.
- route curses keyboard and mouse collection through `TheInputEvent` before
  command dispatch.
- implement delta views once the frame builder can retain previous snapshots.
