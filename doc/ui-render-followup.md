# Native tab and status repaint follow-up

This follows Adrian's Windows/macOS observations on the published CRXTHE 0.9.0
source (`131e514`). The `v0.9.0` tag and published installers remain unchanged.
The signing maintenance at `bc89003` is the parent of this source fix.

## Confirmed defects and repair

- A retained U+0009 had zero logical width through utf8proc but a one-cell
  render substitution. Initial ASCII painting also used a different ETMODE
  glyph from cursor repainting. Cursoring across a tab therefore skipped its
  logical position, duplicated adjacent text and reported the following
  character in HEXDISPLAY. Retained tabs now occupy one editable logical cell
  and consistently display a right arrow; their stored bytes are preserved.
- Status field writers inserted string terminators into the composed row.
  Painting stopped early, hiding mode/runtime flags and retaining old colours.
  Status fields now use bounded copies without interior terminators, and every
  refresh paints the entire status row before its UTF character overlay.
  A shorter preview can replace a longer one without leaving its ASCII tail.
- The tab status preview displays an arrow together with its actual U+9 code.
  Default Tab input still inserts spaces to the next configured tab stop.

## Validation at the first source fix

- The new TextPos tab regression failed 11 position/width assertions before
  the logical-width fix and passes afterwards.
- All 81 local optimized macOS Release tests pass, including both default
  macro profiles and retained-tab navigation/insertion/save.
- `test_curses_repaint.py` uses a real PTY and pyte 0.8.2 to check terminal
  output: repeated slow left/right traversal, one-cell arrows, exact status
  codes for tabs/combining/CJK/variation-selector/supplementary characters,
  clean preview delimiters and post-emoji ASCII tails, all HEXDISPLAY modes,
  clock on/off, uniform full-row backgrounds and resize to 40/120/80 columns.
  It also verifies the original file bytes are unchanged. This checks terminal
  cells and escapes, not the font's final pixels. The Mac candidate workflow
  supplies the test dependency in its own Python virtual environment.
- The Windows agent reproduced the original defects in the native console
  buffer at 120 by 30 cells. Explicit redraw cleared text/preview ghosts but
  retained the wrong tab position and eight stale status background cells.
  The original user file and the agent's fixture remained unchanged.

Exact successor Windows console-buffer and panel-return checks are still
required, followed by Adrian's manual test. No Shmuel handoff is authorised.

## Separate observation

A newly explored LLM profile sequence using Delete or direct
`SOS CUADELCHAR`, followed by save, left the original bytes unchanged. Its
cause is not established and no deletion repair is claimed here. The tab
regression checks its intended navigation/insertion/save boundary separately;
this additional editing/persistence observation needs its own investigation.
