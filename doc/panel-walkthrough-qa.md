# Panel cursor walkthrough — 26 October 2026

Adrian reported a disappearing cursor in the macOS console panel of the signed
`v4.0.0-panels.2` installation. The walkthrough used scratch files, the installed
release first, and then the corrected local development build. The installed
release and its signatures were preserved.

## Findings and correction

The UTF curses driver hides the hardware cursor because ordinary editing uses
a software cursor. READ created an overlay window and moved its cursor, but
did not paint the software cursor. The overlay now paints the active cluster
using the configured cursor style before refreshing, and presents the hardware
cursor when that is the driver's configured mode. Each repaint clears the
previous cursor along with the old field contents.

The actual browser walkthrough also found three cursor problems: an inherited
10px grid margin offset the caret, a full field clipped the caret outside its
right edge, and any mouse click selected the beginning of a field. The panel
now removes the margin, shares a bounded cluster position/width with curses,
and derives the selected cell from the click position. Input position is kept
separate from painting, so filling a field still rejects additional input.
Wide characters are highlighted as a whole cluster.

## Checks

- Installed-release LLM walkthrough with cREXX and Regina: field entry,
  Unicode movement/deletion, protected-label hits, Tab/Backtab, full-field
  overflow rejection, Enter and Escape, and resumed editing/save passed.
- Nine focused development CTest checks passed: panel keyboard input and
  restored editing, shared panel behavior, prefix keyboard input, driver
  boundaries, ordinary browser editing, logical colors, cursor geometry and
  ordinary curses typing.
- Both rendered Chromium panel suites passed with the corrected editor and
  the cREXX tool/runtime cohort from the signed release. The expanded checks
  assert caret alignment, visibility at a full field and mouse-selected cell,
  alongside protection, PF3, cancellation and disconnect/reconnect.
- Direct browser interaction verified the corrected initial/edge cursor,
  click position, two-cell Unicode cursor, Tab, PF3, Escape and refresh.
- TypeScript checking and the web production build passed.

The default development CTest environment selects the sibling cREXX debug
tool/runtime cohort. Its cREXX browser suite repeatedly timed out on reconnection
after navigating away during READ; Regina passed. Both suites passed when the
same corrected editor used the release's bundled cohort. This configuration
difference is established; the underlying cause remains open. A blanket green
development CTest result or compatibility with every installed cREXX is not
claimed.

The computer-control tool denied access to Terminal.app. Console behavior was
therefore driven through the LLM protocol and real pseudo-terminal keyboard
tests. A new visual inspection in Apple Terminal remains Adrian's check; it
must not be represented as completed by the agent. Local preview shortcuts
in `~/THE-macro-tests/` run the corrected development editor with the release
runtime without replacing the signed installation.

## Windows and next-release follow-ups

The Windows chat reports that the installed panels.2 payload passed all 157
manifest hashes, both 121-workflow macro suites with exact parity, both engines'
panels, shared controls and native ConPTY prefix/panel editing. Regina 3.9.7
x64 was subsequently installed and tested there. Those results apply to the
published release before this cursor correction; the new source correction
has not yet been rebuilt or visually qualified on Windows.

Adrian explicitly requested installer path validation for the next release.
The Windows installation had an older `.local/bin/the.exe` first on PATH and
persistent THE resource/runtime overrides into `.local`. Acceptance should
cover effective command resolution, executable/shim precedence, detection of
an earlier THE, and a post-install check of the launcher, driver/profile,
selected cREXX cohort and cache. Preserve unrelated local tools and intentional
explicit overrides. Explain that existing terminal/IDE processes need their
environment refreshed. No installer change was made in this walkthrough.

F8/F9 remain separate release findings: the bundled Mac standalone `crexx`
driver exits 127 for the valid sample, while Windows reports an input path
ending in `hello.crexxmsg`. Direct bundled `rxc` → `rxas` → `rxvm` works on both.
These helpers were not changed as part of the panel cursor repair.

No new installer was published, and nothing was handed to Shmuel. Manual
Windows qualification still gates that handoff. Upstream THE and live XEDIT
were not tested.
