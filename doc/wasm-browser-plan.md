# WebAssembly browser implementation plan

## Intended outcome

Run CRXTHE's C editor in a browser Worker with the existing Preact UI and a
static web deployment. The C core owns file bytes, cursor/focus, commands,
selection, parser state and macro integration. Development stays on `main`,
with version `0.10.dev`. See the [build and user guide](browser-wasm.md).

The agreed sequence is **editor first, cREXX next**. Compiler and assembler
programs remain build-time host tools. The next parsing proof uses a
precompiled Level L Classic Rexx highlighter running in RXVM; it does not need
a browser compiler or execution of the user's Rexx source.

## Implemented foundations

| Component | Implementation and checked behavior |
| --- | --- |
| Browser UI | Shared normalized input and semantic snapshots; native WebSocket and browser Worker transports. |
| Editor execution | Static Wasm driver and shared web-session owner; Asyncify input waits preserve the synchronous editor call stack. No curses, Mongoose or native server in the browser bundle. |
| Workspace | IDBFS restore before startup, explicit synchronization before dirty state clears, failure/retry and one writer through Web Locks. |
| cREXX profiles | Optional static SAA/VM build with a precompiled browser profile generated from the shared defaults. The actual ADDRESS THE callback handles commands and EXTRACT stems. |
| Macro panels | The public panel example suspends inside ADDRESS THE READ, receives field edits through the READ stem, resumes on Enter/Escape, restores cursor/settings and runs repeatedly. |
| Generic VM host | Separate Worker process owning one RXVML context, loaded RXBIN buffers, raw stdin/stdout/stderr, explicit EOF and separate runtime/program status. Temporary starvation suspends rather than ending input. |
| Dependency cohort | Emscripten 6.0.13, pinned utf8proc and cREXX revisions; native compiler/assembler/generated headers/bytecode kept together. Artifact metadata and dependency notices are included. |

The macro VM shares the editor Worker because ADDRESS THE calls the editor
synchronously and can wait inside nested READ. The parser VM has its own
Worker so grammar work can proceed independently. JavaScript messages never
reenter a running or suspended C owner; they only queue input for that owner.
The VM build uses the maintained single-threaded leaf, not native OS workers,
sockets, clocks or dynamic plugins. NTHREADED selects switch dispatch; it is
not by itself a guarantee of a single-threaded runtime.

The cREXX leaf at the pinned revision needs its shared invocation source added
to the consumer target, plus virtual POSIX file lookup for the profile route.
Those adapters live in THE; the dependency checkout is not silently patched.
The standalone buffer/stream host needs no filesystem lookup adapter.

## Next: Classic Rexx highlighting over VM streams

The cREXX/DSLSH owner is preparing a maintained Classic frontend adapter and a
nontrivial example from the existing Level L grammar. Reuse its contextual
classification, source mapping and parser observations; do not create a second
Classic grammar in THE. Highlighting must parse source without executing it.
The Classic interpreter can later use the same stream host with its additional
provider requirements qualified separately.

1. **Qualify the bytecode closure.** Pin matching cREXX and DSLSH revisions and
   prebuild the highlighter plus its exact libraries. Inspect the link manifest
   for native providers. Include only required VM/static components. Prove the
   actual wasm32 closure; a native RXBIN run does not establish that.
2. **Adapt the existing service transport.** Feed DSLSH's byte framing through
   the binary VM pipes. Keep framing independent of output chunk boundaries,
   handle partial reads, explicit EOF, diagnostics, refusal and process failure.
   Reuse `dslshsession.service.handle` and its existing stdio protocol instead of
   inventing a browser grammar interface.
3. **Correlate results in the host.** Current service replies do not carry a
   document/revision envelope. Serialize requests per parser instance and retain
   their buffer ID and revision in the host. Apply only matching results; do not
   advance the mirror on service failure or refused edits. Hypothesis requests
   remain noncommitting.
4. **Keep coordinate conversion in the core.** The service's positions are
   zero-based Unicode codepoints after the Level L byte-to-codepoint mapping.
   THE converts these to its own byte/cell/layout coordinates. Browser rendering
   must not reinterpret parser positions or own the authoritative source text.
5. **Integrate buffer lifecycle.** Route parser results back to the editor owner
   through queued inputs and existing DSLSH critical sections. Cover edits,
   switching/closing buffers, parser restart, stale replies and a grammar error
   followed by correction. Keep the editor usable while the parser waits.

The current generic VM bundle proves byte streams and lifecycle only. It does
not yet provide browser highlighting or bundle the Classic interpreter.

## Remaining browser work

- Import/export UI and filesystem picker fallback; distinguish workspace save
  from exporting to the user's disk. Qualify LF/CRLF, trailing spaces, empty
  files, rename and save-and-close, including saves issued by macros.
- Cooperative interruption and unsaved-edit recovery for long-running macros.
  Terminating a Worker resets it; that alone is not a recovery design.
- User source macros, if separately requested: callable compiler/assembler
  instances, controlled virtual files/imports, versioned caches, diagnostics and
  cancellation. Precompiled-only support must remain explicit until then.
- Firefox/WebKit/mobile/private-browser persistence, subdirectory hosting,
  offline behavior, restrictive CSP deployment, startup/download/memory measures.
  The baseline needs no pthreads, SharedArrayBuffer or cross-origin isolation.
- Native OS commands, Regina and native plugins require separate capabilities;
  none is implied by a browser VM.

## Acceptance and delivery

| Gate | Current evidence / remaining qualification |
| --- | --- |
| Execution | Actual Wasm editor starts and edits from static assets without a native server/WebSocket. |
| Editing | Unicode, prefixes, selection/Delete, undo, file ring, help and resize covered; macro READ submit/cancel and state restoration covered in the cREXX build. |
| Storage | Saved bytes survive reload; refusal retains dirty state and old durable bytes, retry succeeds; competing tab refused. Broader filesystem cases remain above. |
| Profiles | Compiled startup profile, EXTRACT stem, macro shortcut and edited READ stem checked in Wasm. Source compilation remains unavailable. |
| Stream VM | Actual RXBIN execution, binary NUL/UTF-8/255 bytes, delayed/partial reads, separate stderr, EOF, I/O failure, failed-load isolation and restart covered. |
| Parsing | Classic highlighter closure, DSLSH transport and correct-buffer/revision application remain open. |
| Lifecycle | Input and nested READ yield correctly; storage/init refusal and VM restart are checked. Cooperative cancellation/recovery remain open. |
| Compatibility | Chromium qualified; broader browser matrix remains open. Native console/LLM/web regressions must continue passing. |
| Delivery | CI builds static preview artifacts from main with metadata/licenses; no website deployment or release publication is implied. |

Run the focused Node/Wasm and Chromium checks in the build guide, followed by
native candidate tests for shared-source changes. Do not rebuild a runtime
while its tests are running. Changes owned by cREXX/DSLSH stay in those
repositories and must be pinned together before adding the highlighter to the
browser package.
