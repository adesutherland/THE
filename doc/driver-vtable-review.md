# Driver Vtable Review

Last updated: 2026-05-27.

This is the strategic review of `TheDriverOps` after the direct-curses
inventory cleanup closed. It is not a refactor plan for one mechanical sweep.
It is a map for choosing later slices with confidence.

## Architecture Shape

THE should keep one neutral driver surface that curses, LLM/headless, and
test/fake drivers can all expose. The surface can contain operations that are
NOP-capable or deterministic-log-only in a non-terminal driver, but the editor
should not need to know which backend is active for ordinary command flow.

The intended layers are:

- Core editor model: owns buffers, views, logical focus, row roles, logical
  cursor state, command state, normalized input, and semantic transient UI
  state.
- Driver vtable: the current compatibility boundary between legacy editor code
  and physical UI behavior. It should shrink toward semantic operations plus a
  small portable surface API.
- Curses driver: owns ncurses/PDCurses windows, pads, stdscr/curscr behavior,
  terminal key/mouse packets, physical cursor presentation, touch/refresh
  ordering, and terminal-specific UTF repair/materialization.
- LLM/headless driver: exposes semantic snapshots and consumes normalized
  input. Physical paint/cursor/window calls should update a deterministic
  fake surface or operation log only when needed for compatibility.
- Testing/fake driver: shares most headless behavior, records operations, and
  supports targeted integration tests without linking curses.
- Selectable startup/profile mechanism: the main `the` executable should
  eventually choose, load, or swap a driver through startup options and system
  profile configuration rather than hard-wiring curses in `src/thedriver.c`.
- Windows/PDCurses decision: keep the curses driver PDCurses-compatible for
  now. Later work can either preserve that compatibility in the same driver or
  split a Windows/PDCurses driver if terminal behavior diverges enough.

`the_agent` and `the_llm_headless` already prove useful no-curses link
boundaries. `src/headlessdriver.c` now adds the first complete no-curses
`TheDriverOps` implementation. It is a fake/test driver rather than the full
editor runtime: terminal-only operations may be NOPs or deterministic log
entries, while fake windows, role/global slots, cursors, simple input/mouse
state, and touch/refresh/update ordering are represented in memory. Later LLM
behavior can layer on this base or share its patterns, while focused CTest
drivers continue to prove specific integration points.

## Findings

- `TheDriverOps` currently has 138 function pointers.
- `src/cursesdriver.c` initializes all 138 entries in `the_curses_driver_ops`.
- `src/headlessdriver.c` initializes all 138 entries in
  `the_headless_driver_ops` without including curses headers or linking
  curses.
- The Step 2 display-layout extraction removed `clamp_display_col`,
  `display_col_from_logical`, `logical_col_from_display`,
  `viewport_col_for_logical`, and `filearea_target` from the vtable.
  `src/driverlayout.c` now owns those shared helpers, and both curses and
  headless cursor movement use the same helper.
- The surface still mixes portable logical-driver operations, legacy
  role/window compatibility edges, and very low-level curses mechanics.
- A normalized `read_input_event` operation now exists. The old raw key/mouse
  operations remain as compatibility wrappers for the legacy dispatcher,
  readv/dialog/popup paths, `getch.c`, and existing mouse-definition dispatch.
- The most urgent refactor candidates are raw key/mouse reads,
  stdscr/curscr operations, keypad/notimeout/leaveok, and role-specific
  touch/refresh/cursor helpers that reveal too much of the curses window
  topology.
- The safest next code slice is not broad signature churn. Work in a few large
  coherent slices: modal/standard-screen contraction and raw input migration
  should remain separate.
- The public "wide cell" surface is closed. `src/rendercell.c` now provides a
  portable render-cell/render-cluster model for grapheme clusters such as
  flags, keycaps, combining sequences, and ZWJ sequences where logical width,
  display width, cursor width, and paint/repair width may differ.

## Category Meanings

- `core-portable`: should remain available to all drivers as part of the
  portable UI surface.
- `physical-terminal`: real terminal/window behavior; headless/test drivers
  may NOP, keep in-memory state, or log deterministically.
- `transitional-edge`: needed while legacy editor code still talks in physical
  roles/windows; should be split or removed as semantic state takes over.
- `semantic/shared`: logic that probably belongs in common code rather than in
  each driver implementation.
- `curses-private-candidate`: likely too low-level for the long-term public
  driver contract.
- `test-instrumentation`: useful mainly for fake drivers, assertions, or
  operation logs.

## Review Table

Caller counts below are direct `the_driver->operation(...)` calls found in C
and header sources. They do not count internal calls inside
`src/cursesdriver.c`.

| Operation | Current callers/use | Current curses behavior | Category | Headless/test behavior | LLM behavior | Recommendation | Tests/guardrails needed |
|---|---|---|---|---|---|---|---|
| `software_cursor_attr` | 3 calls in `show.c`. | Computes terminal attributes for block/underline software cursor. | `physical-terminal` | Return symbolic/fake attr or log requested shape. | Describe cursor presentation semantically. | Split semantic cursor shape from physical attr. | Virtual cursor overlay tests. |
| `current_window_is_role` | 2 calls in `comm1.c`. | Checks `CURRENT_VIEW->current_window`. | `transitional-edge` | Track fake current role. | Use logical focus zone. | Needs further caller review. | Agent focus tests before removal. |
| `current_window_exists` | 3 calls in `error.c`. | Tests resolved active curses window. | `transitional-edge` | Return fake active-surface presence. | Usually true for semantic surface; log only. | Keep but optional/NOP-capable for now. | No-curses error/status smoke. |
| `screen_window_is_role` | 3 calls in `show.c`. | Checks a screen view's active role. | `transitional-edge` | Track fake screen role state. | Prefer row-role snapshot state. | Needs further caller review. | Renderer tests for active-role decisions. |
| `current_role_exists` | 34 calls across command, edit, cursor, execute, file, query, show. | Tests current screen role window slot. | `transitional-edge` | Track role presence in fake screen. | Map to semantic zones/rows. | Keep now; migrate callers to semantic row/focus state. | Guard role-free command paths. |
| `screen_role_exists` | 25 calls in cursor, execute, mouse, scroll, show. | Tests `screen[scrno].win[role]`. | `transitional-edge` | Track per-screen fake roles. | Use snapshot row-role availability. | Keep now; migrate to semantic screen model. | `test_virtual_screen`, mouse-hit tests. |
| `global_window_exists` | 15 calls in command/edit/the paths. | Tests status, error, divider, or filetabs globals. | `transitional-edge` | Track fake global surfaces. | Expose globals as semantic reserved rows. | Keep now; later replace with semantic chrome model. | Reserved-row snapshot tests. |
| `delete_global_window` | 8 calls in command, error, file, shutdown. | Deletes global window slot and clears it. | `physical-terminal` | Delete fake surface and log. | Remove semantic chrome row or log. | Keep but mark optional/NOP-capable. | Fake surface lifecycle tests. |
| `create_window` | 9 calls in `error.c`, `execute.c`, `util.c`. | Wraps `newwin` as opaque handle. | `physical-terminal` | Allocate fake window buffer. | Allocate logical/fake surface only when legacy path needs it. | Keep portable surface creation for now. | Headless fake-driver lifecycle tests. |
| `create_pad` | 1 call in `execute.c`. | Wraps `newpad` when available. | `curses-private-candidate` | Allocate fake scroll buffer or return null if unsupported. | Prefer semantic popup/list snapshot. | Keep optional; move modal pad use toward transient UI. | Popup/readv tests without curses. |
| `delete_window` | 14 calls in error, execute, util. | Calls `delwin` for non-null handles. | `physical-terminal` | Free fake surface and log. | Free compatibility surface. | Keep with fake implementation. | Fake lifecycle leak/assert tests. |
| `enable_keypad` | 9 calls in error, execute, util. | Calls `keypad(win, TRUE/FALSE)`. | `curses-private-candidate` | Log or ignore. | Not needed; input is normalized. | Move toward curses-private implementation detail. | No-curses link guard plus input tests. |
| `enable_standard_keypad` | 1 call in `the.c`. | Calls `keypad(stdscr, ...)`. | `curses-private-candidate` | NOP/log. | Not needed. | Move toward curses-private startup. | Startup boundary guard. |
| `set_standard_notimeout` | 1 call in `the.c`. | Calls `notimeout(stdscr, ...)`. | `curses-private-candidate` | NOP/log. | Not needed. | Move toward curses-private startup. | Startup boundary guard. |
| `set_window_leaveok` | 1 call in `error.c`. | Calls curses `leaveok`. | `curses-private-candidate` | NOP/log. | Not needed. | Move toward curses-private implementation detail. | No-curses error-window test. |
| `capture_window_cursor` | 5 calls in `show.c`, `util.c`. | Reads `getyx` from explicit window. | `test-instrumentation` | Return fake window cursor. | Use logical cursor; expose physical only in diagnostics. | Keep but optional; prefer logical cursor state. | Cursor fake-state tests. |
| `capture_current_window_cursor` | 50 calls across legacy command/edit/render paths. | Reads `getyx` from active window. | `transitional-edge` | Return fake active cursor. | Use logical focus/cursor; log compatibility calls. | Split logical cursor from physical cursor. | Track reductions with boundary checks. |
| `capture_current_previous_window_cursor` | 2 calls in `comm1.c`, `comm5.c`. | Reads previous-window cursor for current screen. | `transitional-edge` | Track fake previous role cursor. | Prefer logical prior focus state. | Needs further caller review. | Focus transition tests. |
| `capture_current_role_cursor` | 10 calls in command, cursor, edit, execute, print. | Reads cursor from current screen role. | `transitional-edge` | Return fake role cursor. | Use semantic zone cursor. | Split semantic and physical pieces. | Agent focus and print-path smoke. |
| `capture_screen_window_cursor` | 8 calls in cursor, prefix, scroll. | Reads cursor from a screen's active window. | `transitional-edge` | Return fake screen active cursor. | Use logical screen focus. | Needs further caller review. | Multi-screen cursor tests. |
| `capture_screen_role_cursor` | 10 calls in cursor, show, util. | Reads cursor from a screen role. | `transitional-edge` | Return fake screen-role cursor. | Use row/zone cursor in snapshot. | Keep now; migrate to semantic screen model. | Renderer cursor guardrails. |
| `capture_global_window_cursor` | 0 direct calls. | Reads cursor from a global window. | `test-instrumentation` | Return fake global cursor. | Log only. | Keep optional or remove after caller audit. | Vtable completeness check. |
| `window_origin` | 1 call in `util.c`. | Reads `getbegyx` for explicit window. | `physical-terminal` | Return fake origin. | Diagnostic only. | Keep but optional/NOP-capable. | Fake geometry tests. |
| `window_size` | 4 calls in `util.c`. | Reads `getmaxy/getmaxx`. | `core-portable` | Return fake dimensions. | Return configured snapshot dimensions. | Keep in portable surface. | Geometry tests across drivers. |
| `current_window_origin` | 1 call in `cursor.c`. | Reads active-window screen origin. | `transitional-edge` | Return fake active origin. | Prefer logical hit geometry. | Needs further caller review. | Cursor-to-hit mapping tests. |
| `current_window_size` | 5 calls in `commsos.c`, `cursor.c`. | Reads active-window dimensions. | `transitional-edge` | Return fake active dimensions. | Prefer zone dimensions from snapshot. | Split semantic and physical pieces. | SOS edge tests in agent/fake driver. |
| `current_role_size` | 1 call in `cursor.c`. | Reads current screen role dimensions. | `core-portable` | Return fake role dimensions. | Return semantic zone dimensions. | Keep as portable geometry until frame model replaces it. | Cursor bounds tests. |
| `screen_role_size` | 2 calls in `cursor.c`, `execute.c`. | Reads dimensions for screen role. | `core-portable` | Return fake role dimensions. | Return semantic zone dimensions. | Keep as portable geometry. | Dialog/popup geometry tests. |
| `current_window_cursor_screen_point` | 3 calls in `execute.c`. | Adds window origin to active cursor. | `test-instrumentation` | Compute from fake origin/cursor. | Diagnostic only; prefer logical hit. | Move toward shared geometry helper. | Modal hit tests. |
| `save_current_role_window` | 0 direct calls; helper paired with replace. | Saves current role slot. | `transitional-edge` | Save fake slot. | Avoid; transient UI should own modal state. | Keep only while replace API exists. | Modal role lifecycle tests. |
| `replace_current_role_with_relative_window` | 1 call in `execute.c`. | Creates `derwin` or `subwin` and swaps role slot. | `curses-private-candidate` | Swap fake child surface. | Use transient semantic snapshot instead. | Move modal relative-window detail into curses driver. | Readv/dialog no-curses tests. |
| `restore_current_role_window` | 2 calls in `execute.c`. | Restores saved role slot. | `transitional-edge` | Restore fake slot. | Avoid with semantic modal lifecycle. | Keep temporarily; retire with modal migration. | Modal lifecycle guard. |
| `delete_current_role_window` | 2 calls in `execute.c`. | Deletes current role window slot. | `physical-terminal` | Delete fake role surface. | End compatibility modal surface. | Keep optional; migrate modal callers. | Transient UI integration tests. |
| `clear_current_screen_roles` | 1 call in `the.c`. | Nulls all role slots for current screen. | `transitional-edge` | Clear fake role map. | Reset semantic screen model. | Keep for screen lifecycle; later move to driver init/reset. | Startup/shutdown fake-driver tests. |
| `move_window_cursor` | 11 calls in execute, show, util. | Calls `wmove` on explicit window. | `physical-terminal` | Update fake window cursor and log. | Log only unless compatibility fake surface is active. | Keep but mark optional/NOP-capable. | Fake operation log tests. |
| `move_current_window_cursor` | 43 calls across legacy command/edit/render paths. | Moves active curses window cursor. | `transitional-edge` | Update fake active cursor and log. | Prefer logical cursor update; log compatibility calls. | Split logical movement from physical cursor move. | Ratchet direct physical movement by module. |
| `move_current_previous_window_cursor` | 2 calls in `comm1.c`, `comm5.c`. | Moves previous active window. | `transitional-edge` | Update fake previous cursor. | Prefer logical prior focus handling. | Needs further caller review. | Focus transition tests. |
| `move_current_role_cursor` | 22 calls in command, cursor, edit, file, print, show. | Moves current screen role window. | `transitional-edge` | Update fake role cursor. | Use zone cursor where semantic. | Split semantic and physical pieces. | Agent command/prefix/file focus tests. |
| `move_screen_window_cursor` | 9 calls in cursor, execute, prefix, scroll. | Moves active window for a screen. | `transitional-edge` | Update fake screen active cursor. | Use screen snapshot cursor. | Needs further caller review. | Multi-screen cursor tests. |
| `move_screen_role_cursor` | 12 calls in commandset, cursor, execute, show, util. | Moves a specific screen role cursor. | `transitional-edge` | Update fake screen-role cursor. | Use zone/row cursor where possible. | Keep now; migrate to semantic frame commands. | Renderer and setup tests. |
| `move_global_window_cursor` | 9 calls in error, rexx, util. | Moves status/error/divider/filetabs cursor. | `physical-terminal` | Update fake global cursor. | Semantic reserved rows should not need cursor movement. | Keep optional; move chrome rendering higher. | Reserved-row tests. |
| `restore_window_cursor` | 1 call in `show.c`. | Moves explicit window to saved cursor. | `physical-terminal` | Restore fake window cursor. | Log only. | Keep but optional/NOP-capable. | Fake cursor save/restore tests. |
| `restore_current_window_cursor` | 11 calls in `edit.c`, `file.c`. | Restores active window cursor. | `transitional-edge` | Restore fake active cursor. | Prefer logical cursor restoration. | Needs further caller review. | Edit/file cursor regression tests. |
| `restore_current_role_cursor` | 0 direct calls. | Restores current role cursor. | `test-instrumentation` | Restore fake role cursor. | Log only. | Keep optional or remove after audit. | Vtable completeness check. |
| `restore_screen_window_cursor` | 0 direct calls. | Restores screen active-window cursor. | `test-instrumentation` | Restore fake screen cursor. | Log only. | Keep optional or remove after audit. | Vtable completeness check. |
| `restore_screen_role_cursor` | 2 calls in `show.c`. | Restores screen role cursor. | `transitional-edge` | Restore fake role cursor. | Prefer semantic cursor preservation. | Keep now; migrate renderer preservation. | Renderer cursor tests. |
| `restore_global_window_cursor` | 0 direct calls. | Restores global window cursor. | `test-instrumentation` | Restore fake global cursor. | Log only. | Keep optional or remove after audit. | Vtable completeness check. |
| `read_window_cell` | 2 calls in `util.c`. | Reads `winch` from explicit window. | `curses-private-candidate` | Read fake cell buffer. | Avoid; semantic state should be source of truth. | Move toward curses-private or fake-test helper. | Virtual surface cell tests. |
| `read_current_window_cell` | 3 calls in `comm5.c`, `query2.c`. | Reads `winch` from active window. | `transitional-edge` | Read fake active cell. | Avoid screen scraping. | Needs further caller review; replace with model state. | Query/command tests. |
| `read_current_window_cell_attr_at` | 2 calls in `comm5.c`. | Reads active cell attributes. | `curses-private-candidate` | Read fake attr buffer. | Avoid terminal attrs. | Move toward model-owned style state. | Style/span tests. |
| `put_char_current_window` | 3 calls in `comm5.c`. | Calls legacy `put_char` on active window. | `curses-private-candidate` | Write fake cell and log. | Avoid direct cell mutation. | Move toward renderer operation or curses-private helper. | Virtual renderer tests. |
| `set_window_attr` | 18 calls in execute, show. | Calls `wattrset` on explicit window. | `physical-terminal` | Set fake current attr. | Treat as style span/log only. | Keep optional until renderer owns style spans. | Renderer style tests. |
| `set_current_window_attr` | 1 call in `comm5.c`. | Calls `wattrset` on active window. | `transitional-edge` | Set fake active attr. | Avoid terminal attr dependency. | Needs further caller review. | Command rendering tests. |
| `set_current_role_attr` | 12 calls in command and execute paths. | Sets attr on current screen role. | `transitional-edge` | Set fake role attr. | Map to semantic row style. | Split semantic style from physical attr. | Reserved/command style tests. |
| `set_screen_role_attr` | 13 calls in show, util. | Sets attr on a screen role. | `transitional-edge` | Set fake role attr. | Use semantic row style. | Split semantic style from physical attr. | Virtual screen style tests. |
| `set_global_window_attr` | 18 calls in command, error, show, the, util. | Sets attr on global window. | `transitional-edge` | Set fake global attr. | Use semantic chrome style. | Split semantic style from physical attr. | Status/error style tests. |
| `set_window_background` | 2 calls in `execute.c`. | Calls `wbkgd` or attr/clear fallback. | `curses-private-candidate` | Set fake background attr. | Prefer semantic modal background. | Move toward curses-private modal paint detail. | Dialog/readv tests. |
| `clear_line_at` | 1 call in `show.c`. | Moves, sets attr, clears to EOL. | `physical-terminal` | Clear fake row cells and log. | Render semantic blank row. | Keep as optional physical primitive. | Virtual renderer row-clear tests. |
| `clear_current_role` | 0 direct calls. | Calls `wclear` on current role. | `test-instrumentation` | Clear fake role. | Log only. | Keep optional or remove after audit. | Vtable completeness check. |
| `clear_current_role_to_eol` | 7 calls in command, cursor, edit, execute, show. | Clears to EOL on current role. | `transitional-edge` | Clear fake role row to EOL. | Render semantic row update. | Keep now; migrate callers to semantic renderer. | Command-line clear tests. |
| `clear_screen_role_to_eol` | 0 direct calls. | Clears to EOL on screen role. | `test-instrumentation` | Clear fake role row to EOL. | Log only. | Keep optional or remove after audit. | Vtable completeness check. |
| `touch_window` | 4 calls in execute, show. | Calls `touchwin`. | `physical-terminal` | Mark fake surface dirty/log. | No-op except operation log. | Keep but mark optional/NOP-capable. | Fake dirty/refresh log tests. |
| `touch_line` | 4 calls in `show.c`. | Calls `touchline`. | `physical-terminal` | Mark fake row dirty/log. | No-op except render diagnostic. | Keep optional. | Targeted redraw tests. |
| `touch_current_window` | 2 calls in `comm1.c`, `comm4.c`. | Touches active window. | `transitional-edge` | Mark fake active dirty. | Log only. | Needs caller review. | Command refresh tests. |
| `touch_current_role` | 8 calls in `comm1.c`, `edit.c`. | Touches current screen role. | `transitional-edge` | Mark fake role dirty. | Log only. | Keep optional; migrate refresh policy. | Fake refresh ordering tests. |
| `touch_screen_role` | 1 call in `util.c`. | Touches screen role. | `transitional-edge` | Mark fake role dirty. | Log only. | Keep optional. | Window setup refresh tests. |
| `touch_global_window` | 4 calls in commutil, show. | Touches global window. | `physical-terminal` | Mark fake global dirty. | Log semantic chrome update. | Keep optional. | Status/filetabs refresh tests. |
| `touch_and_refresh_current_role` | 6 calls in command paths. | Touches role then `wnoutrefresh`. | `transitional-edge` | Log combined dirty/refresh. | Log only. | Split dirty marking from presentation policy. | Refresh-order fake log tests. |
| `touch_and_refresh_screen_role` | 9 calls in `show.c`. | Touches screen role then queues refresh. | `transitional-edge` | Log combined dirty/refresh. | Log only. | Split or keep optional until renderer cleanup. | Renderer refresh-order tests. |
| `touch_and_refresh_global_window` | 11 calls in command/edit paths. | Touches global window then queues refresh. | `physical-terminal` | Log combined dirty/refresh. | Log chrome update. | Keep optional; later semantic chrome renderer. | Status/error refresh tests. |
| `refresh_window` | 5 calls in commutil, execute, show. | Calls `wnoutrefresh`. | `physical-terminal` | Mark fake presented/log. | No-op/log. | Keep optional. | Fake refresh log tests. |
| `refresh_window_now` | 2 calls in `execute.c`. | Calls `wrefresh`. | `physical-terminal` | Immediate-present log. | No-op/log. | Keep optional; prefer batched update. | Modal refresh tests. |
| `refresh_current_window` | 12 calls in command, edit, error, scroll. | Queues active window refresh. | `transitional-edge` | Log active refresh. | No-op/log. | Needs caller review. | Legacy command refresh tests. |
| `refresh_current_window_now` | 4 calls in command/util. | Calls `wrefresh` on active window. | `transitional-edge` | Immediate active refresh log. | No-op/log. | Prefer batched presentation; caller review. | Interactive key-loop tests. |
| `refresh_current_role` | 5 calls in command and execute paths. | Queues current role refresh. | `transitional-edge` | Log role refresh. | No-op/log. | Keep optional. | Command/readv refresh tests. |
| `refresh_current_role_now` | 3 calls in commutil, execute. | Calls `wrefresh` on current role. | `transitional-edge` | Immediate role refresh log. | No-op/log. | Prefer batched update. | Readv command-line tests. |
| `refresh_screen_window` | 2 calls in scroll, show. | Queues active screen window refresh. | `transitional-edge` | Log screen active refresh. | No-op/log. | Needs caller review. | Scroll/render tests. |
| `refresh_screen_role` | 19 calls in scroll, show, util. | Queues screen role refresh. | `transitional-edge` | Log role refresh. | No-op/log. | Keep optional until renderer owns presentation. | Virtual screen refresh tests. |
| `refresh_global_window` | 6 calls in command, error, show. | Queues global window refresh. | `physical-terminal` | Log global refresh. | Log chrome update. | Keep optional. | Status/error refresh tests. |
| `refresh_global_window_now` | 6 calls in error, rexx, the. | Calls `wrefresh` on global window. | `physical-terminal` | Immediate global refresh log. | Log only. | Keep optional; prefer batched updates. | Error-window prompt tests. |
| `refresh_standard_screen` | 8 calls in command, execute, query, show, the, util. | Calls curses `refresh`. | `curses-private-candidate` | NOP/log. | Not needed. | Move toward curses-private startup/shell detail. | No-curses link guard. |
| `refresh_pad` | 1 call in `execute.c`. | Calls `prefresh` when available. | `curses-private-candidate` | Present fake pad viewport/log. | Prefer semantic popup viewport. | Keep optional; migrate popup pad path. | Popup viewport tests. |
| `update` | 16 calls in command, edit, error, scroll, show. | Calls `doupdate`. | `physical-terminal` | Presentation barrier log. | No-op/log. | Keep optional/NOP-capable. | Refresh-order operation log tests. |
| `present_cursor` | 17 calls in command, edit, execute, show. | Applies cursor visibility, shape escape, and `curs_set`. | `physical-terminal` | Store visible flag/log. | Expose cursor visibility semantically. | Keep but split hardware presentation from logical cursor. | Cursor presentation tests. |
| `set_current_window_timeout` | 2 calls in `edit.c`. | Calls `wtimeout` on active window. | `curses-private-candidate` | Store fake timeout/log. | Input loop should be event-driven. | Move toward curses-private input policy. | Input timeout behavior tests. |
| `draw_box` | 2 calls in `execute.c`. | Calls curses `box`. | `physical-terminal` | Draw fake border/log. | Semantic dialog border optional. | Keep optional until transient UI owns dialogs. | Dialog snapshot/render tests. |
| `draw_vertical_line` | 2 calls in `util.c`. | Calls `wvline` or manual add loop. | `physical-terminal` | Draw fake cells/log. | Semantic divider row/column. | Keep optional; chrome renderer later. | Divider rendering tests. |
| `add_string_at` | 7 calls in execute, util. | Moves then `waddstr`. | `physical-terminal` | Write fake string/log. | Prefer semantic text rows. | Keep optional for legacy paint. | Dialog/setup renderer tests. |
| `add_global_string_at` | 4 calls in error, the. | Writes string into global window. | `physical-terminal` | Write fake global text/log. | Expose status/error text semantically. | Keep optional; migrate chrome text. | Status/error snapshot tests. |
| `add_cell_at` | 13 calls in execute, util. | Moves then `waddch`. | `physical-terminal` | Write fake cell/log. | Prefer semantic renderer cells. | Keep optional for legacy setup/dialogs. | Virtual cell tests. |
| `draw_horizontal_line` | 1 call in `execute.c`. | Calls `whline` or manual add loop. | `physical-terminal` | Draw fake line/log. | Semantic dialog separator optional. | Keep optional. | Dialog render tests. |
| `add_cell` | 10 calls in execute, show, util. | Calls `waddch`. | `physical-terminal` | Append fake cell/log. | Prefer semantic renderer. | Keep optional during renderer migration. | Virtual renderer tests. |
| `insert_cell` | 1 call in `util.c`. | Calls `winsch`. | `curses-private-candidate` | Insert in fake row/log. | Avoid direct terminal insert. | Move toward shared text/render helper. | ETMODE/path tests using fake screen. |
| `delete_cell` | 1 call in `util.c`. | Calls `wdelch`. | `curses-private-candidate` | Delete fake cell/log. | Avoid direct terminal delete. | Move toward shared text/render helper. | ETMODE/path tests using fake screen. |
| `write_cell_span` | 1 call in `show.c`. | Uses `waddchnstr` or per-cell fallback. | `physical-terminal` | Write fake cell span/log. | Prefer semantic row text/style span. | Keep as renderer backend primitive for now. | Renderer span tests. |
| `write_render_cells` | 1 buffered line path and fallback single-cell UTF path in `show.c`. | Lowers portable render cells to `cchar_t` and uses `wadd_wchnstr` or per-cell `wadd_wch`. | `physical-terminal` | Writes fake cells while preserving render metadata for inspection. | Prefer semantic row text/style spans. | Keep as the portable UTF cell backend while `show.c` still uses line buffers. | ASCII/non-ASCII render-cell tests and no-curses guard. |
| `write_render_cluster_at` | Targeted UTF repair/status cluster writes in `show.c`. | Lowers a render cluster to wide output and preserves expected display width for cursor advancement. | `physical-terminal` | Preserves codepoint sequence, UTF slice, widths, repair hint, and style in the fake surface/log. | Expose logical cluster text and width facts. | Keep as the portable cluster backend for keycaps, flags, combining sequences, and ZWJ sequences. | `test_headlessdriver`, `test_virtual_screen`, `test_utfrepair`. |
| `fill_cells_at` | 1 call in `show.c`. | Fills terminal cells with spaces/attr. | `physical-terminal` | Fill fake cells/log. | Render blank semantic span. | Keep backend primitive; later shared renderer. | Blank-cell repair tests. |
| `write_ascii_cells_at` | 1 call in `show.c`. | Writes fixed-width ASCII cells. | `physical-terminal` | Write fake ASCII cells/log. | Expose logical text. | Keep backend primitive for now. | ASCII renderer tests. |
| `read_input_event` | Focused tests and future semantic dispatch. | Reads through the current curses input edge and returns `TheInputEvent` text/key events; raw mouse packets remain private to curses legacy wrappers. | `core-portable` | Pops queued `TheInputEvent` values; fake key hooks queue normalized events and legacy key readers adapt from that queue. | Primary input surface for agent/headless clients. | Keep and migrate callers from raw key/mouse wrappers incrementally. | `test_headlessdriver`, `test_inputevent`, mouse-hit and no-curses guards. |
| `read_current_window_key` | 7 calls in command/edit/query paths. | Calls `my_getch` on active window and maps `KEY_MOUSE`. | `transitional-edge` | Pop fake input queue. | Consume normalized input protocol. | Split to normalized input event API. | `test_inputevent`, agent key tests. |
| `read_current_role_key` | 3 calls in commutil, execute. | Reads key from current role window. | `transitional-edge` | Pop role-scoped fake input. | Consume normalized input protocol. | Split to normalized input event API. | Readv/dialog input tests. |
| `read_global_window_key` | 1 call in `error.c`. | Reads key from global window. | `transitional-edge` | Pop global fake input. | Use normalized prompt response. | Split to transient/normalized input. | Error prompt tests. |
| `read_window_key` | 0 direct calls. | Reads translated key from explicit window. | `physical-terminal` | Pop fake window input. | Not needed directly. | Keep optional or remove after audit. | Vtable completeness check. |
| `read_raw_window_key` | 3 calls in `getch.c`. | Calls raw `wgetch` and maps mouse key. | `curses-private-candidate` | Pop raw fake key for legacy adapter. | Not part of LLM surface. | Move toward curses-private raw input. | `getch` adapter tests. |
| `read_standard_key` | 5 calls in command, execute, rexx. | Calls `my_getch(stdscr)` and maps mouse key. | `transitional-edge` | Pop standard fake input. | Use normalized input. | Split to normalized input event API. | Query/shell input tests. |
| `read_raw_standard_key` | 3 calls in execute, the. | Calls raw `wgetch(stdscr)`. | `curses-private-candidate` | Pop raw standard fake input. | Not needed. | Move toward curses-private resize/startup input. | Resize/startup guard tests. |
| `is_mouse_key` | 11 calls in command, edit, execute, getch, query. | Checks `THE_KEY_MOUSE` and curses `KEY_MOUSE`. | `transitional-edge` | Check fake translated token. | Use input event kind `logical-hit`. | Retire after normalized input migration. | Mouse-token inventory guard. |
| `mouse_key_code` | 1 call in `execute.c`. | Returns `THE_KEY_MOUSE`. | `transitional-edge` | Return compatibility token. | Not needed. | Retire with mouse-token compatibility paths. | Mouse-token guard. |
| `mouse_position_for_screen_role` | 1 call in `mouse.c`. | Projects saved terminal mouse point into role-local coords. | `physical-terminal` | Project queued fake physical point. | Prefer supplied logical hit target. | Split physical packet from logical hit mapping. | `test_mousehit`. |
| `mouse_position_for_global` | 3 calls in `mouse.c`. | Projects saved terminal mouse point into global-window coords. | `physical-terminal` | Project fake point. | Prefer logical chrome hit. | Split physical packet from logical hit mapping. | Mouse chrome hit tests. |
| `saved_mouse_position` | 2 calls in `mouse.c`. | Returns last PDC/ncurses packet screen point. | `physical-terminal` | Return queued fake point. | Not needed for logical hits. | Move toward curses-private packet storage. | Mouse packet guard. |
| `reset_mouse_position` | 1 call in `mouse.c`. | Clears saved packet point. | `physical-terminal` | Clear fake point. | Not needed. | Move toward curses-private packet storage. | Mouse packet guard. |
| `read_mouse_button` | 3 calls in command, mouse, query. | Decodes PDC/ncurses button/action/modifier. | `curses-private-candidate` | Return queued fake physical button or false. | Use normalized logical-hit events. | Move raw packet decode behind curses-private input driver. | PDC/ncurses mouse tests plus `test_mousehit`. |
| `read_current_role_mouse_event` | 1 call in `commutil.c`. | Reads button and role-local coords. | `transitional-edge` | Return queued fake event. | Use logical-hit target. | Split to transient logical-hit input. | Readv mouse tests. |
| `read_mouse_event` | 2 calls in `execute.c`. | Reads button and explicit-window coords. | `transitional-edge` | Return queued fake event. | Use logical transient hit. | Split to transient logical-hit input. | Dialog/popup hit tests. |
| `prepare_standard_screen_for_shell` | 1 call in `execute.c`. | Resets attrs, clears stdscr, moves, refreshes. | `curses-private-candidate` | NOP/log shell transition. | Not needed. | Move toward curses-private shell escape path. | Shell-command smoke tests. |
| `force_background_and_refresh_window` | 0 direct calls. | Broken SysV curses background workaround. | `curses-private-candidate` | NOP/log. | Not needed. | Keep only if platform support requires it; otherwise audit removal. | Platform-specific build guard. |
| `force_background_and_refresh_current_window` | 3 calls in execute, rexx. | Applies broken-curses workaround to active window. | `curses-private-candidate` | NOP/log. | Not needed. | Move toward curses-private compatibility shim. | Shell/REXX smoke tests. |
| `force_background_and_refresh_standard_screen` | 1 call in `the.c`. | Applies broken-curses workaround to stdscr. | `curses-private-candidate` | NOP/log. | Not needed. | Move toward curses-private startup shim. | Startup smoke tests. |
| `touch_current_screen_image` | 1 call in `comm4.c`. | Touches curses `curscr`. | `curses-private-candidate` | NOP/log. | Not needed. | Move toward curses-private implementation detail. | Inventory guard for `curscr`. |
| `clear_standard_window` | 3 calls in execute, query, the. | Calls `wclear(stdscr)`. | `curses-private-candidate` | Clear fake standard surface/log. | Not needed except compatibility log. | Move toward curses-private standard-screen handling. | Query/exit smoke tests. |
| `erase_standard_window` | 1 call in `comm4.c`. | Calls curses `erase`. | `curses-private-candidate` | Clear fake standard surface/log. | Not needed. | Move toward curses-private implementation detail. | Standard-screen guard. |
| `set_standard_attr` | 8 calls in execute, query, show, the. | Calls `attrset`. | `curses-private-candidate` | Set fake standard attr/log. | Not needed. | Move toward curses-private standard-screen handling. | Standard-screen guard. |
| `add_standard_string_at` | 5 calls in execute, query. | Calls `mvaddstr`. | `curses-private-candidate` | Write fake stdscr text/log. | Prefer semantic prompt/status. | Move toward transient/query semantic rendering. | Query/readv tests. |
| `move_standard_cursor` | 6 calls in execute, show, the. | Calls curses `move`. | `curses-private-candidate` | Move fake standard cursor/log. | Not needed. | Move toward curses-private standard-screen handling. | Startup/query tests. |
| `add_standard_ch` | 3 calls in execute, show. | Calls curses `addch`. | `curses-private-candidate` | Write fake stdscr cell/log. | Not needed. | Move toward curses-private standard-screen handling. | Standard-screen guard. |
| `redraw_window` | 1 call in `show.c`. | Iterates cells and rewrites via `put_char`. | `curses-private-candidate` | Replay fake buffer/log. | Avoid physical redraw. | Move toward renderer-owned invalidation. | Renderer invalidation tests. |
| `redraw_current_role` | 8 calls in command paths. | Redraws current role cell-by-cell. | `transitional-edge` | Replay fake role/log. | Prefer semantic row render. | Needs caller review; reduce with renderer model. | Command redraw tests. |
| `redraw_screen_role` | 2 calls in `show.c`. | Redraws screen role cell-by-cell. | `transitional-edge` | Replay fake screen role/log. | Prefer semantic row render. | Move toward renderer-owned invalidation. | Virtual renderer invalidation tests. |
| `redraw_global_window` | 4 calls in command paths. | Redraws global window cell-by-cell. | `physical-terminal` | Replay fake global/log. | Prefer semantic chrome render. | Move toward semantic chrome renderer. | Status/error redraw tests. |
| `draw_software_cell` | 4 calls in `show.c`. | Reads cell and paints software cursor with attrs. | `physical-terminal` | Overlay fake cursor cell/log. | Expose cursor marker in snapshot. | Keep terminal-specific; split from logical cursor. | Software cursor UTF tests. |
| `draw_software_blank_cell` | 4 calls in `show.c`. | Paints blank software cursor cell. | `physical-terminal` | Overlay fake blank cursor/log. | Expose cursor marker in snapshot. | Keep terminal-specific; split from logical cursor. | Blank cursor tests. |
| `refresh_cursor` | 2 calls in `cursor.c`. | Shows status area, updates screen, presents cursor. | `transitional-edge` | Log status/update/cursor presentation. | Emit refreshed snapshot. | Split status update from physical presentation. | Cursor/status integration tests. |
| `redraw_screen_cursor` | 1 call in `cursor.c`. | Rebuilds and displays screen, then refreshes cursor. | `transitional-edge` | Rebuild fake frame/log. | Emit updated semantic snapshot. | Split semantic rebuild from physical repaint. | Screen rebuild/cursor tests. |
| `move_prefix_cursor` | 7 calls in commsos, cursor, show. | Moves prefix role cursor. | `core-portable` | Update fake prefix cursor and logical focus. | Update prefix focus/cursor in snapshot. | Keep high-level op; ensure logical state is primary. | Prefix focus tests. |
| `move_filearea_cursor` | 13 calls in commsos, cursor, execute, show. | Builds logical cursor, stores focus, then `wmove`s display column. | `core-portable` | Update logical cursor and fake cursor/log. | Update semantic file-area focus/cursor. | Keep high-level op; split physical move from logical update. | Agent SOS/file cursor tests. |
| `filearea_cursor_transition` | 1 call in `cursor.c`. | Handles software cursor transition or full redraw. | `transitional-edge` | Log transition or fake overlay update. | Emit logical cursor update and optional diagnostic. | Split semantic movement from curses software-cursor repaint. | Software cursor transition tests. |

## Recommended Implementation Slices

1. Done in this working tree: build a shared headless/test base
   `TheDriverOps` implementation with fake windows, role/global geometry,
   cursors, simple input and mouse fakes, dirty/refresh state, and operation
   logs. This makes every NOP-capable recommendation testable without curses.
2. Done in this working tree: move shared display/input semantics in one
   slice. `src/driverlayout.c` owns `clamp_display_col`,
   `display_col_from_logical`, `logical_col_from_display`,
   `viewport_col_for_logical`, and `filearea_target`; curses and headless
   file-area cursor movement call the same helper. `TheDriverOps` dropped
   those five helper entries and gained `read_input_event`, leaving 141
   entries. Raw PDC/ncurses packet decoding stays private to the curses
   driver. Remaining compatibility wrappers are `read_current_window_key`,
   `read_current_role_key`, `read_global_window_key`, `read_window_key`,
   `read_raw_window_key`, `read_standard_key`, `read_raw_standard_key`,
   `is_mouse_key`, `mouse_key_code`, `read_mouse_button`,
   `read_current_role_mouse_event`, and `read_mouse_event`.
3. Done in this working tree: introduce a portable UTF renderer-cell/render-
   cluster model. `TheDriverOps` dropped `add_wide_cell`,
   `write_wide_cell_span`, `set_wide_cell_codepoint`, `recolour_wide_cell`,
   and `write_wide_string_at`; it added `write_render_cells` and
   `write_render_cluster_at`, leaving 138 entries. `src/rendercell.c`
   represents codepoint sequences, UTF-8 slices, style, width facts, flags,
   fallback output, and terminal repair policy. Curses lowers the model
   privately; headless preserves it for tests.
4. Move modal and standard-screen paths (`create_pad`, stdscr operations,
   relative role windows, shell preparation) behind transient UI snapshots or
   curses-private compatibility helpers.

## Verification Notes

The complete-operation check for this document is:

```sh
perl -ne 'print "$1\n" if /\(\*([A-Za-z0-9_]+)\)/' src/thedriver.h | wc -l
perl -ne 'print "$1\n" if /^\s*\.([A-Za-z0-9_]+)\s*=/' src/cursesdriver.c | wc -l
perl -ne 'print "$1\n" if /^\s*\.([A-Za-z0-9_]+)\s*=/' src/headlessdriver.c | wc -l
```

All three counts are 138 after the portable render-cell/render-cluster slice.
