# Browser WebAssembly preview

The `0.10.dev` editor preview runs the C editor in a dedicated browser Worker,
using the same Preact UI and semantic input/snapshot protocol as the native
web driver. It needs static HTTP hosting, with no native editor server or
WebSocket. Editing, commands, prefixes, selection, Unicode layout, the file
ring, undo and saving stay in the editor core.

This first milestone provides the editor and persistent browser storage.
cREXX profiles/macros and external parser processes are unavailable. The
bootstrap command profile is generated from `profiles/default-profile.json`;
macro shortcuts are replaced with command-line focus. The next milestone adds
only the cREXX VM and precompiled bytecode. A browser compiler is outside that
milestone.

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
bundled help, save callbacks and shutdown. The browser check uses a fresh browser profile, verifies saved
bytes after reload, injects a storage write refusal and checks dirty state and
unchanged durable bytes, retries saving, exercises DOM typing and toolbar
save, selection/Delete and line undo, creates/reopens files, switches buffers,
resizes and rejects a competing
tab. Its debugger instrumentation lives only in the test harness.

See the [remaining browser plan](wasm-browser-plan.md) and
[Emscripten filesystem API](https://emscripten.org/docs/api_reference/Filesystem-API.html)
for the persistence mechanism.
