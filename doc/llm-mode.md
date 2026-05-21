# LLM Mode Agent Guide

This document describes the intended LLM-facing mode for THE. It is written for
agents and tool authors that need to inspect editor state and drive editor input
without depending on curses escape sequences, physical terminal columns, or
screen scraping.

## Status

The first passive implementation is in `src/llmdriver.c` and
`src/llmdriver.h`, with coverage in `tests/test_llmdriver.c`.

This is not yet a live command-line mode. There is currently no supported
runtime switch such as `--llm`, and the live editor input loop is not yet wired
to the LLM driver. Treat the current code as the contract foundation for the
next architecture steps.

## Design Intent

LLM mode is a driver, not a terminal emulator.

The curses driver materializes logical editor state onto a terminal. The LLM
driver should expose the same logical editor state as structured text and accept
normalized editor input. An agent should not infer editor state from ANSI
escapes, hardware cursor position, terminal colour attributes, or terminal
width quirks.

The driver split is:

- Logical editor model: file text, prefix text, command-line text, focus,
  logical cursor position, grapheme-aware `TextPos`, and normalized input
  events.
- Curses driver: curses windows, refreshes, hardware cursor movement, physical
  display columns, mouse decoding, and terminal-specific UTF repair.
- LLM driver: logical screen snapshot, logical cursor/focus information, status
  text, command line text, and normalized text/key/command input.

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
  line text, and current-line marker.
- `command_line`: the command area text when available.
- `status`: status text when available.

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

## Next Implementation Steps

1. Add a live mode switch or embedding entry point for LLM operation.
2. Build screen snapshots from the live logical cursor/focus model rather than
   ad hoc curses state.
3. Route command input through THE command execution.
4. Route key/text input through the same normalized input path used by curses.
5. Add integration tests with a fake driver that exercises navigation, command
   execution, prefix commands, and command-line editing without curses.
6. Keep UTF behavior logical: whole grapheme clusters for editor movement and
   edits, physical terminal strategy only inside curses rendering.
