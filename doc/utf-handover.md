# UTF-8 Cursor/Driver Handover

Last updated: 2026-05-27.

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

High-level editor code calls the current driver vtable through
`the_driver->...`. The real vtable lives in `src/thedriver.h`, `src/thedriver.c`
sets the current build's driver to the curses implementation, and
`src/cursesdriver.c` publishes `the_curses_driver_ops`. `curses_driver_*` names
are now curses implementation details; editor code, including temporary
physical edges that still traffic in `WINDOW *`, `chtype`/`cchar_t`, pads, or
modal local windows, calls the vtable directly.

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
- `src/cursesdriver.c` also owns raw curses mouse packet mechanics: raw
  `KEY_MOUSE` translation to `THE_KEY_MOUSE`, PDC/ncurses packet storage,
  button/action/modifier decoding, saved physical mouse coordinates, and
  window-local mouse coordinate projection.
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
  mouse packet decoding lives in `src/cursesdriver.c` behind
  `curses_driver_read_mouse_event()` and `curses_driver_read_mouse_button()`.
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
- The direct-curses inventory is now a ratchet. The full listing remains
  available, `--summary` separates actionable physical-input, physical-paint,
  mouse-token, and window-state/type debt from allowed driver-edge vtable
  calls, and `test_curses_boundary_inventory` fails only when
  actionable category/file/function buckets exceed
  `tests/inventory_direct_curses.baseline.tsv`.
- Bulk wrapper passes moved high-confidence raw physical paint/input mechanics
  in `comm1.c`, `comm2.c`, `comm3.c`, `comm4.c`, `commset1.c`,
  `commset2.c`, `commutil.c`, `error.c`, `file.c`, `getch.c`, `mouse.c`,
  `prefix.c`, `query.c`, `query2.c`, `rexx.c`, `the.c`, and `util.c`
  behind `curses_driver_*` wrappers. The corrected paint scanner also exposed
  suffixed raw paint/cell calls in `comm3.c`, `comm4.c`, `comm5.c`,
  `commset1.c`, `error.c`, `query.c`, `query2.c`, `show.c`, and `util.c`;
  those are now behind driver wrappers. The later renderer/window-state
  cleanup closed the broad `show.c` display-line storage surface.
- The direct-curses inventory scanner now handles `#if 0 ... #else ...
  #endif` correctly by scanning the active `#else` arm. The ratchet is both a
  CTest (`test_curses_boundary_inventory`) and a build target
  (`curses_boundary_inventory`).
- Adjacent dead platform branches were pruned where they obscured this
  inventory: the unsupported VMS raw-key path in `getch.c` and the unsupported
  DOS/OS2/disabled Win32 cursor visibility overrides in `nonansi.c`.
- The active-window/window-handle cleanup slice moved common role-window
  mechanics into `src/cursesdriver.c`: current/screen role existence checks,
  cursor capture/move/restore, role clear-to-EOL, role touch/refresh/attr,
  current-window key/cell reads, global status/error/divider/filetabs helpers,
  dialog command-window role swapping, and mouse window projection by role.
  Legacy command, cursor, edit, query, scroll, error, and setup-adjacent paths
  now ask the driver for those physical windows by logical role.
- The real driver vtable exists. `TheDriverOps` carries the migrated
  current/screen/global role, cursor, touch/refresh/clear/attr, current-window
  key/cell, mouse projection, standard-screen, opaque temporary window-handle,
  renderer-cell, and logical cursor operations. Editor call sites use
  `the_driver->...`; `src/cursesdriver.c` keeps the `curses_driver_*` function
  names as implementation-private helpers behind `the_curses_driver_ops`.
  `tools/codemod_driver_vtable.py` is the repeatable migration pass: it derives
  exact implementation-to-vtable mappings from `the_curses_driver_ops` and
  rewrites safe current/screen/global role macro call sites.
- The public driver surface is now neutral. `src/thedriver.h` exposes
  `TheDriverAttr`, `TheDriverCell`, `TheDriverWideCell`, and opaque
  `TheDriverWindow` handles; `WINDOW`, `chtype`, and `cchar_t` stay out of
  that header and are guarded by `tests/check_curses_boundary.sh`.
- The renderer/window-state cleanup converted `SCREEN_DETAILS.win`, global
  windows, `SHOW_LINE` highlight/colour storage, reserved-line highlighting,
  parser/colour locals, line-buffer cells, and helper prototypes to neutral
  driver types where practical. `show.c` no longer uses
  `SCREEN_WINDOW_*`/`CURRENT_WINDOW*` macros, and the stale macro definitions
  were deleted.
- The final window-state cleanup is closed. The unsupported ExtCurses colour
  storage and old-curses/VMS `chtype` aliases in core headers/utilities were
  removed, stale raw window/cell macro residue was deleted, and the
  project-wide guardrail now catches raw `WINDOW`, `chtype`, `cchar_t`,
  `CURRENT_WINDOW`, `SCREEN_WINDOW`, or `PENDING_WINDOW` residue outside the
  driver/vendor areas.

## Active Slice

No active inventory cleanup slice remains after closing the inventory ratchet,
bulk physical wrapper pass, physical input/paint cleanup, raw mouse packet
driver-ownership cleanup, corrected suffixed-paint cleanup, the first
active-window/window-handle cleanup, the real driver-vtable migration, the
neutral public driver/window-state cleanup, and the final legacy compatibility
window-state closure.

Next step: driver shape review. Step back over `TheDriverOps`, opaque window
handles, remaining allowed driver-edge calls, and the supported build surfaces
before selecting any refactor.

## Direct Curses Inventory

`tests/inventory_direct_curses.sh` reports remaining direct curses dependencies
outside `src/cursesdriver.*`, `src/thedriver.*`, bundled PDCurses, and contrib
code. It now has
four useful modes:

- default full inventory: every classified finding.
- `--summary`: debt-oriented category totals, `window-state` sub-buckets, plus
  top category/file/function buckets.
- `--baseline`: aggregate category/file/function buckets for the checked-in
  ratchet baseline.
- `--fail-on-new`: CTest/build-target ratchet against
  `tests/inventory_direct_curses.baseline.tsv`.

Current ratcheted counts:

- actionable `physical-input`: 0
- actionable `physical-paint`: 0
- actionable `mouse-token`: 0
- actionable `window-state`: 0
- allowed/migrated `driver-wrapper`: 781

Current `window-state` summary:

- `window-handle`: 0
- `active-window-macro`: 0
- `cell-attr-type`: 0
- `renderer-cell-type`: 0
- `header-prototype`: 0

The final cleanup reduced total `window-state` from 249 to 0. It closed
`header-prototype`, `renderer-cell-type`, `active-window-macro`,
`window-handle`, and `cell-attr-type` completely. The last 13 findings were
legacy compatibility surfaces: `src/the.h` ExtCurses/old-curses `chtype` and
`stdscr` compatibility defines, `src/util.c` ExtCurses colour-pair storage and
`init_pair()` compatibility, the VMS-only `put_char()` attribute split, plus
stale raw window/cell comments and macros.

For the cleaned transient functions, the sweep finds no raw `physical-input` or
`physical-paint` calls in `readv_cmdline()`, `execute_dialog()`, or
`execute_popup()`. Remaining transient findings are explicitly classified:
`WINDOW` ownership in the curses path and opaque vtable physical edge calls.
Direct `KEY_MOUSE` branch tokens now use the driver abstraction and no
longer appear as mouse-token debt.

The raw mouse packet guardrail is now stricter: outside `src/cursesdriver.*`,
`src/thedriver.*`, bundled PDCurses, and contrib code, raw `MEVENT`, `getmouse`,
`request_mouse_pos`, `MOUSE_X_POS`, `MOUSE_Y_POS`, `BUTTON_CHANGED`,
`BUTTON_STATUS`, `BUTTON_ACTION_MASK`, `MOUSE_MOVED`, `BUTTON1_*`,
`BUTTON2_*`, `BUTTON3_*`, modifier button masks, wheel scroll tokens, and
direct `KEY_MOUSE` are classified as actionable `physical-input`.

The ratchet is a project-wide no-new-debt gate for actionable categories, not a
must-fix-all-existing-debt gate. Reductions are allowed without updating every
other bucket. `driver-wrapper` entries are counted for visibility but are
treated as migrated/allowed and do not fail the ratchet. New editor call sites
should use `the_driver->...`; new `curses_driver_*` call sites must stay inside
`src/cursesdriver.*`.

The older wrapper passes reduced the scanner's raw `physical-input` count from
16 to 12 to 0 and raw `physical-paint` count from 198 to 31 to 0, but the
paint regex still missed suffixed calls such as `wattrset`, `wclear`,
`mvaddstr`, `winch`, and `setcchar`. The corrected scanner exposed 52 active
raw `physical-paint` findings; this pass reduced them to 0 and moved the new
wrapper visibility into `driver-wrapper`. The mouse-boundary cleanup reduced
`mouse-token` from 24 to 0 and moved the old raw packet decoding source of
truth from `src/mouse.c` to `src/cursesdriver.c`. The scanner also stops
treating comments, prototypes/function names, `#if 0` bodies, inactive `#if 0`
arms before active `#else` branches, and raw-token `#undef` compatibility
guards as raw call sites. Windows PDCursesMod `wincon` was not build-verified
in this pass.

## Deferred Buckets

Boundary debt:

- Actionable direct curses inventory is closed: `physical-input`,
  `physical-paint`, `mouse-token`, and `window-state` are all zero outside the
  driver/vendor areas.
- `mouse-token` is currently zero. Future raw mouse packet symbols outside the
  driver should fail as actionable `physical-input`; editor-level mouse command
  encoding should continue to use driver-owned button/action/modifier constants
  or logical hit targets.
- Remaining `driver-wrapper` entries outside the driver are direct vtable
  calls, including temporary low-level physical edges for opaque window
  handles, pads, renderer cell storage, modal local windows, and compatibility
  helpers. `curses_driver_*` helpers are private to the curses implementation;
  classify the remaining physical edges slice by slice.
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
