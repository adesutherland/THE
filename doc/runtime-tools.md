# RXBIN tools, profiles and console

THE's [common architecture](architecture.md#common-desktop-and-browser-application)
is implemented by a C session model (`toolhost.c`), editor integration
(`tools.c`) and native/Wasm resource adapters. The native-host and Wasm browser
modes use the same Program console UI. The terminal driver docks the
**Program console** on the right when there is room for at least 72 editor
columns, a separator and 44 console columns (117 columns total); otherwise it
appears below the editing area. This is a development feature
in `0.10.dev`.

## Run and interact

Open a source file and choose **Run file**, or enter `RX RUN`.
THE selects the last registered `runner` matching the filename's
extension, ignoring extension case, then checks its availability. A file
without an associated runner shows an unavailable Run action; it is not
silently sent to Classic Rexx. An explicit
`RX RUN tool-id` always selects that tool.
THE captures the whole buffer, including the pending working record. Running
preserves text, cursor, dirty state and saved bytes. The console keeps the
captured identity/revision; editing can continue independently.

`RX RUN "/absolute/workspace/program.rxbin"` runs a compiled program.
The graphical console's tool selector provides the same operation. Browser
**Import RXBIN** copies a binary into the persistent workspace, refusing name
collisions; native builds use normal workspace files. Browser bundles with the
cREXX editor and Classic interpreter include `examples/ToolHello.rxbin`, compiled from
`examples/tool-program.crexx`, for an import/run check. Arguments
are separate values, one per line in the graphical field, or quoted operands
after the command's tool/image. `RX RUN` alone runs the current buffer;
`RX RUN tool-id` runs a registered tool. Registered IDs take precedence over
filenames. For example, `RX RUN classic-rexx "Mixed Case"` passes one argument
to the Classic runner. These are not shell commands. The standard
library is loaded before ordinary RXBIN programs. Images must match the
packaged runtime and its available static providers.

Graphical controls provide **Stop**, **Clear**, line input and **End input**.

In the browser, actions beside the selected file follow its purpose. A
`name.the` macro offers **Build macro** and **Run macro**; a
`name.profile.the` profile offers **Build profile** and **Apply profile**.
Both compile with the same Level B toolchain. Profile builds use an immutable
filename ending in `.profile.rxbin`; ordinary macro builds end in `.rxbin`.
Applying a profile changes the editor setup and selects it for restoration at
startup. Existing `profile.the`, `browser.the` and `MiniBASIC-profile.the`
remain recognized as profiles. Build and Apply results appear in
**Tools & Profiles**. A `.rexx` file runs with its associated Classic runner.
Other file types run only when a profile associates a runner with that type.
**Tools & Profiles** also contains explicit tool selection, RXBIN import,
profile editing and build state. The browser **Program console** contains only
program output, input, Stop, EOF and Clear. Choosing a file never executes it.
File actions recheck the selected source and action definition before running;
a changed source or profile requires another explicit selection. A profile can
also define a contextual Build macro for an extension through
[`WORKBENCH ACTION ... FILEEXT ... PURPOSE BUILD`](workbench-actions-menus.md).

**Apply compiled profile** in Tools & Profiles accepts a workspace file ending
in `.profile.rxbin`. THE checks the RXBIN header and available cREXX runtime,
copies the image to an immutable private artifact, then applies and selects
that copy for startup. Editing or replacing the imported file later does not
change the selected profile. Compatibility with the current cREXX bytecode
cohort is established by loading the image, not by its filename or header.
An invalid image or failed execution leaves the prior selection usable.
If browser storage fails after execution, THE reports that the new profile is
applied in this session but its startup selection was not saved.
The compiled image is not opened as editor text. **Edit profile** opens an
editable source, or creates the default source if the selected profile came
from a compiled image without one.

Terminal commands are:

| Command | Effect |
| --- | --- |
| `RX CONSOLE` | Show the split and toggle console input focus (legacy). |
| `RX CONSOLE ON` | Show the split and focus Program input. |
| `RX CONSOLE OFF` | Hide the split without stopping its job. |
| `RX CONTROLS` | Focus the terminal Console launcher (FILETABS on). |
| `RX INPUT text` | Send a line to the running program. |
| `RX EOF` | Close stdin after queued input drains. |
| `RX STOP` | Terminate the isolated execution. |
| `RX CLEAR` | Clear visible output. |
| `RX STATUS` | Display job state. |

The default terminal profile keeps actual editor responses immediately above
the editor command line, leaving that row quiet while idle. **MENU (F10)** at
the upper left, **Console**, and **Run file** are visible controls. Console
shows the dock and focuses Program input; Run file captures the current buffer.
F10 opens
the same pull-down menu for file, editing, navigation, run and
profile actions, with separate actions to show/focus or hide the Program console.
The menu remains available while Program input has focus; a Run chosen there
starts after the menu closes. **Key definitions** retains the complete key guide. This
editor response area is separate from the Program console and its labelled
**Program input** field. Custom profiles can still choose their own MSGLINE
position and reserved rows.

The terminal toolbar provides **Run**, **Stop**, **Clear**, **Hide** and
**Editor**, with mouse access. Run captures the current editor buffer. Hide
retains output and any running program; Editor returns keyboard focus to the
editor. Run is unavailable during a job, and Stop is available while a job
runs. The terminal reserves the console's space before laying out
the editor, including its response, command and status rows.

In the default terminal profile, **Shift-Tab** focuses the Console launcher;
another Shift-Tab focuses Menu, then Run file. While a top control has focus,
Tab, Shift-Tab or Left/Right moves between the three, Enter or Space activates the
selected launcher, and Esc restores the editor caret. Ordinary editor Tab
keeps the profile's indentation command. F10 remains a direct menu shortcut.
The terminal-specific Shift-Tab definition is visible in Key definitions;
GTK and browser controls handle the same focus transition in their drivers.

Choose **Console** or **F10 Menu → Show or focus Program console** to reach Program input from the
keyboard. While the console has focus, Tab switches between input and the
toolbar. Shift-Tab moves from input to the toolbar, then to the Console
launcher; Left/Right selects a toolbar control, Home/End selects the first/last,
and Enter or Space activates it. Esc focuses the THE command line; F6 or the
Editor control returns to the file. Clicking the editor also restores file
focus. Ordinary editor Tab continues to follow the active profile.

In Program input, Enter submits a line, Backspace edits, Ctrl-D sends EOF,
Ctrl-C stops, F10 opens the menu, Esc focuses the THE command line, and F6
returns to the file. PageUp/PageDown scroll retained output, Home shows the
earliest output, and End returns to the latest output while Program input has
focus. The current scroll position stays put as new output arrives.
Input closes at EOF or completion. Panels and
macro READ retain their own input route. At fewer than 16 terminal rows or 32
columns the split hides and input focus returns to the editor; use commands
or enlarge the terminal. Graphical consoles start
collapsed and expand on a run; their divider adjusts height.

A native job uses a separate VM process with pipes. A browser job uses a fresh
Worker and private MEMFS; workspace storage is not mounted into that VM.
Stdout/stderr and typed completion remain separate. Stop discards job resources
without claiming Rexx cleanup. Native stdin supports queued input without
blocking the editor; a native pipe does not report whether Rexx is currently
waiting. Browser input starvation suspends execution until input or EOF.
Output is limited to 8 MiB per job with 256 KiB of UTF-8 scrollback. The browser
has a 120-second active execution deadline, paused while waiting for stdin.

## Edit, build and activate a profile

**Edit profile** / `RX PROFILE` opens an editable Level B source template.
New browser workspaces use `user.profile.the`; existing `profile.the` sources
remain in place. Save normally. **Build profile** / `RX BUILD` compiles the entire current
buffer with the matching compiler and assembler; it does not save that buffer.
Compiler diagnostics appear in the browser's Tools & Profiles Build results
(and in the native host's console presentation). A successful build is admitted as
an immutable RXBIN artifact. **Apply profile** / `RX ACTIVATE` executes
that artifact through the session's cREXX context and real `ADDRESS THE` bridge.
Source changes after building require another build before activation.

`SAY` and supported runtime diagnostics during activation use the context-local
output writer and appear with the Build results in the browser. Editor commands, EXTRACT and READ use
the existing synchronous bridge. Profile `PULL` still uses the legacy runtime
stdin; use `ADDRESS THE READ` for editor interaction. Program console stdin is
a separate channel. Activation requires cREXX's optional output-writer API.

The selected artifact is persisted only after successful activation. Failed
builds retain that selection. Profile-owned RX registrations and workbench
definitions are replaced on successful reactivation and restored after failed
activation; arbitrary editor changes made by a profile still cannot be rolled
back. Startup restores a selection only for the matching runtime cohort.
The profile directory defaults to `$HOME/.crxthe-tools` on
native systems (`USERPROFILE` when HOME is absent), or
`/workspace/.crxthe-tools` in the browser. `THE_TOOL_CONFIG_DIR` overrides it.
Browser profiles, independent registry entries and selection are committed to
IndexedDB; active-profile registrations are recreated by replaying that
profile at startup. Older registry entries without ownership information are
preserved. Ordinary unsaved editor text still requires Save.

## Register language tools and programs

`RX REGISTER id role image extensions "label"` adds or replaces a tool.
Roles are `program`, `runner`, `profile` and `language` for user-provided images. IDs
are at most 30 letters, digits, hyphens or underscores. `image` is a workspace
RXBIN path; extensions are comma-separated, such as `.rexx,.rex`. Registrations
persist beside the profile when registered independently. Entries made during
activation belong to that profile and are replaced on its next successful
activation. A profile can issue these same commands.

A `runner` uses the `source-file` entry contract: THE captures the whole current
buffer, including its pending record, and stages those immutable snapshot bytes
in the job's private filesystem. It invokes the image's ordinary RXBIN `main`
with the relative snapshot filename as its first argument, followed by the
ordinary argument vector. Program stdin stays available for interactive input;
source bytes never travel through stdin. The standard library is loaded first.
The snapshot basename follows the source filename, with unsafe characters and
reserved platform names made portable. The console retains the original logical
filename, source identity and revision. The private directory is never passed as
the source argument; arbitrary text produced by the runner is not rewritten.
At most 127 ordinary arguments fit beside the source argument within the shared
128-argument, 1 MiB budget. The existing 2 MiB source limit also applies.

For separately prepared, matching MiniBASIC images, a browser profile can
associate `.bas` with both the interpreter and grammar-backed highlighter
without changing Classic Rexx registrations:

```text
rx register minibasic runner /workspace/MiniBASIC.rxbin .bas "MiniBASIC"
rx register minibasic-syntax language /workspace/MiniBASICHighlight.rxbin .bas "MiniBASIC syntax"
```

Native profiles use absolute paths to the same kind of prepared images.

Opening a `.bas` file does not execute it. `RX RUN` captures the unsaved editor
source when invoked; it does not save that source. Changes or closure of the
buffer after launch do not change the running snapshot. Native jobs remove the
staged file at completion or Stop; browser jobs discard their private Worker
filesystem. This staging is not an operating-system sandbox.

The runner must be compiled for the exact configured runtime and available
providers. A newly built cREXX Level L MiniBASIC image is not qualified merely
because the generic source-file runner works. The optional browser review
candidate packages matching MiniBASIC interpreter and DSLSH images, plus three
`.bas` source files; ordinary browser builds do not. Missing images and source
files are copied from the package without replacing saved workspace copies.
If an image was removed, import its matching copy. Place the two registrations
above in the editable user profile and build/activate it. The candidate also places
`Profile sources/MiniBASIC-profile.the` in the workspace as a complete editable
example; it is never activated automatically.

For example:

```text
rx register my-rexx language /workspace/rexx-highlight.rxbin .rexx,.rex "My Rexx highlighter"
rx language my-rexx
```

Language images implement DSLSH's framed stdin/stdout service protocol with
`--stdio`. They run in independent sessions, with C-owned document/revision
admission. Browser associations use the last matching registration. Native
registration installs an SDSLH parser and extension mappings; `RX LANGUAGE id`
restarts/reselects it. The built-in `classic-highlight` selects the packaged
Level L Classic highlighter on native builds. Other native SDSLH configurations
remain available. Browser parser restart also uses the existing status control.
Protocol frames are kept out of the program console.

## Prepare native tools

The opt-in helper bundle uses a separately configured static VM and a matching
SDK. Supply the prepared, checksum-pinned Classic interpreter and optional
DSLSH images; this build never regenerates grammars.

```sh
cmake -S cmake/native-tools -B build/native-tools -G Ninja \
  -DTHE_CREXX_SOURCE=/path/to/CREXX \
  -DTHE_CREXX_HOST_BUILD=/path/to/matching/native-build \
  -DTHE_CLASSIC_IMAGE=/path/to/llcbrowser.rxbin \
  -DTHE_LANGUAGE_IMAGE=/path/to/dslsh-classic.rxbin
cmake --build build/native-tools --parallel 8
cmake -S . -B build/native-editor \
  -DTHE_NATIVE_TOOL_ROOT="$PWD/build/native-tools/tools"
cmake --build build/native-editor --parallel 8
```

`THE_TOOL_ROOT` selects a prepared bundle at runtime. CMake copies configured
assets into `share/the/tools` on installation. The helper, SDK, images and
`cohort.json` travel together. Native jobs are not OS sandboxes; their static
filesystem providers have the user's native file authority. Cross-platform
release packaging and Windows helper execution need separate qualification.

## Browser compiler and dependency cohorts

The browser source-profile compiler is optional. Build cREXX's maintained
single-threaded leaf with `CREXX_BUILD_COMPILER=ON` under Emscripten 6.0.13,
using the configured revision's native generated headers and SDK. Supply the
resulting `rxc_single.mjs/.wasm` and `rxas_single.mjs/.wasm` directory as
`THE_WASM_COMPILER_DIRECTORY`. Each invocation uses fresh CLI Modules, with
only the packaged library/exits and current source staged into their MEMFS.
The main editor requires the pinned runtime's context output-writer API.

The current local browser review cohort pins its runtime, compiler,
interpreter and parser VM to cREXX `43807b436`, with the DSLSH source at
`ee025115`. The prepared images and compiler modules have separate reviewed
checksums. This cohort has not yet been published upstream. The native helper
described above retains its earlier `8e8cfff4` bundle and provenance; it is
not silently relabeled as the browser candidate. See the
[browser build instructions](browser-wasm.md) for the remaining inputs.

## Verification

`test_toolhost` covers identity, bounded streams, UTF-8 chunks, EOF, runner
association/argument limits and stale
replies. `test_native_tools` exercises the real LLM editor and VM process,
including pending unsaved source-file snapshots, independent stdin, preserved
argv and saved bytes, Stop, conditions, profile activation and registry restore.
`test_tool_console_curses` uses a real PTY for the split, Unicode input/output,
focus and resize. Native graphical and Wasm browser checks exercise the common
console through their actual transports; browser checks also cover storage
failure/retry and compiler/activation failure recovery.
The Wasm VM contract harness also runs an ordinary source-file RXBIN fixture
through the actual private Worker filesystem and rejects an argument vector
whose prepended source filename would exceed the VM argument limit.
