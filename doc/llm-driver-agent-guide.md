# LLM Driver Agent Guide

Last updated: 2026-06-02.

This guide describes the agent-facing LLM driver surface. It intentionally
avoids migration planning detail; use `doc/utf-handover.md` for status and
next closable tasks.

## Purpose

The LLM driver is a first-class THE UI driver. It lets an agent edit,
navigate, inspect, and debug THE without scraping a terminal screen or
depending on curses cursor behavior.

Use `the --driver llm` as the agent/editor target. It boots the real THE
runtime, selects the runtime-loaded `the_driver_llm` module, opens real
buffers/views, and exposes the LLM protocol over stdin/stdout.

`the_driver_llm.so` is the headless physical driver module behind that mode,
not a separate user-facing editor.

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
- Submit normalized input or real THE commands. Do not add a second command
  language or a second editor model.
- Discover support through `the --driver llm capabilities`.

## Start

Build the real editor and LLM driver module:

```sh
cmake --build cmake-build-debug --target the the_driver_llm -j2
```

Run against a file:

```sh
./cmake-build-debug/the --driver llm -n path/to/file.txt
```

Typical noninteractive probe:

```sh
printf '%s\n' \
  'capabilities' \
  'look filearea compact max=120 prefix=0 command=0 status=0' \
  'command c/old/new/' \
  'delta filearea compact max=120' \
  'quit' \
  | TERM= THE_HOME_DIR="$PWD/cmake-build-debug/release" \
    ./cmake-build-debug/the --driver llm -n path/to/file.txt
```

## Protocol

Supported stdin commands:

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
- `command COMMAND`
- `transient readv [TEXT]`, `transient dialog [TEXT]`, or `transient popup`
- `transient look`, `transient key NAME`, `transient text TEXT`,
  `transient hit ROW COL`, `transient result`, `transient close`,
  `transient cancel`
- `debug NAME`
- `quit` or `exit`

`command ...` uses THE's real command dispatcher. Profiles, prefix commands,
parser/SDSLH state, file-ring behavior, editor variables, and CREXX macros
therefore stay in the full editor runtime.

## View Modes

Use the smallest view that can answer the immediate question:

- `full`: all visible semantic rows, command line, status, focus, prefixes,
  and text. Use sparingly.
- `filearea`: editable file rows only. This is the default for scrolling
  through a file.
- `reserved`: non-file informational rows such as TOF/EOF, scale, bounds,
  tab lines, status, prompt, and reserved rows.
- `prefix`: prefix command text only.
- `focus`: only the row containing the logical cursor.

Formatting options:

- `first_row` and `row_count` restrict screen rows.
- `max_text_cols` truncates long lines.
- `include_prefix`, `include_command`, `include_status`, and
  `include_cursor` omit stable chrome while scrolling.
- `compact` uses short field names for high-frequency loops.

## Snapshot Fields

`LlmDriverScreenView` snapshots include:

- logical cursor/focus fields: zone, line number, row, cell, desired cell.
- visible rows with row role, file line number, prefix text, semantic prefix
  command text, file text, syntax/style spans, current-line marker, and cursor
  marker.
- command-line text and status text when requested.
- current buffer path, dirty flag, and line count.
- file-ring buffer metadata.
- real block/selection state.
- parser diagnostics when available.

Style spans describe parser/editor categories, not terminal colors. Agents
should use the cursor fields and visible rows to decide the next editor action.
Do not parse terminal escape output or rely on physical cursor parking.

## Recommended Workflows

Read and scroll with compact file views:

```text
look filearea compact max=120 prefix=0 command=0 status=0
key pagedown
delta filearea compact max=120 prefix=0 command=0 status=0
```

Move focus or click logical targets from a snapshot:

```text
focus command
text locate /target/
key enter
hit filearea 42 3 12
```

Run real THE commands:

```text
command find target
command c/target/replacement/
command set pending on d
command save
```

Inspect prefix state:

```text
look prefix compact max=120
command set pending on d
look prefix compact max=120
```

Drive readv/dialog/popup modal flows:

```text
command readv cmdline seed
transient look
transient text X
transient key enter
transient result
```

Debug logical state rather than dumping terminal text:

- `debug describe-focus`
- `debug describe-row`
- `debug list-visible-rows`
- `debug dump-cursor-mapping`
- `debug dump-driver-ops`
- `debug explain-last-render`

## Boundaries

- Build/test execution is host automation. Run shell, CMake, and CTest
  outside THE.
- Recursive project search and repository indexing belong to shell/agent tools
  such as `rg`.
- Terminal mouse escape packets are physical input owned by the curses driver.
  Agents should use logical `hit` commands.
- Unsupported or build-dependent behavior must be reported through
  `the --driver llm capabilities` or a focused diagnostic.

## Coverage

Focused no-curses/LLM tests:

```sh
cmake --build cmake-build-debug --target \
  the the_driver_llm test_inputevent test_llmdriver test_llmruntime \
  test_transientui test_virtual_screen test_headlessdriver -j2
ctest --test-dir cmake-build-debug \
  -R 'test_inputevent|test_llmdriver|test_llmruntime|test_transientui|test_the_llm_full_runtime|test_the_llm_parser_diagnostics|test_the_llm_profile_crexx|test_driver_modules|test_curses_boundary|test_curses_boundary_inventory' \
  --output-on-failure
```

Coverage includes semantic formatting, compact views, buffer metadata, input
conversion and queues, debug snapshots, virtual frames/fake-driver logs,
logical hits, real-runtime command dispatch, prefix/block/file-ring state,
syntax/style spans, parser diagnostics, profile/CREXX paths, transient UI
state transitions, command-triggered modal continuations, and no-curses link
checks for `the` and `the_driver_llm.so`.
