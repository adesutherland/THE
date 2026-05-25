# Cursor Driver Architecture

Last updated: 2026-05-25.

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

### 2026-05-23 Review

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
  owns logical-to-physical display column mapping for the curses path. It also
  wraps common physical window operations used by migrated code, including
  cursor capture/move/restore, window origin/size reads, refresh/update,
  attribute/touch helpers, and input timeouts.
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
  software-cursor cell painting, UTF/ascii cell write/fill primitives, and
  render-entry cursor save/restore helpers now live behind `cursesdriver.c`.
  Curses attribute, touch, refresh, and update calls from the renderer are also
  routed through driver helpers. Targeted command-line redraws now draw the
  software cursor from live logical command focus, targeted prefix redraws build
  a fresh `UiFrame` when possible, SDSLH bracket highlighting reads logical
  file focus directly, and UTF whole-line cursor-repair redraws reuse a
  frame-backed overlay. `show_statarea()` now inspects HEXDISPLAY text from the
  live logical file-area, prefix, or command cursor when one is available, and
  `prepare_view()`/`advance_view()` prefer logical cursor targets when
  preserving and restoring view-switch cursor positions. The old physical
  snapshot path remains only as a narrow fallback for renderer paths that still
  do not have a frame or logical area model.
- The main curses input loop now reads keys through `cursesdriver.c` and
  normalizes the collected key with `TheInputEvent` before handing the same
  legacy key code to existing dispatch. This is a compatibility adapter, not
  yet a full logical mouse-hit or command-dispatch replacement.
- `src/llmdriver.c` can build role-aware semantic snapshots from `UiFrame`,
  accept normalized input events through the shared input layer, and format
  cursor mapping plus driver operation logs for deterministic diagnostics.
- `src/agentdriver.c` plus `tools/the_agent.c` provide a no-curses executable
  proof target with scripted agent interaction and a no-curses dependency
  guard. The proof target now covers both file-area focus and command-line
  focus/cursor movement, reports its supported/unsupported surface through a
  stable `capabilities` response, and returns explicit unsupported-command
  diagnostics for full-editor commands that are not yet routed through the
  agent subset.
- Macro/agent-visible diagnostics now include both THE message history and
  SDSLH parser diagnostics: `EXTRACT /MESSAGES/`, `QUERY MESSAGES`,
  `SDSLHWAIT`, `EXTRACT /PMSGS/`, and `QUERY PMSGS`. SDSLH diagnostics are
  collected from the parse tree, including zero-length parser messages that are
  not attached to a painted character.
- Ordinary `execute.c` cursor migration has started. `execute_move_cursor()`
  now derives file-area row from logical cursor/focus state and materializes the
  cursor through `cursesdriver.c` in both UTF and no-UTF builds.
  `execute_makecurr()` and the normal block rearrange cursor-preservation path
  now preserve logical file-area/prefix cells rather than capturing and
  restoring curses cursor coordinates. `insert_new_line()` now places the
  inserted-line cursor from logical row/cell state and materializes file-area or
  prefix focus through the driver. `selective_change()` now positions its
  prompt cursor from the match's logical `TextPos` cell and driver viewport
  visibility rather than from the physical curses cursor. The remaining
  `execute.c` OS shell bridge, `EDITV LIST` screen, dialog/popup transient
  windows, popup placement, and popup/dialog input mechanics now call
  driver-owned physical wrappers instead of curses primitives directly.
- `src/cursor.c`, `src/comm5.c`, `src/query1.c`, `src/query2.c`, and
  `src/edit.c` no longer contain direct `getyx`, `wmove`, `getbegyx`,
  `getmaxx`, `getmaxy`, or `wtimeout` calls; those paths now go through
  `cursesdriver.c` wrappers.

The remaining implementation still has several physical cursor authorities:

- `execute.c` no longer calls the common curses cursor/window primitives
  directly. Ordinary cursor effects for `execute_move_cursor()`,
  `execute_makecurr()`, block rearrange cursor preservation, `insert_new_line()`,
  and `selective_change()` prompt placement are logical-first, while OS
  suspend/resume, `EDITV LIST`, popup placement, and popup/dialog mechanics are
  still physical behavior routed through `cursesdriver.c` wrappers. A real
  logical popup/dialog model remains a later slice.
- `commsos.c` no longer has direct curses cursor/window operations in SOS edit
  and navigation logic. Its remaining driver contact points are an active
  curses-driver cursor query fallback and prefix cursor materialization bridge.
- `show.c` still captures and restores physical cursor positions for render
  entry/exit and a few old targeted fallbacks. Status/HEXDISPLAY and
  view-switch preservation now prefer logical cursor targets, but retain a
  narrow physical snapshot fallback for paths that still move curses windows
  without publishing logical cursor state.
- `cursesdriver.c` owns the migrated physical primitives, but many callers
  still make logical decisions from legacy physical coordinates before calling
  the driver. The next separation slices should replace those decisions with
  logical focus/row/cell state rather than merely wrapping more calls.

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
   overlay selection. Software-cursor attribute, cell painting, UTF/ascii cell
   write/fill primitives, and render cursor save/restore helpers now live in
   the curses driver. Renderer attribute, touch, refresh, and update calls also
   go through driver helpers. Targeted command-line redraw, targeted prefix
   redraw, SDSLH bracket matching, and UTF whole-line cursor-repair repaint now
   use logical or frame-backed cursor data. The remaining work is to move the
   broader targeted redraw requests to driver-level logical render operations
   and retire the remaining legacy snapshot fallbacks.

6. Bring prefix and command line under the same model.
   Prefix and command-line focus now have logical cursor state. Normal `TEXT`
   editing for both areas mutates logical command/prefix buffers and redraws
   through area display helpers, and position/field reporting prefers logical
   cells. Remaining work in these areas is SOS/key-navigation cleanup and
   routing input through normalized events.

7. Normalize input.
   `inputevent` now owns normalized text/key/command/logical-hit/debug events,
   legacy key-code conversion, and an input queue. `llmdriver` delegates to
   that shared layer. The live curses loop now has a first compatibility
   adapter: collected keys pass through `TheInputEvent` and then back to the
   existing legacy key dispatcher. Remaining work is to make command dispatch
   consume normalized events directly where practical and to model mouse hits
   as logical targets instead of only legacy mouse key codes.

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

## Current Sequence

The active sequence is now intentionally aggressive. We are past the point
where isolated cleanup slices reduce risk; they leave too many half-migrated
paths alive. Prefer larger behavior groups when the group can be made visible
through `the_agent`, `llmdriver`, a fake/virtual screen CTest, or CREXX/pty in
the same commit.

Done for a migration slice means:

- the behavior is observable through a no-curses agent/LLM surface, a virtual
  screen/fake-driver test, a CREXX full-editor test, or a focused unit CTest;
- the real curses path uses the same logical input/frame/cursor data as the
  test surface;
- remaining unsupported behavior is declared through `the_agent capabilities`
  or a documented fallback;
- direct-curses guardrails are tightened for any module or behavior class that
  is fully cleaned.

High-priority groups:

1. Virtual UI harness. Build CTests that synthesize `UiFrame`/logical screen
   state for file-area, prefix, command, status, tabline, divider, window
   selection, and UTF fixture rows. Compare semantic snapshots and fake-driver
   operation logs. This is the main accelerator for renderer migration.
2. Agent command/input bridge. Expand `the_agent` beyond the subset dispatcher
   by routing normalized text/key/command input through shared editor paths.
   Add agent script CTests for every newly supported group and keep CREXX tests
   for full-editor parity when the agent cannot yet run a command.
3. Normalized input and mouse. Move dispatch decisions from legacy key
   conversion to `TheInputEvent` consumers in groups: text, named navigation,
   command submission, prefix editing, and logical mouse hits. Preserve legacy
   key definitions at the driver edge while making behavior observable through
   inputevent/agent CTests.
4. Renderer fallback purge. Use virtual renderer coverage to remove remaining
   non-frame `cursor_focus_*` fallbacks, render entry/exit logical decisions,
   and status/view-switch snapshot fallbacks from `show.c`. Physical
   save/restore, refresh, touch/update, UTF/ascii cell writes, software cursor
   painting, and cursor parking stay in `cursesdriver.c`.
5. Command/query fallback retirement. Retire active-driver row/cell fallbacks
   outside the renderer once equivalent logical query or agent/CREXX behavior
   is covered. Tighten `tests/check_curses_boundary.sh` module by module.
6. Logical popup/dialog/window lifecycle. Do this as a larger modeled slice
   only when popup/dialog/window state can appear in LLM snapshots and virtual
   screen tests. Until then, keep these as driver-owned physical mechanics.

## Test Surface Limits

The test surfaces intentionally cover different risks:

- `the_agent` proves no-curses driver behavior and logical snapshots. It does
  not yet execute arbitrary THE/SOS commands through the real dispatcher, so a
  command such as `SOS TOPEDGE` can be unsupported there even when the curses
  editor and CREXX tests cover it.
- CREXX/pty tests exercise the full editor command processor and are currently
  the best automated surface for SOS command behavior. They require CREXX
  support, the CREXX import/runtime files, and a pty-capable host, and they do
  not prove the no-curses LLM driver path.
- Manual smoke tests remain valuable for terminal paint regressions, but any
  generally useful gap found during manual testing should become either a
  `the_agent` capability/unsupported-command disclosure, a CREXX test, or a
  driver/unit test before the related area is considered closed.
- Virtual screen and fake-driver tests should become the preferred proof for
  renderer migration because they can compare semantic rows, cursor overlays,
  and requested physical operations without depending on curses paint timing.
