# RXBIN tools, profiles and console

THE's [common architecture](architecture.md#common-desktop-and-browser-application)
is implemented by a C session model (`toolhost.c`), editor integration
(`tools.c`) and native/Wasm resource adapters. The native graphical driver
and browser use the same console component. The terminal driver presents a
split below the editing area. This is a development feature in `0.10.dev`.

## Run and interact

Open a Classic Rexx file and choose **Run Rexx**, or enter `TOOL RUN`.
THE captures the whole buffer, including the pending working record. Running
preserves text, cursor, dirty state and saved bytes. The console keeps the
captured identity/revision; editing can continue independently.

`TOOL RXBIN "/absolute/workspace/program.rxbin"` runs a compiled program.
The graphical console's tool selector provides the same operation. Browser
**Import RXBIN** copies a binary into the persistent workspace, refusing name
collisions; native builds use normal workspace files. Compiler-enabled browser
bundles include `examples/ToolHello.rxbin`, compiled from
`examples/tool-program.crexx`, for an import/run check. Arguments
are separate values, one per line in the graphical field, or quoted operands
after the command's tool/image. These are not shell commands. The standard
library is loaded before ordinary RXBIN programs. Images must match the
packaged runtime and its available static providers.

Graphical controls provide **Stop**, **Clear**, line input and **End input**.
Terminal commands are:

| Command | Effect |
| --- | --- |
| `TOOL CONSOLE` | Show the split and toggle console input focus. |
| `TOOL CONSOLE OFF` | Hide the split. |
| `TOOL INPUT text` | Send a line to the running program. |
| `TOOL EOF` | Close stdin after queued input drains. |
| `TOOL STOP` | Terminate the isolated execution. |
| `TOOL CLEAR` | Clear visible output. |
| `TOOL STATUS` | Display job state. |

When the terminal console has focus, Enter submits a line, Backspace edits,
Ctrl-D sends EOF, Ctrl-C stops, and Esc returns to the editor. Panels and
macro READ retain their own input route. At fewer than 16 terminal rows, hide
the split and use commands or enlarge the terminal. Graphical consoles start
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

**Edit profile** / `TOOL PROFILE` opens an editable Level B source template.
Save normally. **Build profile** / `TOOL BUILD` compiles the entire current
buffer with the matching compiler and assembler; it does not save that buffer.
Compiler diagnostics appear in the console. A successful build is admitted as
an immutable RXBIN artifact. **Activate profile** / `TOOL ACTIVATE` executes
that artifact through the session's cREXX context and real `ADDRESS THE` bridge.
Source changes after building require another build before activation.

`SAY` and supported runtime diagnostics during activation use the context-local
output writer and appear in the console. Editor commands, EXTRACT and READ use
the existing synchronous bridge. Profile `PULL` still uses the legacy runtime
stdin; use `ADDRESS THE READ` for editor interaction. Program console stdin is
a separate channel. Activation requires cREXX's optional output-writer API.

The selected artifact is persisted only after successful activation. Failed
builds retain that selection; arbitrary profile commands cannot be rolled back
if activation itself fails. Startup restores a selection only for the matching
runtime cohort. The profile directory defaults to `$HOME/.crxthe-tools` on
native systems (`USERPROFILE` when HOME is absent), or
`/workspace/.crxthe-tools` in the browser. `THE_TOOL_CONFIG_DIR` overrides it.
Browser profiles, registry and selection are committed to IndexedDB; ordinary
unsaved editor text still requires Save.

## Register language tools and programs

`TOOL REGISTER id role image extensions "label"` adds or replaces a tool.
Roles are `program`, `profile` and `language` for user-provided images. IDs
are at most 30 letters, digits, hyphens or underscores. `image` is a workspace
RXBIN path; extensions are comma-separated, such as `.rexx,.rex`. Registrations
persist beside the profile. A profile can issue these same commands.

For example:

```text
tool register my-rexx language /workspace/rexx-highlight.rxbin .rexx,.rex "My Rexx highlighter"
tool language my-rexx
```

Language images implement DSLSH's framed stdin/stdout service protocol with
`--stdio`. They run in independent sessions, with C-owned document/revision
admission. Browser associations use the last matching registration. Native
registration installs an SDSLH parser and extension mappings; `TOOL LANGUAGE id`
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

The common development cohort pins runtime/compiler to cREXX `8e8cfff4`.
The closed interpreter RXBIN was generated at `7ac74bc6`; the independent
highlighter VM/image retain their qualified `67a5e928` cohort. These boundaries
are recorded separately, including image checksums; they are not represented
as one source revision. `THE_WASM_PARSER_VM_DIRECTORY` permits the qualified
parser VM to be reused while building the editor with the newer profile SDK.
See [browser build instructions](browser-wasm.md) for the remaining inputs.

## Verification

`test_toolhost` covers identity, bounded streams, UTF-8 chunks, EOF and stale
replies. `test_native_tools` exercises the real LLM editor and VM process,
including interactive stdin, Stop, conditions, profile activation and restore.
`test_tool_console_curses` uses a real PTY for the split, Unicode input/output,
focus and resize. Native graphical and Wasm browser checks exercise the common
console through their actual transports; browser checks also cover storage
failure/retry and compiler/activation failure recovery.
