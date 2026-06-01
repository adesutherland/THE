# UTF-8 Cursor/Driver Handover

Last updated: 2026-06-01.

This is the compact handover for the UTF, cursor, driver, and LLM
reorganization. Durable detail lives in:

- `doc/cursor-driver-architecture.md`: ownership contract and guardrails.
- `doc/driver-vtable-review.md`: operation-by-operation vtable review and
  removed-surface history.
- `doc/llm-headless-capabilities.md`: current no-curses agent/editor
  capability inventory.
- `doc/llm-driver-agent-guide.md`: no-curses agent protocol.
- `doc/llm-mode.md`: LLM-facing design and usage rules.
- `doc/utf-design.md`: historical UTF findings and terminal details.

## Architecture Rule

```text
editor command -> logical UI model -> physical driver
physical input -> normalized input event -> editor command
```

The editor owns logical focus, row roles, line numbers, `TextPos`, desired
horizontal cell, logical viewport start, semantic transient UI state, and text
mutation ranges.

The physical driver owns curses windows, physical cursor save/restore,
refresh/update/touch ordering, cell writes/fills, software cursor painting,
UTF repair execution, physical cursor materialization, hardware cursor parking,
raw terminal input packets, and terminal-specific policy. Shared display
mapping lives in `src/driverlayout.c`; terminal mechanics stay in
`src/cursesdriver.c` or in explicit physical vtable operations.

New logical behavior should be proved through a no-curses surface first:
`the_agent`, `the_llm_headless`, `llmdriver`, `llmruntime`,
virtual/fake-driver tests, focused unit tests, or CREXX/pty full-editor tests.

## Current Checkpoint

- Latest completed slice: first full-runtime LLM target proof,
  `the --driver llm`.
- Live public driver surface: 53 `TheDriverOps` entries.
- Curses and headless implementations both initialize all 53 entries.
- Public driver types are neutral: `TheDriverAttr`, `TheDriverCell`,
  `TheRenderCell`, `TheRenderCluster`, and opaque `TheDriverWindow`.
- `src/thedriver.h` is free of `WINDOW`, `chtype`, and `cchar_t`.
- `cursor_focus_sync_current()` is removed.
- Actionable direct-curses inventory is closed:
  `physical-input: 0`, `physical-paint: 0`, `mouse-token: 0`,
  `window-state: 0`.
- Allowed/migrated `driver-wrapper` visibility is 576. This is vtable usage,
  not raw curses debt.
- `the --driver llm` now selects the headless/LLM driver during real THE
  startup, skips curses initialization, opens files through the real file/view
  runtime, and runs `command ...` through THE's real command dispatcher. The
  protocol loop preserves the `the_agent` command shape: `look`, `delta`,
  `capabilities`, `focus`, `hit`, `key`, `text`, `type`, `command`, `debug`,
  `transient`, and `quit`.
- Full-runtime LLM snapshots are built from real `screenframe`/`SHOW_LINE`
  state and include row roles, file line numbers, text, prefixes, command and
  status fields, logical focus, current buffer metadata, dirty state, file ring
  metadata, block/selection state, and syntax/style spans where THE has real
  highlighting state available.
- CREXX remains a strategic full-runtime LLM capability. `capabilities` reports
  `crexx_macros` according to the build, and `command ...` uses the same
  profile/macro dispatcher path as the full editor when CREXX is enabled.
- SDSLH/parser-backed syntax remains a strategic full-runtime LLM capability.
  Syntax/style spans are surfaced in snapshots after the real runtime enables
  colouring. Parser diagnostics are available through real commands such as
  `SDSLHWAIT` and `EXTRACT /PMSGS/`; a first-class snapshot diagnostics array
  is still a concrete follow-up.
- The current `the` binary still links the curses driver for the default
  `--driver curses` path. The `--driver llm` path does not initialize curses or
  execute curses-driver behavior at runtime; a separate full-runtime no-curses
  link target remains a future build split if strict link isolation is needed.
- `the_agent` now covers the serious no-curses protocol harness subset:
  open/new/save/write, logical snapshots with buffer path/dirty/line metadata,
  search/find, replace/replace-all, line operations, logical hits, key/text
  input, file/prefix/command focus, semantic prefix commands, selection/range
  operations, bounded agent-side undo/redo, buffer open/switch/list/close,
  flat project listing, retained-frame deltas, live transient readv/dialog/popup
  demo protocol, and the existing SOS navigation/edit subset. It is retained
  as a protocol harness, no-curses contract test surface, and fallback oracle
  for LLM formatting/input behavior, not as the strategic final agent editor.
- `the_llm_headless --mini-session` performs a realistic no-curses edit/save
  run against a file and is covered by CTest.

The live vtable now contains:

- explicit physical window primitives such as create/delete, explicit cursor
  capture/move/restore, origin/size reads, drawing, writing, touch, refresh,
  redraw, update, and cursor presentation.
- high-level terminal lifecycle/report/repair/input operations:
  `read_input_event`, terminal report, shell preparation, terminal repair,
  terminal clear/sync, and update.
- portable renderer operations: cell spans, render cells, render clusters,
  fixed-width ASCII and fill operations.
- high-level cursor semantics that still bridge logical editor state to the
  physical driver: `move_prefix_cursor`, `move_filearea_cursor`, and
  `filearea_cursor_transition`.

## Closed Workstreams

1. Direct-curses inventory and neutral public types:
   Raw `physical-input`, `physical-paint`, `mouse-token`, and `window-state`
   findings are zero outside approved driver/vendor areas. `SCREEN_WINDOW_*`,
   `CURRENT_WINDOW*`, public `WINDOW`, `chtype`, and `cchar_t` residue are
   guarded.
2. Headless/test driver base:
   `src/headlessdriver.c` provides a complete no-curses fake/test
   implementation of the current `TheDriverOps` surface with deterministic
   state/logging and no curses link.
3. Shared display/input semantics:
   `src/driverlayout.c` owns display/logical mapping helpers, and
   `read_input_event` is the portable input API. Legacy integer-key callers use
   `the_driver_read_legacy_key()` where needed.
4. Portable UTF render cells/clusters:
   `src/rendercell.c` owns neutral UTF render metadata. Curses lowers it
   privately; headless preserves it for tests.
5. Modal/standard-screen contraction:
   Pad, `stdscr`/`curscr`, relative role-window, shell-preparation, standard
   clear/paint/cursor, and broken-curses background helper operations were
   removed from the public vtable. Popup rendering uses transient snapshots;
   query/list output uses terminal report operations.
6. Raw input wrapper retirement:
   Public raw key/mouse reader, mouse-token, saved-packet, and packet-decoder
   vtable operations were removed. Raw `KEY_MOUSE`, PDC/ncurses packet
   decoding, and saved packet coordinates stay in `src/cursesdriver.c`.
7. Role/window/cursor presentation contraction:
   Current/screen/global role cursor aliases, touch/refresh/redraw aliases,
   cell scrape/mutation helpers, topology/existence aliases, role-window
   lifecycle helpers, keypad/notimeout/leaveok entries, and zero-caller role
   clear helpers were removed from the public table. Callers now resolve
   existing logical windows through `src/driverwindow.h` before using explicit
   physical primitives.
8. LLM/headless agent editor capability fill:
   The no-curses agent subset is now a credible editor target for agents. It
   has stable semantic snapshots/deltas, buffer metadata, file
   open/save/write, search/find, replace, line operations, prefix commands,
   selection/range operations, undo/redo visibility, buffer switching, flat
   project listing, live transient modal demo flow, status/error reporting,
   logical hits, and a headless mini-session proof. Items that require the full
   editor runtime or host automation are classified in
   `doc/llm-headless-capabilities.md`.

Historical vtable counts:

- initial reviewed surface: 145 entries.
- after shared display/input: 141.
- after portable render cells/clusters: 138.
- after modal/standard-screen contraction: 130.
- after raw input wrapper retirement: 114.
- after role/window/cursor presentation contraction: 53.

## Roadmap

Recommended next slice:

1. Full-runtime LLM capability fill.
   Keep `the_agent` stable as the protocol harness while extending
   `the --driver llm` where real runtime gaps remain: first-class parser
   diagnostics in snapshots, modal/readv/dialog protocol adaptation, stricter
   no-curses link target if needed, and broader real command/profile/CREXX
   smoke coverage.

Then:

2. Remaining vtable stabilization.
   Review the 53 live operations after a selectable no-curses target exists.
   Do not reopen the removed current/screen/global role cursor, refresh, touch,
   redraw, raw input, or modal/stdscreen wrapper families. Focus only on
   operations that still prove awkward for the new target.
3. Full-runtime integration decisions.
   Keep full THE dispatcher, profile, parser/SDSLH, and CREXX macro behavior
   in the full editor runtime and expose them through the LLM driver. Keep
   build/test execution in host automation.
4. Terminal and platform decisions.
   Finish keycap/terminal materialization proof loops, add Linux/Windows
   Terminal/iTerm2 baselines, and decide whether Windows stays in the curses
   driver through PDCurses or gets a separate driver.
5. Housekeeping.
   Remove legacy source branches and reduce build-warning noise only after the
   architecture is stable, unless a warning blocks the active slice.

## Do Not Reopen

These families are closed unless a failing test proves a concrete regression:

- pad/stdscreen/curscr public vtable operations.
- public raw key/mouse wrappers and saved packet helpers.
- current/screen/global role cursor, refresh, touch, redraw, existence, and
  cell-scrape aliases.
- terminal-cell scraping as editor logic for `Text()`,
  `EXTRACT /SPACECHAR/`, or filetab navigation.
- `cursor_focus_sync_current()`.

## Closing Rules

A migration task is closed only when all applicable items are true:

- logical behavior is observable through a no-curses surface, virtual/fake
  driver, focused CTest, or CREXX/pty full-editor test.
- the real curses path uses the same logical input/frame/cursor data.
- unsupported behavior is explicit in `the_agent capabilities` or this file.
- physical mechanics remain inside `cursesdriver.c` or an explicit physical
  vtable operation.
- guardrails are tightened for the cleaned module or behavior class.
- `git diff --check` and focused tests pass.

## Useful Verification

```sh
perl -ne 'print "$1\n" if /\(\*([A-Za-z0-9_]+)\)/' src/thedriver.h | wc -l
perl -ne 'print "$1\n" if /^\s*\.([A-Za-z0-9_]+)\s*=/' src/cursesdriver.c | wc -l
perl -ne 'print "$1\n" if /^\s*\.([A-Za-z0-9_]+)\s*=/' src/headlessdriver.c | wc -l
bash tests/inventory_direct_curses.sh --summary /Users/adrian/CLionProjects/THE
git diff --check
```

Focused tests that commonly matter:

- `test_headlessdriver`, `test_inputevent`, `test_mousehit`
- `test_uidriver`, `test_screenframe`, `test_virtual_screen`
- `test_llmdriver`, `test_llmruntime`, `test_agentdriver`
- `test_transientui`, `test_the_agent_no_curses`,
  `test_the_llm_headless_no_curses`,
  `test_the_llm_headless_no_curses_mini_session`,
  `test_the_llm_full_runtime`
- `test_curses_boundary`, `test_curses_boundary_inventory`
- CREXX/pty tests such as `test_normal_area_queries`,
  `test_sos_navigation_queries`, `test_sos_logical_edit_queries`, and
  `test_selective_change_prompt` when CREXX is enabled.
