# Browser WebAssembly preview

The `0.10.dev` editor preview runs the C editor in a dedicated browser Worker,
using the same Preact UI and semantic input/snapshot protocol as the native
web driver. It needs static HTTP hosting, with no native editor server or
WebSocket. Editing, commands, prefixes, selection, Unicode layout, the file
ring, undo and saving stay in the editor core.

The editor/storage build remains available without cREXX. The optional
`THE_WASM_CREXX=ON` build adds a static cREXX VM, a compiled browser profile,
quick-action macros and the public panel example. Profiles use the real
`ADDRESS THE` callback, including EXTRACT and READ stems. The compiler and
assembler run on the build host; the browser accepts precompiled `.rxbin`
macros only. Native OS commands, Regina and native parser processes are
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
runs the core and Chromium checks. It does not deploy a website or make a
release. Chromium is locally qualified; Firefox, WebKit, mobile browsers and
private-browsing persistence remain open qualification work.

## Precompiled cREXX profiles and macros

Use the pinned cREXX source revision from `cmake/CrexxWasm.cmake`. Prepare a
matching native compiler, assembler, library and certified compiler exits
before cross-compiling. For a separate dependency checkout:

```sh
git clone https://github.com/adesutherland/CREXX.git build/crexx-source
git -C build/crexx-source checkout --detach c566025eedc5ef9f48b4c4a8f8f8919fd89793c5
cmake -S build/crexx-source -B build/crexx-host -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build/crexx-host --target rxc rxas library compiler_exit_bin --parallel 8
source build/toolchains/emsdk/emsdk_env.sh
emcmake cmake -S . -B build-wasm-profile -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DTHE_BUILD_WASM=ON -DTHE_WASM_CREXX=ON \
  -DTHE_CREXX_SOURCE="$PWD/build/crexx-source"
cmake --build build-wasm-profile --parallel 8
python3 -m http.server 8769 --bind 127.0.0.1 --directory build-wasm-profile/site
```

`THE_CREXX_HOST_BUILD` defaults to `build/crexx-host`; set it explicitly for a
different location. Configuration checks the source revision, clean VM/tool
sources and generated host version header. Do not mix bytecode or tools from
another cREXX cohort. The bundle records the dependency revision and licenses.
The cREXX source checkout is not modified by the build. The browser adapter
supplies virtual POSIX file lookup and links the shared invocation owner
missing from this revision's optional single-threaded build target.

Ctrl-F and Ctrl-G run the bundled quick-find and quick-line macros. Ctrl-O
keeps the browser workspace file picker; `quickopen.rxbin` is also bundled.
F7 runs `paneldemo.rxbin`: type in its fields, then Enter to submit or
Escape to cancel. These operations execute through the same hosted cREXX API
and editor callback as native THE. The VM can suspend through a nested READ
while the editor Worker awaits browser input. The browser-specific profile
uses the shared default key definitions without native PATH or shell probes.

An explicit `macro /workspace/example.rxbin` can run a prepared bytecode file
in the virtual workspace. Source `.the`/`.rexx` compilation is
unavailable and reports that precompiled bytecode is required. Import/export
UI is still future work. Long-running macros currently need a Worker restart;
cooperative interruption and recovery of unsaved edits remain open.

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
diagnostics. The edited program is never executed. The status bar identifies
**Level L Classic Rexx** and reports `pending`, `ready`, `refused`, `error` or
`too-large`. Click that status to restart the parser without restarting the
editor. Until a current result is ready, that file's parser colours are cleared.
Other file types retain their existing colourers.

The first connection sends complete C-owned logical source snapshots through
DSLSH's existing eight-hex-byte framing. It coalesces unsent revisions and keeps
one ordered request in flight. The service has one replaceable mirror; the host
retains each real document ID and revision. Replies enter C through its input
queue and are checked again against the current revision. Closing a file retires
its identity, so a late reply cannot attach to a reopened file. JavaScript does
not own editor text or apply syntax spans. The C owner uses the shared DSLSH
codec and critical-section helpers, validates bounded coverage and converts
Unicode codepoint spans to its semantic frame/diagnostic coordinates.

This is a full-snapshot connection, rather than the native incremental
CodeBuffer transport. Source is limited to 2 MiB per file, replies to 8 MiB and
200,000 records; requests time out after ten seconds. Active SRCMAP, embedded
NUL and invalid UTF-8 are refused by the parser contract. The status exposes a
refusal instead of silently displaying legacy colours. A later valid edit or a
parser restart can recover. The Classic interpreter is not bundled.

The highlighter cohort is cREXX `67a5e928cddf569595961267bee5d25bf92f267f`
and DSLSH `0ea4db815abd55cdc5db4a5c41539d7d920e9cb3`. These supporting commits
are currently local; the ordinary hosted editor/profile CI remains on its
published cohort. The Classic browser configuration is locally qualified and
requires a prepared linked image from that cohort. Build it using DSLSH's
`crexx/README.md`; THE does not regenerate the grammar. The supplied image
contains its bytecode library closure and needs no external RXPA provider.

Prepare native tools from that cREXX revision as in the profile instructions,
using separate `build/crexx-classic-source` and `build/crexx-classic-host`
directories, then configure:

```sh
emcmake cmake -S . -B build-wasm-classic -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DTHE_BUILD_WASM=ON -DTHE_WASM_CREXX=ON -DTHE_WASM_HIGHLIGHT=ON \
  -DTHE_CREXX_SOURCE="$PWD/build/crexx-classic-source" \
  -DTHE_CREXX_HOST_BUILD="$PWD/build/crexx-classic-host" \
  -DTHE_CREXX_REVISION=67a5e928cddf569595961267bee5d25bf92f267f \
  -DTHE_DSLSH_SOURCE=/absolute/path/to/DSL-Syntax-Highlighter \
  -DTHE_WASM_CLASSIC_IMAGE=/absolute/path/to/dslsh-classic.rxbin
cmake --build build-wasm-classic --parallel 8
node web/test/wasm-classic-framing.mjs
node web/test/wasm-classic-core.mjs build-wasm-classic
python3 -m http.server 8770 --bind 127.0.0.1 --directory build-wasm-classic/site
THE_WEB_URL=http://127.0.0.1:8770 \
CHROMIUM_BIN='/path/to/chrome' node web/test/wasm-browser-smoke.mjs
```

Configuration pins the reviewed image SHA-256 from `cmake/TheWasmHighlight.cmake`.
A rebuilt image with another checksum needs explicit qualification and
`THE_WASM_CLASSIC_SHA256`. The browser verifies that checksum before loading
bytecode. The package records both source revisions, the image checksum and
licenses. `ClassicDemo.rexx` is copied into a new browser workspace only if
absent; open it through File/Open. It demonstrates Unicode, nested comments,
control flow, a continuation and a routine. Existing saved files are preserved.

## Browser workspace

The editor restores `/workspace` from IndexedDB before startup. File/Open lists
that workspace. New accepts a workspace-relative filename; intermediate
directories must already exist. THE initially focuses the command line for
an empty file: use `input your first line`, then click its file row to edit.
Opening another saved file adds it to the file ring. The initial
`Welcome.txt` is created only if absent.

Ctrl-S and the Save button write the core's bytes, then wait for `FS.syncfs`
to finish. Only successful synchronization clears the buffer's dirty state.
The storage status reports saving, saved or failure. A failed save retains
the modified buffer and can be retried. Closing an unsaved buffer is refused;
leaving the page prompts when the file ring has unsaved changes. A clean last
buffer closes the editor; reload to reopen the workspace.

Storage belongs to this browser profile and exact origin, including its port.
Reload restores saved files, not unsaved editor sessions. Clearing site data,
using another profile/origin or browser storage eviction can remove those
files. Browser import/export and direct local filesystem access are later
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
