# Cursor Driver Architecture

Last updated: 2026-05-21.

## Goal

THE must separate editor intent from terminal mechanics. Editor commands should
work in logical terms: file lines, screen rows, logical text cells, grapheme
clusters, command-line text, prefix text, and focus zones. Physical drivers then
materialize that logical state for curses, an LLM client, or another future UI.

The rule is:

```text
editor command -> logical UI model -> physical driver
physical input -> normalized input event -> editor command
```

Editor command code must not infer logical state from curses cursor state. UTF
repair strategies, cursor widths, replacement widths, and terminal quirks are
physical concerns and belong behind the physical driver boundary.

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

The logical layer may use `textpos`, `logcursor`, `utflayout`, and terminal
profile metadata where it needs to ask whether a logical cursor is physically
visible. It must not call curses.

### Physical Driver Layer

The physical driver owns:

- curses `WINDOW *` access.
- `getyx`, `wmove`, `wadd*`, `mvwadd*`, `touchline`, `wnoutrefresh`,
  `doupdate`, and hardware cursor parking.
- logical-to-physical display column mapping.
- software cursor painting.
- UTF repair strategy execution.
- physical refresh ordering.

For the current curses UI, this boundary is implemented by `cursesdriver.c`
plus the rendering code that is being moved behind it from `show.c`.

### Input Drivers

Input drivers own device-specific input collection. They return normalized
events: text input, named keys, mouse hit requests, and command submission.
Command dispatch should consume normalized input and logical cursor state rather
than raw curses coordinates.

The LLM driver must use the same normalized input and logical screen model, so
an LLM client exercises the same editor behavior as a curses terminal.

### LLM Driver Requirements

The LLM driver is a real UI driver, not a convenience dump of terminal text. It
should make THE comfortable and reliable for an LLM to operate:

- expose a stable semantic screen snapshot with row roles, line numbers, prefix
  text, file text, command text, status text, marks, current focus, and logical
  cursor position.
- use logical coordinates (`zone`, `line_number`, `row`, `cell`) rather than
  terminal escape positions.
- preserve enough physical metadata for debugging (`display_col`, terminal
  class, repair strategy, and driver operation log) without making the LLM
  depend on curses behavior.
- accept normalized input events: text insertion, named key actions, mouse-like
  logical hits, command submission, and higher-level editor intents.
- provide safe introspection commands such as "describe focus", "describe row",
  "list visible rows", "dump cursor mapping", "dump pending driver ops", and
  "explain last render decision".
- avoid ambiguous screen scraping. Repeated calls should return deterministic
  JSON-like structures that can be compared in tests and summarized in logs.
- minimize tokens by exposing view modes: file-area only, reserved/status rows
  only, prefix commands only, focus row only, and full screen when explicitly
  requested.
- support row ranges, text truncation, optional prefix/command/status/cursor
  metadata, and compact field names so scrolling through a file does not
  repeatedly resend stable screen chrome.
- support debug workflows for THE itself: capture a reproducible scenario,
  replay normalized input, compare logical frame output, and compare physical
  driver operation logs.

The LLM driver should not bypass editor commands or mutate editor buffers
directly. It should feed the same command/input layer used by curses after input
normalization, so LLM automation and manual terminal use exercise one code path.

## Current Problem

The implementation is being migrated away from multiple physical cursor
authorities. Some old paths remain, but the first stable checkpoints are now in
place:

- `src/uidriver.c` defines a logical frame, row roles, cursor overlays, and a
  fake driver operation log.
- `src/cursesdriver.c` materializes UTF file-area logical cursor requests and
  owns logical-to-physical display column mapping for the curses path.
- UTF file-area left/right movement, text insertion, `SOS DELBACK`, and
  `SOS DELCHAR` now prefer `VIEW_DETAILS.logical_cursor` and derive edit byte
  ranges from logical `TextPos`.
- software cursor overlay capture for the file area is logical-first and rejects
  EOF/TOF/out-of-bounds rows by row role and line number.
- command-line and prefix focus now record logical cursor zones, so the renderer
  no longer needs to treat those areas only as curses positions.
- `src/llmdriver.c` can build role-aware semantic snapshots from `UiFrame`,
  accept logical-hit and debug input events, and format cursor mapping plus
  driver operation logs for deterministic diagnostics.

The remaining implementation still has several physical cursor authorities:

- `cursor.c` directly reads and writes curses cursor positions.
- `execute.c`, `comm5.c`, and `commsos.c` contain direct `getyx`/`wmove`
  cursor paths.
- `show.c` captures a physical cursor position, paints software cursor overlays
  in several branches, and then restores curses cursor state.
- `cursesdriver.c` owns only part of the file-area UTF cursor path.

The visible symptoms follow from that split authority: EOF/prefix underline,
stale software cursor after scroll, incorrect after-EOL edits, and keycap
cursor jumps can all be caused by stale or inconsistent physical cursor state
being treated as logical editor state.

## Refactor Sequence

Each step is intended to be buildable, testable, and committable. Steps 1-7
have an initial implementation checkpoint. Step 8 remains deliberately
incomplete until the live renderer no longer needs direct `show.c` access to
curses.

1. Record architecture and add guardrails.
   Add this document, update `doc/utf-handover.md`, and add a script/CTest that
   reports direct curses calls outside the approved driver/rendering boundary.

2. Add logical UI frame and fake driver types.
   Introduce small driver-neutral structures for row roles, logical cells,
   cursor overlays, and physical operation recording. Add unit coverage before
   changing live behavior.

3. Route file-area cursor movement through logical requests.
   `THEcursor_left/right/up/down/home/move/goto/file` should update
   `VIEW_DETAILS.logical_cursor` and ask the curses driver to materialize the
   result. Direct `wmove`/`getyx` use in these paths should disappear.

4. Route file-area editing through logical positions.
   `Text`, `SOS DELBACK`, `SOS DELCHAR`, word movement, tab movement, and
   after-EOL behavior should derive byte offsets from logical `TextPos`, not
   from curses `x`.

5. Consolidate software cursor painting.
   The cursor overlay is now represented in `UiFrame` and file-area capture is
   logical-first. The remaining work is to make `show.c` build a full logical
   frame and have the curses driver paint each overlay from that frame, removing
   the remaining per-branch overlay calls.

6. Bring prefix and command line under the same model.
   Prefix and command-line focus now have logical cursor state. The remaining
   work is to move their editing and viewport logic behind normalized command
   helpers instead of direct curses cursor reads.

7. Normalize input.
   `llmdriver` now has normalized text/key/command/logical-hit/debug event
   structures. The remaining work is to make curses keyboard/mouse collection
   feed the same event type before command dispatch.

8. Tighten guardrails.
   Once the migration is complete, make the curses-boundary test strict: editor
   logic files may not call curses directly. Obsolete legacy platform paths
   outside macOS, Linux, and Windows can be removed when they block the cleanup.

## Testing Strategy

Every step should run:

```sh
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
```

Coverage should increase as ownership moves:

- logical cursor unit tests for file-area, prefix, command line, virtual cells,
  and row roles.
- fake-driver tests for physical operations requested by keycap, flag, ZWJ, and
  ASCII lines.
- fixture validation for `tests/fixtures/utf-render.txt` and focused keycap
  demonstrators.
- manual macOS terminal smoke tests only after unit/fake-driver behavior is
  deterministic.

## Keycap Debugging Rule

Do not special-case keycaps in editor logic. Keycaps are just one terminal
profile class that can select conservative physical repair strategies. If a
keycap line fails but ZWJ works, compare logical frame output and driver
operation logs. Fix the shared driver path, strategy planner, or terminal
profile, not logical cluster boundaries.
