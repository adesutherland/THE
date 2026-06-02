# LLM Driver Capability Inventory

Last updated: 2026-06-02.

This inventory describes the supported no-curses agent/editor surface:
`the --driver llm`.

`the_driver_llm.so` is the runtime-loaded headless driver module used by that
surface. It is not a separate agent editor. The retired lightweight harness and
headless mini-session targets no longer define user-facing capabilities.

## Capability Status

| Area | Status | Covered behavior | Tests |
|---|---|---|---|
| Full-runtime startup | Implemented | `the --driver llm` selects the LLM/headless driver, skips curses initialization, opens files through the real THE runtime, and serves the LLM protocol over stdin/stdout. Normal `the` and `the --driver curses` remain curses-first. | `test_the_llm_full_runtime`, `test_driver_modules` |
| Protocol verbs | Implemented | The real-runtime protocol accepts `look`, `delta`, `capabilities`, `focus`, `hit`, `key`, `text`, `type`, `command`, `debug`, `transient`, and `quit`. | `test_the_llm_full_runtime`, `test_inputevent` |
| Capability discovery | Implemented | `the --driver llm capabilities` is the authoritative machine-readable capability report. It reports the full dispatcher, real buffers, syntax/style support, parser diagnostics availability, CREXX availability, transient support, and external build/test ownership. | `test_the_llm_full_runtime`, `test_the_llm_profile_crexx` |
| Full THE command dispatcher | Implemented | `command ...` calls THE's real `command_line(..., COMMAND_ONLY_FALSE)`, so existing editor commands, profiles, and macro dispatch paths stay in the full runtime. | `test_the_llm_full_runtime`, CREXX/pty tests when enabled |
| File open/save/write | Implemented through real runtime | Files are opened by real startup and editor commands such as `EDIT`, changed through the real command dispatcher or normalized input, and saved/written by real THE commands. | `test_the_llm_full_runtime`, CREXX/pty tests |
| Buffer/file-ring state | Implemented | Snapshots include real current-buffer metadata, dirty state, file-ring entries, line counts, and current-buffer markers. | `test_the_llm_full_runtime`, `test_llmruntime` |
| Cursor movement/reporting | Implemented | Snapshots report logical zone, file line, row, cell, desired cell, and focused row. Protocol `focus`, `hit`, `key`, `text`, and `type` enter through the real runtime. | `test_the_llm_full_runtime`, `test_inputevent`, `test_llmdriver` |
| Prefix command model | Implemented through real runtime | Prefix state and execution are THE's real prefix model. Snapshots expose prefix text and semantic prefix command fields where the runtime has them. | `test_the_llm_full_runtime`, `test_llmdriver` |
| Selection/block state | Implemented through real runtime | Snapshots expose real block/selection state from the current editor view. | `test_the_llm_full_runtime`, `test_llmdriver` |
| Screen/snapshot output | Implemented | Stable compact/full semantic snapshots include row roles, prefixes, file text, command/status rows, cursor state, styles, diagnostics, buffer metadata, and file-ring data. | `test_llmdriver`, `test_llmruntime`, `test_the_llm_full_runtime` |
| Delta views | Implemented | `delta ...` and `look delta ...` return retained previous-frame deltas with changed focus/status/buffer flags and changed semantic rows. | `test_llmdriver`, `test_the_llm_full_runtime` |
| Syntax/style spans | Implemented where runtime highlighting is active | Full-runtime snapshots are built from real `screenframe`/`SHOW_LINE` state and surface THE syntax/style spans after the real runtime enables colouring/parser state. | `test_the_llm_full_runtime`, `test_llmruntime` |
| Parser diagnostics | Implemented in full-runtime snapshots | SDSLH diagnostics remain in the full runtime. `look` output includes a first-class diagnostics array when parser messages exist, and `EXTRACT /PMSGS/` still uses the same collected PMSGS data. | `test_the_llm_parser_diagnostics`, `test_sdslh_integration` |
| CREXX/profile integration | Build-dependent full-runtime capability | `the --driver llm capabilities` reports `crexx_macros` according to the build. When CREXX is enabled, commands and profiles use the same full-editor CREXX integration path. | `test_the_llm_profile_crexx`, CREXX tests when enabled |
| Transient/modal UI | Implemented | `transient readv`, `transient dialog`, and `transient popup` expose no-curses look/input/hit/result flows through the shared `transientui` model. Command-triggered `READV CMDLINE`, `DIALOG`, and `POPUP` start resumable protocol continuations instead of terminal-only blocking loops. | `test_transientui`, `test_the_llm_full_runtime` |
| Unicode/UTF rendering metadata | Implemented within current renderer model | Render cells/clusters preserve wide cells, combining marks, keycaps, flags, emoji, ZWJ sequences, logical/display/cursor/paint widths, and repair strategy hints where the current renderer model supports them. | `test_headlessdriver`, `test_virtual_screen`, `test_llmdriver` |
| Mouse/logical hit reporting | Implemented logical subset | Protocol `hit` targets filearea, prefix, command, prompt, status, tabline/filetabs, divider, window selection, and transient UI targets without terminal packets. Terminal mouse escape packets remain curses-driver input and are converted before shared dispatch. | `test_the_llm_full_runtime`, `test_mousehit`, `test_inputevent` |
| Link boundary | Implemented | The main `the` executable and `the_driver_llm.so` do not link curses. The curses module owns curses startup/shutdown and terminal mechanics. | `test_driver_modules`, `test_curses_boundary`, `test_curses_boundary_inventory` |

## Outside The LLM Driver

| Item | Architectural reason | Current route |
|---|---|---|
| Build/test execution hooks | Running builds/tests is host automation, not editor behavior. Embedding process execution in the driver would couple buffer editing to shell orchestration and CI policy. | Host agents should run shell, CMake, and CTest directly outside THE. |
| Recursive project indexing | Deep project search, ignore-file handling, and repository indexing duplicate existing shell/agent tools. | Use `rg`, shell file tools, or future external project services. |
| Raw terminal mouse packets | Terminal escape packets are physical input owned by the curses driver. | Use logical `hit` protocol commands in `the --driver llm`; curses converts terminal packets for the live editor. |
| Unsupported editor behavior | Unsupported or build-dependent behavior must be machine-readable. | Report it through `the --driver llm capabilities` or focused diagnostics. |

## Agent UI Baseline

The strategic no-curses target now has the core controls expected by an agent
editor:

- stable machine-readable snapshots and retained-frame deltas.
- real THE command dispatch, profiles, prefix commands, file-ring state, and
  CREXX integration when built.
- logical key/text/hit input independent of terminal packets.
- real syntax/style spans and parser diagnostics where available.
- logical modal protocol for readv/dialog/popup flows.
- UTF-aware logical cursor movement decoupled from terminal rendering.
- no-curses link proof for both the main executable and the LLM driver module.

Capabilities supplied by the host agent rather than THE:

- shell/build/test execution and progress channels.
- repository-scale search and file indexing.
- workflow or skill enforcement beyond the documented protocol and
  `the --driver llm capabilities`.
