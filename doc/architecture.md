# THE Architecture Overview

Last updated: 2026-06-02.

THE is a procedural C editor descended from XEDIT/KEDIT. This fork keeps the
classic command, file-ring, view, and Rexx macro model, but the UI architecture
now separates editor state from physical terminal mechanics.

For the detailed cursor/driver contract, read
`doc/cursor-driver-architecture.md`. For UTF design, status, and outstanding
items, read `doc/utf-design.md`.

## Supported Platforms

The maintained platform targets are:

- macOS.
- Linux/POSIX.
- native Windows.

Historical DOS, OS/2, VMS, Amiga, BeOS, QNX, DJGPP/GO32, and ancient compiler
source branches have been retired. Some command names and help text still use
historical names such as `DOS` because they are editor command compatibility
surface, not supported platform declarations.

## Runtime Shape

The main executable is `the`. It owns command-line parsing, editor startup,
file/profile setup, command dispatch, macro integration, and driver module
selection. It does not link curses directly.

Drivers are runtime-loaded modules:

- `the_driver_curses`: the default terminal UI. It owns curses startup/shutdown,
  windows, physical cursor mechanics, refresh ordering, raw terminal input,
  mouse packet decoding, terminal palette allocation, and software cursor
  painting.
- `the_driver_llm`: the no-curses LLM UI. It boots the real editor runtime and
  exposes semantic snapshots and normalized input over stdin/stdout through
  `the --driver llm`.

Both drivers implement the neutral `TheDriverOps` surface from
`src/thedriver.h`. Public driver types are curses-free.

## Core Editor Model

The editor core manages:

- `LINE`: file text in doubly-linked lists.
- `FILE_DETAILS`: loaded file metadata and file-ring state.
- `VIEW_DETAILS`: per-view display, cursor, selection, prefix, and screen state.
- command tables in `src/command.h` and command implementations in
  `src/comm*.c`, `src/execute.c`, and related modules.
- Rexx/CREXX profile and macro execution through `src/rexx.c` and
  `src/crexx.c`.

Core editor code owns logical state: file lines, logical cursor positions,
row roles, prefix and command-line text, block/selection state, syntax/style
categories, logical colors, and logical key identity. It must not infer editor
state from physical curses cursor or window state.

## Logical UI And Rendering

Several modules provide the driver-neutral view of the editor:

- `src/textpos.c`, `src/logcursor.c`, `src/utflayout.c`, `src/utfrepair.c`,
  and `src/utfterm.c` model logical UTF positions and terminal repair policy.
- `src/uidriver.c` and `src/screenframe.c` build logical row-role snapshots.
- `src/rendercell.c` models UTF render cells and clusters independently of
  curses `chtype` / `cchar_t`.
- `src/transientui.c` models readv/dialog/popup state without terminal APIs.
- `src/inputevent.c` models normalized text, key, command, logical-hit, and
  debug input events.

The curses driver lowers this logical model to a terminal. The LLM driver
formats the same logical model as structured text for agents.

## LLM Driver

`the --driver llm` is the strategic no-curses agent/editor surface. It uses the
real THE runtime: real buffers, views, command dispatch, profiles, syntax
state, parser diagnostics, file-ring state, block state, and CREXX integration
when available.

Supported protocol verbs include `look`, `delta`, `capabilities`, `focus`,
`hit`, `key`, `text`, `type`, `command`, `debug`, `transient`, and `quit`.

Capability details live in `doc/llm-driver-capabilities.md`; agent usage
guidance lives in `doc/llm-mode.md` and `doc/llm-driver-agent-guide.md`.

## Scripting And Extension

THE remains deeply integrated with Rexx-style scripting. CREXX support is
build-dependent and uses `crexxsaa` to compile/cache source profiles and
macros, run `.rxbin` macros, and register `ADDRESS THE` as a native callback
environment. CREXX scripts issue normal THE commands through the existing
command subsystem.

See `doc/crexx.md` for the current bridge contract.

## Testing And Guardrails

Key guardrails:

- `the` and `the_driver_llm.so` must not link curses.
- `src/the.h` and `src/thedriver.h` must remain curses-free.
- raw curses input, paint, window, cursor, and cell mechanics must stay in
  `src/drivers/curses/**` or explicit physical driver operations.
- real editor behavior for agents should be tested through `the --driver llm`,
  not through a fake editor harness.

Useful verification:

```sh
git diff --check
bash tests/inventory_direct_curses.sh --summary /Users/adrian/CLionProjects/THE
ctest --test-dir cmake-build-debug \
  -R 'test_driver_modules|test_the_llm_full_runtime|test_curses_boundary|test_curses_boundary_inventory' \
  --output-on-failure
```
