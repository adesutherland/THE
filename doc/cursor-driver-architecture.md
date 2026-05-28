# Cursor Driver Architecture

Last updated: 2026-05-28.

This document is the ownership contract for the cursor/driver split. The active
status ledger lives in `doc/utf-handover.md`.

## Goal

THE must separate editor intent from terminal mechanics. Editor commands should
work in logical terms: file lines, screen rows, logical text cells, grapheme
clusters, command-line text, prefix text, and focus zones. Physical drivers then
materialize that logical state for curses, an LLM client, or a future UI.

```text
editor command -> logical UI model -> physical driver
physical input -> normalized input event -> editor command
```

Editor command code must not infer logical state from curses cursor state. UTF
repair strategies, cursor widths, replacement widths, blank-cell quirks, and
terminal refresh ordering are physical concerns and belong behind the physical
driver boundary.

## Ownership

### Logical Layer

The logical layer owns:

- focused zone: file area, prefix, command line, prompt/dialog, status.
- logical file line number.
- logical screen row and row role.
- logical text position (`TextPos`) and desired horizontal cell.
- logical viewport start (`verify_col` as a logical column).
- editable/non-editable row decisions.
- text mutation byte ranges derived from logical `TextPos`.

The logical layer may use `textpos`, `logcursor`, `utflayout`, `uidriver`,
`screenframe`, and terminal profile metadata when it needs to ask whether a
logical cursor is physically visible. It must not call curses.

### Physical Driver Layer

The physical driver owns:

- curses `WINDOW *` access.
- `getyx`, `wmove`, `wadd*`, `mvwadd*`, `touchline`, `touchwin`,
  `wnoutrefresh`, `doupdate`, `wgetch`, input timeouts, raw mouse key
  translation, mouse packet storage, button/action/modifier decoding, and
  mouse packet position decoding.
- physical cursor save/restore and hardware cursor parking.
- physical cursor materialization from shared logical/display targets.
- software cursor painting.
- UTF repair strategy execution.
- cell writes/fills and physical refresh ordering.

Shared logical/display mapping lives in `src/driverlayout.c`. It uses
`utflayout.c` when UTF is enabled and feeds the portable
renderer-cell/render-cluster model where rendering needs explicit width facts.

For the current curses UI, these mechanics belong in `src/cursesdriver.c` or a
clearly physical edge that is being migrated to that driver. Do not move
physical cursor save/restore, refresh, touch/update, cell writes, software
cursor painting, or cursor parking back into logical command code.

Editor code reaches migrated high-level driver behavior through the current
driver vtable, `the_driver->...`, defined by `TheDriverOps` in
`src/thedriver.h`. `src/thedriver.c` owns the current-driver pointer and the
explicit selection helpers. The normal curses build selects
`the_curses_driver_ops` by default; no-curses tests can select
`the_headless_driver_ops` without linking `src/cursesdriver.c`. During this
migration, all drivers are expected to expose the same `TheDriverOps` surface.
Terminal-only operations may be NOPs, in-memory fake-surface updates, or
deterministic log entries in non-terminal drivers.

`curses_driver_*` function names are implementation-private there. The public
driver types are neutral: `TheDriverAttr`, `TheDriverCell`, `TheRenderCell`,
`TheRenderCluster`, and opaque `TheDriverWindow` handles. `WINDOW`, `chtype`,
and `cchar_t` are implementation-private curses types and must not reappear in
`src/thedriver.h`. Temporary physical edges that still need windows call
opaque vtable operations directly; pad, `stdscr`/`curscr`, modal relative
window, and broken-curses background mechanics have been contracted behind
transient snapshots or higher-level terminal lifecycle operations. Do not add
a neutral wrapper API parallel to the vtable.

`src/rendercell.c` owns the portable UTF renderer model. Render clusters carry
codepoint sequences, source UTF-8 slices, style, logical width, display width,
cursor width, paint width, repair strategy hints, flags for substituted or
expanded output, and fallback representation for non-UTF surfaces. Curses
lowers this model to `cchar_t`, `wadd_wch`, `wadd_wchnstr`, or wide-string
writes inside `src/cursesdriver.c`; headless/LLM drivers keep the semantic
cluster.

### Input Drivers

Input drivers own device-specific input collection and return normalized
events: text input, named keys, mouse-like logical hits, command submission, and
debug requests. Command dispatch should consume normalized input and logical
cursor state rather than raw curses coordinates.

The current curses key loop has a compatibility adapter: it reads through
`the_driver->read_input_event()` where focused paths have moved, normalizes the
key through `TheInputEvent`, then hands the equivalent legacy key to existing
dispatch where necessary. Raw curses `KEY_MOUSE` is translated in
`cursesdriver.c` to the editor-owned `THE_KEY_MOUSE`. That is a transition
step, not the final architecture. Live curses mouse packets now follow the same
transition pattern: `cursesdriver.c` decodes terminal mouse packets and
window-local physical coordinates, while `mouse.c` maps the driver-owned saved
packet to `TheInputEvent` logical-hit targets and routes legacy
mouse-definition dispatch through the saved target window id. Migrated
consumers such as `CURSOR MOUSE` and `TABFILE` consume logical target kind,
line number, row, cell, screen, and window id instead of raw terminal
coordinates. The legacy raw key/mouse wrappers remain until the dispatcher,
readv/dialog/popup, and `getch.c` paths are converted.

### LLM Driver

The LLM driver is a UI driver, not screen scraping. It must expose deterministic
semantic snapshots with row roles, line numbers, prefix text, file text,
command/status text, marks, current focus, and logical cursor position. It
should accept the same normalized text/key/command/logical-hit/debug events as
other drivers.

Physical metadata such as display column, terminal class, repair strategy, and
driver operation logs is useful for debugging, but agents must reason from
logical coordinates: `zone`, `line_number`, row, and logical cell.

## Current Checkpoint

Closed checkpoints are summarized here; details and next tasks are in
`doc/utf-handover.md`.

- `src/textpos.c`, `src/logcursor.c`, `src/utflayout.c`, `src/utfrepair.c`,
  and `src/utfterm.c` provide the logical UTF and physical profile foundation.
- `src/uidriver.c` defines logical row roles, frames, cursor overlays, and
  fake-driver operation logs.
- `src/screenframe.c` builds live file-area `UiFrame` snapshots and rebases
  saved logical cursors onto rebuilt rows.
- `src/thedriver.h` and `src/thedriver.c` define the real driver vtable,
  current-driver pointer, and explicit selection helpers. The public header is
  free of curses public types and exposes only neutral driver
  attrs, cells, render cells, render clusters, and opaque window handles.
  Editor code calls `the_driver->...` for migrated high-level operations and
  for temporary opaque physical edges. The live vtable now has 114 entries
  after shared display helpers moved out, the wide-cell surface collapsed to
  render cells/clusters, modal/standard-screen mechanics were contracted, and
  raw input compatibility wrappers were retired.
- `src/cursesdriver.c` owns the migrated physical curses mechanics, raw mouse
  packet decoding, file-area physical cursor materialization from shared
  layout targets, and the `the_curses_driver_ops` vtable.
- `src/headlessdriver.c` owns the first complete no-curses `TheDriverOps`
  implementation. It provides fake opaque windows, screen-role and global
  slots, cursor state, queued normalized input events plus the shared
  legacy-key adapter, cell storage, render-cell/cluster preservation, and deterministic
  touch/refresh/update plus terminal-report/shell/repair logs. It is a
  compatibility base for tests and future headless work, not a full editor
  runtime switch.
- `doc/driver-vtable-review.md` is the detailed map of the current vtable. It
  now tracks the 114-entry `TheDriverOps` surface and records which operations
  should remain portable, which are NOP/log-capable physical terminal
  operations, and which should move toward curses-private details. Future
  curses, headless/LLM, and fake/test drivers should expose the same surface
  while this migration is in progress.
- `src/inputevent.c` defines shared normalized input events.
- `src/driverlayout.c` defines shared display/cursor mapping helpers:
  `clamp_display_col`, `display_col_from_logical`,
  `logical_col_from_display`, `viewport_col_for_logical`, and
  `filearea_target`. Curses and headless drivers use this same helper rather
  than duplicating layout behavior.
- `src/mousehit.c` maps driver-edge mouse packets to shared logical-hit
  targets for normal live curses mouse dispatch.
- `src/transientui.c` defines the curses-free logical transient UI model for
  readv, dialog, and popup snapshots: geometry, row roles, prompt/title/edit
  text, buttons/items, selected/active state, viewport offsets, focus, and hit
  targets.
- `src/llmdriver.c` formats semantic snapshots, compact view modes, and debug
  diagnostics.
- `src/agentdriver.c` plus `tools/the_agent.c` provide the no-curses proof
  target with capability reporting, explicit unsupported-command diagnostics,
  logical hits, command/file/prefix focus, and the closed Step 2 SOS
  navigation/edit subset.
- `show.c`, `execute.c`, `query1.c`, `query2.c`, and `commsos.c` have removed
  several active-window cursor snapshot fallbacks from the focused cursor,
  query, SOS, render-exit, status, prefix, and view-switch paths. The closed
  Step 3 renderer path now uses live `UiFrame` row/cell/text targets for UTF
  file-area and prefix renderer decisions; command cursor placement requires
  editor-owned logical command state.
- `cursor_focus_sync_current()` remains only as a documented transition bridge:
  it seeds logical cursor state from the physical window cursor when older
  entry paths reach render without logical state. Remove it after all
  file-area, prefix, and command entry points set `VIEW_DETAILS.logical_cursor`
  before render.
- The transient UI headless boundary is closed for readv, dialog, and popup.
  `test_transientui` proves the model without curses, the curses paths
  materialize snapshots before painting/handling modal input, and modal mouse
  handling consumes logical hit targets where practical. Popup rendering no
  longer exposes pad allocation or pad refresh through `TheDriverOps`; the
  visible viewport is painted from the transient popup snapshot.
- `the_llm_headless` is the current no-curses executable skeleton for the
  broader LLM/headless editor direction. It links the transient model and is
  checked by `test_the_llm_headless_no_curses`.
- `the_agent`, `the_llm_headless`, and future proof targets such as
  `agentthe` or `testingthe` should continue proving that selected non-curses
  drivers can link without `src/cursesdriver.c`.
- The main `the` executable still initializes the current driver to the curses
  implementation. A future startup/system-profile mechanism should select,
  load, or swap drivers explicitly. The Windows strategy remains open: keep
  the curses driver PDCurses-compatible or split a Windows/PDCurses driver if
  the backend-specific behavior becomes too different.
- `tests/inventory_direct_curses.sh` is the repeatable debt sweep and ratchet.
  Current counts are actionable `physical-input: 0`, `physical-paint: 0`,
  `mouse-token: 0`, and `window-state: 0`; `driver-wrapper: 716` is counted
  as migrated/allowed. The summary now splits `window-state` into
  `window-handle: 0`, `active-window-macro: 0`, `cell-attr-type: 0`,
  `renderer-cell-type: 0`, and `header-prototype: 0`. The ratchet is
  available as both CTest `test_curses_boundary_inventory` and build target
  `curses_boundary_inventory`. The cleaned transient functions and current
  project-wide inventory have no raw `physical-input`, `physical-paint`,
  `mouse-token`, or `window-state` findings outside `src/cursesdriver.*` or
  `src/thedriver.*`.

## Status Model

Use this status model for every future slice:

- `Done`: behavior is covered by a no-curses agent/LLM surface, virtual or
  fake-driver CTest, CREXX/pty full-editor test, or focused unit CTest; the
  live curses path uses the same logical data; unsupported behavior is declared;
  and guardrails are tightened for any fully cleaned module or behavior class.
- `In progress`: behavior has a logical foundation but still has legacy
  dispatcher, renderer fallback, mouse, command, or physical-local mechanics.
- `Active slice`: the boundary task currently selected for closure in
  `doc/utf-handover.md`; it may be `none` immediately after a slice closes.
- `Deferred`: a larger model or capability gap that should wait until the
  active boundary slice can expose it cleanly.

The current active categories are:

- Done: logical UTF primitives, terminal profile/repair foundation, UI frame
  and fake-driver foundation, LLM snapshot formatting, no-curses agent proof,
  file-area logical cursor/editing foundation, command-dispatch Step 2
  coverage, major render cursor fallback removals, execute wrapper migration,
  focused query/SOS active-driver fallback removals, normalized live mouse
  input for normal `THEMouse` dispatch, transient readv/dialog/popup snapshot
  model and curses-path materialization, `the_llm_headless`, focused
  guardrails, the no-new-debt direct-curses inventory ratchet, and
  project-wide removal of raw `physical-input`/`physical-paint` findings
  outside the driver, corrected suffixed-paint inventory coverage, and driver
  ownership of raw mouse packet decoding, the first active-window/window-handle
  role-helper cleanup, the real `TheDriverOps` vtable with high-level editor
  call sites migrated to `the_driver->...`, and neutral public driver types
  with the `show.c` renderer/window-state macro surface closed, final closure
  of the legacy ExtCurses/old-curses/VMS window-state compatibility residue,
  the driver vtable review in `doc/driver-vtable-review.md`, the first
  complete no-curses headless/test `TheDriverOps` base, the shared
  display/input semantics slice, the portable render-cell/render-cluster
  slice that reduced the vtable to 138 entries and added
  `write_render_cells` / `write_render_cluster_at`, and the
  modal/standard-screen contraction that reduced the vtable to 130 entries,
  and raw input compatibility wrapper retirement that reduced it to 114.
- Active slice: none selected after the inventory ratchet, bulk wrapper pass,
  physical input/paint cleanup, raw mouse packet driver-ownership cleanup,
  corrected suffixed-paint cleanup, the first active-window/window-handle
  role-helper cleanup, the real driver-vtable migration, the neutral public
  driver/window-state cleanup, the driver-shape review, the headless/test
  driver base, shared display/input semantics, portable render-cell/render-
  cluster semantics, modal/standard-screen contraction, and raw input
  compatibility wrapper retirement. The next close-down slice is
  role/window/cursor presentation contraction; use `doc/utf-handover.md` as
  the source of truth.
- Deferred: full agent dispatcher integration, full prefix command execution,
  live agent protocol integration for transient snapshots, retained-frame
  delta views, the isolated keycap blank-cell physical materialization/profile
  follow-up, Windows/PDCurses strategy, additional terminal baselines, and
  legacy source-branch/build-warning cleanup that is not blocking the active
  slice.

## Guardrails

Guardrails are only useful when they close with behavior coverage. Tighten them
module by module, not by aspiration.

- Logical foundation modules must stay curses-free.
- `src/thedriver.h` must stay free of `WINDOW`, `chtype`, and `cchar_t`; the
  curses implementation performs all casts between neutral driver types and
  curses types.
- Core source outside `src/cursesdriver.*`, `src/thedriver.*`, bundled
  PDCurses, and contrib must stay free of raw `WINDOW`, `chtype`, `cchar_t`,
  `CURRENT_WINDOW`, `SCREEN_WINDOW`, and `PENDING_WINDOW` residue.
- Editor code should call `the_driver->...` for operations present in
  `TheDriverOps`. Direct `curses_driver_*` calls must stay inside
  `src/cursesdriver.*`; temporary low-level physical edges outside the driver
  should use opaque vtable operations while the broader model is still being
  split.
- `execute.c` direct curses calls are already guarded and should continue using
  the driver boundary for physical behavior.
- `readv_cmdline()`, `execute_dialog()`, and `execute_popup()` are guarded as a
  cleaned transient UI surface. Do not reintroduce raw curses input, refresh,
  paint, or mouse-position calls there; add logical snapshot state first and
  physical mechanics through `src/cursesdriver.c`.
- When a command, query, SOS, mouse, or renderer group is fully migrated, extend
  `tests/check_curses_boundary.sh` for that newly closed surface.
- `the_agent` must not link curses, `show.c`, or `cursesdriver.c`.
- `the_llm_headless` must not link curses or `cursesdriver.c`.
- Capability output must remain exact when the no-curses agent supports only a
  subset of full editor behavior.

## Test Surface Limits

- `the_agent` proves no-curses driver behavior and logical snapshots. It does
  not yet execute arbitrary THE/SOS commands through the real dispatcher.
- `the_llm_headless` proves the headless link boundary and can emit a transient
  snapshot demo, but it is not yet the full editor runtime.
- CREXX/pty tests exercise the full editor command processor. They require
  CREXX support, a working CREXX compiler/import runtime, and a pty-capable
  host.
- Virtual screen and fake-driver tests are the preferred proof for renderer
  migration.
- Manual terminal smoke tests are still needed for paint regressions, but any
  generally useful finding should turn into a CTest, agent capability
  disclosure, or focused driver diagnostic.

## Keycap Rule

Do not special-case keycaps in editor logic. Keycaps are one terminal profile
class that can select conservative physical repair strategies. If a keycap line
fails but ZWJ works, compare logical frame output and driver operation logs.
Fix the shared physical driver path, strategy planner, or terminal profile, not
logical cluster boundaries.
