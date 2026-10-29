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

GTK uses the regular user profile, including `-p` and `-n`. It skips the
platform terminal-calibration system profile, whose cursor/repaint values
describe terminal emulators. Font fallback and final glyph appearance depend
on the installed Pango fonts.

## Program console

The Program menu and Run button use the existing native RXBIN tool host.
Configure its matching bundle as described in [runtime tools](runtime-tools.md).
Run captures the current unsaved source without saving it. The console
shows bounded stdout/stderr and completion state. Its separate Program input
field sends a line of stdin; EOF and Stop act on the selected job. Stderr is
coloured separately. The editor's command line remains available for editor
commands, including profile build and activation.

The active file's profile colours also control the GTK console. A
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
stdin and checks completion. It writes `gtk-runtime.png` in the build folder.
The probe also changes all eight console colour areas during a running job,
switches files and checks widget colours, rendered divider pixels, stderr
styling, job identity, output selection and unsent input. It verifies the
wide/right and narrow/below layouts and captures `gtk-runtime-narrow.png`.
The test skips explicitly when no GTK display is available; a skipped GUI
test is not visual qualification.

```sh
ctest --test-dir build/gtk --output-on-failure -R '^test_gtk_'
```

This first version provides keyboard editing and point-click focus. Drag
selection, system clipboard Copy/Cut, drag-and-drop, font/zoom preferences,
GUI tool registration and platform accessibility integration are not yet
implemented. Existing keyboard selection and editor commands remain usable.
The Program console captures the registered tool host's streams; arbitrary
shell escapes still use the launching process's standard streams. Long
synchronous macros and shell escapes can hold the GUI event loop.

Local macOS GTK checks do not qualify Linux, Windows, GTK distribution,
installation bundles, signing or a release.
