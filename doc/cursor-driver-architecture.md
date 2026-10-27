# Cursor Driver Architecture

This document is the ownership contract for the cursor/driver split. UTF
design lives in [UTF profiles](utf-design.md); outstanding work lives in the
[roadmap](roadmap.md).

## Goal

THE must separate editor intent from terminal mechanics. Editor commands should
work in logical terms: file lines, screen rows, logical text cells, grapheme
clusters, command-line text, prefix text, and focus zones. Physical drivers then
materialize that logical state for curses, an LLM client, or the browser UI.

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

The logical layer also owns editor color, style, and key identity. Core code
uses `THE_COLOR_*`, `THE_STYLE_*`, `TheRenderAttr`, `TheDriverCell`, and
`THE_KEY_*` from `src/thecolour.h` and `src/thekeys.h`; it must not encode
state as `COLOR_PAIR(pair) | A_BOLD` or depend on curses headers for key
constants. The base color numbers and many key values intentionally remain
curses-compatible where that preserves existing maps, but they are THE-owned
contracts. RGB colours from `#RRGGBB` and SVG/X11 colour names are logical
THE colour IDs backed by the core colour registry, not preallocated physical
driver palette slots.

### Physical Driver Layer

The physical driver owns:

- curses `WINDOW *` access.
- `getyx`, `wmove`, `wadd*`, `mvwadd*`, `touchline`, `touchwin`,
  `wnoutrefresh`, `doupdate`, `wgetch`, input timeouts, raw mouse key
  translation, mouse packet storage, button/action/modifier decoding, and
  mouse packet position decoding.
- lowering `TheRenderAttr` and `TheDriverCell` into curses `chtype`,
  `cchar_t`, `attr_t`, `COLOR_PAIR`, and physical alternate-character cells.
- mapping logical RGB colours to terminal palette entries and colour pairs.
- physical cursor save/restore and hardware cursor parking.
- physical cursor materialization from shared logical/display targets.
- software cursor painting.
- UTF repair strategy execution.
- cell writes/fills and physical refresh ordering.

Shared logical/display mapping lives in `src/driverlayout.c`. It uses
`utflayout.c` when UTF is enabled and feeds the portable
renderer-cell/render-cluster model where rendering needs explicit width facts.

For the current curses UI, these mechanics belong in `src/drivers/curses/cursesdriver.c` or a
clearly physical edge that is being migrated to that driver. Do not move
physical cursor save/restore, refresh, touch/update, cell writes, software
cursor painting, or cursor parking back into logical command code.

Editor code reaches migrated high-level driver behavior through the current
driver vtable, `the_driver->...`, defined by `TheDriverOps` in
`src/thedriver.h`. `src/thedriver.c` owns the current-driver pointer, portable
module loader, and explicit selection helpers. The main `the` executable loads
`the_driver_curses` by default or for `--driver curses`, loads
`the_driver_llm` for `--driver llm`, and can load the opt-in `the_driver_web`
module for `--driver web`; it no longer links curses directly.
No-curses tests and the LLM runtime can select `the_headless_driver_ops`
without linking `src/drivers/curses/cursesdriver.c`. All drivers expose the same `TheDriverOps` surface.
Terminal-only operations may be NOPs, in-memory fake-surface updates, or
deterministic log entries in non-terminal drivers.

`curses_driver_*` function names are implementation-private there. The public
driver types are neutral: `TheDriverAttr`, `TheDriverCell`, `TheRenderCell`,
`TheRenderCluster`, and opaque `TheDriverWindow` handles. `WINDOW`, `chtype`,
and `cchar_t` are implementation-private curses types and must not reappear in
`src/the.h` or `src/thedriver.h`. Temporary physical edges that still need
windows call opaque vtable operations directly; pad, `stdscr`/`curscr`, modal
relative window, and broken-curses background mechanics have been contracted
behind transient snapshots or higher-level terminal lifecycle operations. Do
not add a neutral wrapper API parallel to the vtable.

`src/rendercell.c` owns the portable UTF renderer model. Render clusters carry
codepoint sequences, source UTF-8 slices, style, logical width, display width,
cursor width, repaint width, repair strategy hints, flags for substituted or
expanded output, and fallback representation for non-UTF surfaces. Curses
lowers this model to `cchar_t`, `wadd_wch`, `wadd_wchnstr`, or wide-string
writes inside `src/drivers/curses/cursesdriver.c`; headless/LLM drivers keep the semantic
cluster.

### Input Drivers

Input drivers own device-specific input collection and return normalized
events: text input, named keys, mouse-like logical hits, command submission, and
debug requests. Command dispatch should consume normalized input and logical
cursor state rather than raw curses coordinates.

The current curses key loop has a compatibility adapter: it reads through
`the_driver->read_input_event()`, normalizes the key through `TheInputEvent`,
then hands the equivalent legacy key to existing dispatch where necessary.
Raw curses `KEY_MOUSE` is translated in `cursesdriver.c` to the editor-owned
`THE_KEY_MOUSE`. Live curses mouse packets follow the same boundary:
`cursesdriver.c` decodes terminal packets and window-local physical
coordinates, while `mouse.c` maps the driver-owned saved packet to
`TheInputEvent` logical-hit targets and routes legacy mouse-definition dispatch
through the saved target window id. Migrated consumers such as `CURSOR MOUSE`
and `TABFILE` consume logical target kind, line number, row, cell, screen, and
window id instead of raw terminal coordinates. The public raw key/mouse wrapper
vtable operations are removed; remaining raw terminal mechanics are
curses-private.

### LLM Driver

The LLM driver is a UI driver, not screen scraping. It must expose deterministic
semantic snapshots with row roles, line numbers, prefix text, file text,
command/status text, marks, current focus, and logical cursor position. It
should accept the same normalized text/key/command/logical-hit/debug events as
other drivers.

The concrete no-curses LLM-facing editor surface is `the --driver llm`. It
boots real THE, skips curses initialization, uses real
buffers/views/profiles/command dispatch/parser state, preserves CREXX
integration when built, and exposes the LLM protocol over stdin/stdout.
Formatter and input behavior that does not need the editor runtime belongs in
focused unit tests, not in a second editor harness.

The main `the` executable does not link curses directly. Runtime-loaded driver
modules export `the_driver_module_ops()` and optional lifecycle hooks; the
curses module owns curses startup/shutdown, the LLM module owns the headless
full-runtime protocol driver, and the optional web module owns the localhost
HTTP/WebSocket adapter described in `doc/web-driver-poc.md`.

`test_driver_modules` guards the main executable for both dynamic dependency
cleanliness and accidental raw curses API symbol exports. A small compatibility
allowlist remains for legacy shared-runtime names in the main executable:
`my_wmove`, `curses_started`, `ncurses_screen_resized`, `suspend_curses`,
`resume_curses`, `the_driver_is_curses`, and `the_driver_use_curses`.
Those names are deferred naming debt; do not add to that list unless a bounded
migration proves why the name must remain public.

Physical metadata such as display column, terminal class, repair strategy, and
driver operation logs is useful for debugging, but agents must reason from
logical coordinates: `zone`, `line_number`, row, and logical cell.

## Runtime contracts

The core loads matching curses, LLM or optional web modules at startup and
validates their exported ABI version before using their operation tables.
Driver lifecycle and physical surface operations are described in the
[operation reference](driver-vtable-review.md).

`src/screenframe.c` rebuilds live row-role snapshots and rebases logical cursor
positions onto visible rows. `src/driverlayout.c` supplies shared logical/display
conversion and viewport targets. `src/mousehit.c` converts physical edge
positions to logical hit targets. `src/llm/llmruntime.c` adapts this state for
semantic snapshots, and `src/llm/llmsession.c` dispatches protocol inputs to the
real editor. Modal continuations use `src/transientui.c`; shared panel state
supports READ without adding a second editing model.

The working record `rec` is distinct from its persisted in-memory `LINE`.
Commit mutations through `post_process_line` before any cursor movement or
refresh that can reload the record. That function owns alteration counters,
recovery entries and parser synchronization. Presentation is allowed to read
state; it must not silently discard an uncommitted edit.

## Guardrails

Guardrails are only useful when they close with behavior coverage. Tighten them
module by module, not by aspiration.

- Logical foundation modules must stay curses-free.
- `src/the.h` and `src/thedriver.h` must stay free of curses includes,
  `WINDOW`, `chtype`, `cchar_t`, `COLOR_PAIR`, `PAIR_NUMBER`, `A_COLOR`, and
  raw curses `A_*` attributes; the curses implementation performs all lowering
  between neutral driver types and curses types.
- The main `the` executable must stay free of direct curses dynamic
  dependencies and raw curses API symbol exports. Compatibility names listed
  above are allowed only while legacy call sites still share editor runtime
  state with the curses module.
- Core source outside `src/drivers/curses/**`, `src/thedriver.*`, bundled
  PDCurses, and contrib must stay free of raw `WINDOW`, `chtype`, `cchar_t`,
  `CURRENT_WINDOW`, `SCREEN_WINDOW`, `PENDING_WINDOW`, curses includes,
  curses color-pair macros, and raw curses `A_*` attribute residue.
- Editor code should call `the_driver->...` for operations present in
  `TheDriverOps`. Direct `curses_driver_*` calls must stay inside
  `src/drivers/curses/**`; temporary low-level physical edges outside the driver
  should use opaque vtable operations while the broader model is still being
  split.
- `execute.c` direct curses calls are already guarded and should continue using
  the driver boundary for physical behavior.
- `readv_cmdline()`, `execute_dialog()`, and `execute_popup()` are guarded as a
  cleaned transient UI surface. Do not reintroduce raw curses input, refresh,
  paint, or mouse-position calls there; add logical snapshot state first and
  physical mechanics through `src/drivers/curses/cursesdriver.c`.
- When a command, query, SOS, mouse, or renderer group is fully migrated, extend
  `tests/check_curses_boundary.sh` for that newly closed surface.
- `the` and `the_driver_llm.so` must not link curses directly.
- Capability output from `the --driver llm` must remain exact for supported,
  unsupported, and build-dependent behavior.

## Test Surface Limits

- `the --driver llm` proves full editor behavior through the real runtime.
- `test_llmdriver`, `test_llmruntime`, `test_transientui`, `test_inputevent`,
  and virtual/fake-driver tests prove formatter, runtime-adapter, transient
  model, protocol/input parsing, and driver instrumentation behavior that does
  not require a live editor session.
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
