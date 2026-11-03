# GTK desktop frontend

The optional GTK 3 driver opens a native desktop window on the real THE
editor. File bytes, edit records, cursor positions, profiles, commands and
program jobs remain owned by the existing C core. GTK draws the same driver
windows used by the interactive renderer, using Cairo and Pango rather than
a terminal library.

## Build and launch

Install GTK 3.24 development files and `pkg-config`, alongside the normal
[build dependencies](testing.md). GTK is disabled by default, so existing
console and browser builds do not acquire a GTK dependency.

```sh
cmake -S . -B build/gtk -DUSE_GTK_DRIVER=ON
cmake --build build/gtk --parallel
build/gtk/release/the --driver gtk path/to/file.rexx
```

Use the usual CMake SDK settings when selecting a particular cREXX cohort.
`USE_SDSLH=ON` retains the normal profile's syntax services. A graphical
display is required; `xvfb-run` can supply one for automated Linux testing.
For compiler-backed syntax services, the selected `rxc` must support
`--syntaxhighlight`; older SDKs can build programs while lacking this service.
Select the compiler, assembler, shared runtime, import library and native tool
bundle from one matching cohort. See [runtime tools](runtime-tools.md) for
preparing the bundle and selecting `THE_NATIVE_TOOL_ROOT`.
The runtime module is `the_driver_gtk`; keep it with the executable from the
same build. Normal installation includes the GTK module when enabled.
Distributing GTK's shared libraries or creating a signed desktop app package
is a separate packaging step.

## Editing and profiles

Open and New use native file choosers. Save, Close, Undo and Next file invoke
normal core actions. Closing the application window closes unchanged buffers
and retains buffers with outstanding changes, using THE's `CANCEL` command.
The window's `*` marker includes changes in the pending edit record.

The editor area is a fixed grid with the real prefix area, command line,
file tabs, syntax colours, profile status line and reserved key guide. Click
the file, prefix or command area to move focus. Type through GTK's input
method, use profile-defined editor keys, or choose Command to enter a THE
command. Physical Ctrl-letter keys reach the profile; macOS Command-letter
keys map to the same logical Ctrl-letter identities. The native menus add
no key accelerators that override those definitions. Use the Paste menu or
button for clipboard text; CRLF, CR and LF become normal Enter input and
tabs follow the profile's Tab definition. Queued text is limited to 1 MiB.
The status and reserved rows are display information, separate from file
bytes. Clicking them retains the current GTK control's keyboard focus and
the previous logical insertion point. If the editor already has focus,
subsequent typing continues at that insertion point; if Program input has
focus, typing stays in Program input.

GTK uses the regular user profile, including `-p` and `-n`. It skips the
platform terminal-calibration system profile, whose cursor/repaint values
describe terminal emulators. Font fallback and final glyph appearance depend
on the installed Pango fonts.

## File actions and profiles

The action row follows the current file's shared Build/Run/Apply state. Labels,
availability and refusal tooltips come from the editor core. A Classic Rexx
source can Run directly; a `.crexx` source uses Build source, then Run. Editing
invalidates its previous Run artifact until a successful rebuild. A queued
action retains the source/revision/generation shown when clicked; the core
rejects it if an earlier queued edit changed that source.

New profile sources use `.profile.the`: Build profile, then Apply profile.
Other `.the` files use Build macro and Run macro. Edit profile opens the user
profile source; Apply compiled profile opens a native `.profile.rxbin` chooser.
Profile validation, immutable artifacts, replacement and startup persistence
use the same native tool host as editor commands. Configure its matching
bundle as described in [runtime tools](runtime-tools.md).

## Program console and build results

Program and Build result are separate auxiliary pages. Build/profile output
and completion appear in Build result; compiling does not erase Program
history, its selection or an unsent input draft. Run captures the current
unsaved source without saving it. Program shows bounded stdout/stderr and
completion state, with stderr coloured separately.

Program input submits one stdin record per Enter or Send, including an
explicitly empty line. Successful submission returns focus to that field;
F6 or deliberate navigation leaves it. EOF and Stop act on the running job.
The editor's command line remains available for editor commands.

Output follows the latest text while the reader is already at the end.
Scrolling back or selecting text retains that position as output arrives.
The tab and Latest control indicate unread output; Latest clears the cue and
resumes following. Output updates do not select another page or take keyboard
focus from the editor or a control.

The console appears to the right when the window can fit 80 editor columns,
the console controls' measured minimum width and the divider. It moves below
the editor in narrower windows. The font and GTK theme determine the exact
breakpoint. Resizing within either layout retains a manually adjusted divider.

The active file's profile colours also control both output pages. A
`SET COLOR` change repaints it immediately, and switching files applies the
new file's colours while preserving the running job, output selection and
unsent Program input.

| Colour area | GTK presentation |
| --- | --- |
| `CONSOLEBODY` | Output text and panel background |
| `CONSOLEHEADER` | Program console heading |
| `CONSOLEINPUT` | Program input field and label |
| `CONSOLECONTROL` | Console buttons and toolbar Run button |
| `CONSOLESELECTED` | Text selection and hovered, focused or pressed buttons |
| `CONSOLEDIVIDER` | Divider between editor and console |
| `CONSOLEERROR` | Stderr text |
| `CONSOLESTATUS` | Job status and completion line |

For example, `set color consolebody #D4D4D4 on #1E1E1E` changes the output
colours. GTK supports the roles' logical/RGB colours, reverse, bold, italic,
underline, strikeout and dim styling. Terminal line-edge and blinking text
attributes have no GTK widget equivalent.

## Qualification and current limits

`test_gtk_input` checks physical key translation and UTF-8/line-ending input.
`test_gtk_runtime` loads a test-only module compiled from the production GTK
driver and drives its real callbacks through the editor. It checks rendered
file cells, click/cursor mapping, UTF-8 editing, pending dirty state/title,
Ctrl-S, saved bytes, close/reopen, and command-line focus. With a configured
native tool bundle it also runs a real Rexx program, checks its output, sends
separate ASCII, Unicode and empty stdin records, and checks completion. The
contextual-action journey builds/runs a `.crexx` source, rejects stale and
failed builds, and builds/applies a profile through the GTK controls. Reader
checks cover retained selection, scroll position, unread output and Latest.
It writes `gtk-runtime.png` in the build folder.
The probe also changes all eight console colour areas during a running job,
switches files and checks widget colours, rendered divider pixels, stderr
styling, job identity, output selection and unsent input. It verifies the
wide/right and narrow/below layouts and captures `gtk-runtime-narrow.png`.
The test skips explicitly when no GTK display is available; a skipped GUI
test is not visual qualification.
It also checks all eight default RGB console roles without reverse video,
captures the default launch, wide and narrow layouts, and verifies that typing
after a status or reserved-row click reaches the retained file cursor.
Backspace and Undo remove those edits, and saved bytes remain unchanged.
During a running program, the same clicks preserve Program input focus and
typed input without changing editor text or saved bytes.

```sh
ctest --test-dir build/gtk --output-on-failure -R '^test_gtk_'
```

This first version provides keyboard editing and point-click focus. Drag
selection, system clipboard Copy/Cut, drag-and-drop, font/zoom preferences,
GUI tool registration and platform accessibility integration are not yet
implemented. Existing keyboard selection and editor commands remain usable.
On macOS, the current generated File, Edit and Workbench menus use the system
menu bar when the GTK window is active. GTK uses the same menu model inside
the window on other desktops. F10 opens a separate popup from that model;
it does not focus the exported macOS menu bar. Shift-Tab moves from the editor
to visible controls. About and Settings/Preferences application actions remain
unimplemented. The macOS application label for this unpackaged
development executable follows its process name (`the`); a desktop app bundle
will need its own packaging review.
The Program console captures the registered tool host's streams; arbitrary
shell escapes still use the launching process's standard streams. Long
synchronous macros and shell escapes can hold the GUI event loop.

Local macOS GTK checks do not qualify Linux, Windows, GTK distribution,
installation bundles, signing or a release.

The [graphical workbench usability proposal](roadmap.md#graphical-workbench-usability-proposal)
defines the next menu, setup, typography and clipboard work. Shared action
and runtime acceptance does not establish visual or human task-flow parity
with the browser reference.
