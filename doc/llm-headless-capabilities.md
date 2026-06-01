# LLM/Headless Capability Inventory

Last updated: 2026-05-29.

This inventory is the source of truth for the no-curses LLM/headless editor
surface. The executable source of truth is `the_agent capabilities`; this file
explains the architecture and test coverage behind that output.

## Capability Status

| Area | Status | Covered behavior | Tests |
|---|---|---|---|
| Startup/session lifecycle | Implemented | `the_agent` starts with configurable rows/cols and optional file; `the_llm_headless --mini-session` drives a temp editing session; `new`/`new!` reset buffers; `capabilities` reports the supported agent surface. | `test_the_agent_script`, `test_the_agent_capabilities`, `test_the_llm_headless_no_curses_mini_session`, `test_agentdriver` |
| File open/save/write | Implemented | Load existing or new files, `open`/`load`/`edit`, forced `open!`, `save [PATH]`, `write [PATH]`, stable save status, snapshot buffer path/dirty/line count. | `test_the_agent_script`, `test_the_llm_headless_no_curses_mini_session`, `test_agentdriver` |
| Buffer/file navigation | Implemented for agent target | File navigation supports top/bottom/goto/page/SOS visible-edge movement. Agent buffers support `buffer-open`, `buffer-switch`, `buffer-list`, and `buffer-close[!]`; snapshots include a buffer list with current/dirty/line-count metadata. | `test_the_agent_script`, `test_agentdriver` |
| Project/file awareness | Implemented for agent target | `project-list [DIR]` exposes a deterministic flat directory listing in snapshots. Recursive tree semantics and editor project indexing are outside the target; agents should use shell tools for deep project queries. | `test_the_agent_script` |
| Cursor movement/reporting | Implemented | Logical filearea, prefix, and command cursor zones; left/right/up/down/home/end/page/tab movement; snapshots report zone, line, row, cell, desired cell, and focused row. | `test_agentdriver`, `test_llmdriver`, `test_virtual_screen`, `test_screenframe` |
| Text insertion/deletion/replacement | Implemented | Text/codepoint input, `insert`/`type`, delete/backspace, SOS `DELCHAR`/`DELBACK`/`DELEND`/`DELWORD`, `replace`, `replace-all`, virtual-space padding, UTF-aware cursor movement. Prefix focus also supports text insertion, delete, backspace, and delete-to-end for the prefix field. | `test_agentdriver`, `test_the_agent_script` |
| Line operations | Implemented | `setline`, `insertline`, `appendline`, `deleteline`, `duplicateline`. | `test_agentdriver`, `test_the_agent_script` |
| Search/find navigation | Implemented | `find`, `search`, `find-next`, `find-prev` across visible and non-visible buffer lines, including UTF text as byte-exact search terms with logical-cell cursor placement. | `test_agentdriver`, `test_the_agent_script` |
| Agent command model | Implemented subset | `the_agent` routes normalized command input through a deliberate no-curses command subset for editing work. Unsupported commands return structured diagnostics and point callers to `capabilities`. | `test_the_agent_capabilities`, `test_the_agent_script` |
| Prefix command model | Implemented subset | Snapshots expose semantic `pc` / `prefix_command` state. Agents can enter/clear/execute `d`, `del`, `delete`, `dup`, `copy`, `r TEXT`, `i TEXT`, and `a TEXT` through `prefix`, `prefix-clear`, and `prefix-execute`. | `test_agentdriver`, `test_the_agent_script`, `test_llmdriver` |
| Selection/range model | Implemented subset | Snapshots expose active selection endpoints and clipboard text. Commands support `select L1 C1 L2 C2`, `selection-copy`, `selection-delete`, `selection-replace TEXT`, and clear operations, including wide-cell UTF replacement. | `test_agentdriver`, `test_the_agent_script`, `test_llmdriver` |
| Undo/redo | Implemented agent-side | Bounded agent-side history covers content mutations made through the agent target; snapshots expose undo/redo availability. Undo/redo preserves dirty state and file content for agent mutations. | `test_agentdriver`, `test_the_agent_script`, `test_llmdriver` |
| Status/error reporting | Implemented | Every command returns JSON ack status; unsupported commands return structured diagnostics; snapshots include status plus buffer path/dirty/line count. | `test_the_agent_capabilities`, `test_llmdriver` |
| Screen/snapshot output | Implemented | Stable compact/full semantic snapshots with row roles, prefixes, semantic prefix commands, file text, command/status rows, cursor, styles, history, selection, buffers, project files, and buffer metadata. | `test_llmdriver`, `test_llmruntime`, `test_virtual_screen`, `test_agentdriver` |
| Delta views | Implemented | `delta ...` and `look delta ...` return retained previous-frame deltas with changed focus/status/buffer flags and changed semantic rows. | `test_agentdriver`, `test_the_agent_script`, `test_llmdriver` |
| Transient/modal UI | Implemented demo protocol | `transient readv`, `transient dialog`, and `transient popup` expose live no-curses look/input/hit/result flows using the shared `transientui` model. `the_llm_headless --transient-demo` remains a standalone formatting proof. | `test_transientui`, `test_the_agent_script`, `test_the_llm_headless_no_curses` |
| Unicode/UTF oddities | Implemented within current renderer model | Render cells/clusters preserve wide cells, combining marks, keycaps, flags, emoji, ZWJ sequences, logical/display/cursor/paint widths, and repair strategy hints where the current renderer model supports them. Agent editing uses UTF-aware `TextPos` movement. | `test_headlessdriver`, `test_virtual_screen`, `test_agentdriver` |
| Mouse/logical hit reporting | Implemented logical subset | Logical hits target filearea, prefix, command, prompt, status, tabline/filetabs, divider, and window selection without terminal packets. Terminal mouse escape packets are handled by the curses driver and converted before reaching shared input. | `test_agentdriver`, `test_the_agent_script`, `test_mousehit` |

## Outside The LLM/Headless Editor Target

These items are classified outside the LLM/headless editor target because they
require a different runtime surface or are intentionally owned by the host
agent:

| Item | Architectural reason | Current route |
|---|---|---|
| Full THE command dispatcher | It depends on the full editor command/profile runtime and broad command semantics. Pulling it into `the_agent` would turn the no-curses agent target into a second full editor frontend instead of a bounded driver-boundary editor surface. | Use the normal `the` executable, CREXX/pty tests, or future selectable-driver startup work. |
| CREXX macros | CREXX requires macro/profile integration, optional external libraries, and full editor runtime state. The no-curses agent target links neither curses nor the full macro/runtime stack. | Use full-editor CREXX integration paths. |
| Terminal mouse packets | The agent path consumes logical hit targets from snapshots. Raw terminal escape packets are physical input and belong in `src/cursesdriver.c`. | Use `hit TARGET LINE ROW CELL` in `the_agent`; curses converts terminal mouse packets for the live editor. |
| Build/test execution hooks | Running builds/tests is a host automation concern. Embedding process execution in the editor driver would couple buffer editing to shell orchestration and CI policy. | Agents should run shell, CMake, and CTest directly outside THE, then use THE snapshots/status for editing. |
| Recursive project indexing | Deep project search, ignore-file handling, and repository indexing duplicate existing shell/agent tools. The editor target exposes a flat `project-list` for quick context only. | Use `rg`, shell file tools, or future external project services. |

## Modern Coding-Agent UI Comparison

The no-curses target now has the core editor controls expected by an agent UI:

- stable machine-readable snapshots and retained-frame deltas.
- precise text, line, selection, prefix, and file commands.
- status and structured unsupported-command diagnostics.
- buffer list and flat project listing metadata.
- undo/redo availability for agent mutations.
- logical modal demo protocol for readv/dialog/popup flows.
- UTF-aware logical cursor movement decoupled from terminal rendering.
- no-curses link proof for both the interactive agent target and headless
  mini-session target.

Capabilities intentionally supplied by the host agent rather than THE:

- shell/build/test execution and progress channels.
- repository-scale search and file indexing.
- workflow or skill enforcement beyond the documented protocol and
  `the_agent capabilities`.
