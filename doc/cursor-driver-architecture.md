# Cursor Driver Architecture

Last updated: 2026-05-27.

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
  `wnoutrefresh`, `doupdate`, `wgetch`, input timeouts, and mouse packet
  position decoding.
- physical cursor save/restore and hardware cursor parking.
- logical-to-physical display column mapping for the curses path.
- software cursor painting.
- UTF repair strategy execution.
- cell writes/fills and physical refresh ordering.

For the current curses UI, these mechanics belong in `src/cursesdriver.c` or a
clearly physical edge that is being migrated to that driver. Do not move
physical cursor save/restore, refresh, touch/update, cell writes, software
cursor painting, or cursor parking back into logical command code.

### Input Drivers

Input drivers own device-specific input collection and return normalized
events: text input, named keys, mouse-like logical hits, command submission, and
debug requests. Command dispatch should consume normalized input and logical
cursor state rather than raw curses coordinates.

The current curses key loop has a compatibility adapter: it reads through
`cursesdriver.c`, normalizes the key through `TheInputEvent`, then hands the
equivalent legacy key to existing dispatch. That is a transition step, not the
final architecture. Live curses mouse packets now follow the same transition
pattern: `mouse.c` decodes terminal mouse packets and window-local physical
coordinates at the driver edge, maps them to `TheInputEvent` logical-hit
targets, and then routes legacy mouse-definition dispatch through the saved
target window id. Migrated consumers such as `CURSOR MOUSE` and `TABFILE`
consume logical target kind, line number, row, cell, screen, and window id
instead of raw terminal coordinates.

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
- `src/cursesdriver.c` owns the migrated physical curses mechanics and
  file-area logical-to-physical cursor materialization.
- `src/inputevent.c` defines shared normalized input events.
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
  handling consumes logical hit targets where practical.
- `the_llm_headless` is the current no-curses executable skeleton for the
  broader LLM/headless editor direction. It links the transient model and is
  checked by `test_the_llm_headless_no_curses`.
- `tests/inventory_direct_curses.sh` is the repeatable debt sweep and ratchet.
  Current counts are actionable `physical-input: 0`, `physical-paint: 0`,
  `mouse-token: 24`, and `window-state: 397`; `driver-wrapper: 629` is
  counted as migrated/allowed. The ratchet is available as both CTest
  `test_curses_boundary_inventory` and build target
  `curses_boundary_inventory`. The cleaned transient functions and current
  project-wide inventory have no raw `physical-input` or `physical-paint`
  findings outside `src/cursesdriver.*`.

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
  outside the driver.
- Active slice: none selected after the inventory ratchet, bulk wrapper pass,
  and physical input/paint cleanup. Choose the next slice from the
  inventory-backed boundary debt in `doc/utf-handover.md`.
- Deferred: full agent dispatcher integration, full prefix command machinery in
  the agent, agent protocol integration for transient snapshots, full live
  frames for command/prompt/status/window rows, removal of the transitional
  cursor-focus bridge, retained-frame delta views, remaining direct curses
  window-state/type and mouse-token debt in legacy command/render/setup
  modules, the isolated keycap blank-cell physical materialization/profile
  follow-up, and additional terminal baselines.

## Guardrails

Guardrails are only useful when they close with behavior coverage. Tighten them
module by module, not by aspiration.

- Logical foundation modules must stay curses-free.
- `execute.c` direct curses calls are already guarded and should continue using
  `cursesdriver.c` wrappers for physical behavior.
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
