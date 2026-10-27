# WebAssembly browser implementation plan

## Intended outcome

Run CRXTHE's C editor inside a browser tab, using the existing Preact browser
UI and a static web deployment. The browser build should preserve THE's
commands, file ring, prefix editing, Unicode positions, cREXX integration and
macro panels. The C core remains the owner of editor state.

The first implementation milestone is the editor, shared browser transport and
persistent browser storage. See [the build and user guide](browser-wasm.md).
Work stays on `main`, with version `0.10.dev`.

The agreed sequence is **editor first, cREXX next**. The next macro milestone
needs only the statically linked cREXX VM and precompiled profiles/examples;
compiler and assembler programs remain build-time host tools. Browser source
compilation and parser integration are later, separately scoped work.

## What the current implementation provides

| Component | Source finding | Consequence for the port |
| --- | --- | --- |
| Browser UI | [main.tsx](../web/src/main.tsx) sends normalized messages and renders semantic snapshots. Its transport is currently a WebSocket. | Reuse the UI and protocol shapes; extract a transport interface supporting native WebSocket and browser Worker implementations. |
| Native web driver | [webdriver.c](../src/drivers/web/webdriver.c) combines Mongoose serving, JSON decoding, input queues, file discovery and snapshot publication. | Separate reusable session behavior from native networking. The Wasm driver uses worker messages rather than a local HTTP server. |
| Driver model | [thedriver.c](../src/thedriver.c) has static driver selection for tests as well as native module loading. The normal build creates curses and LLM modules unconditionally. | Add a statically linked Wasm driver and an explicit browser build configuration; bypass native driver discovery and curses discovery. |
| Editor execution | [edit.c](../src/edit.c) loops around `process_key`; [panel.c](../src/panel.c) also waits for input inside READ. | Input suspension must work through both ordinary editing and nested modal/macro calls. Replacing only the outer editor loop is insufficient. |
| File saving | [comm4.c](../src/comm4.c) clears alteration counters after `save_file` succeeds. | Browser persistence must complete before a save is reported successful and unsaved state is cleared. |
| Macro hosting | [crexx.c](../src/crexx.c) supports `.rxbin` and source macros. [crexxloader.h](../src/crexxloader.h) loads a native shared runtime. | Link the browser runtime statically and retain ADDRESS THE callbacks and variable exchange. |
| cREXX tool invocation | The sibling cREXX `interpreter/crexxsaa.c` invokes native compiler and assembler programs for source macros. | Source compilation needs a browser tool invocation API, not native subprocess emulation. |
| Parser integration | DSLSH `codebuffer/comms.c` launches parser processes and communicates through pipes; its codebuffer also has thread support. | Browser parsing needs an adapted transport and scheduling model. Existing native parser executables cannot simply be packaged as browser assets. |

The investigation used THE `0affb89`, cREXX `c566025` and DSLSH `4a34228`.
The cREXX source already contains static runtime libraries and
`CREXX_VM_SINGLE_THREADED` paths that may help the port. These are starting
points, not an established Wasm target. `NTHREADED` selects VM switch dispatch
and does not by itself remove OS-thread dependencies. Preserve cREXX's
64-bit integer and UTF-8 contracts; the mainframe byte-string profile is a
different target. Emscripten is not currently installed in this workspace.

## Proposed architecture

```text
Preact UI on the browser main thread
             |  events and snapshots through worker messages
             |
Dedicated Worker with one editor execution context
  THE core + static Wasm driver + utf8proc + cREXX runtime
             |
  virtual workspace, bundled resources and durable browser storage

Compiler/assembler and parser workers are added through explicit adapters.
```

The Worker owns editor globals. UI messages enter a JavaScript queue and are
consumed by the driver; snapshots and action/file catalogs return through
`postMessage`. JavaScript must not call back into an executing or suspended
editor to mutate its state. Keep the existing input dispatcher, command
policy, action registry, file ring and snapshot producers as shared owners.
Use a single editor per Worker and serialize commands and persistence.

### Build and runtime

Use Emscripten's `wasm32` C/CMake toolchain with pinned tool and dependency
versions. A proposed `THE_BUILD_WASM` configuration should select the browser
target before native dependency discovery. Factor shared source lists and web
asset generation so the Wasm build does not link curses, Mongoose, native
launchers, shared runtime loaders or platform installers.

Register the static driver on the browser startup route so it retains the
restricted browser profile and action policy. The LLM startup route currently
enters a stdin protocol loop and is not the browser runtime entry point.

Start with static linkage and one execution thread in a dedicated Worker.
Emscripten supports dynamic linking, but recommends avoiding it for most
projects; static linkage also avoids maintaining separate browser driver and
runtime module ABIs during this port.
[Emscripten build guidance](https://emscripten.org/docs/compiling/Building-Projects.html#dynamic-linking).

Evaluate Asyncify at the driver's asynchronous input wait first. It can
suspend the existing synchronous C call chain, including a macro waiting in
READ. Test indirect calls, nested ADDRESS THE callbacks, Escape/cancel and
save before accepting this design. JSPI can be evaluated as an optimization
for supported browsers. Emscripten documents both the code-size tradeoff and
the reentrancy constraints of suspension.
[Asynchronous execution](https://emscripten.org/docs/porting/asyncify.html).

Avoid requiring pthreads or SharedArrayBuffer in the baseline package.
Emscripten pthreads require cross-origin isolation headers, which would make
deployment more demanding. Existing cREXX/DSLSH concurrency facilities must be
audited and adapted rather than assumed absent because the editor has one
Worker. [Pthreads deployment requirements](https://emscripten.org/docs/porting/pthreads.html).

### Files and persistence

Give THE a POSIX-style virtual workspace. Bundle profiles, help, syntax data
and matching bytecode separately from writable files. Import files through
the browser picker or drag and drop, and provide download/export everywhere.
Offer direct local-file access as a capability when supported.
[File picker availability](https://developer.mozilla.org/en-US/docs/Web/API/Window/showOpenFilePicker).

Use MEMFS with an IDBFS-backed workspace as the first persistence candidate:
restore it before editor startup and explicitly await storage synchronization
on saves. Add a small host persistence hook in the common save path, before
alteration counters are cleared. Cover SAVE, Ctrl-S, save-and-close and saves
issued by macros. Quota, permission and storage failures must retain an
unsaved indication and keep the editor buffer available.
[Emscripten filesystem and synchronization](https://emscripten.org/docs/api_reference/Filesystem-API.html).

Distinguish saving the browser workspace from exporting a file to the user's
disk. Browser shutdown cannot be the only persistence mechanism. Qualify
reload, denied storage, private browsing and concurrent tabs. Start with one
writer per workspace and an explicit conflict response.

Evaluate OPFS after the initial save contract works, especially if memory
duplication or large files justify it. Synchronous OPFS access is available
inside dedicated Workers, but opening a sync handle is itself asynchronous
and it does not expose arbitrary user-visible files.
[OPFS Worker access](https://developer.mozilla.org/en-US/docs/Web/API/FileSystemSyncAccessHandle).

### cREXX and parsing

Build the embedded VM and required native function libraries as Wasm/static
components, preserving the existing hosted API. Prove matching bytecode can
be loaded and run under `wasm32`; do not assume host pointer-sized structures
or generated artifacts are interchangeable.

Precompile browser profiles, quick actions and panel examples using a pinned
native compiler/assembler cohort. Supply a browser-specific profile that
does not probe PATH or run shell commands, and explicitly select its compiled
startup form. Validate the standard key bindings
and ADDRESS THE variable exchange against the native runtime.

For user source macros, port `rxc` and `rxas` as callable Wasm tools and replace
the hosted runtime's subprocess invocation with a tool adapter. Define source,
imports, generated bytecode, diagnostics, status and cancellation explicitly.
Compile tool instances in their own execution context where necessary;
transfer files through controlled virtual filesystem operations. Keep cache
keys tied to tool/runtime versions.

For DSLSH, retain its buffer and span contracts while replacing native process
transport with browser messages or an in-process parser adapter. Begin with
the cREXX parser. Apply asynchronous results only to the matching buffer
revision. Additional native parser programs need their own browser builds;
report their availability accurately.

Native OS commands, native plugins and Regina are separate capabilities, not
implicit browser features. Unsupported operations must return clear errors.
Long-running macros and compilation must leave the UI responsive and provide
a defined interruption/recovery route. Worker termination alone is not a
substitute for cooperative cancellation and unsaved-file recovery.

## Implementation steps

1. **W1 Cross-build and execution proof (editor implemented).** Pin Emscripten; inventory native
   dependencies and imported symbols; prove static core startup, one input,
   one snapshot and shutdown. The next cREXX milestone must prove bytecode
   execution and the nested macro/READ suspension path. Record the chosen async mechanism,
   required dependency changes, module sizes and peak memory. Exit with a
   working technical proof and a dependency list, not a product-completion claim.
2. **W2 Shared browser transport (editor implemented).** Extract UI transport and native web-session
   behavior; add Worker transport and static Wasm driver. Serve a local static
   bundle and reuse current file, key, prefix, Unicode, selection, resize and
   panel scenarios against the real C core. Panel qualification follows the
   cREXX VM. Preserve the native web route.
3. **W3 Usable workspace and hosted profiles (storage implemented).** Package matching cREXX runtime
   and precompiled browser profiles/examples. Implement import/export,
   persistent workspace restoration and save acknowledgement. Verify restart,
   failure and clean/dirty state through both direct and macro save routes.
4. **W4 Optional source macros and parser integration (outside the requested milestones).** Add browser compiler/assembler
   invocation, source cache and cREXX parser transport. Cover compilation
   errors, repeated execution, buffer switching, stale parse results, modal
   input and cooperative interruption. A precompiled-only milestone must
   advertise that limit until this step passes.
5. **W5 Qualification and delivery.** Build the static artifact in CI from
   `main`; run browser tests in Chromium, Firefox and WebKit, retain native
   regression coverage, and verify deployment paths and failure behavior.
   Serve Wasm with the correct MIME type and verify worker loading and the
   deployment's content security policy, including permission to compile Wasm
   without enabling general JavaScript evaluation. See
   [CSP and Wasm](https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Headers/Content-Security-Policy/script-src#unsafe_webassembly_execution).
   Measure compressed download size, startup and representative file memory
   after correctness is established. Publish only when separately authorized.

The editor build uses Emscripten 6.0.13, Asyncify and IDBFS in one Worker.
The shared session code retains native WebSocket behavior. Core startup,
editing, Unicode bytes, save callbacks and shutdown pass; Chromium covers
IndexedDB restore, failed-save dirty state and unchanged durable bytes, retry,
UI file creation/reopening, file-ring switching, resize and exclusive tab
locking. Native qualification passes all 86 tests, including pending-record
dirty reporting. The static bundle
includes dependency metadata/licenses and has a `main` CI artifact workflow.
The cREXX bytecode/READ proof, import/export, broader browser matrix and parser
integration remain open. This editor milestone does not claim those gates.

The cREXX investigation found a reusable single-threaded VM target, but native
filesystem selection and the embedding source closure need qualification for
Wasm. No cREXX source changes are shipped in this milestone.
Changes needed in cREXX and DSLSH belong in their owning repositories, using
their instructions and pinned compatible revisions. No dependency checkout is
silently patched or mixed with an unrelated runtime cohort.

## Acceptance criteria

- **A1 Execution:** static assets start THE locally in a browser Worker and
  editing does not require a native editor server or localhost WebSocket.
- **A2 Editing parity:** shared scenarios assert text, cursor, prefix state,
  selection, file ring and dirty state, including Delete/Backspace, retained
  tabs, combining clusters, wide characters and panel return.
- **A3 Saving:** saved bytes survive reload; save/export destinations are clear;
  persistence failures preserve unsaved state. Test LF/CRLF, trailing spaces,
  empty files, rename/save-and-close, macro saves and storage refusal.
- **A4 Macros:** shipped profiles and cREXX panels work in the browser. Source
  compilation, diagnostics, repeated execution and ADDRESS THE variable
  exchange pass before source-macro support is declared complete.
- **A5 Parsing:** browser cREXX styles and diagnostics track the correct buffer
  and revision. Missing additional parsers are exposed as unavailable.
- **A6 Lifecycle:** modal waits yield correctly; long tasks can be interrupted;
  closing, restarting, failed initialization and storage conflicts have tested
  recovery behavior without an unresponsive UI.
- **A7 Compatibility:** the agreed browser matrix passes with import/export
  fallback and without mandatory SharedArrayBuffer. Native console, LLM and
  browser regression suites continue to pass.
- **A8 Delivery:** CI emits a versioned static bundle with pinned dependency
  metadata, licenses, browser capability notes and deployment instructions.

A1 is implemented and checked in Chromium. A2/A3/A6/A8 have the editor/storage
coverage described above; their broader cases remain open. A4/A5/A7 remain
open. Full implementation is complete when the agreed criteria pass; an editor-only linker proof or a native server displayed in a browser
does not satisfy this outcome.
