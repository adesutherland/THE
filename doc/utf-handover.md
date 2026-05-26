# UTF-8 Cursor/Driver Handover

Last updated: 2026-05-26.

This is the practical status ledger for the UTF, cursor, driver, and LLM
reorganization. Keep it short. Put detailed design history in `doc/utf-design.md`
and protocol details in `doc/llm-driver-agent-guide.md`.

## Architecture Rule

```text
editor command -> logical UI model -> physical driver
physical input -> normalized input event -> editor command
```

The editor owns logical focus, row roles, line numbers, `TextPos`, desired
horizontal cell, logical viewport start, and text mutation ranges.

The physical driver owns terminal mechanics: curses windows, cursor
save/restore, refresh/update/touch ordering, cell writes/fills, software cursor
painting, UTF repair execution, logical-to-physical display-column mapping, and
hardware cursor parking. These mechanics stay in `src/cursesdriver.c` or in a
temporary physical edge explicitly being migrated there.

New logical behavior must be proved through a no-curses surface first:
`the_agent`, `llmdriver`, `llmruntime`, virtual/fake-driver tests, focused unit
tests, or CREXX/pty full-editor tests.

## Platform Policy

Current supported build surfaces are intentionally narrow:

- Linux/macOS: the CMake `the` target with system ncurses/curses, plus existing
  no-curses tools and tests.
- Windows: the CMake `the` target with the bundled PDCursesMod `wincon`
  backend only.
- No-curses/headless: `the_agent`, `the_llm_headless`, and their existing
  focused tests.

Dropped legacy support includes DOS, OS/2, Amiga, VMS, BeOS/Haiku packaging,
QNX, X11/SDL/VT PDCurses variants, WinGUI PDCurses, Plan 9/framebuffer/OpenGL
PDCurses backends, old Watcom/DJGPP/Borland/TurboC-style makefiles, bundled
PDCurses demos/tests/CI, and obsolete package/pkg-config recipes not used by
current CMake.

This housekeeping pass deliberately removed unbuilt files and vendor ballast
only. Many active source files still contain `#if defined(DOS)`,
`#if defined(OS2)`, `#if defined(VMS)`, `#if defined(AMIGA)`, `USE_XCURSES`,
`USE_SDLCURSES`, `USE_VTCURSES`, or similar compatibility branches. Leave
those in place until a later scripted source simplification pass can remove
them mechanically and verify the supported targets in one sweep.

## Done

- UTF and cursor primitives are separated and tested:
  `src/textpos.c`, `src/logcursor.c`, `src/textedit.c`, `src/utflayout.c`,
  `src/utfrepair.c`, and `src/utfterm.c`.
- `src/cursesdriver.c` owns the migrated physical primitives used by the
  cursor/paint work: cursor capture/move/restore, window origin/size reads,
  input timeouts, refresh/update/touch helpers, cell writes/fills, software
  cursor painting, UTF repair execution, and cursor parking.
- `src/uidriver.c`, `src/screenframe.c`, `src/llmdriver.c`, and
  `src/llmruntime.c` provide semantic frames, role-aware snapshots, cursor
  overlays, fake-driver operation logs, compact views, and debug formatting.
- `src/agentdriver.c` plus `tools/the_agent.c` provide the no-curses proof
  target. It supports file-area, command, and prefix focus; normalized
  key/text/command/logical-hit/debug input; exact capability reporting; and the
  closed SOS/navigation/edit subset.
- Normal live curses mouse input is closed for the main `THEMouse` dispatch
  path. Terminal packets are converted at the driver edge into
  `TheInputEvent` logical-hit targets for file area, prefix, command line,
  status, file tabs, divider, and window selection.
- The transient UI headless boundary is closed for readv, dialog, and popup.
  `src/transientui.c` provides a curses-free logical snapshot model for
  geometry, row roles, prompt/title/edit/button/item state, popup viewport
  offsets, focus, and hit targets. `test_transientui` proves readv editing,
  dialog focus/button/hit transitions, and popup navigation/selection without
  curses.
- The curses readv/dialog/popup paths now materialize transient snapshots and
  route modal mouse clicks through logical hit targets where practical. Raw
  mouse decoding moved behind `curses_driver_read_mouse_event()`.
- `the_llm_headless` is a real no-curses LLM/headless build target. It links
  the agent, LLM formatter, input model, and transient UI model without
  `src/cursesdriver.c`; `test_the_llm_headless_no_curses` checks dependencies
  and symbols.
- Command-dispatch coverage Step 2 is closed. `the_agent` covers `SOS DELWORD`,
  `SOS PREFIX`, `SOS TABFIELDF`, `SOS TABFIELDB`, and prefix/filearea edge
  behavior for `TOPEDGE`, `BOTTOMEDGE`, and `LEFTEDGE`. Full prefix commands
  and the full THE dispatcher remain explicitly unsupported in capabilities.
- Renderer and terminal-paint Step 3 is closed. UTF `show.c` targeted
  file-area/prefix restore, `prepare_view()` cursor placement, and one-line
  prefix repaint choose row/cell/text targets from the live `UiFrame`.
  `test_virtual_screen` covers the frame-backed renderer targets and the
  keycap/space/after-EOL demonstrator; `test_utfrepair` compares that case
  with the probe's working `first` and `whole` repair paths.
- Focused guardrails are active. `test_curses_boundary` keeps logical modules,
  the no-curses agent, `execute.c` direct curses usage, the cleaned SOS cursor
  surface, and the cleaned readv/dialog/popup transient paths from regressing.
  `test_the_agent_no_curses` and `test_the_llm_headless_no_curses` prove the
  no-curses executables do not link curses or expose curses-driver symbols.

## Active Slice

No active migration slice is selected after closing the transient UI headless
boundary. Pick the next slice from the debt buckets below and close it with the
same proof pattern: no-curses model first, curses path uses that model, then
guardrail the cleaned surface.

## Direct Curses Inventory

`tests/inventory_direct_curses.sh` reports remaining direct curses dependencies
outside `src/cursesdriver.*`, bundled PDCurses, and contrib code. Current sweep
counts:

- `physical-input`: 33
- `mouse-token`: 30
- `physical-paint`: 171
- `driver-wrapper`: 408
- `window-state`: 429

For the cleaned transient functions, the sweep finds no raw `physical-input` or
`physical-paint` calls in `readv_cmdline()`, `execute_dialog()`, or
`execute_popup()`. Remaining transient findings are explicitly classified:
`WINDOW` ownership in the curses path, `KEY_MOUSE` branch tokens, and
`curses_driver_*` physical wrapper calls.

Use the inventory as a planning tool, not a project-wide failure gate yet. The
project still has legitimate legacy debt in command modules, colour/setup,
window lifecycle, render refresh paths, and compatibility code. Tighten hard
failures only after a behavior group is migrated and tested.

## Deferred Buckets

Boundary debt:

- Remaining direct curses calls in legacy command/render/setup modules outside
  `src/cursesdriver.c`, especially the `physical-input`, `physical-paint`, and
  `window-state` categories above.
- `driver-wrapper` entries outside the driver are allowed for migrated physical
  mechanics today, but many still indicate command code owning physical window
  timing or cursor placement. Classify them slice by slice.
- Removal of `cursor_focus_sync_current()` after all file-area, prefix, and
  command entry paths set editor-owned logical cursor state before render.
- Full live `UiFrame` snapshots for command, prompt, status, and window
  lifecycle rows beyond the transient UI model.
- Broader modal/window lifecycle cleanup: readv/dialog/popup now have logical
  snapshots, but the full window creation/deletion/colour-paint mechanics still
  live in the curses path by design.

LLM/headless feature gaps, after the headless boundary exists:

- Full `the_agent` integration with THE's complete command dispatcher.
- Full prefix command machinery in the no-curses agent.
- Full normalized key-command dispatch after the current compatibility bridge.
- Agent protocol integration for transient UI snapshots beyond the
  `the_llm_headless --transient-demo` debug/demo surface.
- Delta LLM views based on retained previous frames.

Terminal/profile follow-ups:

- The remaining keycap blank-cell terminal symptom. Logical target cells,
  fake-driver operation sequence, and `first`/`whole` repair plans pass. The
  next hypothesis is physical curses refresh/materialization or terminal
  profile policy, not logical segmentation.
- Linux, Windows Terminal, iTerm2, and other terminal baselines. Finish the
  macOS Apple Terminal proof loop first.

## Closing Rules

A migration task is closed only when all applicable items are true:

- logical behavior is observable through a no-curses surface, virtual/fake
  driver, focused CTest, or CREXX/pty full-editor test.
- the real curses path uses the same logical input/frame/cursor data.
- unsupported behavior is explicit in `the_agent capabilities` or this file.
- physical mechanics remain inside `cursesdriver.c` or a documented physical
  edge scheduled for migration.
- guardrails are tightened for the cleaned module or behavior class.
- `git diff --check` and focused tests pass.

## Useful Tests

- `test_textpos`, `test_logcursor`, `test_textedit`
- `test_utfterm`, `test_utflayout`, `test_utfrepair`, `test_utf_fixture`
- `test_inputevent`, `test_mousehit`, `test_uidriver`, `test_screenframe`
- `test_llmdriver`, `test_llmruntime`, `test_virtual_screen`
- `test_agentdriver`, `test_the_agent_script`,
  `test_the_agent_capabilities`, `test_the_agent_no_curses`
- `test_transientui`, `test_the_llm_headless_no_curses`
- `test_curses_boundary`, `test_curses_boundary_inventory`
- CREXX/pty tests such as `test_normal_area_queries`,
  `test_sos_navigation_queries`, `test_sos_logical_edit_queries`, and
  `test_selective_change_prompt` when CREXX is enabled.

## Important Artifacts

- `doc/cursor-driver-architecture.md`: ownership contract and guardrails.
- `doc/llm-driver-agent-guide.md`: agent-facing protocol and no-curses proof
  target usage.
- `doc/llm-mode.md`: conceptual guide for agents and tool authors.
- `doc/utf-design.md`: historical UTF design notes and detailed findings.
- `tools/utf_terminal_probe.c`: interactive terminal calibration/probe tool.
- `tools/the_llm_headless.c`: no-curses headless/LLM executable skeleton with
  transient UI model linkage.
- `tests/inventory_direct_curses.sh`: direct-curses debt inventory by
  file/function/category.
- `src/utfterm_defaults.h`: shared THE/probe coded default terminal table.
- `src/cursesdriver.c`, `src/inputevent.c`, `src/mousehit.c`,
  `src/transientui.c`, `src/uidriver.c`, `src/screenframe.c`,
  `src/llmdriver.c`, `src/llmruntime.c`, `src/agentdriver.c`: current
  driver-boundary modules.
