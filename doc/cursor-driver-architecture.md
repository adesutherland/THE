# Cursor Driver Architecture

Last updated: 2026-05-22.

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

### 2026-05-22 Review

The current documents and code agree on the core boundary, and the first
executable proof now exists as `the_agent`: an agent-interactive target that
links the logical editor/LLM driver modules and deliberately omits curses,
`show.c`, and `cursesdriver.c`. That target opens a file, exposes semantic LLM
snapshots, accepts normalized text/key/command input, and edits a small logical
buffer and command-line focus through `TextPos`/`LogicalCursor`. It is not
expected to replace the full curses editor yet; its purpose is to prove that
useful editor interaction can happen without terminal state or curses calls.

The proof target must satisfy these checks:

- no curses libraries in the target link line or dynamic dependencies.
- no `cursesdriver.c`, `show.c`, or curses-window symbols in the target source
  closure.
- interactive stdin/stdout protocol suitable for an agent loop.
- semantic `full`, `filearea`, `focus`, `reserved`, and `prefix` snapshots
  using the existing LLM formatter.
- normalized key/text/command handling through `TheInputEvent`.
- tests that exercise file loading, cursor motion, text insertion, compact
  views, and the no-curses build guard.

The implementation is being migrated away from multiple physical cursor
authorities. Some old paths remain, but the first stable checkpoints are now in
place:

- `src/uidriver.c` defines a logical frame, row roles, cursor overlays, and a
  fake driver operation log.
- `src/screenframe.c` builds live `UiFrame` snapshots from THE's current
  file-area rows, including prefix metadata and row-role validation.
- `src/cursesdriver.c` materializes UTF file-area logical cursor requests and
  owns logical-to-physical display column mapping for the curses path.
- `src/inputevent.c` defines normalized input events shared by curses-facing
  key codes and LLM-facing text/key/command/logical-hit/debug requests.
- UTF file-area left/right movement, text insertion, `SOS DELBACK`, and
  `SOS DELCHAR` now prefer `VIEW_DETAILS.logical_cursor` and derive edit byte
  ranges from logical `TextPos`.
- software cursor overlay capture for the file area is logical-first and rejects
  EOF/TOF/out-of-bounds rows by row role and line number.
- command-line and prefix focus now record logical cursor zones, so the renderer
  no longer needs to treat those areas only as curses positions.
- `show.c` builds a live `UiFrame` during full file-area redraw and uses that
  frame to decide file-area and prefix software cursor overlays. The actual
  software-cursor cell painting primitives and render-entry cursor
  save/restore helpers now live behind `cursesdriver.c`. The old snapshot path
  remains only as a fallback for targeted redraws that do not yet receive a full
  frame.
- `src/llmdriver.c` can build role-aware semantic snapshots from `UiFrame`,
  accept normalized input events through the shared input layer, and format
  cursor mapping plus driver operation logs for deterministic diagnostics.
- `src/agentdriver.c` plus `tools/the_agent.c` provide a no-curses executable
  proof target with scripted agent interaction and a no-curses dependency
  guard. The proof target now covers both file-area focus and command-line
  focus/cursor movement.

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
   The cursor overlay is represented in `UiFrame`. Full file-area redraw now
   builds a live frame and uses it for file-area and prefix software cursor
   overlay selection. Software-cursor attribute, cell painting, and render
   cursor save/restore helpers now live in the curses driver. The remaining
   work is to move targeted redraw requests to driver-level logical render
   operations and remove fallbacks that still rely on the legacy cursor
   snapshot.

6. Bring prefix and command line under the same model.
   Prefix and command-line focus now have logical cursor state. The remaining
   work is to move their editing and viewport logic behind normalized command
   helpers instead of direct curses cursor reads.

7. Normalize input.
   `inputevent` now owns normalized text/key/command/logical-hit/debug events,
   legacy key-code conversion, and an input queue. `llmdriver` delegates to
   that shared layer. The remaining work is to make curses keyboard and mouse
   collection feed `TheInputEvent` before command dispatch.

8. Tighten guardrails.
   Once the migration is complete, make the curses-boundary test strict: editor
   logic files may not call curses directly. Obsolete legacy platform paths
   outside macOS, Linux, and Windows can be removed when they block the cleanup.

9. Add a no-curses agent proof target.
   Done as `the_agent`. The executable agent driver uses `uidriver`,
   `llmdriver`, `inputevent`, `logcursor`, and `textpos` without linking
   curses. It gives agents a functional interactive surface and gives the
   refactor a concrete separation proof while the full curses editor is still
   being migrated.

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
