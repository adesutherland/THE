# Extensible THE workbench: staged design and frozen contract

**Status: phase 1, phase-2A LOG, phase-2B live DOCUMENT, phase-3A forms,
phase-3B edit groups, phase-3C CTLCHAR projections and bounded phase-4 FILE/native
source-tool increments are implemented in development source. The complete
phase-4 gate and later extensions remain separate.** Use [shared workbench actions and menus](workbench-actions-menus.md)
for the implemented grammar, limits, transactions and surface behavior. This
document retains the broader frozen contract and staged design; it does not
claim that later commands, registries or protocol extensions are implemented or
qualified. The subsequent order is named log/read-only document panes, forms
and editable fields, then qualified file IO and ooRexx adapters. Implementation,
platform qualification and release publication remain separate gates. This
extends the current editor and Program console into a profile-configurable
workbench; it does not replace THE with a different editor or make a browser
layout authoritative.

Phase 1 provides the shared C action/menu/item registry, owner-bound definition
transactions, outer profile publication, QUERY/EXTRACT pages and explicit
builtin/command/macro invocation. Terminal, GTK, browser/Wasm and LLM adapters
consume the published definitions. Later increments below add their own bounded
contracts; file-backed DOCUMENT snapshots remain deferred. Existing RX tools
and console behavior retain their own interfaces.
The [phase-2A LOG guide](workbench-log-panes.md) describes transactional LOG
panels, runtime presentation requests, singleton-job routes and bounded content
inspection. The [phase-2B DOCUMENT guide](workbench-document-panes.md) describes
immutable live-buffer bindings, conservative revision tokens and copied bounded
pages, including safe pending records and close/reopen isolation. FILE-path
snapshots remain deferred; physical frontend qualification is a separate gate.
The [phase-3A form guide](workbench-forms.md) describes independent staged values
and submitted results. The [phase-3B edit-group guide](workbench-edit-groups.md)
describes conditional same-line apply/undo, its separate write token, and the
native parser pause boundary. The bounded [CTLCHAR file-field projection](workbench-file-fields.md)
uses that service for Apply; the conservative read epoch is not an apply token.
The initial [native FILE route increment](workbench-file-io.md) implements
exact-byte finite stdin and one staged atomic `CREATE` sink on native POSIX.
It is limited to the canonical startup workspace and refuses every output
target open in the editor. `REPLACE`, `APPEND`, Windows and browser file routes
remain unavailable; the complete phase-4 acceptance gate is still separate.
The [explicit native source-tool guide](workbench-native-tools.md) describes
transactional TOOL definitions/actions and RUN, qualified on macOS arm64 with
process-exit-only completion. Other native hosts and Wasm refuse execution;
restricted native browser sessions retain their policy refusal. The macro
engine and legacy RXBIN registry remain independent.

The implementation's invocation generation guard uses `INVOKE id EXPECT n`;
macro argument vectors are not implemented in phase 1. Examples and wider object
models below must be read with those boundaries.

## Objective and finish line

A profile or explicitly invoked macro should be able to add/remove menus and
items, expose tools, show/hide additional panels, request a right-hand pane,
bind a file to a field-oriented view, and choose program IO destinations. The
same definitions must have usable terminal, GTK, browser/Wasm and LLM
representations. Classic Rexx, ooRexx, RXBIN, DSLSH and future tools use adapters
with explicit capabilities, rather than special cases scattered through each
frontend.

The first complete workbench milestone is reached when a public profile can
define a Tools menu,
run the existing Classic runner, display its isolated streams in a named pane,
open a read-only file pane and a submitted field form, and remove those objects
without changing editor bytes, cursor, key guide or status definitions. A later
delivery adds editable file-field projections and qualified ooRexx/native
process adapters. Multiple panel definitions do not initially require multiple
concurrent program jobs.

## Existing contracts and gaps

These are source contracts inspected in the current tree, not new platform or
release qualification. Line references identify the relevant definitions.

| Existing behavior | Evidence | Design consequence |
| --- | --- | --- |
| Core owns file bytes, logical cursor, commands, selection, parser and macro integration; drivers own physical UI. | [Engineering contract](../AGENT_README.md), [architecture](architecture.md#core-editor-model). | New panels and actions are C session objects. No driver or tool owns a second editable copy of the file. |
| Frontend actions are a fixed registry for create/open/save/close/switch/undo; file actions become real THE built-ins, deliberately bypassing synonyms after action policy checks. Arguments reject quotes/control characters. | [`frontendaction.c:7–16,18–30,118–148`](../src/frontendaction.c#L7), [`inputdispatch.c:25–58`](../src/inputdispatch.c#L25). | Extend a typed action boundary; keep Save/Open independent of user aliases and do not send menu labels or unvalidated command interpolation to the dispatcher. |
| Tool definitions, requests, adapter callbacks and one console are C-owned. Requests carry source ID/revision, argv and source bytes. | [`toolhost.h:12–45`](../src/toolhost.h#L12), [`toolhost.c:9–15,105–150`](../src/toolhost.c#L9). | Keep the current host as the initial execution provider. The singleton is an explicit first-phase limit. |
| A second job is rejected; adapters supply start/input/EOF/stop/poll. Output admits only the current running job ID and fd 1/2, with bounded retention and a hard output limit. | [`toolhost.c:105–150,165–208,257–265`](../src/toolhost.c#L105), [`toolhost.h:6–10,25–31`](../src/toolhost.h#L6). | Panel removal must not become implicit Stop; later concurrency requires a job table and job-qualified events, not duplicated singleton state. |
| Source capture includes the pending working record, retains identity/revision, and refuses capture during macro/panel execution. | [`editorsource.c:49–88`](../src/editorsource.c#L49), [`tools.c:123–128,145–195`](../src/tools.c#L123). | Preserve unsaved execution without Save. Queue eligible actions at safe editor boundaries with a bound target identity. |
| `RX` supplies run, register, profile/build/activate, language, input/EOF/stop/clear/status and console visibility. UI payloads use owned pending requests rather than interpolated argv. | [`tools.c:80–128,341–551`](../src/tools.c#L80), [runtime tools](runtime-tools.md). | Keep `RX` compatibility; general presentation commands must not imply a Rexx-only application model. |
| Phase-1 menus share one C-owned action/menu/item registry; terminal, GTK and browser adapters lower the published graph. The generated popup remains a legacy fallback. | [Implemented workbench guide](workbench-actions-menus.md), [registry API](../src/workbench.h), [shared profile definition](../profiles/default-profile.json). | Keep later panel/tool presentation on the same session ownership boundary. |
| `READ` forms use shared editable/protected field state and preserve caller cursor/focus. `READ ALL` stages file replacements before committing; NOCHANGE/cancel retain the file. | [`panelui.h:7–37`](../src/panelui.h#L7), [`panel.c:232–324,327–365`](../src/panel.c#L232), [macro panels](panels.md). | Reuse field/input semantics. A persistent field pane needs explicit revision, commit and lifetime rules beyond current modal READ. |
| Driver ABI 3 exposes low-level windows/input and one optional workbench focus callback; it has no general menu/panel capability registry. | [`thedriver.h:21–31,155–245`](../src/thedriver.h#L21). | Add a versioned optional workbench interface; retain the existing renderer and older-driver fallback. |
| Terminal console is right-docked at sufficient width, otherwise below; it hides below 16 rows/32 columns. GTK measures its console controls and reserves 80 editor columns. | [`cursesdriver.c:1026,1102–1116`](../src/drivers/curses/cursesdriver.c#L1026), [`gtkdriver.c:19`](../src/drivers/gtk/gtkdriver.c#L19), [GTK layout](gtk-driver.md#program-console). | Placement is a preference with minimum sizes and fallback order, not a promise of physical coordinates. |
| Browser shares semantic snapshots, fields and tool state. Native-host requests use restricted command/action paths; Wasm has a separate Worker host and excludes Regina/native OS/parser processes. | [`websession.c:169–212,300–325`](../src/websession.c#L169), [`wasmdriver.c:201–213`](../src/drivers/wasm/wasmdriver.c#L201), [Wasm limits](browser-wasm.md#browser-webassembly-preview). | Browser-native and browser-Wasm are different hosts behind a similar UI; negotiate both UI and execution capabilities. |
| LLM runs the real dispatcher, advertises `tools`/`toolwait` and workbench schema 1, and appends a published workbench summary to look/delta. Full action/menu/item pages use QUERY/EXTRACT. | [Implemented workbench guide](workbench-actions-menus.md), [LLM session adapter](../src/llm/llmsession.c). | Extend the normalized protocol coherently; agents discover IDs and state rather than scrape graphical controls. |
| Unknown commands default to neither implied macro nor OS execution. `MACRO`, `OS`/`!`, and `SET SYNONYM` are explicit facilities; browser synonyms are constrained to safe editor commands. | [`commset1.c:5434–5504`](../src/commset1.c#L5434), [`commset2.c:3165–3209`](../src/commset2.c#L3165), [`frontendpolicy.c:515–597,600–677`](../src/frontendpolicy.c#L515). | Raw command actions obey normal dispatch and frontend policy; typed built-in actions preserve their intentional alias bypass. Neither path gains new authority. |
| Active macro engines are cREXX, optional Regina, or NONE, selected at startup; ooRexx is not an active engine. | [`rexx.c:2149–2174`](../src/rexx.c#L2149), [interpreter guide](rexx-interpreters.md). | Treat a future ooRexx program adapter separately from any future in-process macro-engine integration. |

The architecture guide describes the intended common host boundaries. The
actual registry currently has fewer fields than that direction requires; for
example, per-tool dependency/cohort requirements and fine-grained host
capabilities need representation rather than being inferred from `available`.
ooRexx execution is a proposed adapter, not a claim that the current cREXX or
Regina macro bridge already supplies an ooRexx program runner.

## Required behavior

1. **Keep THE recognizable.** Prefix area, TOF/EOF, file ring, command line,
   current view and profile-defined status/reserved guide remain available.
   Auxiliary panels consume their own layout space; a macro must not remove
   the guide as an accidental consequence of showing a pane. Existing profiles
   can still explicitly change their own reserved rows and status settings.
2. **Configure once.** A profile defines semantic objects by stable ID; drivers
   render them. Terminal/GTK/browser equivalents need semantic parity, not
   identical pixel layout. LLM receives the same IDs, state and operations.
3. **Execute explicitly.** Defining, importing, selecting, showing or focusing
   an object never launches a program, macro or shell. Invocation uses an
   explicit action or command. Unknown commands keep their existing refusal;
   native `SET IMPOS ON` remains a deliberate compatibility opt-in.
4. **Keep routes distinct.** THE commands/responses, macro READ, program stdin,
   stdout/stderr, typed completion and language-service frames are separate
   channels. A text pane cannot execute its contents by receiving output.
5. **Preserve editor authority.** A source run captures bytes including the
   pending record without saving. Field commits use the core mutation path;
   parsing remains DSLSH-owned with protected reads and revision admission.
6. **Support keyboard and constrained layouts.** Every action/pane is reachable
   without a mouse. Right panes stack/tab below on narrow windows and phones;
   unsupported placement returns the chosen fallback and reason.
7. **Make failures inspectable.** Disabled actions include a reason; rejected
   operations return stable codes plus a human message. Rendering fallback is
   distinct from unavailable execution, denied authority, busy state and stale
   data. Never report a missing parser as clean diagnostics.
8. **Bound the extension.** Profiles describe menus, fields, text/log/document
   panes and typed actions. Arbitrary HTML/JavaScript, native widget handles,
   terminal emulation, shell pipelines and an IDE plugin marketplace are outside
   the initial contract.
9. **Acquire file paths visibly.** Actions and document panels that need a file
   offer a chooser or searchable path/directory prompt, as well as an explicit
   path operand. Selecting a file supplies data for the requested operation;
   it does not run that file. Chooser presentation cannot grant access: the
   core still resolves the selected workspace path and enforces containment,
   read-only status and operation policy. Cancel leaves the caller unchanged.

## Representative use cases

| Use case | Shared behavior and boundary |
| --- | --- |
| Classic Rexx exercise | Run current unsaved source; Program pane shows labelled stdout/stderr and stdin; Stop and EOF remain distinct; editor can continue changing independently. |
| ooRexx development | An installed, explicitly registered native process adapter runs a captured source or explicit file with argv. Browser-Wasm shows `unsupported-host` unless a separately qualified implementation exists; it never substitutes Classic Rexx. |
| RXBIN utility | Registered image with matching runtime cohort, arguments and completion. No shell parsing or extension-based execution merely from opening the file. |
| DSLSH language assistance | A persistent protocol session sends styles/diagnostics to the document model; an optional diagnostics pane displays admitted results. Protocol stdout cannot be routed into a Program pane. |
| Profile-owned commands/menu | A profile adds Tools → Check using an explicit macro action and removes it later. `SET SYNONYM` remains the way to create a typed command alias. |
| Build and compare | One build job feeds a build log pane; the editor and a read-only generated-file pane remain visible on a wide screen, selectable on a narrow screen. Opening a generated file does not run it. |
| Configuration form | A file contains CTLCHAR fields, or a macro defines named fields. Submit validates changes; Cancel preserves bytes; Save is a separate operation. |
| Agent-assisted editing | An agent discovers menus/panels/actions and refuses unsupported execution deterministically. It can edit named fields, commit against a revision, and verify bytes through the real THE runtime. |

## Shared object model

The core owns a `WorkbenchSession` alongside existing editor/tool state. Objects
have an opaque stable ID, owner ID, generation, title, creation order and
session revision. Public IDs use ASCII letters/digits/dot/underscore/
hyphen, begin with a letter, and are at most 63 bytes. IDs are case-sensitive;
command keywords follow THE's case-insensitive convention. `the.*` is reserved
for built-ins. IDs are unique within each object kind across the session;
item IDs are unique among all items, including items in different menus.
Phase3A field IDs are scoped to their form, identified by `(form-id, field-id)`.
An ID
may be reused in a different kind, but references are always typed. Removing and
recreating an ID allocates a new generation; replacement also advances its
generation. Generations and revisions are opaque nonnegative integers, never
inferred from a label or an array index. Labels are UTF-8 data, never command
identifiers or delimiters.

| Object | Required data and ownership |
| --- | --- |
| Action | ID; label/help; kind (`builtin`, `command`, `macro`, `tool`, `panel`); typed operands; required capabilities; enabled state/reason; source owner. Built-ins reuse frontend action IDs. |
| Menu / item | Stable menu/item IDs, label, order, action reference or separator, visibility and enabled reason. Initially one submenu level. No callback source inside a label. |
| Panel | Kind (`log`, `document`, `fields`, `diagnostics`), model reference, placement preference/fallbacks, minimum size, visible state and close policy. Focus and viewport are separate from model lifetime. |
| Document / view | Existing file-ring identity plus a content revision token, retained reference and independent view state. Durable revision tokens must include pending-record changes; current tool snapshot revision alone is insufficient between captures. |
| Form | Named fields with value, protection/type/length, layout hints, staged changes and form revision. A file binding additionally holds document ID, base revision and byte ranges. |
| Tool definition | ID, role/entry protocol, host adapter, executable/image, argv schema, source mode, dependency/cohort requirements and IO contracts. Separate program, profile and language-service authority. |
| Job | Unique ID/generation, retained tool definition and source snapshot, state, streams, route set, EOF/exit/error, limits and cancellation. Re-registration does not change an already running job. |
| Route / stream | Source job/service/profile stream, sink model, format/encoding, bounds, append/replace policy and lifecycle. A pane displays a model; it is not itself a file descriptor. |
| Owner | Built-in, profile activation or macro/session scope, with origin/trust and allowed operations. An owner groups replaceable definitions, not operating-system privileges. |

Drivers keep only physical resources and presentation cache: GMenu objects,
terminal windows, DOM nodes, scroll positions and physical focus. They emit
normalized requests with IDs/generations; the core validates and publishes
resulting state. No callback from a Worker, GTK signal or transport reenters an
executing/suspended editor stack.

Configuration is idempotent: `PUT` replaces an object with the same owner/ID;
`REMOVE` of an absent object succeeds. Duplicate IDs owned by someone else are
rejected. The core binds the owner to the trusted execution context: an `OWNER`
operand selects only an owner that context may manage. It cannot impersonate
another profile, remove another owner's definitions, claim `the.*`, or create
authority by inventing an ID. `BEGIN`/`COMMIT` stages **workbench definitions only**: validate all
references, limits and trust before accepting the draft; `ABORT` discards it.
Outside profile activation, a successful `COMMIT` publishes one revision.

All definition changes require an explicit `BEGIN OWNER id`; transactions do
not nest and have one owner. A mutation or `COMMIT` outside a transaction
returns `transaction-required`; nested `BEGIN` returns `transaction-active`.
`ABORT` discards an open draft, and repeated `ABORT` succeeds. A rejected
mutation leaves the draft unchanged but marks it failed; `COMMIT` then returns
`transaction-failed` and publishes nothing until `ABORT` discards the draft.
Complete-graph validation happens at commit, so definitions may be added or
removed in any order inside one transaction. A failed final validation also
marks the transaction failed. Inspection remains available to diagnose the
draft. Invocation is a runtime request, never a definition change, and cannot
invoke an unpublished candidate.

Profile activation adds an outer workbench staging scope inherited by macros
called from that profile. Explicit `COMMIT` merges into that activation's
candidate definitions; it does **not** publish them to drivers or escape the
scope. The profile can query its candidate, while other consumers see the last
published revision. Publish once only after the complete profile returns
success, every workbench definition operation succeeded, no definition
transaction remains open, and final validation passes. Any failure discards
the candidate, retaining the previously published definitions even if an
earlier explicit `COMMIT` succeeded. Actions in the unpublished candidate
cannot be invoked. Re-running a profile replaces its owned definitions without
duplicates. This staging covers only workbench definitions: arbitrary editor
commands, existing explicit execution and their external effects retain the
existing profile rollback limitations.

Reference validation applies to the complete candidate graph. Every new or
replaced item/action/panel/route must reference an object present in that
candidate. Removing a referenced definition fails with `object-in-use` unless
the same transaction removes or rebinds every dependent reference. Removing a
menu also removes its contained items, but leaves their actions. An owner reset
removes its complete definition graph atomically. Initial references may target
the same owner or declared built-ins; cross-profile references require a later
explicit import/export contract. Runtime capability loss disables an existing
action with its capability reason; it does not delete the referenced definition
or permit dangling references. Queued events for removed objects are refused by
ID/generation. Thus creation, replacement and removal use the same integrity
rule, and an incomplete removal never partially publishes.

Closing/hiding a pane never stops its producer or closes its file. Removing a
panel removes its view; retained models/jobs remain discoverable until explicitly
closed/released. An owner reset removes its declarations and idle views, marks
its existing jobs detached, and keeps Stop/status/log access in a built-in Jobs
view. A running job pins its routes/models until completion. The initial host
allows one interactive program job; language-service sessions remain separate.
A later job table may permit concurrent programs without changing public IDs or
request semantics. Session exit performs existing dirty-file checks, then
explicitly terminates remaining execution resources; it never claims graceful
Rexx cleanup after hard Stop.

The job state machine is `created → starting → running ↔ waiting-input →
publishing → completed`, with failure/Stop/limit transitions from active states.
Jobs without a file sink skip `publishing`. Starting may fail directly. EOF is an
independent closed-input flag, not completion. Only adapters that can prove
input starvation publish `waiting-input`; native pipes currently cannot do so.
One terminal transition drains/publishes preceding output, freezes completion,
releases execution resources once and retains bounded inspectable history.

## Frozen command grammar

The following is the version-1 contract, **not a blanket implementation claim**. Use one
`WORKBENCH` family to avoid overloading `RX` with every UI concept. Initial
keywords require full spelling until abbreviation compatibility is reviewed.
A profile may define `SET SYNONYM wb WORKBENCH` once the command exists.
Action IDs do not introduce new THE command names; `SET SYNONYM` continues to
own that facility.

```text
WORKBENCH BEGIN OWNER owner-id
WORKBENCH COMMIT
WORKBENCH ABORT
WORKBENCH RESET OWNER owner-id

WORKBENCH ACTION PUT id LABEL "label" BUILTIN frontend-action-id [ARGUMENT "value"]
WORKBENCH ACTION PUT id LABEL "label" COMMAND "explicit THE command"
WORKBENCH ACTION PUT id LABEL "label" MACRO "macro-path" [ARG "value"]...
WORKBENCH ACTION PUT id LABEL "label" TOOL tool-id SOURCE ACTIVE [ARG "value"]...
WORKBENCH ACTION PUT id LABEL "label" PANEL panel-id OP SHOW|FOCUS|HIDE
WORKBENCH ACTION REMOVE id
WORKBENCH INVOKE action-id [EXPECT generation]

WORKBENCH MENU PUT menu-id LABEL "label" [ORDER integer]
WORKBENCH ITEM PUT menu-id item-id LABEL "label" ACTION action-id [ORDER integer]
WORKBENCH ITEM PUT menu-id item-id SEPARATOR [ORDER integer]
WORKBENCH ITEM REMOVE menu-id item-id
WORKBENCH MENU REMOVE menu-id

WORKBENCH PANEL PUT panel-id KIND LOG MODEL model-id
WORKBENCH PANEL PUT panel-id KIND DOCUMENT BUFFER ACTIVE ACCESS READONLY
WORKBENCH PANEL PUT panel-id KIND DOCUMENT FILE "path" ACCESS READONLY
WORKBENCH PANEL PUT panel-id KIND FIELDS BUFFER ACTIVE MARKUP CTLCHAR ACCESS STAGED
WORKBENCH PANEL PUT panel-id KIND FIELDS FORM form-id
WORKBENCH PANEL PLACE panel-id RIGHT FALLBACK BELOW,TAB MINCOLS 32
WORKBENCH PANEL SHOW panel-id
WORKBENCH PANEL HIDE panel-id
WORKBENCH PANEL FOCUS panel-id
WORKBENCH PANEL REMOVE panel-id

WORKBENCH FORM PUT form-id
WORKBENCH FIELD PUT form-id field-id LABEL "label" VALUE "value" ACCESS EDIT MAXCELLS 40
WORKBENCH FORM SUBMIT form-id EXPECT revision
WORKBENCH FORM CANCEL form-id
WORKBENCH PANEL APPLY panel-id EXPECT document-revision

WORKBENCH ROUTE PUT route-id FROM tool:tool-id/stdout TO log:model-id
WORKBENCH ROUTE PUT route-id FROM tool:tool-id/stderr TO log:model-id
WORKBENCH ROUTE PUT route-id FROM input:panel-id TO tool:tool-id/stdin MODE LINE
WORKBENCH ROUTE PUT route-id FROM FILE "path" TO tool:tool-id/stdin FORMAT BYTES EOF END
WORKBENCH ROUTE PUT route-id FROM tool:tool-id/stdout TO FILE "path" FORMAT BYTES MODE CREATE|REPLACE|APPEND [EXPECT file-token] PUBLISH SUCCESS
WORKBENCH ROUTE PUT route-id FROM tool:tool-id/stderr TO FILE "path" FORMAT BYTES MODE CREATE|REPLACE|APPEND [EXPECT file-token] PUBLISH SUCCESS
WORKBENCH ROUTE REMOVE route-id
WORKBENCH TOOL PUT tool-id ROLE RUNNER ADAPTER NATIVE-PROCESS EXECUTABLE "path" SOURCE SNAPSHOT
WORKBENCH TOOL REMOVE tool-id
WORKBENCH RUN tool-id SOURCE ACTIVE [ARG "value"]...
WORKBENCH JOB INPUT job-id TEXT "line"
WORKBENCH JOB EOF job-id
WORKBENCH JOB STOP job-id
WORKBENCH FILE INSPECT "path"

QUERY WORKBENCH [SUMMARY|CAPABILITIES|ACTIONS|MENUS|ITEMS] [OFFSET integer] [LIMIT integer]
EXTRACT /WORKBENCH [SUMMARY|CAPABILITIES|ACTIONS|MENUS|ITEMS] [OFFSET integer] [LIMIT integer]/
```

Phase 1 admits definition transactions, owner reset, actions of kind `BUILTIN`,
`COMMAND` and `MACRO`, menus/items, explicit invoke and the five inspection
sections above. `TOOL`/`PANEL` actions and all panel/form/field/route/tool/job/file
commands are reserved for their later phase. A recognized later-phase request
returns `unsupported-feature`; it must not partially define an object or fall
through to macro/OS dispatch. Later capability-advertised inspection sections
may add `PANELS`, `FORMS`, `JOBS`, `ROUTES` and `TOOLS` without changing the
version-1 page envelope.

`QUERY` displays the same bounded JSON page returned by `EXTRACT`.
`SUMMARY` is the default and includes counts, limits and the last workbench
result. `ACTIONS`, `MENUS` and `ITEMS` pages have `schemaVersion: 1`, the published definition `revision`,
`total`, `nextOffset` (null at the end), and `records`. `OFFSET` defaults to zero;
`LIMIT` defaults to eight and accepts 1 through 8. Offset beyond the end returns
an empty successful page. `EXTRACT` sets `WORKBENCH.0` to 1 and `WORKBENCH.1` to
that one JSON page, without assuming cREXX and Regina queue implementations are
identical. It never truncates JSON: serialization above 65,536 bytes returns
`limit-exceeded` without replacing a previous extract result. Existing `READ`
results and return codes remain unchanged.

Operands are parsed once into typed values; argv entries remain separate.
Keywords and enum operands require their full spelling and are case-insensitive.
IDs remain case-sensitive. A quoted string starts/ends with double quotes;
two consecutive double quotes inside it encode one literal double quote.
Backslashes are literal, including in Windows paths: there are no C, shell,
JSON, variable, wildcard or backslash escapes. Empty quoted strings are accepted
where the operand's own validation permits them. String operands shown quoted
in the grammar require quotes. Space and tab separate tokens outside quotes;
all other ASCII control bytes, DEL, NUL, invalid UTF-8 and embedded line breaks
are rejected. A closing quote must be followed by whitespace or end of command.
There is exactly one `WORKBENCH` operation per addressed command; no trailing
command separator or comment syntax is recognized. The parser rejects
unterminated quotes, extra tokens, duplicate options, unknown options, integer
overflow and signed values where an unsigned operand is required, without
partial changes. It must not
reuse the permissive current `RX` tokenizer for arbitrary interpolated commands.
Programmatic frontends use structured arguments, not generated command strings.
Literal command actions store a fixed explicit command; runtime file paths or
field values are never substituted into that text.

For example, `LABEL "Say ""hello"""` stores `Say "hello"`,
`MACRO "C:\Tools\check.rexx"` stores the backslashes literally, and
`LABEL "x; OS unwanted"` remains one label. A `COMMAND` action is different:
its explicitly supplied fixed command is executed only by a later explicit
invocation, through the existing command dispatcher and policy. The new quoting
contract grants no new command-chain or shell authority. `ORDER` accepts
0 through 2,147,483,647 and defaults to zero; ties retain creation order, then ID.

`PANEL PUT ... KIND LOG MODEL id` creates a bounded session log if absent,
otherwise attaches to an existing compatible log. It does not create a file.
All retained model references must resolve when a commit is accepted, including
after removals. `BUILTIN ... ARGUMENT` binds the
single typed argument supported by the existing action; it is not appended to
a generated command. New typed actions retain built-in synonym bypass. A
`COMMAND` action deliberately has normal command/synonym semantics and the
same restricted-session admission checks as a typed command line.

### Phase-1 profile example: shared action and menu

This contract example uses an existing typed built-in. Definitions have no
effect on the file until the user explicitly invokes the item/action.

```rexx
address THE
'workbench begin owner user.tools';
'workbench action put user.save label "Save current file" builtin file.save';
'workbench menu put user.tools label "Tools" order 50';
'workbench item put user.tools user.saveitem label "Save current file" action user.save';
'workbench commit';
```

### Phase-2 contract example: menu and an output pane

This is proposed profile source using existing Rexx `ADDRESS THE` conventions.
For cREXX, addressed command literals have explicit semicolons.

```rexx
address THE
'workbench begin owner user.tools';
'workbench action put user.run label "Run current Rexx" tool classic-rexx source active';
'workbench menu put user.tools label "Tools" order 50';
'workbench item put user.tools user.runitem label "Run current Rexx" action user.run';
'workbench panel put user.output kind log model user.programlog';
'workbench panel place user.output right fallback below,tab mincols 40';
'workbench route put user.stdout from tool:classic-rexx/stdout to log:user.programlog';
'workbench route put user.stderr from tool:classic-rexx/stderr to log:user.programlog';
'workbench commit';
```

This declares a launch action and routes; it does not run the program. Choosing
the item invokes it through the core, shows the output pane without stealing
file focus, and binds a fresh job ID. A separate explicit `WORKBENCH PANEL
FOCUS user.output` reaches its controls/input. Removing only the item leaves
the action callable; removing the menu removes its owned items but not actions.
Removing that action while its item remains returns `object-in-use`; remove or
rebind the item in the same transaction to remove the action. None of these
operations invokes anything as a side effect.

The existing form remains useful today:

```text
SET CTLCHAR ! ESCAPE
SET CTLCHAR p PROTECT WHITE
SET CTLCHAR u NOPROTECT GREEN
SET RESERVED 2 !pName: !uGuest           !p
READ NOCHANGE TAG
```

A proposed named-field form avoids editing the caller's reserved guide:

```text
WORKBENCH BEGIN OWNER user.forms
WORKBENCH FORM PUT user.person
WORKBENCH FIELD PUT user.person name LABEL "Name" VALUE "Guest" ACCESS EDIT MAXCELLS 40
WORKBENCH PANEL PUT user.personpane KIND FIELDS FORM user.person
WORKBENCH COMMIT
WORKBENCH PANEL SHOW user.personpane
WORKBENCH PANEL FOCUS user.personpane
```

Submit returns structured field values; it does not issue the values as THE
commands. A macro can explicitly read the result and decide its next command.
The first version supports no arbitrary action callback on every keystroke.

### Tool examples and compatibility

These RXBIN/DSLSH registrations **already exist**:

```text
RX REGISTER myprogram program /workspace/myprogram.rxbin .rxbin "My program"
RX RUN myprogram "Mixed Case"
RX REGISTER mylanguage language /workspace/myparser.rxbin .example "Example language"
RX LANGUAGE mylanguage
```

This native ooRexx registration is **implemented in the bounded native
source-tool increment**, subject to its host/dependency qualifications:

```text
WORKBENCH TOOL PUT user.oorexx ROLE RUNNER ADAPTER NATIVE-PROCESS EXECUTABLE "/configured/path/rexx" SOURCE SNAPSHOT
WORKBENCH ACTION PUT user.oorun LABEL "Run with ooRexx" TOOL user.oorexx SOURCE ACTIVE
```

The native adapter stages an immutable source snapshot and passes its pathname
and argv as separate process arguments. Its initial fixed working-directory,
environment, source-encoding and include-resolution settings are explicit in
[capability discovery](workbench-native-tools.md). A later source tool
that requires the original saved file uses an explicit `SOURCE FILE` contract
instead; it cannot silently save unsaved text. Changing THE's macro interpreter
does not select this runner, and running this tool does not change the macro
engine. Missing ooRexx or a Wasm host returns an unavailable/unsupported result.

## File panes and field panels

“Open a file as a panel” has three distinct, explicit meanings:

| Kind | Source and editing | Save, undo and stale behavior |
| --- | --- | --- |
| Document pane | A retained view of an existing file-ring buffer, or an explicit file open through the normal path/policy. Read-only by default; later `ACCESS EDIT` uses ordinary THE editing. | One underlying buffer/dirty state; core save and ordinary undo semantics; closing the pane does not close the buffer. A document view follows core revisions and never writes a rendered copy back. |
| File-field projection | A `BUFFER` binding plus CTLCHAR or a future explicit field schema. Capture document ID/revision, byte spans and protected text; stage editable fields independently while the editor can remain usable. | Apply validates the complete patch against the captured revision and readonly/truncation rules, then commits all fields through the core as one defined edit group. Save remains explicit. If the buffer changed, return `stale-revision` and retain staged values for explicit reload/rebase/cancel; never guess offsets or overwrite new text. |
| Named-field form | Form-owned values, field IDs, types/protection and logical layout; optionally loaded from a file by an explicit macro. No implicit buffer binding. | Submit publishes a form result/version. It does not dirty/save a file or enter file undo. A macro must explicitly apply any resulting file changes. Conflicting form revisions are rejected. |

Opening by path checks canonical workspace policy and read-only status just as
an ordinary file open does. Reuse an existing matching file identity rather
than read the disk into an unsynchronized panel buffer. A file-field projection
pins its buffer until it is removed/cancelled; file close reports the dependency
and requires an explicit cancel/release, not silent loss of staged values.
Pending-record text is included in revision comparisons.

The current READ fixed-cell overwrite, Unicode movement, protected boundaries,
changed-field order and cancellation semantics are reused where applicable.
Byte spans and logical field cells are distinct; no UTF-16/browser offset may
become a file byte offset. New persistent projections do not weaken current
`READ ALL` validation-before-commit. The bounded [CTLCHAR projection](workbench-file-fields.md) applies through
the separately qualified core edit group and conditional group undo. Save
remains explicit; general editable document panes and other projection schemas
remain outside this implementation.

## Execution and IO routing

Use separate capability-bearing adapters for RXBIN/cREXX, Classic source,
native ooRexx/processes, profile execution and DSLSH services. A language name
selects no executable by itself. Registering a tool names its role and protocol;
the resolver checks host, runtime cohort/providers and workspace authority at
launch. An ooRexx adapter uses an explicitly configured executable and argv,
never a shell string. A browser-Wasm adapter cannot inherit native filesystem or
process authority. Existing `RX REGISTER` continues to mean its documented
RXBIN registration, not arbitrary native process registration.

| Channel | Routing rules |
| --- | --- |
| THE command / response | Commands execute through the normal dispatcher and policy; responses retain their editor message channel/serials. Optionally mirror responses to a labelled read-only log, without replacing MSGLINE or program output. |
| Program stdin | One explicit input producer per job: console line input, fixed bytes, or an explicitly opened file/snapshot. Never consume the THE command line, LLM transport stdin or macro READ implicitly. EOF follows queued input and is irreversible for that job. |
| Program stdout / stderr | Preserve stream identity, job ID and sequence. Both may display in one ordered log, or separate models; merging is presentation, not loss of stream metadata. No callback executes output. |
| Completion / conditions | Typed final state and return/condition data, after preceding output is published. It is not inferred by parsing stdout. |
| Profile/macro output | Context-local writer where supported; identify profile activation separately from program jobs. `PULL`/READ retain their engine bridge contracts; a UI route cannot safely capture arbitrary global process stdio. |
| DSLSH protocol | Reserved framed channel owned by the service adapter. Parser stdout/control frames cannot be selected as a general log source; diagnostics and human stderr are separately typed where supported. |

Tool-level route definitions are templates copied into a new job; they cannot
retarget a running job or make old output appear in a new job. Route changes
affect later invocations. Each stream retains one canonical bounded inspection history;
an initial route chooses one primary log projection per stdout/stderr and
defaults to append. Two projections of the same model do not duplicate the
underlying bytes. With no explicit template, retain the current Program console
routes. The built-in Jobs view can always inspect the canonical streams even
after a routed pane is removed. The initial routing increment supports bounded
text logs and line input; the phase-4 file source/sink contract below adds exact
bytes. Arbitrary pipelines and cross-job stdin remain outside that contract.
Until a host qualifies file routes, requests report unsupported rather than
falling back to decoded logs or shell redirection.

### Bounded file redirection, proposed for phase 4

`FROM FILE` opens a read-only workspace file at launch, takes an immutable byte
snapshot and records its identity, version/fingerprint and length. It sends
those bytes without UTF-8 repair, newline insertion or encoding conversion,
then EOF after the queued bytes drain. An empty file sends immediate EOF.
Interactive input is disabled for that job; later edits to the source file do
not change the captured input. Size/authority failure prevents launch.

`TO FILE` stages raw stdout or stderr bytes **before** the display decoder,
subject to advertised input/output/staging limits. File capture is exact even
for NUL, invalid UTF-8 and bytes split across transport messages. The log remains
a separately labelled, bounded text projection, not proof of captured bytes.
Each file has one writer/stream per job; routing stdout and stderr to the same
path or using an input file as an output target is initially rejected.

The initial publication modes are:

| Mode | Target and publication contract |
| --- | --- |
| `CREATE` | Target must be absent. Publish the staged file without replacing an existing target; a collision at publication fails. |
| `REPLACE` | Existing target requires an `EXPECT` token acquired by inspection/selection. Preserve its bytes while the job runs; conditionally publish the complete staged replacement after final validation. A changed target returns `stale-revision`. |
| `APPEND` | Existing target requires `EXPECT`; capture its exact starting bytes and stage starting bytes plus new output. Conditionally publish that complete result. This is atomic append by staged replacement, not live streaming append; a changed target fails. An absent target uses `CREATE` explicitly. |

`WORKBENCH FILE INSPECT` and chooser results expose a canonical workspace
identity, read-only state, existence, size and opaque file token. A token is a
concurrency check, not a grant of authority. Structured frontends receive the
same fields. Tokens and route definitions do not keep a target writable if
policy or filesystem state changes before publication.

`PUBLISH SUCCESS` means publication occurs only after the program completes
successfully and all stream bytes drain. The job remains in a publishing state
until the storage adapter acknowledges commit; only then report successful
completion and the target's new token/length. Program failure, Stop, timeout,
output limit, confirmed pre-publication storage failure or stale target discards
staging and retains the original target (or absence). Failed publication is an IO failure,
not successful execution with a silently missing file. Multi-file atomic commit
is outside the first contract: a job may have at most one file-output sink;
the other stream can remain in the console. Publishing partial output after a
failed job would require a later explicit policy, never an implicit fallback.
If storage or transport loses acknowledgement and cannot establish whether the
atomic publication committed, return `publication-unknown`; do not claim the
target is unchanged or retry append blindly. Reconcile the target token/bytes
before admitting another write to that target.

Definition/import/selection does not create, truncate or append a file. At
launch and publication the core applies canonical workspace path, parent,
read-only and session policy checks, including aliases/symlinks and destination
changes. The adapter must provide conditional atomic publication and cleanup,
or advertise that mode unsupported. Native implementations need an exclusive
target/ownership or locking strategy sufficient for that guarantee; a preflight
fingerprint followed by an unguarded rename is insufficient under external
writers. Browser implementations acknowledge successful persistent storage
commit, not merely a MEMFS write. Native OS paths and browser workspace paths
share this contract without acquiring one another's authority. Successful file
publication does not clear an independently open editor buffer's dirty state;
refuse an output target that has a dirty or staged-field buffer until the user
explicitly resolves it. A clean open buffer is marked externally changed for
normal reload/conflict handling rather than silently replaced in memory.

Keep the current 2 MiB source, 1 MiB aggregate argv, 128 argv entries, 8 MiB job
output and 256 KiB retained text as initial limits, surfaced by capabilities.
The following limits freeze the initial workbench contract. Limits for a later
phase do not imply that phase is available; a constrained host may advertise a
lower effective limit before accepting a definition or launch. Byte lengths
exclude C terminators. Quotas apply to a complete candidate graph, including
retained live dependencies, rather than accepting an over-limit draft for a
frontend to trim.

| Resource | Initial ceiling | First applicable phase |
| --- | --- | --- |
| ID / label / stored action operand | 63 / 255 / 2,048 UTF-8 bytes | 1 |
| One definition command / serialized inspection page | 8,192 / 65,536 bytes | 1 |
| Owners / actions / menus / menu items | 32 / 128 / 32 / 256 per session | 1 |
| Definition transaction | One open transaction; no nesting | 1 |
| Inspection page | 8 records; explicit limit 1 through 8 | 1 |
| Active invocation nesting | 8; active-ID cycles are refused | 1 |
| Panels / retained log models | 32 / 32 per session | 2 |
| Log text / aggregate retained model text | 256 KiB per model / 8 MiB per session | 2 |
| Program jobs / queued normalized requests | One active program / 64 queued requests | 2 |
| Browser invocation receipts | 256 retained IDs per session nonce; no replay after eviction | 1 |
| Forms / fields / field value | Phase3A: 32 forms / 128 fields per form, 512 per session / 1,024 UTF-8 bytes per field; later expansion to 4,096 requires transport qualification | 3 |
| Field logical width | 1 through 1,024 cells; byte and cell limits both apply | 3 |
| Tools / routes | 32 tool definitions / 32 route templates per session | 4 |
| File stdin / staged file output | 8 MiB / 8 MiB, including starting bytes for APPEND | 4 |
| Published file-output targets | One per program job | 4 |

No phase silently evicts published definitions or live jobs to admit another
one. Bounded log history may omit oldest text with explicit omission metadata;
it never changes raw file-capture bytes. Receipt eviction is an inspection
boundary, not permission to execute an old request again. Preserve UTF-8 decoder state per
stream, mark omitted history, and preserve a user's output scroll/selection when
new data arrives. Binary output needs a declared byte sink; it must not be
silently repaired into supposedly exact text. Rendering text escapes controls
instead of executing terminal/HTML markup. Stop, timeout, output-limit and host
failure publish distinct terminal states and release resources exactly once.

Source-bound actions invoked from menus may wait for the current macro/menu to
return, as the existing menu-run path does. The queued request records the
intended buffer/view identity; the core captures pending bytes at the safe point
and returns the captured revision. If that target disappeared, it refuses rather
than running whichever file is current later. An active modal READ retains
exclusive input; a launch request receives `busy-modal` and is not replayed
unexpectedly when the form closes.

## Capabilities, policy and trust

Effective capability is the intersection of **core feature**, **driver
presentation**, **host resource**, **tool/runtime** and **session policy**.
Reporting a GTK menu does not imply a process adapter; reporting native process
support does not authorize its use in a restricted browser session. Capability
records include version, supported operations, limits, alternatives and reason.
Tool definitions and each action/panel expose their resolved state.

Stable result codes complement THE's existing numeric return codes. Consumers
use `code` to distinguish refusals that share a numeric RC; `message` is human
text and is not parsed for authority decisions. The mapping is:

| Stable code | Numeric RC | Existing THE constant |
| --- | --- | --- |
| `ok` | 0 | `RC_OK` |
| `invalid-operand`, `missing-object`, `missing-action` | 5 | `RC_INVALID_OPERAND` |
| `unsupported-feature`, `unsupported-host`, `capability-unavailable`, `policy-denied`, `busy-modal`, `busy-job`, `object-in-use`, `stale-generation`, `stale-revision`, `transaction-required`, `transaction-active`, `transaction-failed`, `activation-pending`, `activation-failed` | -3 | `RC_INVALID_ENVIRON` |
| `readonly` | 12 | `RC_ACCESS_DENIED` |
| `limit-exceeded` | 94 | `RC_OUT_OF_MEMORY` |
| `host-failed` | 99 | `RC_SYSTEM_ERROR` |
| `io-failed`, `publication-unknown` | 100 | `RC_IO_ERROR` |
| `command-failed` | Actual dispatched command RC | Preserve the existing command's result |

For `INVOKE`, admission success does not hide a later dispatched command
failure. Its result preserves that command's numeric RC and uses
`command-failed`. Phase 1 implements only applicable codes; later-phase codes
are reserved until their feature is qualified. Existing unrelated commands and
`READ` retain their return-code contracts. No workbench error becomes implicit
macro/shell dispatch.

Trusted installed/startup profiles may declare workbench objects under their
owner. Importing a document, RXBIN, workspace configuration or remote message
does not make it a trusted profile. Declared actions carry origin/trust and
required authority. An ordinary program or DSLSH service has no `ADDRESS THE`
authority merely because its output is displayed by THE. A future privileged
bridge needs a separate explicit contract.

Native explicit `MACRO` and `OS`/`!` actions remain subject to existing native
session rules; registration is not an OS sandbox. Restricted browser clients
must not create arbitrary command/macro/native-process actions or disguise them
through synonyms, chains, menu clicks or field submissions. A trusted bundled
profile can expose a narrowly defined semantic action only if session policy
permits that action at invocation. Revalidate policy when invoked, after path
resolution and after any typed operands are bound. UI disabled state is never
the authorization check. Preserve safe `SET SYNONYM` restrictions and deliberate
native IMPOS/IMPMACRO compatibility settings without enabling either implicitly.

No new blanket confirmation dialogue is required for ordinary pane/menu
configuration. Execution is authorized by explicit invocation within the
session's existing authority. A UI declaration transaction contains no runnable
startup hook; an explicitly executed profile remains executable code with its
existing authority and rollback limits.

## Rendering and interaction by surface

| Contract | Terminal/curses | GTK | Browser native-host / Wasm | LLM |
| --- | --- | --- | --- | --- |
| Menus | Shared model lowered to popup/list, with an escape-safe label path rather than slash-delimited labels. F10 and command access. | Shared model lowered to GMenu/GAction; macOS system menu where supported, in-window fallback elsewhere. | Shared model lowered to accessible buttons/menu sections; touch and keyboard. | Structured menu/item/action list; invoke by ID. No synthetic mouse or pixel coordinates. |
| File selection | Searchable `DIRECTORY` listing/path prompt or equivalent, with keyboard selection and explicit path entry. | Native file chooser, with explicit path entry/fallback. | Workspace file picker; browser-native open/import chooser where supported. Import first obtains a workspace identity and requires explicit collision handling; it does not expose an arbitrary host path. | Explicit workspace path argument; structured missing-path/denied-path result, with no interactive chooser required. |
| Wide layout | Right pane only when editor minimum, divider and pane minimum fit; otherwise below/tab. | Measured widgets plus reserved editor columns; user-resizable split. | Side panes on wide windows; splitter semantics and accessible controls. | Requested/effective placement metadata; pane contents are independent semantic objects. |
| Narrow/phone | Stack below or select one auxiliary tab; very small screens keep command access and report collapsed panes. | Below/tab fallback when measured minimum fails. | Phone stacks or selects one auxiliary pane with persistent editor access; no fixed-width form clipped without scroll/reflow. | No width requirement; query a chosen pane or field range without dumping every log. |
| Focus | Profile-owned editor Tab remains intact. Explicit workbench entry then Tab/Backtab/arrow traversal; Esc restores recorded editor zone/caret. | Native focus traversal outside editor; no menu accelerators overriding profile definitions. | DOM focus order, visible focus, Escape return and touch labels. | `focus`/invoke by semantic ID; report editor focus separately from focused pane/control. |
| Fields | Shared logical fields/protection and PF/PA/submit/cancel actions. | First support must use a qualified field renderer; until then advertise unsupported, rather than claim current modal READ is native GTK form support. | Shared field values/protection; scrollable/reflowable presentation with reachable submit/cancel. | Field IDs, values, protection, limits, revision and structured edit/submit/cancel. |
| Identity/footer | Preserve prefix, TOF/EOF, file tabs, real command/status/guide. Reserve auxiliary layout before editor layout. | Core-rendered editor/status/guide retained. | Render core status and wrapping reserved guide; no independently invented status truth. | Semantic rows/status/guide plus independent workbench state. |

Opening/showing and focusing are separate operations. New output, waiting stdin,
diagnostics or a newly visible pane may indicate attention but never steals
typing focus. Return restores the exact editor zone and logical caret, including
prefix/command focus. Menu cancellation restores the invoking control. Closing a
focused pane returns to the prior valid editor view; if it closed, choose an
explicit documented fallback and report it. Background completion never makes
an unsubmitted THE command or program input execute.

Legacy driver modules can receive a core-rendered popup/text fallback if their
ABI supports it; otherwise retain typed command/query access and return a
deterministic unavailable-renderer reason. New workbench callbacks must be
optional/versioned; changing shared structs requires the project's ABI version
discipline. Drivers must not import one another's widget types.

## LLM and browser protocol extension

Workbench data uses `schemaVersion: 1`. This is independent of the existing
driver ABI number and does not authorize changing shared ABI structs. Add a
versioned `workbench` section to capability discovery and semantic snapshots,
with separate revisions for definitions, panel content and job state.
Retain existing `look`, `delta`, `tools`, `command` and panel fields for
compatibility. Advertise actual supported inputs, including existing tool
inspection/polling. Phase 1 uses the existing LLM `command` path for the frozen
command grammar and existing `EXTRACT` bridge for JSON pages. A schema-version
field alone does not advertise a new input verb or later-phase operation.

The later structured request extension uses one UTF-8 JSON object per message,
with required `type: "workbench"` and `version: 1`. Browser transports pass that
object to the normalized request layer. For the line-oriented LLM transport,
the reserved form is `workbench ` followed by that compact JSON object on one
line; strings containing line breaks use JSON escapes. There is no shell-like
quoting around the JSON. This verb is advertised only once its generation,
receipt and refusal checks are implemented and tested. Example later requests:

```json
{"type":"workbench","version":1,"operation":"invoke","action":"user.run","requestId":"r17","expectGeneration":3}
{"type":"workbench","version":1,"operation":"field.set","panel":"user.personpane","field":"name","value":"Alice","expectRevision":8,"requestId":"r18"}
{"type":"workbench","version":1,"operation":"panel.apply","panel":"user.settings","expectDocumentRevision":"doc7:12","requestId":"r19"}
```

Unknown versions refuse with `unsupported-feature` and no mutation; malformed
JSON or unknown/duplicate fields refuse with `invalid-operand`. Each operation
has a fixed typed schema; arbitrary JSON keys cannot become command operands.
Responses identify version, request ID, stable result code, numeric THE RC, resulting
revision and any job ID/effective placement. Deltas identify removed objects and
omitted content. A bounded request receipt cache prevents a transport retry from
running the same job twice. Within a connection epoch the same request ID with
the same complete payload returns the retained receipt without execution;
the same ID with a different payload returns `invalid-operand`. Expired/unknown
receipts and reconnecting clients require explicit inspection/reconciliation,
never blind replay. Generations reject events for removed and recreated
objects. Structured declaration strings use JSON's own escaping; they are
typed values, not text fed back through the command tokenizer.

Agents inspect a panel or stream by ID with bounded paging; they do not parse
terminal output or menu labels to infer jobs. Unsupported operations produce the
same code as human commands. Logs/field values are untrusted data and cannot
reconfigure actions or change the agent's declared policy. Browser transports
enforce the same core policy even if a client sends forged direct requests.

## Phased delivery and acceptance

Each phase is a separate reviewable increment in the authorized order. Phase 0
freezes the contract and public fixtures; completion of later phases requires
the acceptance evidence below. Commit/push and releases remain separate
authorization boundaries. The contract fixtures are
[`tests/fixtures/workbench-contract-v1.json`](../tests/fixtures/workbench-contract-v1.json);
they are non-executable acceptance data, not evidence of a passing runtime.

| Phase | Bounded delivery | Acceptance gate |
| --- | --- | --- |
| 0. Freeze contract | IDs, quoting, count/byte limits, ownership, numeric RC mapping, inspection schema and later protocol version are fixed here. | Public fixtures describe success/refusal/lifetime without platform-specific command meanings. Existing features remain distinct from staged contracts. |
| 1. Actions and menus | C-owned registry, definition transactions and activation-scoped publication, QUERY/EXTRACT, capability/refusal records; default profile generated definitions; adapters for existing static actions. | One profile adds/replaces/removes a menu/item on terminal, GTK, browser and LLM; replay has no duplicates; no declaration executes code; late profile failure after explicit COMMIT retains the prior published graph; invalid reference creation/removal publishes nothing; cancellation and keyboard traversal preserve editor caret and pending command. |
| 2. Panels and existing console | Named log/document panes, placement/fallback model, existing singleton job integration, typed stdout/stderr routes and semantic snapshots. Preserve `RX` commands. | Existing Classic/RXBIN run, input, EOF, Stop and hide/show work through all qualified hosts; wrong-host tools are disabled with reasons; source bytes/cursor/dirty/save state are preserved; wide/narrow/phone and real-PTY resize pass. |
| 3. Forms and file fields | Independent named forms and qualified field renderers, then staged editable projections after core edit-group/revision support. Read-only document panes belong to phase 2. | Protected/Unicode fields, submit/cancel, readonly, stale revision, offscreen text/markup and multi-record validation tested. An accepted apply is undoable as specified and save/reopen matches bytes; cancelled/rejected apply changes nothing. |
| 4. File IO and additional execution adapters | Bounded exact-byte file stdin and staged CREATE/REPLACE/APPEND output with conditional atomic publication; explicit native ooRexx/process registration and dependency/authority checks; DSLSH diagnostic panes without protocol rerouting. | File bytes, finite-input EOF, collisions/stale targets, readonly/workspace refusal, raw binary capture and failure-before-publication preservation pass on each qualified host; unsupported atomic modes refuse explicitly. Actual ooRexx executable on each claimed native platform; argv/IO/conditions/Stop preserve editor; browser refusal and stale parser replies are tested. No interpreter substitution. |
| 5. Optional concurrency and persistence | Job table, selected job per pane, retained model quotas and user-selected durable workspace definitions. | Concurrent output/EOF/Stop never crosses job IDs; removing panes/owners retains Stop/log access; restore validates schema/cohort/trust and never restarts jobs automatically. |

Focused checks should extend `test_frontendaction`, `test_toolhost`,
`test_native_tools`, `test_panels`, `test_workbench_curses`,
`test_tool_console_curses`, GTK probes and browser workbench/panel smoke tests.
Agent/core checks use the real `the --driver llm` runtime; terminal input/resize
uses a real PTY; browser behavior uses rendered browser acceptance. Run affected
checks and the required complete candidate suite for implementation changes.
Contract-only documentation/fixture edits require JSON and documentation
validation; they do not by themselves require a compiler/test run.

Required cross-cutting acceptance cases:

- A poisoned label, quoted path, malformed option or field value cannot create
  a second command, macro, shell invocation or browser markup.
- Unknown commands, menu invocation, synonyms and direct protocol requests give
  consistent authority/refusal results. Native explicit OS remains explicit.
- Open a file/action target through each surface's chooser or path interface,
  including names with spaces and Unicode; cancel and retry without changing
  bytes/cursor. Test keyboard-only terminal selection, GTK/browser selection
  and browser import, plus LLM explicit paths. A chosen path outside policy is
  rejected just like the same typed path; a chooser never expands authority.
- A delayed run uses its bound buffer and captured pending record; buffer close,
  modal READ, readonly and missing runtime produce deterministic results.
- A profile commits definitions and subsequently fails: no candidate menu,
  panel, route or action becomes visible. A successful activation publishes
  once. Missing-reference creation and referenced-action removal are rejected;
  removing/rebinding all dependents in one transaction succeeds.
- File stdin preserves empty/binary input and sends EOF exactly once after all
  bytes drain. File stdout/stderr capture preserves NUL, invalid UTF-8 and chunk
  boundaries. Verify CREATE collision, REPLACE/APPEND stale tokens, policy and
  symlink changes, output quota, Stop and injected storage/commit failure; each
  rejected or confirmed pre-publication failure leaves target bytes or absence
  unchanged. Lost commit acknowledgement produces `publication-unknown` and
  requires read-back reconciliation without blind append replay. Success
  publishes the expected complete file, survives reopen/reload and reports its
  token/length only after storage commit. Dirty editor targets remain protected.
- Hide/remove/recreate a pane during output; reconnect/retry a launch; deliver
  stale job/field/action events. No cross-job output, duplicate execution, lost
  staged values or reentrant editor stack is admitted.
- Test Ctrl/Meta shortcuts, keyboard-only entry/traversal/return, menu cancel,
  user scrolled output and active prefix/command caret on every claimed surface.
- Verify profile status/reserved guide, TOF/EOF, prefix/file ring, parser states
  and ordinary THE editing survive every auxiliary-pane transition.
- For mutations assert text, logical cursor, dirty state, undo/recovery behavior
  and saved/reopened bytes. Hosted CI, installed packages, visual platform QA,
  signing and release qualification require their own evidence.

## Settled defaults and qualification boundaries

The user's staged implementation order settles the first-scope decision. The
proposal recommendations are the defaults for the remaining design choices:

1. Use one explicit `WORKBENCH` command family; user aliases use existing
   `SET SYNONYM` after the command exists.
2. Deliver shared actions/menus, then named log/read-only document panes, then
   independent forms and separately qualified editable file projections, then
   exact file IO and additional execution adapters. Do not move editable fields
   ahead of the core revision/edit-group proof.
3. Use ordered left/right/below auxiliary regions with below/tab fallback,
   preserving editor minima and explicit user splitter preferences. Arbitrary
   split trees remain deferred.
4. Retain a clearly labelled Classic Rexx action. A future generic Run requires
   a visible explicit language/tool resolution and refusal on ambiguity.
5. Profiles authoritatively recreate definitions. Persist user layout preferences
   separately; never persist or automatically restart runnable state implicitly.
6. Keep one program job initially, reserving stable job IDs and detached-job
   Stop/status/log access. Language-service sessions remain separate.
7. Register named native adapters/executables with separate argv. Existing
   explicit `OS`/`!` follows native policy and does not become a shell tool
   protocol or gain browser authority.
8. Preserve fixed CTLCHAR cell semantics. Optional flow hints apply only to
   independent named forms; file byte spans are never reflowed.

The phase-1 limits are frozen implementation constraints. Numeric ceilings
listed for phases 2 through 4 are explicit initial design defaults for those
unimplemented, host-qualified stages; qualification may lower advertised limits
before accepting work. Capability discovery must identify the actual phase and
supported operations. Documentation of a later contract is not an availability
claim, and source qualification is not hosted CI, installation or release proof.

Tangents retained for later: generic terminal emulation, arbitrary native/DOM
widgets, network plugin installation, shell pipelines, rich binary viewers and
global undo redesign. None is required to generalize the present editor and
Program console into the shared profile-owned workbench described here.
