# Browser WebAssembly preview

The `0.10.dev` editor preview runs the C editor in a dedicated browser Worker,
using the same Preact UI and semantic input/snapshot protocol as the native
web driver. It needs static HTTP hosting, with no native editor server or
WebSocket. Editing, commands, prefixes, selection, Unicode layout, the file
ring, undo and saving stay in the editor core.

The editor/storage build remains available without cREXX. The optional
`THE_WASM_CREXX=ON` build adds a static cREXX VM, a compiled browser profile,
quick-action macros and the public panel example. Profiles use the real
`ADDRESS THE` callback, including EXTRACT and READ stems. The base variant accepts precompiled `.rxbin` macros. The optional common
runtime adds source-profile compilation with a matching Wasm compiler and
assembler; see [tools and profiles](runtime-tools.md). Native OS commands, Regina and native parser processes are
unavailable.

## Build and run

Use CMake, Ninja, Git, Python 3 and npm. Install the pinned Emscripten SDK:

```sh
git clone --branch 6.0.13 --depth 1 https://github.com/emscripten-core/emsdk.git build/toolchains/emsdk
build/toolchains/emsdk/emsdk install 6.0.13
build/toolchains/emsdk/emsdk activate 6.0.13
source build/toolchains/emsdk/emsdk_env.sh
emcmake cmake -S . -B build-wasm -G Ninja -DCMAKE_BUILD_TYPE=Release -DTHE_BUILD_WASM=ON
cmake --build build-wasm --parallel 8
python3 -m http.server 8766 --bind 127.0.0.1 --directory build-wasm/site
```

Open `http://127.0.0.1:8766`. The bundle is `build-wasm/site/`, including
`wasm/`, dependency/source metadata in `manifest.json`, and `licenses/`.
The build installs locked web dependencies and checks TypeScript. Native and
Wasm UI builds both use `web/node_modules`; run those builds sequentially.
utf8proc is pinned to `e5e799221b45bbb90f5fdc5c69b6b8dfbf017e78` (2.11.3).

Deploy the whole directory at the origin root. Serve `.wasm` as
`application/wasm` and `.mjs`/`.js` as JavaScript. Use HTTPS outside localhost;
IndexedDB and Web Locks must be available. A restrictive content security
policy needs same-origin Workers and permission to compile WebAssembly
(`wasm-unsafe-eval`). The build uses Asyncify without SharedArrayBuffer,
pthreads, COOP or COEP. Subdirectory deployment and an offline service worker
are not qualified in this milestone.

The CI workflow `wasm-browser.yml` builds a preview artifact from `main` and
runs the core and Chromium checks against its separately pinned published
cREXX profile cohort. The current combined Rexx/MiniBASIC review candidate
uses newer local dependency commits and has local qualification only until
those commits are published and CI is updated. CI does not deploy a website
or make a release. Firefox, WebKit, mobile browsers and private-browsing
persistence remain open qualification work.

## Precompiled cREXX profiles and macros

Use the pinned cREXX source revision from `cmake/CrexxWasm.cmake`. Prepare a
matching native compiler, assembler, library and certified compiler exits
before cross-compiling. The current browser review cohort is in the local
`../CREXX` development checkout; it has not yet been published upstream.
For that checkout:

```sh
cmake -S ../CREXX -B build/crexx-host -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build/crexx-host --target rxc rxas library compiler_exit_bin --parallel 8
source build/toolchains/emsdk/emsdk_env.sh
emcmake cmake -S . -B build-wasm-profile -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DTHE_BUILD_WASM=ON -DTHE_WASM_CREXX=ON \
  -DTHE_CREXX_SOURCE="$PWD/../CREXX"
cmake --build build-wasm-profile --parallel 8
python3 -m http.server 8769 --bind 127.0.0.1 --directory build-wasm-profile/site
```

`THE_CREXX_HOST_BUILD` defaults to `build/crexx-host`; set it explicitly for a
different location. Configuration checks the source revision, clean VM/tool
sources and generated host version header. Do not mix bytecode or tools from
another cREXX cohort. The bundle records the dependency revision and licenses.
The cREXX source checkout is not modified by the build. The browser adapter
supplies virtual POSIX file lookup and links the shared invocation owner when
the optional single-threaded build target does not already include it.

Ctrl-F and Ctrl-G run the bundled quick-find and quick-line macros. Ctrl-O
keeps the browser workspace file picker; `quickopen.rxbin` is also bundled.
F7 runs `paneldemo.rxbin`: type in its fields, then Enter to submit or
Escape to cancel. These operations execute through the same hosted cREXX API
and editor callback as native THE. The VM can suspend through a nested READ
while the editor Worker awaits browser input. The browser-specific profile
uses the shared default key definitions without native PATH or shell probes.

Browser command input supports safe editor aliases through [SET SYNONYM](commands/SET_SYNONYM.md).
Aliases contain one allowed editor command; their fixed and invocation operands
are checked on every use. Arbitrary `MACRO` commands, inline Rexx, OS commands,
alias command chains and prefix macros are unavailable through browser input.
The bundled shortcuts above run through the trusted profile path. The base macro
host accepts precompiled bytecode; source `.the`/`.rexx` compilation is unavailable
there. The Program console can import compiled RXBIN files; general file
import/export is still future work. Long-running bundled macros
currently need a Worker restart; cooperative interruption and recovery of
unsaved edits remain open.

## Stream-based VM host

A separate `THE_BUILD_WASM_VM=ON` configuration builds a reusable RXBIN process
host for parser services. Use a different build directory and the same pinned
cREXX source/host tools:

```sh
emcmake cmake -S . -B build-wasm-vm -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DTHE_BUILD_WASM_VM=ON -DTHE_CREXX_SOURCE="$PWD/build/crexx-source"
cmake --build build-wasm-vm --parallel 8
node web/test/wasm-vm-smoke.mjs build-wasm-vm
python3 -m http.server 8768 --bind 127.0.0.1 --directory build-wasm-vm/site
THE_VM_URL=http://127.0.0.1:8768/test.html \
CHROMIUM_BIN='/path/to/chrome' node web/test/wasm-vm-browser.mjs
```

`web/vm/vm-client.mjs` exports `CrexxVMProcess.start({images, arguments,
stdout, stderr})`. Supply matching RXBIN byte arrays, then `write(Uint8Array)`
and `endInput()`; await `exit` for distinct runtime/program status. Output
callbacks receive raw byte chunks on separate stdout/stderr channels. Chunk
boundaries are arbitrary. Temporary input starvation waits; only an explicit
close produces EOF after queued bytes drain. `failInput()` reports an I/O
failure instead. Input is bounded to 16 MiB queued, images to 64 MiB total and
arguments to 1 MiB/128 entries. Excess input is refused for the caller to retry.

Each process owns one isolated Worker/context and runs once. Only its execution
owner enters C; incoming messages feed a JS byte queue while stdin is suspended.
`terminate()` discards that process; start a new instance to recover. The
current RXBIN reader can abort on malformed input, so a failed load also
requires a fresh Worker. This host is for matching build-produced bytecode.
It installs no native process, socket, filesystem or Classic runtime providers.
It is separate from the profile VM, which must share the editor's synchronous
`ADDRESS THE` callback. Both use the same VM source cohort.

## Level L Classic Rexx highlighting

`THE_WASM_HIGHLIGHT=ON` adds the maintained Classic parser in a second Wasm VM
Worker. It highlights `.rexx` and `.rex` files and publishes located parser
diagnostics. Errors, warnings and information appear on the affected editor
text using the profile's `CBERROR`, `CBWARN` and `CBINFO` colours. When the
cursor is on marked text, the profile's `PMSG.1` status field shows the
message with its `PMSGERROR`, `PMSGWARN` or `PMSGINFO` colour. There is no
separate browser Diagnostics pane. The edited program is never executed. The
parser indicator identifies **Level L Classic Rexx** and reports `pending`,
`ready`, `refused`, `error` or
`too-large`. Click that status to restart the parser without restarting the
editor. While an edited revision is pending, the browser retains the previous
lexical paint only for unchanged visible text in the same source/parser
identity. Changed text receives no cached colours. A completed result replaces
that paint; a parser/profile restart, refusal or failure clears it. Diagnostic
severity is never retained from an older result. The stock browser profile
maps `.crexx` sources to the built-in Rexx lexical colourer. This gives comments,
keywords, strings and numbers their syntax colours without compiler diagnostics;
it does not claim a browser cREXX DSLSH parser. Other file types retain their
existing colourers. A combined MiniBASIC candidate also registers its bundled
DSLSH language image for `.bas` by default. Its runner remains opt-in through
`MiniBASIC-profile.the` or the editable user profile.

The first connection sends complete C-owned logical source snapshots through
DSLSH's existing eight-hex-byte framing. It coalesces unsent revisions and keeps
one ordered request in flight. The service has one replaceable mirror; the host
retains each real document ID and revision. Replies enter C through its input
queue and are checked again against the current revision. Closing a file retires
its identity, so a late reply cannot attach to a reopened file. JavaScript does
not own editor text or authoritative parser results. The C owner uses the
shared DSLSH codec and critical-section helpers, validates bounded coverage and converts
Unicode codepoint spans to its semantic frame/diagnostic coordinates.

This is a full-snapshot connection, rather than the native incremental
CodeBuffer transport. Source is limited to 2 MiB per file, replies to 8 MiB and
200,000 records; requests time out after ten seconds. Active SRCMAP, embedded
NUL and invalid UTF-8 are refused by the parser contract. The status exposes a
refusal instead of silently displaying legacy colours. A later valid edit or a
parser restart can recover. Classic execution is an additional optional build,
described below.

The current local browser review cohort is cREXX
`43807b4368cb3dcda49d67c0ac15599cf1e22594` and DSLSH
`ee025115bbc31a48575b151f990d2d3533d69a57`. Prepare the linked Classic
image using DSLSH's `crexx/README.md` and matching native cREXX tools; THE
does not regenerate the grammar. The image contains its bytecode library
closure and needs no external RXPA provider. With those local checkouts and
the prepared image, configure:

```sh
emcmake cmake -S . -B build-wasm-classic -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DTHE_BUILD_WASM=ON -DTHE_WASM_CREXX=ON -DTHE_WASM_HIGHLIGHT=ON \
  -DTHE_CREXX_SOURCE="$PWD/../CREXX" \
  -DTHE_CREXX_HOST_BUILD="$PWD/../CREXX/cmake-build-debug" \
  -DTHE_CREXX_REVISION=43807b4368cb3dcda49d67c0ac15599cf1e22594 \
  -DTHE_DSLSH_SOURCE="$PWD/../DSL-Syntax-Highlighter" \
  -DTHE_WASM_CLASSIC_IMAGE=/absolute/path/to/dslsh-classic.rxbin
cmake --build build-wasm-classic --parallel 8
node web/test/wasm-classic-framing.mjs
node web/test/wasm-classic-core.mjs build-wasm-classic
python3 -m http.server 8770 --bind 127.0.0.1 --directory build-wasm-classic/site
THE_WEB_URL=http://127.0.0.1:8770 \
CHROMIUM_BIN='/path/to/chrome' node web/test/wasm-browser-smoke.mjs
THE_WEB_URL=http://127.0.0.1:8770 THE_WASM_DEFAULT_SYNTAX_ONLY=1 \
CHROMIUM_BIN='/path/to/chrome' node web/test/wasm-browser-smoke.mjs
THE_WEB_URL=http://127.0.0.1:8770 \
CHROMIUM_BIN='/path/to/chrome' node web/test/wasm-paint-smoke.mjs
node web/test/highlight-presentation.mjs
```

The paint smoke requires a running highlighter-enabled site, so it is a manual
browser check rather than a native CTest dependency. It delays only delivery of
completed UI snapshots to force observable pending frames, then checks pixel
continuity on untouched text, DOM selection and caret retention, and exact
saved/reopened bytes. The presentation check also covers identity changes,
parser refusal/failure and removal of stale diagnostic styles.

Configuration pins the reviewed image SHA-256 from `cmake/TheWasmHighlight.cmake`.
A rebuilt image with another checksum needs explicit qualification and
`THE_WASM_CLASSIC_SHA256`. The browser verifies that checksum before loading
bytecode. The package records both source revisions, the image checksum and
licenses. `ClassicDemo.rexx` is copied into a new browser workspace only if
absent; open it through File/Open. It demonstrates Unicode, nested comments,
control flow, a continuation and a routine. Existing saved files are preserved.

## Classic Rexx interpreter and console

`THE_WASM_INTERPRETER=ON` adds a visible **Run Rexx** action and a Program
console. On wide screens the console sits beside the editor; on phones it
follows the editor in the vertical workbench. Run opens the console and executes
the current buffer. When Classic Rexx is unavailable, the action remains visible
with a reason. Open `RexxRunner.rexx` through File/Open for a working
example with console input, loops, stems, a routine and `INTERPRET`.

Run executes the entire current buffer, including unsaved changes and the
current working line. It does not save or clear the buffer's modified flag.
The console identifies the filename/revision used and reports subsequent buffer
changes. Enter in the console input field sends a line. End input sends EOF;
temporarily having no input does not end the program. Arguments are separate
positions, one per line; empty lines within that field retain empty positions.
An empty field supplies no arguments. This is not a shell command line.

Output and runtime errors appear in the console. Program return status is
separate from a VM failure or typed Rexx condition; a failed result's zero
status is never reported as successful completion. Available cleanup failures
are retained separately. Runtime VM module/address values do not necessarily
identify a Rexx source line, so the console does not invent source locations.

Each execution uses a fresh Worker and a compiled `llcbrowser.rxbin` entry
around cREXX's existing Classic compiler/IR interpreter. Source, logical
filename and arguments arrive through RXVML values, leaving stdin for the
program. Ordinary routines, BIFs, private queues, `SAY`, `PULL`, `LINEIN` and
`INTERPRET` use the existing runtime. External source lookup is disabled in this
embedding. The editor's profile/macro `ADDRESS THE` bridge is separate; programs
in this console do not address the editor.

Named streams can create temporary files in execution-private MEMFS. They do
not reach the persistent editor workspace, host disk or browser filesystem
handles. No filesystem is mounted into the execution Worker. Dynamic plugins,
process channels and host commands remain unavailable. Stop terminates only
that Worker and discards its files; it is not graceful language cleanup. A
subsequent Run starts with fresh streams and resources.

The initial limits are 2 MiB valid UTF-8 source, a 4096-byte logical filename,
128 NUL-free argument positions with 1 MiB aggregate argument bytes, the existing bounded
stdin queue, 8 MiB total output per run and 256 KiB of UTF-8 visible
scrollback. The 120-second execution deadline pauses while waiting for console
input. A limit or Stop leaves the editor and highlighter available. The bounded
completion envelope can truncate large values/condition messages and reports
that fact.

Build the runner separately with `THE_BUILD_WASM_RUNNER=ON`, supplying the
qualified cREXX source revision, matching native generated headers, prepared
`llcompiler_llcbrowser_opt` image and SHA-256. This enables
`CREXX_VM_CLASSIC_FULL_PROVIDERS` only in that build. Pass its `runner/` output
as `THE_WASM_RUNNER_DIRECTORY` when enabling `THE_WASM_INTERPRETER` in the
editor build. For compiled RXBIN programs, enable `THE_WASM_CREXX` and build
the editor against the same cREXX runtime revision as the runner. This also
packages `examples/ToolHello.rxbin` for a direct run/import check. The packaged
manifest records the runtime, interpreter image and their hashes. Generated
RXBIN, Wasm and site assets remain build outputs.

The local review candidate pins the interpreter image and runtime to cREXX
`43807b4368cb3dcda49d67c0ac15599cf1e22594`; the reviewed image checksum
is in `cmake/ClassicInterpreterCohort.cmake`. The runner needs matching
generated native headers; the combined editor build also needs the matching
native compiler, assembler and library. A stale version header or prepared
grammar from an older build does not qualify this cohort.

```sh
# CREXX_RUNNER_SOURCE is a clean checkout at the qualified runtime revision above.
cmake -S "$CREXX_RUNNER_SOURCE" -B build/crexx-runner-host -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build/crexx-runner-host --parallel 8
emcmake cmake -S . -B build-wasm-rexx -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DTHE_BUILD_WASM_RUNNER=ON -DTHE_CREXX_SOURCE="$CREXX_RUNNER_SOURCE" \
  -DTHE_CREXX_HOST_BUILD="$PWD/build/crexx-runner-host" \
  -DTHE_WASM_INTERPRETER_IMAGE=/absolute/path/to/llcbrowser.rxbin
cmake --build build-wasm-rexx --target the_wasm_runner --parallel 8
# Add these options to the editor/profile/highlighter configuration above:
emcmake cmake -S . -B build-wasm-classic \
  -DTHE_WASM_CREXX=ON -DTHE_CREXX_SOURCE="$CREXX_RUNNER_SOURCE" \
  -DTHE_CREXX_HOST_BUILD="$PWD/build/crexx-runner-host" \
  -DTHE_WASM_INTERPRETER=ON -DTHE_WASM_RUNNER_DIRECTORY="$PWD/build-wasm-rexx/runner"
cmake --build build-wasm-classic --parallel 8
```

```sh
node web/test/wasm-run-result.mjs
node web/test/wasm-interpreter.mjs build-wasm-rexx/runner
node web/test/wasm-classic-core.mjs build-wasm-interpreter
THE_WEB_URL=http://127.0.0.1:8771 \
CHROMIUM_BIN='/path/to/chrome' node web/test/wasm-browser-smoke.mjs
```

The interpreter check executes the actual Wasm/RXBIN cohort. The browser check
covers Unicode interactive I/O, argument positions, EOF, unsaved working-record
execution, typed failure, Stop/restart, an infinite program and split resizing,
alongside storage, profiles and highlighting when those capabilities are built.
With the editor and interpreter enabled, it also imports and runs the packaged
compiled RXBIN example directly and as a registered program.

## Browser workspace

### Examples

The browser workspace receives `Examples.txt` if absent, with the exact Run steps for
the examples included in that build. The combined candidate includes two
Classic Rexx programs (`RexxWords.rexx`, `RexxLedger.rexx`), two Level G source
files with matching precompiled RXBIN images (`LevelGAccumulator` and
`LevelGFibonacci`), and two additional MiniBASIC programs
(`MiniBASICPrimes.bas`, `MiniBASICStrings.bas`). The example index names the
required runtime and expected output. `LevelBSquares.crexx` demonstrates Level B
source compilation and program execution, printing a sum of 55. Saved workspace copies are never
replaced by an updated site. The optional browser compiler builds THE macro,
profile and `.crexx` sources. Select a Level G source, choose **Build source**,
then **Run with cREXX**. Build captures unsaved edits into an immutable RXBIN;
Run requires that exact source identity and revision, so editing requires a
new build. The program runs in an isolated VM with output in Program console,
separate from macro/profile `ADDRESS THE` execution. Precompiled images run
through **Tools & Profiles > RXBIN program** with an explicit workspace path,
or `RX RUN "/workspace/program.rxbin"`; they are not text editor buffers.

This compiler worker receives one source snapshot and the bundled standard
library/compiler exits. It does not resolve other workspace source modules or
imports. The two packaged Level G examples qualify typed routine and class
execution in this cohort; native plugins and host commands remain unavailable.

The [static ZIP candidate instructions](../packaging/README.md#browser-static-site-candidate)
cover archive validation and local HTTP checks. This package is a review
candidate, not a release or deployment.

The editor restores `/workspace` from IndexedDB before startup. File/Open lists
that workspace. New accepts a workspace-relative filename; intermediate
directories must already exist. THE initially focuses the command line for
an empty file: use `input your first line`, then click its file row to edit.
Opening another saved file adds it to the file ring. The initial
`Welcome.txt` is created only if absent.

**Files → Profile sources** opens editable copies of the bundled browser
profile (`browser.the`) and macro sources. These copies are saved in the
browser workspace and are never overwritten on reload or update, so an older
copy can differ from the installed version. Startup uses the packaged image.
**Tools & Profiles → Edit profile** opens the user profile source. A fresh
workspace creates `/workspace/.crxthe-tools/user.profile.the`; an existing
`profile.the` remains available without renaming. Save that source, then use
**Build profile** and **Apply profile** beside the selected file; the
compiler also accepts unsaved buffer text, but Save is needed to retain the
source across reloads. A successful build creates an immutable artifact whose
name ends in `.profile.rxbin`; it uses the ordinary RXBIN format.
**Tools & Profiles → Apply compiled profile** also accepts a prepared
`.profile.rxbin` in the browser workspace. Import it through the adjacent
control or enter its workspace path. THE validates and copies it to an
immutable private artifact before applying it; the original import is not
opened in the editor or used for later startup. The bytecode must match the
packaged cREXX runtime. A failed load retains the previous profile selection.

Ctrl-S and the Save button write the core's bytes, then wait for `FS.syncfs`
to finish. Only successful synchronization clears the buffer's dirty state.
The storage status reports saving, saved or failure. A failed save retains
the modified buffer and can be retried. Closing an unsaved buffer is refused;
leaving the page prompts when the file ring has unsaved changes. A clean last
buffer closes the editor; reload to reopen the workspace.

Storage belongs to this browser profile and exact origin, including its port.
Reload restores saved files, not unsaved editor sessions. Clearing site data,
using another profile/origin or browser storage eviction can remove those
files. General browser file import/export and direct local filesystem access are later
work; this preview has no native filesystem access. One tab holds an exclusive
workspace lock. A second tab explains the conflict; close the first and reload
the second to take over. Initialization/storage refusal stops startup rather
than silently creating a replacement workspace.

## Verification

After building:

```sh
node web/test/wasm-core-smoke.mjs build-wasm
THE_WEB_URL=http://127.0.0.1:8766 \
CHROMIUM_BIN='/path/to/chrome' node web/test/wasm-browser-smoke.mjs
```

The core check exercises the actual Wasm module, Unicode edits, prefixes,
bundled help, save callbacks and shutdown. With cREXX enabled it also checks
profile startup, a macro shortcut, edited READ stems, cursor restoration and
repeated submit/cancel. The browser check runs those macro cases through DOM
input when the artifact advertises cREXX support. The browser check uses a fresh browser profile, verifies saved
bytes after reload, injects a storage write refusal and checks dirty state and
unchanged durable bytes, retries saving, exercises DOM typing and toolbar
save, selection/Delete and line undo, creates/reopens files, switches buffers,
resizes and rejects a competing
tab. Its debugger instrumentation lives only in the test harness.

See the [remaining browser plan](wasm-browser-plan.md) and
[Emscripten filesystem API](https://emscripten.org/docs/api_reference/Filesystem-API.html)
for the persistence mechanism.
