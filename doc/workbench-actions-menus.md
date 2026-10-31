# Shared workbench actions and menus

Phase 1 is implemented in the current development source: profiles and macros
can define actions, menus and items in one C-owned session registry. Terminal,
GTK, native browser, browser/Wasm and LLM consumers use the published definitions.
Check an installed package's version before applying this guide to an older
release. The [staged design](workbench-extension-design.md) describes later
panels, forms, routes and execution adapters; those features are outside phase 1.
Existing [RX tools and Program console](runtime-tools.md) remain available.
The implemented [phase-2A LOG core](workbench-log-panes.md) adds named log panes
and stream routes; [phase-2B DOCUMENT panes](workbench-document-panes.md) add
bounded read-only views of live editor buffers.
The [phase-3A form core](workbench-forms.md) adds independently staged values and
structured submitted results without editor writes.

## Define a menu

This cREXX profile adds a Tools menu containing the existing Save action. For
Regina, omit `options levelb`; the addressed commands are the same.

```rexx
options levelb
address THE
'workbench begin owner user.tools';
'workbench action put user.save label "Save current file" builtin file.save';
'workbench menu put user.tools label "Tools" order 50';
'workbench item put user.tools user.saveitem label "Save current file" action user.save';
'workbench commit';
return 0
```

The declaration changes no file bytes or cursor position and does not save the
file. Choosing the item, or entering `WORKBENCH INVOKE user.save`, explicitly
invokes Save. `PUT` replaces the same owner's object with that ID, so replay
creates no duplicate entries. IDs do not introduce new editor command names;
use existing `SET SYNONYM` facilities for command aliases.

To remove these definitions together:

```text
WORKBENCH BEGIN OWNER user.tools
WORKBENCH MENU REMOVE user.tools
WORKBENCH ACTION REMOVE user.save
WORKBENCH COMMIT
```

Run that removal in the context that owns `user.tools`: a profile cannot be
impersonated by a later interactive command. Removing a menu removes its items
and leaves its actions. Removing a referenced action alone fails at commit.

## Implemented command grammar

Keywords require full spelling and are case-insensitive. IDs and builtin action
names are case-sensitive. IDs are unique within each object kind; item IDs are
unique across all menus. Quoted operands below require double quotes.

```text
WORKBENCH BEGIN OWNER owner-id
WORKBENCH COMMIT
WORKBENCH ABORT
WORKBENCH RESET OWNER owner-id

WORKBENCH ACTION PUT id LABEL "label" BUILTIN builtin-id [ARGUMENT "value"]
WORKBENCH ACTION PUT id LABEL "label" COMMAND "explicit THE command"
WORKBENCH ACTION PUT id LABEL "label" MACRO "macro-path"
WORKBENCH ACTION REMOVE id
WORKBENCH INVOKE action-id [EXPECT generation]

WORKBENCH MENU PUT menu-id LABEL "label" [ORDER integer]
WORKBENCH MENU REMOVE menu-id
WORKBENCH ITEM PUT menu-id item-id LABEL "label" ACTION action-id [ORDER integer]
WORKBENCH ITEM PUT menu-id item-id SEPARATOR [ORDER integer]
WORKBENCH ITEM REMOVE menu-id item-id

QUERY WORKBENCH [SUMMARY|CAPABILITIES|ACTIONS|MENUS|ITEMS] [OFFSET integer] [LIMIT integer]
EXTRACT /WORKBENCH [SUMMARY|CAPABILITIES|ACTIONS|MENUS|ITEMS] [OFFSET integer] [LIMIT integer]/
```

One operation occupies one command. Space and tab separate tokens outside
quotes. Double a quote inside a quoted string: `LABEL "Say ""hello"""` stores
`Say "hello"`. Backslashes are literal, including in Windows paths. Labels are
UTF-8 data; `/`, markup characters and the active LINEND byte are not executable
label syntax. Malformed quoting, controls, extra or duplicate options and
invalid UTF-8 are rejected. Labels and command/macro operands must be nonempty.
`ORDER` defaults to zero and accepts 0 through 2,147,483,647; equal orders retain
creation order. Menus are flat; there is no submenu definition syntax.

| Builtin ID | Argument |
| --- | --- |
| `file.create` | File path |
| `file.open` | File path |
| `buffer.switch` | File path |
| `file.save` | None |
| `file.close` | None |
| `edit.undo` | None |

A builtin binds one typed argument with `ARGUMENT`. Path arguments reject quotes
and control characters and remain data when LINEND is enabled. A builtin keeps
its editor meaning even if a user synonym shadows Save, Open or another verb.
When a path is unbound, graphical/terminal menus acquire it through their path
UI; a command-line invocation needs a bound argument. `INVOKE` has no trailing
path or argument-list syntax.

`COMMAND` stores a fixed literal command and uses normal editor dispatch,
including synonyms, only when invoked. Values are never substituted into that
command. `MACRO` uses the existing macro interpreter and macro search path;
phase 1 accepts no `ARG` vector. Its path cannot contain a double quote and its
resolved filename must fit the editor's 1,000-byte filename limit. Defining an
action neither runs the command nor proves that a macro/runtime/path exists.

## Transactions and profile activation

All definition changes, including `RESET OWNER`, require one explicit,
non-nested transaction. `RESET OWNER` must name the transaction's owner and
removes that owner's declarations. An absent `REMOVE` succeeds; `ABORT` also
succeeds when no transaction is open.

A rejected definition operation leaves the published graph intact and marks the
draft failed. `COMMIT` then refuses until `ABORT`. References are validated
against the complete candidate graph at commit, so items and actions may be
added, removed or rebound in any order within one transaction. References may
target the same owner or builtins; cross-owner references are refused. Query or
invocation failures do not poison an otherwise valid definition draft.

New owners bind to the current session, or to the active profile's execution
context. A context cannot replace another context's definitions. `the.*` IDs
and owners are reserved for builtins. Ownership grants no operating-system
privileges. Owner bindings remain after reset and count toward the session
owner limit.

Outside profile activation, successful `COMMIT` publishes one new revision.
Profile activation supplies an outer scope, inherited by called macros: inner
commits update a candidate, and publication occurs once after the whole profile
succeeds, with no failed definition operation or open transaction. A later
failure discards all candidate definitions, including earlier inner commits.
Rerunning a successful profile replaces its owned definitions and removes
omitted ones. Arbitrary editor changes and explicit external effects retain
existing profile rollback limitations.

Inspection normally reads published objects. Inside a profile it can read the
committed activation candidate; an open transaction's uncommitted draft is not
published by querying it. Candidate actions cannot run before activation
completes. Interactive startup continues after a profile error with the prior
graph, a visible error and `lastResult.code` of `activation-failed`. Batch and
explicit activation report failure.

Restricted native-browser and Wasm sessions admit typed builtin definitions,
not arbitrary `COMMAND` or `MACRO` actions. Trusted startup profiles can declare
those builtin menus. Importing or compiling a workspace profile/image does not
grant declaration trust. Invocation rechecks generation, current path/session
policy and modal state; a disabled UI item is not an authorization check.

## Use the published menu

- **Terminal:** the default F10 binding opens the shared menu. Arrows or
  Tab/Backtab move, Enter selects, and Escape cancels. An unbound path opens a
  directory/prefix list: type a path, use Up/Down to select, Tab to complete,
  Enter to enter a directory or choose a file, and Ctrl-U to clear.
- **GTK:** the application menu uses the shared definitions. Default F10 opens
  it; path actions use a native chooser. A profile's replacement F10 binding
  retains its authority.
- **Browser and Wasm:** Menu or F10 opens the shared entries. Arrows,
  Home/End and Enter navigate; Escape returns to the invoking control. Open
  uses the workspace explorer; other unbound path actions use a path prompt.
- **LLM:** inspect and invoke IDs through normal commands, without mouse input.

Cancellation preserves the caller's pending command and editor caret. Invoking
an item retains the selected action's normal editing/focus behavior. The default
native Workbench menu is generated from the [shared profile definition](../profiles/default-profile.json);
regenerate it with the [profile generator](../scripts/generate-default-profiles.py). The generated popup
macro remains the fallback for drivers without shared-menu presentation.

## Inspect and invoke from LLM or Rexx

```text
capabilities
command query workbench capabilities
command query workbench actions offset 0 limit 8
command query workbench menus
command query workbench items
command workbench invoke user.save
look full compact
```

LLM capabilities advertise workbench schema 1 and command-based invocation.
`look` and `delta` include a `workbench` summary. `QUERY` JSON appears in the
response/snapshot's `last_message`; there is no separate LLM `workbench` JSON
input verb. Use the reported generation with `WORKBENCH INVOKE id EXPECT n`
when a queued selection must reject a replaced action.

`SUMMARY` reports counts, limits, transaction/activation flags and
`lastResult` (`code`, numeric `rc`, human `message`). `CAPABILITIES` reports the
implemented feature set and limits. `ACTIONS`, `MENUS` and `ITEMS` return pages
with `schemaVersion`, published `revision`, `section`, `total`, `offset`,
`nextOffset` and `records`. Follow `nextOffset` until it is null; an offset past
the end returns an empty page. Action records include kind, builtin/operand,
owner, generation, `requiresArgument`, `enabled` and `reason`.

`EXTRACT` returns one JSON page: `WORKBENCH.0 = 1`, `WORKBENCH.1 = JSON` with
Regina. With cREXX, expose a `.string[]` named `workbench` on the addressed
EXTRACT and read `workbench[0]` and `workbench[1]`. Query results are data and
must not be sent back as editor commands.

Common refusal codes are `invalid-operand`/`missing-action`/`missing-object`
(RC 5), ownership/policy/transaction/activation/generation/modal refusals
(RC -3), and `limit-exceeded` (RC 94). A dispatched command failure is
`command-failed` with its original numeric RC. An invocation cycle returns
`object-in-use`; invocation nesting beyond eight returns `limit-exceeded`.
Inspect the stable code rather than parsing the message.

## Limits and adapter boundary

| Limit | Value |
| --- | --- |
| ID | 63 ASCII bytes; starts with a letter, then letters/digits/`.`/`_`/`-` |
| Label | 255 UTF-8 bytes |
| Stored action operand | 2,048 bytes; action-specific checks also apply |
| WORKBENCH operand text | 8,192 bytes |
| Actions / menus / items / owners | 128 / 32 / 256 / 32, including builtins |
| Inspection page | 1–8 records; default 8 |
| JSON output buffer | 65,536 bytes; no truncated JSON |
| Active invocation nesting | 8; active-ID cycles are refused |

The [core API](../src/workbench.h) provides published revision/count/accessors,
action state, JSON pages and explicit invocation. Borrowed object pointers
expire on publication. Drivers copy an ID/generation and queue
`THE_INPUT_WORKBENCH_INVOKE` through the [normalized input API](../src/inputevent.h);
the editor invokes it at its input boundary. Choosers can supply the typed
builtin argument without constructing a command string. Driver ABI 3 remains
unchanged.

Browser transports additionally use version-1 `workbench` list/invoke messages
with action ID, expected generation and a request ID composed from the server's
session nonce plus a monotonically increasing decimal sequence. A bounded cache
retains 256 exact-payload receipts. Retrying a retained ID returns its receipt;
conflicting or expired IDs refuse instead of executing again. A queued receipt
is distinct from the final result. See the [shared transport interface](../src/websession.h)
for the wire adapter's current fields.

Source tests cover real-editor transactions, inert declarations, profile
rollback, engine EXTRACT, typed dispatch and refusal boundaries. Graphical,
terminal, installed-package, hosted-CI and release qualification remain separate
checks described in the [testing guide](testing.md).
