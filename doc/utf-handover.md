# UTF-8 Cursor/Driver Handover

Last updated: 2026-05-28.

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
painting, UTF repair execution, physical cursor materialization, and hardware
cursor parking. Shared logical/display mapping lives in `src/driverlayout.c`;
terminal-specific mechanics stay in `src/cursesdriver.c` or in a temporary
physical edge explicitly being migrated there.

High-level editor code calls the current driver vtable through
`the_driver->...`. The real vtable lives in `src/thedriver.h`, and
`src/thedriver.c` provides explicit driver selection helpers. Builds that link
curses can select `the_curses_driver_ops`; builds that link the fake driver can
select `the_headless_driver_ops`. The normal `the` executable still defaults
to curses. `curses_driver_*` names are now curses implementation details;
editor code, including temporary physical edges that still need opaque
windows, calls the vtable directly.

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
  `TheDriverAttr`, `TheDriverCell`, portable `TheRenderCell` /
  `TheRenderCluster`, and opaque `TheDriverWindow` handles; `WINDOW`,
  `chtype`, and `cchar_t` stay out of that header and are guarded by
  `tests/check_curses_boundary.sh`.
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
- The driver-shape review is complete in `doc/driver-vtable-review.md`. It
  originally reviewed all 145 `TheDriverOps` function pointers; after the
  shared display/input, portable render-cell, and modal/standard-screen
  contraction slices, the live vtable has 130 entries, with curses and
  headless initializers covering the same 130
  entries. The remaining surface is classified into portable,
  physical-terminal, transitional, curses-private-candidate, and
  test-instrumentation work.
- The first headless/test driver slice is implemented. `src/headlessdriver.c`
  publishes a complete no-curses `the_headless_driver_ops` initializer with
  all 130 entries present. It supports fake opaque windows, screen-role
  and global-window slots, current/previous role state, cursor
  capture/move/restore, simple cell writes, queued normalized input events,
  legacy fake key/mouse hooks, and a deterministic operation log for
  touch/refresh/update-style presentation calls plus terminal-report and
  shell/repair transitions. `test_headlessdriver` and its no-curses guard
  prove the base links without `src/cursesdriver.c` or a curses library.
- The shared display/input semantics slice is implemented. `src/driverlayout.c`
  owns `clamp_display_col`, `display_col_from_logical`,
  `logical_col_from_display`, `viewport_col_for_logical`, and
  `filearea_target`; `cursor.c`, `execute.c`, `mouse.c`, `show.c`, and both
  curses/headless file-area cursor paths call that shared helper. Those five
  helper entries were removed from `TheDriverOps`, and the vtable gained
  `read_input_event`. Remaining raw input compatibility wrappers are
  `read_current_window_key`, `read_current_role_key`,
  `read_global_window_key`, `read_window_key`, `read_raw_window_key`,
  `read_standard_key`, `read_raw_standard_key`, `is_mouse_key`,
  `mouse_key_code`, `read_mouse_button`, `read_current_role_mouse_event`, and
  `read_mouse_event`.
- The portable UTF render-cell/render-cluster slice is implemented.
  `src/rendercell.c` owns neutral `TheRenderCell` and `TheRenderCluster`
  construction, codepoint and UTF-8 slice preservation, style, logical/display/
  cursor/paint width facts, fallback representation, and replacement repair
  strategy hints. `TheDriverOps` dropped `add_wide_cell`,
  `write_wide_cell_span`, `set_wide_cell_codepoint`, `recolour_wide_cell`, and
  `write_wide_string_at`, and added `write_render_cells` plus
  `write_render_cluster_at`. `show.c` now builds render cells/clusters first;
  `src/cursesdriver.c` privately lowers them to `cchar_t`, `wadd_wch`,
  `wadd_wchnstr`, or wide-string writes. `src/headlessdriver.c` preserves
  render metadata in its fake surface and exposes a focused inspection hook for
  tests.
- The modal/standard-screen contraction slice is closed. `TheDriverOps`
  dropped `create_pad`, `refresh_pad`,
  `replace_current_role_with_relative_window`,
  `prepare_standard_screen_for_shell`, `refresh_standard_screen`,
  `touch_current_screen_image`, the low-level standard-screen clear/erase/
  attr/string/cursor/cell helpers, and the broken-curses background refresh
  variants. Popup rendering now uses the existing transient popup snapshot and
  paints the visible viewport into the popup window without pads. Dialog
  editfields use the existing role save/restore surface with an absolute
  temporary command window. `EDITV LIST` and `QUERY STATUS` use the portable
  terminal-report surface; shell escape, startup/resize/redraw synchronization,
  and broken-curses background repair are represented by the higher-level
  `prepare_for_shell_escape`, `sync_terminal_screen`,
  `clear_terminal_screen`, and `repair_terminal_background` operations.

## Active Slice

No active implementation slice is selected. Inventory cleanup, the
driver-shape review, the first headless/test `TheDriverOps` base, and the
shared display/input semantics, portable UTF renderer-cell, and
modal/standard-screen contraction slices are closed.

Next implementation work should close down remaining driver surface area rather
than add more wrappers. The next large slice is raw input compatibility
wrapper retirement: migrate legacy key/mouse readers toward
`read_input_event` or logical transient hit events while keeping raw terminal
packet decoding private to `src/cursesdriver.c`.

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
- allowed/migrated `driver-wrapper`: 742

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
`execute_popup()`. Popup pad creation/prefresh and dialog relative-window
replacement are gone from the public vtable; remaining transient findings are
opaque vtable physical edge calls. Direct `KEY_MOUSE` branch tokens now use
the driver abstraction and no longer appear as mouse-token debt.

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

## Outstanding Close-Down Plan

Actionable direct curses inventory is closed: `physical-input`,
`physical-paint`, `mouse-token`, and `window-state` are all zero outside the
driver/vendor areas. The remaining `driver-wrapper` count is visibility for
vtable use, not raw curses debt. Close it by removing low-level operations from
the public driver surface in a few large slices.

Closed close-down slice:

1. Modal and standard-screen contraction. Closed with no target operation left
   in `TheDriverOps`: popup pads were replaced by transient-snapshot viewport
   painting, dialog relative windows by role save/restore plus a temporary
   command window, standard-screen list/status output by terminal-report
   operations, and shell/startup/resize terminal mechanics by
   shell/sync/repair operations. Vtable count fell from 138 to 130.

Close during the remaining driver rearchitecture:

1. Raw input compatibility wrapper retirement. Migrate legacy callers of
   `read_current_window_key`, `read_current_role_key`,
   `read_global_window_key`, `read_window_key`, `read_raw_window_key`,
   `read_standard_key`, `read_raw_standard_key`, `is_mouse_key`,
   `mouse_key_code`, `read_mouse_button`, `read_current_role_mouse_event`, and
   `read_mouse_event` to `read_input_event` or logical transient hit events.
   Close when raw packet/key wrappers are removed from `TheDriverOps` or made
   curses-private, with `test_inputevent`, `test_mousehit`,
   `test_headlessdriver`, and agent capability tests updated.
2. Role/window/cursor presentation contraction. Reduce current/screen/global
   role cursor, touch, refresh, redraw, and cell-scrape compatibility helpers
   after command/file/prefix paths set logical cursor/focus before rendering.
   Close when `cursor_focus_sync_current()` is gone and physical cursor
   save/restore, refresh/update, touch, software-cursor painting, cell writes,
   and cursor parking remain driver-owned.
4. Driver selection and no-curses THE targets. Keep `the` curses-first, but add
   an explicit startup/profile or build-target path for selecting headless/test
   drivers. Close when `the_agent`, `the_llm_headless`, and any test-specific
   target prove no curses dependency while sharing the same driver surface.

Explicitly defer until the driver surface stabilizes:

- Full `the_agent` integration with THE's complete command dispatcher.
- Full prefix command execution in the no-curses agent.
- Live agent protocol integration for modal transient snapshots.
- Retained-frame delta LLM views.
- The keycap blank-cell terminal/profile proof loop and broader Linux,
  Windows Terminal, iTerm2, and terminal baseline matrix.
- Windows/PDCurses strategy: keep current compatibility now; decide later
  whether to keep one curses driver or split a Windows/PDCurses driver.
- Legacy source-branch cleanup and build-warning noise, except for warnings
  that block or obscure the current slice's tests.

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
