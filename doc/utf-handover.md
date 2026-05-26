# UTF-8 Cursor/Driver Handover

Last updated: 2026-05-26.

This is the practical source of truth for the UTF, cursor, driver, and LLM
reorganization. Use `doc/cursor-driver-architecture.md` for the ownership
contract and `doc/llm-driver-agent-guide.md` for the agent protocol, but use
this file to decide what is closed, what remains, and what proof is required.

## Architecture Rule

```text
editor command -> logical UI model -> physical driver
physical input -> normalized input event -> editor command
```

The logical editor model owns focus, file line, logical screen row, row role,
logical `TextPos`, desired horizontal cell, logical viewport start, and text
mutation byte ranges derived from logical positions.

The curses driver owns terminal mechanics: physical cursor save/restore,
refresh ordering, touch/update calls, curses cell writes, software cursor
painting, UTF repair execution, logical-to-physical display-column mapping, and
hardware cursor parking. These mechanics remain in `src/cursesdriver.c`; do not
move them into editor command code while cleaning up the split.

The LLM and fake-driver surfaces must prove new logical behavior before the
same behavior is treated as migrated in the curses path. Use no-curses agent
tests, virtual/fake-driver tests, focused unit tests, or CREXX/pty full-editor
tests depending on which risk the slice touches.

## Done

- Architecture and baseline guardrails are documented. `test_curses_boundary`
  keeps the logical foundation modules curses-free and requires `execute.c`
  direct curses calls to go through `cursesdriver.c`.
- UTF terminal profiles, logical text positioning, layout, and repair planning
  are separated. `src/textpos.c`, `src/logcursor.c`, `src/utflayout.c`,
  `src/utfrepair.c`, and `src/utfterm.c` have focused tests.
- `src/cursesdriver.c` owns the migrated physical primitives used by the
  current work: cursor capture/move/restore, window origin/size reads,
  refresh/update/touch helpers, input timeouts, cell write/fill helpers,
  software cursor painting, UTF file-area target calculation, cursor repair
  transitions, and cursor parking/presentation.
- `src/uidriver.c` and `src/screenframe.c` provide logical frames, row roles,
  cursor overlays, cursor rebasing, and fake-driver operation logs. The virtual
  harness covers file rows, prefixes, command/status rows, tabline, divider,
  window rows, UTF fixture rows, compact LLM views, cursor overlays, targeted
  redraw row selection, logical hits, and fake-driver logs.
- `src/inputevent.c` owns normalized text, key, command, logical-hit, and debug
  events plus legacy key conversion and queues.
- Live curses mouse input now converts terminal packets at the driver edge
  into `TheInputEvent` logical-hit targets for file area, prefix, command
  line, status, file tabs, divider, and window selection. The normal
  `THEMouse` dispatch path stores those targets, `CURSOR MOUSE` consumes the
  saved logical row/cell/line/screen/window data, `TABFILE` consumes the
  file-tab target cell, and `test_mousehit` covers the no-curses target
  mapping helper. Modal readv/popup/dialog mouse loops remain physical and
  deferred because they bypass the normal live dispatch path and lack logical
  popup or dialog models.
- `src/llmdriver.c` exposes role-aware semantic snapshots, compact token-saving
  view modes, input wrappers, cursor-mapping diagnostics, driver-operation log
  formatting, and last-render explanation text.
- `src/agentdriver.c` and `tools/the_agent.c` are a no-curses proof target.
  They load a file, expose semantic snapshots, support file-area and
  command-line focus, accept normalized key/text/command/logical-hit/debug
  input, and keep unsupported full-editor commands explicit through
  `capabilities`.
- `the_agent` has the small SOS/navigation/edit subset needed for
  cursor-driver confidence:
  `TOPEDGE`, `BOTTOMEDGE`, `LEFTEDGE`, `RIGHTEDGE`, `FIRSTCOL`, `LASTCOL`,
  `ENDCHAR`, `FIRSTCHAR`, `DELCHAR`, `CUADELCHAR`, `DELBACK`,
  `CUADELBACK`, `DELEND`, `DELWORD`, `PREFIX`, `TABFIELDF`,
  `TABFIELDB`, `QCMND`, and `EXECUTE`.
- File-area logical cursor work is established for UTF left/right movement,
  text insertion, `SOS DELBACK`, `SOS DELCHAR`, and `SOS DELWORD`.
  These paths prefer `VIEW_DETAILS.logical_cursor` and derive byte ranges
  through `TextPos`.
- Command-dispatch coverage step 2 closed 2026-05-26. The no-curses agent
  now covers `SOS DELWORD`, `SOS PREFIX`, `SOS TABFIELDF`, `SOS TABFIELDB`,
  and the prefix/filearea edge cases for `TOPEDGE`, `BOTTOMEDGE`, and
  `LEFTEDGE`, while `capabilities` still declares the agent subset and keeps
  full prefix commands and full THE dispatcher behavior unsupported. Agent
  CTests cover the no-curses behavior and capability output, CREXX/pty tests
  cover the corresponding full-editor SOS behavior, and
  `test_curses_boundary` now guards the agent files plus the cleaned
  `commsos.c` physical cursor-call surface.
- Renderer cursor ownership has moved substantially: full file-area redraw
  builds a live `UiFrame`; file-area, prefix, command, status/HEXDISPLAY,
  view-switch restoration, render-exit materialization, targeted prefix redraw,
  targeted command redraw, SDSLH bracket matching, and UTF whole-line repair
  use logical or frame-backed cursor data instead of active-window cursor
  snapshots.
- Manual terminal smoke for the Step 2 baseline is green as of 2026-05-26:
  startup paints a software cursor, prefix/file-area vertical movement keeps
  logical cursor columns stable and visible, and command-line Shift-Tab back to
  the file area repaints after the field transition. CREXX/pty coverage now
  exercises the same column-preservation and tab-field cases where they are
  query-observable.
- `execute.c` no longer calls the guarded curses primitives directly. Ordinary
  cursor effects for move-cursor, make-current, block rearrange preservation,
  inserted-line placement, selective-change prompt placement, and transient
  windows either use logical state or driver-owned physical wrappers.
- `src/cursor.c`, `src/comm5.c`, `src/query1.c`, `src/query2.c`, and
  `src/edit.c` are clean for direct `getyx`, `wmove`, `getbegyx`, `getmaxx`,
  `getmaxy`, and `wtimeout` calls. `query1.c`, `query2.c`, and `commsos.c`
  have also retired the active-driver cursor snapshot fallback for the focused
  query and SOS row/cell surface.
- Macro and agent diagnostics now distinguish THE message history
  (`EXTRACT /MESSAGES/`, `QUERY MESSAGES`) from SDSLH parser diagnostics
  (`SDSLHWAIT`, `EXTRACT /PMSGS/`, `QUERY PMSGS`).

## In Progress

- Renderer fallback purge. `show.c` still contains targeted redraw and local
  physical cursor-preservation mechanics. Those are acceptable only while they
  remain physical mechanics routed through `cursesdriver.c`; logical decisions
  should continue moving to `UiFrame` or editor-owned state.
- Normalized input. Curses key collection now passes through `TheInputEvent`
  before legacy key dispatch, and the normal live mouse path now converts
  driver-edge packets to logical-hit targets before legacy mouse-definition
  dispatch. Command dispatch still mostly consumes legacy key codes, and modal
  readv/popup/dialog mouse loops remain deferred physical behavior.
- Agent command bridge. `the_agent` is useful and tested, but it remains an
  intentional agent subset. It does not run THE's full command dispatcher,
  full prefix command machinery, CREXX macro/profile behavior, popup/dialog
  behavior, or arbitrary SOS commands.
- Command and SOS cleanup. The focused Step 2 command/SOS coverage is closed.
  Larger command groups such as full key-navigation and full prefix-command
  machinery remain deferred rather than part of the completion path.
- Guardrail ratchet. `tests/check_curses_boundary.sh` is intentionally
  permissive outside logical modules and `execute.c`. Tighten it only when a
  module or behavior group has equivalent logical or driver-owned coverage.
- Live frame coverage. `screenframe_build()` captures the current live
  file-area rows. Command, prompt, status, popup/dialog, and full window
  lifecycle state still need broader live logical snapshots before they can be
  treated as first-class non-curses UI state.
- Keycap/blank-cell paint bug. Current evidence points below logical mapping,
  in curses refresh/materialization or terminal treatment of blank cells after
  keycap glyphs. Do not fix it by changing grapheme segmentation or adding
  keycap-specific editor logic.

## Three-Step Completion Plan

The remaining work should finish in three concrete steps. Do not add new
parallel "next" lists unless one of these steps is closed or explicitly
deferred.

1. Close input. Closed 2026-05-26.
   Live curses mouse packets are converted at the driver edge into
   `TheInputEvent` logical-hit targets for file area, prefix, command line,
   status, file tabs, divider, and window selection. Existing mouse dispatch is
   routed through the saved targets where practical: `CURSOR MOUSE` consumes
   logical row/cell/line/screen/window data and `TABFILE` consumes the file-tab
   target cell. Covered by `test_inputevent`, `test_mousehit`,
   `test_agentdriver`, `test_the_agent_script`, and
   `test_the_agent_no_curses`. Modal readv/popup/dialog mouse loops are
   excluded because those transient UI models are deferred.

2. Close command dispatch coverage. Closed 2026-05-26.
   `the_agent` now supports the remaining small SOS/navigation/edit cases
   needed for cursor-driver confidence: `SOS DELWORD`, `SOS TABFIELDF`,
   `SOS TABFIELDB`, `SOS PREFIX`, and the prefix/filearea edge-navigation
   cases. Unsupported full-editor behavior remains explicit in
   `capabilities`: the agent is still an agent subset, full prefix commands
   are not modeled, and CREXX/profile behavior still belongs to the full
   editor surface. Covered by `test_agentdriver`, `test_the_agent_script`,
   `test_the_agent_capabilities`, `test_the_agent_no_curses`, and CREXX/pty
   `test_sos_navigation_queries` plus existing `test_sos_logical_edit_queries`
   coverage. `tests/check_curses_boundary.sh` now guards the no-curses agent
   files and the cleaned SOS physical cursor-call surface.

3. Close renderer and terminal paint.
   Remove the remaining `show.c` targeted-redraw logical fallbacks by replacing
   them with frame-backed row/cell decisions. In the same step, add the keycap
   one-line demonstrator and compare the emitted `cursesdriver.c` operation
   sequence with the probe's working `first`/`whole` paths. Close when
   `test_virtual_screen` covers the renderer decisions, the curses path uses
   the same logical targets, physical save/restore, refresh, touch/update, cell
   writes, software cursor painting, and cursor parking remain in
   `cursesdriver.c`, and the keycap failure is either fixed or isolated to a
   documented physical driver/profile bug.

## Deferred Or Larger Slices

- Full `the_agent` integration with THE's complete command dispatcher.
- Full prefix command machinery in the no-curses agent.
- Logical popup, dialog, and window lifecycle models. Until these have
  snapshots and virtual tests, keep their window management as driver-owned
  physical behavior.
- Delta LLM views based on retained previous frames.
- A strict no-curses rule for all editor modules outside the driver/renderer
  boundary. Tighten module by module as coverage lands.
- Linux, Windows Terminal, iTerm2, and other terminal baselines. Finish the
  macOS Apple Terminal proof loop first.

## Closing Rules

A migration task is closed only when all applicable items are true:

- Logical behavior is observable through `the_agent`, `llmdriver`, a virtual
  screen/fake-driver test, CREXX/pty, or a focused unit CTest.
- The real curses path uses the same logical input/frame/cursor data as the
  test surface.
- Unsupported behavior remains explicit in `the_agent capabilities` or in this
  handover.
- Physical mechanics remain inside `cursesdriver.c` or a clearly documented
  physical edge.
- The relevant guardrail is tightened when a module or behavior class is fully
  cleaned.
- `git diff --check` passes, and any touched code has focused build/test
  coverage.

## Test Surfaces

- `the_agent`: no-curses driver-boundary proof for logical snapshots,
  normalized input, command-line/file-area/prefix focus, logical hits,
  explicit unsupported commands, and the closed Step 2 SOS subset. It does not
  prove the full editor dispatcher.
- CREXX/pty tests: full-editor command and SOS behavior. They require CREXX
  support, a working CREXX compiler/import runtime, and a pty-capable host.
  Skip messages should name missing prerequisites.
- Virtual/fake-driver tests: preferred proof for renderer migration because
  they compare semantic rows, cursor overlays, and requested operations without
  curses paint timing.
- Manual terminal smoke: final paint check only. A useful manual regression
  should become a CTest, agent capability disclosure, or focused diagnostic.

Important focused tests:

- `test_textpos`, `test_logcursor`, `test_textedit`
- `test_utfterm`, `test_utflayout`, `test_utfrepair`, `test_utf_fixture`
- `test_inputevent`, `test_mousehit`, `test_uidriver`, `test_screenframe`
- `test_llmdriver`, `test_llmruntime`, `test_virtual_screen`
- `test_agentdriver`, `test_the_agent_script`,
  `test_the_agent_capabilities`, `test_the_agent_no_curses`
- `test_curses_boundary`
- CREXX/pty tests such as `test_normal_area_queries`,
  `test_sos_navigation_queries`, `test_sos_logical_edit_queries`, and
  `test_selective_change_prompt` when CREXX is enabled.

## Important Artifacts

- `doc/cursor-driver-architecture.md`: ownership contract and guardrails.
- `doc/llm-driver-agent-guide.md`: agent-facing protocol and no-curses proof
  target usage.
- `doc/llm-mode.md`: shorter conceptual guide for agents and tool authors.
- `doc/utf-design.md`: historical UTF design notes and detailed findings.
- `tools/utf_terminal_probe.c`: interactive terminal calibration/probe tool.
- `src/utfterm_defaults.h`: shared THE/probe coded default terminal table.
- `src/utflayout.c`, `src/uidriver.c`, `src/screenframe.c`,
  `src/cursesdriver.c`, `src/inputevent.c`, `src/mousehit.c`,
  `src/llmdriver.c`,
  `src/agentdriver.c`: current driver-boundary modules.
- `system-osx.the`: macOS system UTF-8 profile consumed by THE and generated by
  the probe.
- `tests/fixtures/utf-render.txt`: manual editor fixture for UTF rendering.

## macOS Apple Terminal Baseline

The current macOS baseline is `system-osx.the`. It is a complete system
profile, not a defaults-plus-overrides pair. Important observed choices:

- `regional-flag`: default `L3 C3`, cursor `cells`, replacement `suffix`.
- `keycap`: `L2 C2`, cursor `first`, replacement `first`.
- `modifier`: `L4 C4`.
- ZWJ grouped display: use `substitute` for `short-zwj`, `heart-zwj`, and
  `family-zwj`.
- ZWJ component display: `short-zwj` uses `native L4 C4`; `heart-zwj` uses
  `expanded L6 C6`; `family-zwj` uses `expanded L8 C8`.

Temporary cursor tracing has been removed, but keep the finding: the macOS
build uses ncurses (`USE_NCURSES=1`, linked to
`/usr/lib/libncurses.5.4.dylib`). Focused keycap traces showed THE advancing
the logical target and curses-driver display target correctly through
end-of-line, so the remaining visible jump does not look like a logical cursor
or viewport calculation failure.

The diagnostic-dot experiment was useful but is not a fix. Painting after-EOL
cells as `.` suppressed the symptom because the terminal had visible glyphs to
materialize; repainting normal spaces still left the issue. Keep repairs in
the shared physical driver/profile path.

## Probe Usage

Build the probe:

```sh
cmake --build cmake-build-debug --target utf_terminal_probe -j2
```

Open interactive calibration:

```sh
./cmake-build-debug/utf_terminal_probe calibrate all \
  --profile-dir ./cmake-build-debug/release
```

Validate the saved macOS profile non-visually:

```sh
./cmake-build-debug/utf_terminal_probe calibrate all --no-visual \
  --profile system-osx.the
```

Add `--write-profile` to make a scripted non-visual run rewrite the profile
after validation.
