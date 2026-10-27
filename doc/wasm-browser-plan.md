# WebAssembly browser implementation plan

## Intended outcome

Run CRXTHE's C editor in a browser Worker with the existing Preact UI and a
static web deployment. The C core owns file bytes, cursor/focus, commands,
selection, parser state and macro integration. Development stays on `main`,
with version `0.10.dev`. See the [build and user guide](browser-wasm.md).

The agreed sequence is **editor first, cREXX next**. Compiler and assembler
programs remain build-time host tools. The optional parser connection uses a
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
| Classic highlighting | Optional Level L VM/DSLSH connection with C-owned source/revision checks, Unicode spans, located diagnostics and parser restart. |
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

## Classic Rexx connection

The optional browser highlighter is implemented using the maintained Level L
Classic frontend and DSLSH adapter. Its linked RXBIN contains the parser and
library closure, with no external RXPA provider. Source is parsed without
execution. The macro VM still shares the editor Worker; the parser VM owns a
separate Worker and binary stdin/stdout/stderr.

The first connection sends bounded full snapshots and coalesces unsent changes.
One ordered request at a time replaces a single DSLSH service mirror. The
transport remembers the C-generated document/revision envelope until the reply
arrives. C admits only the current revision, uses the shared wire codec under
DSLSH critical sections and converts spans/diagnostics to its own coordinates.
Closing/reopening retires identity. Pending, refused or failed parses clear old
colours; restarting the parser resubmits owned snapshots without changing file
bytes or restarting the editor.

The browser does not yet use the native C CodeBuffer delta/emergency-parsing
transport. Incremental edits and hypothesis requests are later work, preserving
the existing version/base/refusal contracts. Full-snapshot bounds, local cohort
pins, prepared-image requirements and test commands are in the
[build guide](browser-wasm.md#level-l-classic-rexx-highlighting).
The supporting cREXX/DSLSH commits remain local, so hosted qualification of this
optional configuration follows their authorized publication. The ordinary
editor/profile workflow continues using its published cohort.

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
| Parsing | Actual Classic VM/DSLSH parsing, Unicode styles, diagnostics/repair, pending-record edits, stale-result rejection, buffer identity, refusal, restart and reopen pass locally in Wasm/Chromium. Incremental transport remains open. |
| Lifecycle | Input and nested READ yield correctly; storage/init refusal and VM restart are checked. Cooperative cancellation/recovery remain open. |
| Compatibility | Chromium qualified; broader browser matrix remains open. Native console/LLM/web regressions must continue passing. |
| Delivery | CI builds static preview artifacts from main with metadata/licenses; no website deployment or release publication is implied. |

Run the focused Node/Wasm and Chromium checks in the build guide, followed by
native candidate tests for shared-source changes. Do not rebuild a runtime
while its tests are running. Changes owned by cREXX/DSLSH stay in those
repositories; the optional highlighter package pins them together and records
the prepared image checksum.
