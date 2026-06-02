# UTF-8 Cursor/Driver Handover

Last updated: 2026-06-02.

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
`src/drivers/curses/cursesdriver.c` or in explicit physical vtable operations.

New logical behavior should be proved through a no-curses surface first:
`the_llm_harness`, `the_llm_headless`, `llmdriver`, `llmruntime`,
virtual/fake-driver tests, focused unit tests, or CREXX/pty full-editor tests.

## Current Checkpoint

- Latest completed slice: full-runtime LLM blocker closure: runtime-loaded
  driver modules, first-class parser diagnostics, full-runtime transient
  protocol, broader skip-safe smoke coverage, harness rename, and driver/LLM
  directory reorganization.
- Live public driver surface: 53 `TheDriverOps` entries.
- Curses and headless implementations both initialize all 53 entries.
- Public driver types are neutral: `TheDriverAttr`, `TheDriverCell`,
  `TheRenderCell`, `TheRenderCluster`, and opaque `TheDriverWindow`.
- `src/the.h` and `src/thedriver.h` are free of curses headers, `WINDOW`,
  `chtype`, `cchar_t`, curses color-pair encoding, and raw curses `A_*`
  attributes.
- Core colours/styles/keys are THE logical state. `src/thecolour.h` owns
  `THE_COLOR_*`, `THE_STYLE_*`, `TheRenderAttr`, and `TheDriverCell`
  encoding helpers; `src/thekeys.h` owns logical key codes including
  Back-Tab/Shift-Tab, shifted arrows, function-key modifier ranges, mouse,
  and parser-complete. Basic color values and many key values intentionally
  stay numerically curses-compatible where that preserves existing maps.
- RGB colours from `#RRGGBB` and SVG/X11 colour names are stored as logical
  THE colour IDs backed by a core RGB registry. They are not curses palette
  slots in editor state.
- Curses lowers logical attrs, cells, alternate cells, colors, and physical
  key packets privately in `src/drivers/curses/cursesdriver.c`. Its private
  lowering allocates terminal palette colours and pairs as needed. Core editor
  state no longer stores `COLOR_PAIR(pair) | A_BOLD` or depends on curses
  pair numbering.
- `cursor_focus_sync_current()` is removed.
- Actionable direct-curses inventory is closed:
  `physical-input: 0`, `physical-paint: 0`, `physical-attr: 0`,
  `curses-include: 0`, and `window-state: 0`.
- Allowed/migrated `driver-wrapper` visibility is 542. This is vtable usage,
  not raw curses debt.
- Drivers are runtime-loaded modules. `the` loads `the_driver_curses` by
  default or for `--driver curses`, loads `the_driver_llm` for `--driver llm`,
  and no longer links curses directly. The portable loader uses `dlopen` /
  `dlsym` on POSIX and `LoadLibrary` / `GetProcAddress` on Windows, with
  `THE_DRIVER_PATH`, executable/release directories, and the installed driver
  directory in the search path.
- The main executable is guarded for both link cleanliness and accidental raw
  curses API symbol exports. The curses-shaped compatibility names still
  allowed in `the` are `my_wmove`, `curses_started`,
  `ncurses_screen_resized`, `suspend_curses`, `resume_curses`,
  `the_driver_is_curses`, and `the_driver_use_curses`; these are legacy shared
  runtime-state names, not direct curses API links. The fallback core
  `doupdate()` shim was renamed to `the_driver_fallback_update()`, and curses
  module cursor callbacks are private `curses_driver_*` lifecycle callbacks.
- CMake no longer exposes `src/drivers/curses` as a global include directory,
  and the main binary does not link curses. One include-scope hardening task
  remains: `${CURSES_INCLUDE_DIRS}` is still present in a top-level
  `include_directories(...)` block and should be moved onto curses-only
  targets.
- `the --driver llm` now selects the headless/LLM driver during real THE
  startup, skips curses initialization, opens files through the real file/view
  runtime, and runs `command ...` through THE's real command dispatcher. The
  protocol loop preserves the `the_llm_harness` command shape: `look`, `delta`,
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
  colouring. Parser diagnostics now appear as a first-class `diagnostics`
  snapshot array when parser messages exist, and `EXTRACT /PMSGS/` preserves
  the existing PMSGS behavior through the shared collector.
- Full-runtime `transient readv`, `transient dialog`, and `transient popup`
  use the shared `transientui` model and support snapshot, key/text input,
  logical hit input, result reporting, close, and cancel through the LLM
  protocol.
- `the_llm_harness` now covers the serious no-curses protocol harness subset:
  open/new/save/write, logical snapshots with buffer path/dirty/line metadata,
  search/find, replace/replace-all, line operations, logical hits, key/text
  input, file/prefix/command focus, semantic prefix commands, selection/range
  operations, bounded harness-side undo/redo, buffer open/switch/list/close,
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
   Raw `physical-input`, `physical-paint`, `physical-attr`,
   `curses-include`, and `window-state` findings are zero outside approved
   driver/vendor areas. `SCREEN_WINDOW_*`, `CURRENT_WINDOW*`, public
   `WINDOW`, `chtype`, `cchar_t`, curses color-pair macros, and raw curses
   `A_*` attributes are guarded.
2. Headless/test driver base:
   `src/drivers/llm/headlessdriver.c` provides a complete no-curses fake/test
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
   decoding, and saved packet coordinates stay in `src/drivers/curses/cursesdriver.c`.
   The editor-visible mouse key is `THE_KEY_MOUSE`.
7. Role/window/cursor presentation contraction:
   Current/screen/global role cursor aliases, touch/refresh/redraw aliases,
   cell scrape/mutation helpers, topology/existence aliases, role-window
   lifecycle helpers, keypad/notimeout/leaveok entries, and zero-caller role
   clear helpers were removed from the public table. Callers now resolve
   existing logical windows through `src/driverwindow.h` before using explicit
   physical primitives.
8. LLM/headless agent editor capability fill:
   The no-curses harness subset is now a credible editor target for agents. It
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

1. Driver boundary and LLM runtime hardening.
   Close this as one aggressive implementation slice:
   target-scope CMake curses include directories; mechanically convert core
   editor use of legacy `KEY_*` names to `THE_KEY_*`; deepen full-runtime
   modal/readv/dialog/popup flows so command-triggered blocking interactions
   become resumable LLM protocol continuations; and expand `the --driver llm`
   fixtures for syntax/style spans, parser diagnostics, profiles, CREXX where
   available, prefix/block/file-ring state, and realistic modal workflows.
   Keep `the_llm_harness` stable as the protocol harness and formatting/input
   oracle while the real runtime work happens in `the --driver llm`.

Then:

2. Terminal and platform decisions.
   Verify the portable module loader on Windows, decide whether Windows stays
   in the curses driver through PDCurses or gets a separate driver, and finish
   Linux/Windows Terminal/iTerm2/keycap materialization baselines.
3. Logical key identity decision.
   After the mechanical `THE_KEY_*` rename, decide only if a real non-curses UI
   needs unique logical key identities beyond the preserved historical
   curses/PDCurses numeric values.
4. Remaining vtable stabilization.
   Review the 53 live operations after the full-runtime LLM target has driven
   real usage. Do not reopen the removed current/screen/global role cursor,
   refresh, touch, redraw, raw input, or modal/stdscreen wrapper families.
   Focus only on operations that still prove awkward for non-curses drivers.
5. Housekeeping.
   Rename misleading legacy shared-runtime symbols such as
   `curses_started`/`suspend_curses`, remove stale codemod residue, remove
   legacy source branches, and reduce build-warning noise after the boundary
   slice is stable unless a warning blocks that slice.

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
- unsupported behavior is explicit in `the_llm_harness capabilities` or this file.
- physical mechanics remain inside `cursesdriver.c` or an explicit physical
  vtable operation.
- no curses include path is globally visible except through curses-only
  targets when the task claims include-boundary closure.
- core/editor callers use `THE_KEY_*` names after the key-name cleanup, with
  raw curses/PDCurses key symbols limited to the curses driver, curses key-map
  tests, and compatibility definitions in `src/thekeys.h`.
- full-runtime LLM protocol behavior is proved through `the --driver llm`
  whenever the behavior depends on THE's real dispatcher, profiles, parser,
  syntax state, CREXX, file ring, block state, or modal command flow.
- guardrails are tightened for the cleaned module or behavior class.
- `git diff --check` and focused tests pass.

## Useful Verification

```sh
perl -ne 'print "$1\n" if /\(\*([A-Za-z0-9_]+)\)/' src/thedriver.h | wc -l
perl -ne 'print "$1\n" if /^\s*\.([A-Za-z0-9_]+)\s*=/' src/drivers/curses/cursesdriver.c | wc -l
perl -ne 'print "$1\n" if /^\s*\.([A-Za-z0-9_]+)\s*=/' src/drivers/llm/headlessdriver.c | wc -l
bash tests/inventory_direct_curses.sh --summary /Users/adrian/CLionProjects/THE
git diff --check
```

Focused tests that commonly matter:

- `test_headlessdriver`, `test_inputevent`, `test_mousehit`
- `test_uidriver`, `test_screenframe`, `test_virtual_screen`
- `test_llmdriver`, `test_llmruntime`, `test_agentdriver`
- `test_transientui`, `test_the_llm_harness_no_curses`,
  `test_the_llm_headless_no_curses`,
  `test_the_llm_headless_no_curses_mini_session`,
  `test_the_llm_full_runtime`, `test_the_llm_parser_diagnostics`,
  `test_the_llm_profile_crexx`, `test_driver_modules`
- `test_curses_boundary`, `test_curses_boundary_inventory`
- CREXX/pty tests such as `test_normal_area_queries`,
  `test_sos_navigation_queries`, `test_sos_logical_edit_queries`, and
  `test_selective_change_prompt` when CREXX is enabled.
