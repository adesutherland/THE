# LLM Mode Agent Guide

This document describes the intended LLM-facing mode for THE. It is written for
agents and tool authors that need to inspect editor state and drive editor input
without depending on curses escape sequences, physical terminal columns, or
screen scraping.

## Status

The first passive implementation is in `src/llmdriver.c` and
`src/llmdriver.h`, with coverage in `tests/test_llmdriver.c`.

The first live proof target is `the_agent`, a separate no-curses executable
rather than a runtime switch inside the curses editor. It is interactive over
stdin/stdout, uses the same `LlmDriverScreenView` and `TheInputEvent`
contracts, and is covered by a build guard that rejects curses dependencies and
curses-driver symbols. Treat that target as the first agent surface while the
full editor input loop is still being migrated.

Current limitation: `the_agent` is not yet wired to THE's full command
dispatcher. It covers logical file-area and command-line focus plus a small
command subset, but arbitrary THE/SOS commands return an explicit unsupported
command response even though the full editor handles them. Use the
`capabilities` protocol command to inspect the stable supported/unsupported
surface. For unsupported full-editor commands, use CREXX/pty integration tests
or manual smoke tests until the agent dispatcher bridge exists.

## Design Intent

LLM mode is a driver, not a terminal emulator.

The curses driver materializes logical editor state onto a terminal. The LLM
driver should expose the same logical editor state as structured text and accept
normalized editor input. An agent should not infer editor state from ANSI
escapes, hardware cursor position, terminal colour attributes, or terminal
width quirks.

The driver split is:

- Logical editor model: file text, prefix text, command-line text, focus,
  logical cursor position, grapheme-aware `TextPos`, logical syntax/style
  categories, and normalized input events.
- Curses driver: curses windows, refreshes, hardware cursor movement, physical
  display columns, mouse decoding, and terminal-specific UTF repair.
- LLM driver: logical screen snapshot, logical cursor/focus information, status
  text, command line text, logical syntax/style spans, and normalized
  text/key/command input.

The LLM driver must remain independent of terminal profiles. Terminal profiles
describe physical display behavior only. They must not change the logical text,
logical cursor position, or input event that an agent sees.

## Screen View Contract

`LlmDriverScreenView` is the current snapshot structure. It contains:

- `rows` and `cols`: the logical visible screen size reported to the agent.
- `cursor`: the logical cursor, including zone, line number, zone row, and
  logical cell position.
- `cursor_screen_row` and `cursor_screen_col`: screen coordinates for display
  and debugging. These are not a substitute for the logical cursor.
- `lines`: visible file-area rows with line number, logical row, prefix text,
  line text, logical syntax/style spans, and current-line marker.
- `command_line`: the command area text when available.
- `status`: status text when available.

Style spans describe parser/editor categories, not terminal colours. They are
derived from THE's existing `ECOLOUR_*`/parser state, including SDSLH-backed
tokens, and are exposed as logical names such as `keyword`, `string`,
`comment`, `function`, or `operator`. A curses profile may paint those
categories with different colours, but the LLM contract remains the category
name and the logical cell range.

Parser diagnostics are editor state too. In the full editor, macros and agents
can use `SDSLHWAIT` followed by `EXTRACT /PMSGS/` to list all SDSLH messages in
the current file without relying on status-line colour or the cursor being on
the diagnostic token.

The formatted view is intentionally line-oriented. A typical snapshot looks like:

```text
screen rows=3 cols=80
cursor zone=filearea line=12 row=1 cell=5 screen_row=1 screen_col=5
command: ====> next
status: LINE 12 COL 6
 0000 line=11 prefix="000011" text="alpha"
>0001 line=12 prefix="000012" text="bravo"
```

Agents should use the `cursor` line and visible rows to decide the next editor
action. Do not parse terminal escape output or rely on physical cursor parking.

## Input Contract

`LlmDriverInput` represents input at the editor boundary.

The current input kinds are:

- `text`: a Unicode code point intended to be inserted or processed as text.
- `key`: a named non-text key such as `left`, `right`, `up`, `down`, `home`,
  `end`, `pageup`, `pagedown`, `enter`, `esc`, `tab`, `backtab`, `backspace`,
  `delete`, `insert`, or function keys `f1` through `f64`.
- `command`: a command-line command such as `next`, `save`, or `set ...`.
- `none`: no input.

The current compatibility bridge can convert `text` and `key` inputs to legacy
THE key codes. `command` input is preserved as a command string and must be
routed through command execution in a later step; it is not a key-code event.

`the_agent` accepts these normalized events over stdin:

- `key NAME`
- `text TEXT`
- `command COMMAND`
- `debug NAME`

It also accepts `look` requests that format the current logical screen snapshot
without changing editor state, `capabilities` requests that describe the
current agent surface, and `focus command` / `focus filearea` requests that
move the logical input focus. In command focus, left/right/home/end, delete,
backspace, and text input operate on the command line; `key enter` submits the
edited command.

## Agent Usage Rules

When LLM mode is wired:

1. Read the latest screen snapshot before acting.
2. Prefer command input for explicit editor commands.
3. Prefer key input for navigation that should behave like user cursor motion.
4. Prefer text input for literal text insertion.
5. After sending input, wait for the next snapshot before deciding on another
   action.
6. Use logical cursor fields for reasoning about position. Treat physical screen
   fields as diagnostic context only.
7. Do not infer UTF layout from visible glyph width. A keycap, flag, ZWJ emoji,
   or combining sequence may have terminal-specific physical width while
   remaining one logical editor cluster.

## Development Rules

Keep the LLM driver aligned with the same logical/physical architecture as the
curses driver:

- Do not add curses includes, `WINDOW *`, `getyx()`, `wmove()`, `wgetch()`, or
  terminal escape handling to `llmdriver.c`.
- Do not expose terminal-profile layout widths as logical text position.
- Do not make LLM input a separate command language. It should produce the same
  normalized editor events that curses input eventually produces.
- Do not make the LLM driver depend on the current terminal. It should be usable
  in tests and future non-terminal front ends.
- Keep formatted output stable enough for agents, but prefer structured fields
  in code over string parsing when possible.

## Current Test Coverage

`tests/test_llmdriver.c` verifies:

- Screen snapshots format rows, cursor, command line, status line, and current
  line marker.
- Named keys map to legacy THE key codes.
- Function keys map through `KEY_F(n)`.
- ASCII text input maps to a legacy key code.
- Command input is not treated as a legacy key.
- The input queue preserves key/text ordering.

Run the focused test with:

```sh
cmake --build cmake-build-debug --target test_llmdriver -j2
./cmake-build-debug/test_llmdriver
```

The non-UTF build also includes this test because the LLM driver is not a UTF
terminal repair feature.

`tests/test_agentdriver.c`, `tests/test_the_agent_script.sh`, and
`tests/test_the_agent_capabilities.sh`, and `tests/check_agent_no_curses.sh`
verify the live proof target:

- loading a file into a logical buffer.
- compact `filearea` and `focus` snapshots.
- normalized key movement and command/text insertion.
- command-line focus, command cursor movement, and Enter submission.
- stable capability output that says the agent uses an `agent-subset`
  dispatcher.
- stable unsupported-command output for full-editor commands that are not yet
  routed through the agent.
- no curses dynamic dependency or exposed curses-driver symbols in
  `the_agent`.

## Aggressive Next Steps

LLM mode is now a migration accelerator, not just a passive proof target. New
driver-boundary work should be visible to agents, fake drivers, or CTest before
the corresponding curses path is considered migrated.

1. Add a virtual screen/fake-driver harness that can build `UiFrame` snapshots,
   drive normalized input, and compare semantic rows, cursor overlays, compact
   views, and fake-driver operation logs without curses.
2. Expand the no-curses agent driver toward THE's real command executor in
   useful groups. Prefer a batch of navigation/SOS/prefix/command behaviors
   with script coverage over one-off smoke helpers.
3. Route command, key, and text input through the same normalized event layer
   used by curses. Keep legacy key conversion as an edge adapter, not as the
   main dispatch model for newly migrated behavior.
4. Add logical-hit input for mouse-like actions: file area, prefix, command
   line, status, file tabs, divider, and window selection. Prove hit mapping
   with `inputevent` or virtual-screen CTests before wiring live curses mouse
   dispatch.
5. Use agent script CTests for no-curses parity and CREXX/pty CTests for
   full-editor parity until the agent command bridge can run the same command.
   Unsupported commands must stay explicit in `capabilities`.
6. Keep UTF behavior logical: whole grapheme clusters for editor movement and
   edits, physical terminal strategy only inside curses rendering.

## CREXX Integration Notes

CREXX remains the most useful automated surface for full-editor behavior while
`the_agent` is incomplete. CREXX profile tests can drive real THE commands,
including SOS commands, through a pty-backed editor instance. Keep these tests
clear about their prerequisites and skip reasons: CREXX support must be enabled,
the CREXX compiler/import runtime must be available, and `script(1)` or another
pty wrapper must exist. CREXX test failures may be macro/compiler interface
failures before they are editor regressions, so focused output labels and small
profiles are preferred. A skipped CREXX test means the full-editor automation
surface was unavailable; it does not weaken the no-curses `the_agent` boundary
tests, which prove a different surface.
