# Draft Skill: THE LLM Driver

Use this draft when an agent should edit through THE's no-curses LLM driver
instead of scraping a terminal.

## When To Use

Use `the --driver llm` as an agent editor when you need to:

- inspect a buffer through stable row-role snapshots or retained deltas.
- drive real THE commands, profiles, prefix commands, file-ring state, parser
  state, or CREXX macros when available.
- make cursor-driven edits through normalized key, text, type, and logical-hit
  input.
- verify logical cursor behavior, UTF text movement, syntax/style spans,
  transient modal state, or no-curses driver boundaries.
- avoid curses, terminal escape sequences, and physical cursor scraping.

Build/test execution and repository-scale search remain host automation
responsibilities.

## Start

Build:

```sh
cmake --build cmake-build-debug --target the the_driver_llm -j2
```

Start the full-runtime LLM target:

```sh
./cmake-build-debug/the --driver llm -n path/to/file.txt
```

Noninteractive probe:

```sh
printf '%s\n' \
  'capabilities' \
  'look filearea compact max=120 prefix=0 command=0 status=0' \
  'quit' \
  | TERM= THE_HOME_DIR="$PWD/cmake-build-debug/release" \
    ./cmake-build-debug/the --driver llm -n path/to/file.txt
```

## Inspect State

Prefer compact views while navigating:

```text
look filearea compact max=120 prefix=0 command=0 status=0
look focus compact prefix=0
look reserved compact
look prefix compact max=120
delta compact max=120
capabilities
```

Use snapshot fields, not glyph widths:

- `focus.zone`
- `focus.line`
- `focus.row`
- `focus.cell`
- row `role`
- row `line`
- row `t`
- row `pc`
- `buffer.path`
- `buffer.dirty`
- `buffer.lines`
- `buffers`
- `selection`
- `diagnostics`

## Edit Safely

Use command input for deliberate real THE actions:

```text
command find target
command c/target/replacement/
command set pending on d
command save
command write path/to/other.txt
```

Use normalized keys for cursor movement:

```text
key left
key right
key up
key down
key home
key end
key pageup
key pagedown
key tab
key backtab
```

Use text input for literal insertion at the current cursor:

```text
text literal text
type more literal text
```

Use logical hits when a snapshot gives a precise target:

```text
hit filearea 42 3 12
hit command 0 22 4
hit prefix 42 3 0
```

Use transient modal commands for readv/dialog/popup flows:

```text
command readv cmdline seed
transient look
transient text X
transient key enter
transient result
transient close
```

If a command reports unsaved changes, save first or use the relevant forced THE
command only when discarding edits is intentional.

## Save And Verify

Save:

```text
command save
command write path/to/other.txt
```

Verify in THE:

```text
look focus compact prefix=0
look filearea compact max=160
delta compact max=160
```

Verify externally when needed:

```sh
ctest --test-dir cmake-build-debug \
  -R 'test_the_llm_full_runtime|test_llmdriver|test_llmruntime|test_transientui|test_inputevent' \
  --output-on-failure
```

Run builds and tests outside THE. The editor target does not execute host
processes by design.

## Surface Boundaries

- Full THE dispatcher: `command ...` in `the --driver llm`.
- CREXX macros: available when `capabilities` reports `"crexx_macros":true`.
- Parser/SDSLH diagnostics: snapshots include a first-class `diagnostics`
  array when parser messages exist.
- Terminal mouse packets: physical input handled by the curses driver; agents
  should use logical `hit` commands.
- Build/test hooks: host automation should run shell, CMake, and CTest
  directly.
- Recursive project indexing: use shell or agent file tools.
- Unsupported behavior: check `the --driver llm capabilities`.

Search and command semantics are THE's real runtime semantics.

## Example Workflow

```sh
printf '%s\n' \
  'capabilities' \
  'look filearea compact max=120 prefix=0' \
  'command find TODO' \
  'look focus compact prefix=0' \
  'command c/TODO/DONE/' \
  'command set pending on d' \
  'look prefix compact max=120' \
  'command save' \
  'delta compact max=120' \
  'quit' \
  | TERM= THE_HOME_DIR="$PWD/cmake-build-debug/release" \
    ./cmake-build-debug/the --driver llm -n path/to/file.txt
```
