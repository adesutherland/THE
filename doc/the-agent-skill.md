# Draft Skill: THE Agent Editor

Use this draft when an agent should edit through THE's no-curses
LLM/headless surface instead of scraping a terminal.

## When To Use

Use THE as an agent editor when you need to:

- inspect a buffer through stable row-role snapshots or retained deltas.
- make precise file, line, search, replace, prefix, selection, or
  cursor-driven edits.
- verify logical cursor behavior, UTF text movement, transient modal state, or
  no-curses driver boundaries.
- avoid curses, terminal escape sequences, and physical cursor scraping.

Use the normal full editor, CREXX/pty tests, or shell tooling for behavior
classified by `the_agent capabilities` as outside the LLM/headless target:
full THE command dispatcher semantics, CREXX macros, terminal mouse packets,
build/test execution, and repository-scale project indexing.

## Start

Build:

```sh
cmake --build cmake-build-debug --target the_agent the_llm_headless -j2
```

Start an interactive agent session:

```sh
./cmake-build-debug/the_agent --rows 24 --cols 100 path/to/file.txt
```

Run the headless mini-session proof:

```sh
./cmake-build-debug/the_llm_headless --mini-session path/to/file.txt
```

## Inspect State

Prefer compact views while navigating:

```text
look filearea compact max=120 prefix=0 command=0 status=0
look focus compact prefix=0
look reserved compact
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
- `history.undo`
- `history.redo`
- `selection`
- `buffers`
- `project`

## Edit Safely

Use command input for deliberate editor actions:

```text
command find target
command replace /target/replacement/
command replace-all old new
command setline exact replacement text
command insertline text before current line
command appendline text after current line
command deleteline
command duplicateline
```

Use prefix commands for line-oriented edits:

```text
command prefix 2 r replacement line
command prefix 3 dup
command prefix 4 d
command prefix-execute
command prefix-clear all
```

Use selection commands for precise ranges:

```text
command select 10 0 12 8
command selection-copy
command selection-replace replacement text
command selection-delete
command undo
command redo
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
```

Use logical hits when a snapshot gives a precise target:

```text
hit filearea 42 3 12
hit command 0 22 4
hit prefix 42 3 0
```

Use buffers and project listing for small editor-local context:

```text
command buffer-open other.txt
command buffer-list
command buffer-switch 0
command buffer-close! 1
command project-list .
```

Use transient modal commands for readv/dialog/popup flows:

```text
transient dialog confirm
transient look
transient key tab
transient key enter
transient result
transient close
```

If `open` or `new` reports `unsaved changes`, save first or use `open!`/`new!`
only when discarding edits is intentional.

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
ctest --test-dir cmake-build-debug -R 'test_the_agent|test_agentdriver' --output-on-failure
```

Run builds and tests outside THE. The editor target does not execute host
processes by design.

## Outside Target

- Full THE dispatcher: requires the full editor command/profile runtime.
- CREXX macros: require CREXX and full macro/profile integration.
- Terminal mouse packets: physical input handled by the curses driver; agents
  should use logical `hit` commands.
- Build/test hooks: host automation should run shell, CMake, and CTest
  directly.
- Recursive project indexing: use shell or agent file tools; THE exposes flat
  `project-list` context.

Search is byte-exact over buffer text and reports logical cells for matches.

## Example Workflow

```sh
printf '%s\n' \
  'look filearea compact max=120 prefix=0' \
  'command find TODO' \
  'look focus compact prefix=0' \
  'command replace /TODO/DONE/' \
  'command prefix 2 dup' \
  'command prefix-execute' \
  'command select 1 0 1 4' \
  'command selection-replace DONE' \
  'command save' \
  'delta compact max=120' \
  'quit' \
  | ./cmake-build-debug/the_agent --rows 24 --cols 100 path/to/file.txt
```
