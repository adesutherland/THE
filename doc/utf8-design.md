# UTF-8 Support Design

This document is the working design record for making THE Unicode-first across
macOS, Linux, and Windows. Keep it updated as implementation choices change.

## Goals

- Build with UTF-8 and wide-character support enabled by default.
- Store file contents as UTF-8 bytes without lossy conversion.
- Treat editor-facing character positions as Unicode code point positions in
  Phase 1.
- Track terminal display positions as cell columns, not bytes or code points.
- Keep the Phase 1 API shape ready for Phase 2 grapheme-cluster support.
- Avoid APIs whose meaning changes under `#ifdef USE_UTF8`.

## Non-Goals For Phase 1

- Full Unicode grapheme-cluster editing semantics.
- Complete handling of zero-width-joiner emoji sequences, flags, and skin-tone
  emoji as single editable characters.
- Rewriting all historical byte-oriented internals at once.

## Units

THE must keep these units distinct:

- **Byte offset**: Offset into the UTF-8 line buffer. This is the storage and
  file I/O unit.
- **Code point index**: Index of a Unicode scalar value in a line. This is the
  Phase 1 editor-facing character unit.
- **Cluster index**: Index of a grapheme cluster. In Phase 1 this is an
  invariant alias of code point index. In Phase 2 it becomes independently
  computed.
- **Cell column**: Terminal display column. This is the rendering, cursor, and
  mouse hit-testing unit.

## Shared Position Types

The shared position model is intentionally explicit:

```c
typedef struct {
    size_t byte_offset;
    size_t codepoint_index;
    size_t cluster_index;
    int cell_column;
} TextPos;

typedef struct {
    LINETYPE line_number;
    TextPos text;
} FilePos;

typedef struct {
    short row;
    short col;
} ScreenPos;

typedef struct {
    FilePos file;
    ScreenPos screen;
} EditorPos;

typedef struct {
    TextPos start;
    TextPos end;
    int leading_cells;
    int content_cells;
    int trailing_cells;
} TextCellSlice;
```

Phase 1 invariant:

```c
pos.cluster_index == pos.codepoint_index
```

`TextPos` represents a caret/insertion boundary in one logical line. It does
not represent a character object. Character metadata is derived from the line
at a `TextPos` when needed.

## Canonical Construction

Callers should not hand-fill individual `TextPos` fields. Positions are
canonical only when produced by the shared helpers:

```c
TextPos textpos_from_byte(const CHARTYPE *line, size_t len, size_t byte_offset);
TextPos textpos_from_codepoint(const CHARTYPE *line, size_t len, size_t codepoint_index);
TextPos textpos_from_cell(const CHARTYPE *line, size_t len, int cell_column, TextSnap snap);
TextPos textpos_next_codepoint(const CHARTYPE *line, size_t len, TextPos pos);
TextPos textpos_prev_codepoint(const CHARTYPE *line, size_t len, TextPos pos);
TextCellSlice textpos_slice_cells(const CHARTYPE *line, size_t len, int start_cell, int width_cells);
```

Rules:

- Byte offsets are normalized to UTF-8 code point boundaries.
- Out-of-range indexes clamp to the end of the line.
- Invalid UTF-8 bytes decode as U+FFFD and consume one byte, so scanning always
  progresses.
- Phase 1 cluster index is always copied from code point index.
- Display slices are expressed in cells. If a slice starts or ends inside a
  wide character, the partial character is omitted and represented as leading
  or trailing padding cells.

## Mouse And Cell Snapping

A mouse click begins as a screen row/column, which maps to a cell column in a
logical line. `textpos_from_cell()` resolves that cell to a canonical `TextPos`.

Phase 1 supports these snap modes:

- `TEXT_SNAP_BACKWARD`: choose the code point boundary at or before the cell.
- `TEXT_SNAP_FORWARD`: choose the code point boundary after the cell when the
  cell lands inside a wide character.
- `TEXT_SNAP_NEAREST`: choose the nearest insertion boundary.

For a double-width character such as U+1F600, both occupied cells map to a
valid insertion boundary according to the selected snap rule; no API receives a
bare ambiguous `column`.

## API Direction

- New semantic APIs should accept or return `TextPos`, `FilePos`, `ScreenPos`,
  or `EditorPos` as appropriate.
- Low-level byte work is allowed only in explicitly byte-named helpers, e.g.
  `byte_offset`, `_bytes`, or `_raw`.
- Existing macro-visible APIs that naturally mean characters should become
  code-point oriented in Phase 1.
- Truly byte-oriented legacy APIs should be renamed, isolated, or documented as
  byte APIs before they are exposed further.

## Rendering Direction

Rendering must use terminal cell widths. It must not assume that one code point
is one cell. Wide characters, combining marks, and invalid bytes must all pass
through the shared layout helpers.

Curses output should use wide-character APIs (`setcchar`, `wadd_wch`,
`wadd_wchnstr`) rather than writing into `cchar_t` internals or passing Unicode
code points to narrow `waddch` paths.

## Platform Direction

- macOS: use Apple SDK curses wide APIs exposed through `<ncurses.h>`.
  CMake should not request a separate `ncursesw` include path on macOS because
  Apple's SDK does not ship one.
- Linux: prefer/link wide curses (`ncursesw`) through CMake's
  `CURSES_NEED_WIDE` path.
- Windows: build/use PDCurses with wide support and forced UTF-8 where needed.
- Locale setup should use the user's environment via `setlocale(LC_ALL, "")`,
  with UTF-8 fallbacks where the platform has them.

## Testing Strategy

Testing should land with each implementation slice.

Foundation tests:

- ASCII byte/codepoint/cell identity.
- Accented BMP characters.
- CJK wide characters.
- Single-codepoint emoji such as U+1F600.
- Combining marks, with Phase 1 cluster index equal to code point index.
- Invalid UTF-8 progress and U+FFFD handling.

Editor integration tests:

- Batch read/save preserves UTF-8 bytes.
- `QUERY UTF8` reports enabled by default.
- Cursor movement and mouse hit-testing use `TextPos`.
- Rendering clips and pads by cells, not bytes or code points.
- Syntax highlighting offsets map through canonical positions.

Current automated coverage:

- `tests/test_textpos.c` covers canonical Phase 1 position construction,
  byte-boundary normalization, code point counting, cell-width mapping,
  cell-based display slicing, invalid UTF-8 progress, and UTF-8 encoding.
- A fresh macOS CMake build with default options verifies that UTF-8/wide
  curses configuration no longer requires `<ncursesw/ncurses.h>`.

Manual renderer fixture:

- `tests/fixtures/utf8-render.txt` is a valid UTF-8 file for visual/manual
  checks in THE.
- Use it to inspect accented text, CJK double-width characters,
  single-codepoint emoji, combining marks, and horizontal viewport clipping.
- Invalid UTF-8 is intentionally excluded from this manual text fixture because
  editors and source tools may silently normalize or repair invalid byte
  sequences. Keep invalid-byte coverage in byte-oriented automated tests or
  generated binary fixtures.

## Implementation Status

- 2026-05-07: Design agreed. Phase 1 starts with default UTF-8 build,
  portable wide-curses selection, locale initialization, and canonical
  `TextPos` helpers plus tests.
- 2026-05-07: Added `src/textpos.h` and `src/textpos.c` as the shared
  position/UTF-8 foundation. Added `tests/test_textpos.c` and wired it into
  CTest. Verified `cmake --build /tmp/the-utf8-phase1 --target the
  test_textpos -j2` and `ctest --test-dir /tmp/the-utf8-phase1
  --output-on-failure` pass on macOS.
- 2026-05-07: Added an include guard to `src/thedefs.h` so shared UTF-8
  headers can safely include the foundational type definitions without
  duplicate typedef warnings.
- 2026-05-07: Corrected the wide-character header gate in `src/the.h` to use
  `USE_WIDE_CHAR`, matching the new default wide build path.
- 2026-05-07: Added `TextCellSlice` and migrated the UTF-8 file-line renderer
  in `src/show.c` to clip and pad by terminal cells. The renderer now emits
  code points through `setcchar`/wide curses on both fast and slow paths, and
  target highlighting uses byte-range overlap instead of code point counts.
- 2026-05-07: Added `tests/fixtures/utf8-render.txt` as a manual renderer
  fixture for visual checks while migrating cursor, mouse, and syntax paths.

## Next Implementation Slice

- Migrate cursor positioning and mouse hit-testing to return/use `EditorPos`
  with byte, code point, cluster, and cell fields populated.
- Audit syntax highlighting producers so highlight arrays are explicitly
  indexed by the same semantic unit the renderer consumes.
- Add integration coverage for cursor placement and mouse hit-testing around
  wide characters and combining marks.
