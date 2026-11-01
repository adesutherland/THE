# Profile-built IDE scope baseline

**Status: fixed first-milestone scope; implementation is incomplete.** Change
this baseline only through an explicit scope decision. This is the finish line
for one small IDE, rather than a list of every possible tool.
The [workbench contracts](workbench-extension-design.md) and implemented guides
remain authoritative for current commands and limits.

## Product rule and reference surface

Rexx profiles and macros assemble the IDE from bounded THE capabilities. The
core owns files, edits, syntax results, actions, panes, jobs and authority;
drivers render the published objects. The host keeps a minimal editor, command
entry, status/key guide and recovery Menu if a profile fails. The stock IDE
profile supplies the File, Run and View menus, chooses action IDs for the
visible launcher strip, and configures the Files explorer, Program console,
tool actions, output panes, language associations and key bindings. The host
supplies the bounded explorer/console capabilities, rather than implementing
them as Rexx widgets. Declaring objects never executes a tool. A user profile
can replace or extend its own definitions without changing frontend source.

**Use browser/WebAssembly as the first reference user experience** for the
guided workbench, including wide and phone layouts. Its visual arrangement is
the first complete journey to review. The shared C object and command contract,
not browser markup, is the semantic authority. GTK and terminal present the
same IDs, operations, results and refusals with native controls and terminal
navigation. Their filesystem/process capabilities can be broader than Wasm's.
The LLM driver exposes the same objects and outcomes as data and commands.

## Required first-version journeys

1. Open or create a file; edit Unicode and a pending record; undo, switch,
   save and reopen with identical bytes. Keep THE's prefix, file ring, command
   line, profile status and key guide visible and meaningful.
2. Activate the stock Rexx IDE profile. It creates the menus, Run action,
   output presentation and language association. Replacing/reloading/removing
   an owner's definitions neither duplicates controls nor runs anything;
   failed activation leaves the previous published composition usable. One
   explicitly activated user profile must add a safe menu item backed by a
   user-authored macro. The browser journey edits that macro's source, builds
   a matching RXBIN image, then invokes it from the menu without
   changing frontend source or gaining host privileges.
3. Highlight Classic Rexx and report real parser diagnostics, including an
   explicit unavailable state when its service is absent. Demonstrate the
   registered highlighter contract with DSLSH's public `toyparser/test.toy`
   fixture on a qualified native build. A DSL runner is a separately registered
   tool in a later increment; syntax association does not execute code. No new
   grammar is required for this baseline.
4. Run the current unsaved Classic Rexx buffer and the bundled, matching
   `examples/ToolHello.rxbin` utility from profile-defined actions. Capture
   source identity and revision. Show stdout, stderr, completion and errors;
   provide separate program input, EOF, Stop and output hide/reopen. Preserve
   the reader's scroll position as output arrives; provide a return-to-latest
   control and unread indication. Continued editing must not change the
   running snapshot.
5. Show a named log and read-only live document, and stage/submit one named
   form. Existing CTLCHAR file-field panes remain an optional profile feature;
   Apply uses the core edit group and Save remains explicit.
6. Complete these operations from keyboard on terminal and GTK, and from
   keyboard, pointer and touch in the browser. LLM inspection and typed
   requests observe the same state. Unsupported actions remain visible with a
   precise reason rather than silently choosing another tool.

One real browser, GTK and PTY journey must exercise the stock profile's
edit/highlight/run/output workflow. Semantic tests must also check text,
cursor, dirty state and saved bytes. These are development-source gates;
installed packages, other OS/browser cohorts and release publication have
separate qualification.

## Browser files and execution

| Source or target | Current WebAssembly build | First-version baseline |
| --- | --- | --- |
| Browser workspace | `/workspace` is IDBFS backed by IndexedDB. Save synchronizes it; saved files survive reload for the same origin/browser profile, subject to browser storage retention. | Keep it as the default file provider and show storage failures without claiming Save succeeded. |
| User's local files | Only RXBIN has a narrow file-picker import. General source import/export and direct file handles are absent. | Add single-file UTF-8 source import and explicit export/download, bounded to 2 MiB, so a source can leave browser storage. Preserve bytes, reject name collisions and never access local disk silently. |
| Server files | The Wasm static site has no server file protocol. The separate native WebSocket browser driver is a different host. | Defer a specific connector until a real service and its authorization/conflict contract are selected. |
| Program execution | In optional interpreter-enabled bundles, Classic source runs in an isolated Worker; matching RXBIN images require the corresponding cREXX runtime and runner bundle. The base editor/storage build has neither run target. These Workers do not mount the editor workspace or address THE. | Ship a qualified interpreter-enabled bundle with Classic and `ToolHello.rxbin` as browser run targets. Another Wasm tool needs an explicit adapter; a server tool needs an explicit service/job adapter. |

Browser APIs can support user-selected file handles on implementations that
provide File System Access, or an origin-private store such as OPFS. Neither is
currently THE's general workspace provider. Any future handle adapter needs
explicit user selection, permission checks and conflict handling; it is not
part of this cross-browser baseline. A browser can call a server over an
authorized HTTP or WebSocket protocol, subject to origin/network rules, but
no such Wasm file or execution service exists in this source tree.

The browser must also close a **profile composition gap** before this baseline
is complete. Its Files, Console and Run Rexx controls are partly hard-coded;
their visibility, labels and actions must derive from published profile IDs.
Current Wasm activation refuses workbench transactions from newly built
workspace profiles; restricted sessions refuse arbitrary COMMAND and MACRO
workbench actions. Move the stock controls into a reviewed bundled profile,
add typed browser-safe tool/console actions, and permit explicitly activated
user profiles to publish only capability-checked objects. A user macro action
must name a matching workspace RXBIN image and be admitted at invocation under
the same restricted editor policy. The browser must offer a matching build path
for authored macro source; explicit RXBIN import remains an alternative. An
imported or compiled profile/macro must
not become a trusted bootstrap or gain OS, local-file or network authority.
Reject incompatible action definitions without partially publishing a profile;
deny unsafe addressed commands at invocation without expanding host authority.
This macro-action policy and its qualification are part of the first-version
gap; bundled, reviewed macros
remain available for the stock workflow.

## Desktop and deferred capabilities

Terminal and GTK share the native editor and tool host. They should have the
same command, profile, tool, file and job semantics; terminal uses compact
right/below/tab presentation and explicit keyboard traversal, while GTK uses
native menus/choosers and a resizable layout. Current native POSIX FILE routes
support finite byte stdin and one atomic CREATE output. An explicit native
process source tool is qualified locally on macOS arm64; this does not qualify
Linux, Windows, GTK packaging or installed ooRexx. Its source snapshot does not
preserve the original file directory for relative includes.

This first version does **not** include a debugger protocol/UI, multiple
concurrent jobs, arbitrary pane tiling, automatic server sync, arbitrary shell
or network commands from browser macros, direct local-directory editing,
FILE REPLACE/APPEND, FILE-path DOCUMENT snapshots, general SOURCE FILE runs,
new DSL grammar development, or release packaging. A future debugger may use
the same tool/action/pane model but needs its own pause, step, breakpoint and
variable protocol; captured stdout alone does not constitute debugging.

Primary browser platform references: [File System Standard](https://fs.spec.whatwg.org/)
for origin-private storage, [File System Access](https://wicg.github.io/file-system-access/)
for user-selected local handles, and [Fetch](https://fetch.spec.whatwg.org/)
and [WebSockets](https://websockets.spec.whatwg.org/) for explicit server links.
