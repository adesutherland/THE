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

The final successor checks and Adrian's Windows approval are recorded below.
No separate Shmuel message is authorised.

## F7 form and syntax review

Adrian clarified that the additional panel report concerns the F7 macro demo.
The review found that READ retained CTLCHAR protection but discarded its
colours, and the native input reader could refresh the caller's window over
the modal form. The panel model now carries declared colours/styles to both
frontends, and native input reads from the modal window. A bare Escape also
needed a nonblocking tail check in the POSIX key decoder.

The demos reserve a clean body without file text, prefix numbers, ruler or
command input, use explicit label/field colours, and restore file, prefix and
command focus. Return values use one summary message instead of obscuring the
caller with a message per field. Browser form actions remain visible when the
body needs scrolling.

New native PTY checks exercise these behaviours with both macro engines and
validate all cells of comment, keyword, string, identifier and number tokens
using the bundled rxc parser. They account for emitted OSC palette definitions
instead of assuming the terminal's original indexed colours. Browser checks
assert the declared RGB colours and visible action-button bounds. Existing
prefix/command/file restoration assertions remain in place.
The combined local optimized macOS suite passes all 83 tests, and the browser
TypeScript check passes. Direct browser review also confirmed field typing,
forward/backward Tab, submission and restored editor focus. Direct control of
the macOS Terminal application was unavailable in this session; its terminal
coverage is from actual PTY output rather than a Terminal pixel inspection.

Windows b851 native checks confirmed the tab/footer repair but exposed seven
remaining Unicode overlay failures. The Windows agent's separate PDCurses
repair keeps legacy output within logical cells, excludes wide continuation
sentinels from Unicode encoding, and bounds ANSI neighbour repair to the
logical screen. Its isolated DLL tests passed both macro engines, Unicode
preview modes and logical/physical neighbour comparisons. The integrated
artifact subsequently passed the qualification below.

## Separate observation

A newly explored LLM profile sequence using Delete or direct
`SOS CUADELCHAR`, followed by save, left the original bytes unchanged. Its
refresh reloads the working record before committing the deletion, including
for plain ASCII. No deletion repair is claimed here. The tab
regression checks its intended navigation/insertion/save boundary separately;
this additional editing/persistence observation needs its own investigation.

## Final integrated qualification and release approval

Source `dc2ea4022b264e30d0cdc87e2ece64dcc35ae485` passed the complete local
macOS Release suite (83/83, no skips), TypeScript validation, hosted macOS
[run 40490388429](https://github.com/adesutherland/THE/actions/runs/40490388429)
(79 executed passes, four optional skips), and hosted Windows
[run 40490392546](https://github.com/adesutherland/THE/actions/runs/40490392546)
(69 executed passes, 12 optional skips). Both packages had clean, matching
source manifests and every payload hash was verified.

Native Windows repaint, five Unicode preview modes, F7 input/return and
syntax token checks passed with both cREXX and Regina. ConPTY capture also
verified the exact RGB colours for the form and five syntax categories.
Adrian then confirmed the Windows manual smoke test passed and authorised
publication of a full CRXTHE 0.9.1 release on 27 October 2026. The versioned
source must be rebuilt and signed through the controlled release gates; the
0.9.0-named test archives are not release inputs for 0.9.1.
