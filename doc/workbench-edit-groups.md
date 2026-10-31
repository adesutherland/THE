# Atomic editor edit groups

Phase 3B provides a core service for replacing existing logical lines as one
undoable group. It includes native command and macro adapters. Independent
[forms](workbench-forms.md) still have no editor binding: editable file-field
projections and their frontend controls remain Phase 3C work.

## Commands and write tokens

```text
QUERY WORKBENCH WRITE [DOCUMENT document-id]
EXTRACT /WORKBENCH WRITE [DOCUMENT document-id]/

WORKBENCH EDIT APPLY DOCUMENT document-id EXPECT write-revision LINE number BEFORE "old line" AFTER "new line" [LINE number BEFORE "old line" AFTER "new line" ...]
WORKBENCH EDIT UNDO group-id DOCUMENT document-id EXPECT write-revision
```

WRITE defaults to the active buffer. It returns a bounded schema-1 JSON object:

```json
{"schemaVersion":1,"section":"write","documentId":"7","writeRevision":"123","group":"31","undoAvailable":true,"parserAvailable":true,"autosaveRc":0}
```

Use the actual returned IDs and revisions. Every identity/revision is a decimal
string in JSON. Command operands require positive canonical decimals. APPLY
checks every complete BEFORE image against the logical editor bytes, including
a certified pending working record. Multiple LINE clauses commit together.
Strings use the existing doubled-double-quote convention; backslashes are
literal. Values are data in the typed C API. WORKBENCH owns its delimiters and
does not accept a chained editor command. EDIT's single-command admission also
rejects the active LINEND byte; turn LINEND off before capturing a token when
line values contain that byte.

A write token combines the immutable live document ID with a separate
conservative mutation epoch. It is **not** a DOCUMENT paging revision. Ordinary
editing, navigation, configuration, macro entry, SAVE and reload admissions may
stale it, including unsuccessful commands. Nested editing commands also stale
it. Pure resolved QUERY/EXTRACT, copied inspection, and independent form-value
requests do not. Safe command-record typing and navigation preserve it, so a
user can type APPLY after QUERY WRITE. A redefined key, synonym or multi-command
LINEND input cannot borrow that exception to mutate before EXPECT validation.

An explicitly admitted synchronous macro can EXTRACT WRITE and then APPLY.
Returning from that macro alone does not stale its result. A later mutating
command does. Profile activation and modal READ refuse core write admission;
modal READ's command input remains owned by that modal operation. Unknown
working-record ownership returns `record-unavailable`; the service never
flushes or guesses the record owner to make a request succeed.

## Atomicity, undo and limits

The C API accepts at most 128 distinct existing lines. All retained before/after
text, validation copies and pending-record snapshots together must fit 1 MiB.
The command adapter additionally retains the existing 8192-byte command,
2048-byte quoted operand and 32-token bounds; it can carry at most four LINE
clauses. Neither interface inserts or deletes logical lines. Before/after
images must be valid UTF-8 without NUL, CR or LF. New bytes must fit current
WIDTH and SET TRUNC. A conditional undo restores the exact existing before-image
even if it originally exceeded the unchanged SET TRUNC limit.

Preparation copies and validates all images and allocates replacements,
record bindings and undo storage. Commit checks identity, write revision,
ownership, bytes, read-only policy, WIDTH and TRUNC again. A refusal changes no
editor bytes, caret, prefix draft, line metadata, dirty counters or previous
undo group. A successful commit publishes all changed lines, the working record,
dirty state and new revision together. Alteration counters advance once and
saturate rather than wrapping to a false clean state. Unchanged clauses do not
flush a pending record or mark an unchanged line.

One latest group is retained per session. UNDO requires that group's ID,
document ID and exact after-revision, and restores the entire group. A later
ordinary SAVE or mutation invalidates eligibility; refreshing a token does not
make an old group eligible again. A successful new group replaces the preceding
undo group. Failed requests and no-ops preserve it. Closing a document forgets
its group; reopening the same path creates a different document ID. Existing
SOS UNDO and RECOVER retain their prior behavior; frontend Undo integration is
not added by this core phase.

## Parser and recovery checkpoints

For native DSLSH-backed files, commit shuts down the old parser snapshot after
all allocations and refusals have finished, then publishes the complete edit
without callbacks or fallible line-update helpers. The parser remains inactive:
`parserAvailable` is false, old styles/diagnostics disappear, and SDSLHWAIT
reports that no parser is active. Explicit `SET COLORING ON AUTO` reselects and
loads the parser from current source; SDSLHWAIT can then qualify that separate
operation. This also handles a previously undiscovered magic-string parser.
The edit tail never calls the existing parser loader, whose dependency can
terminate on allocation failure. `parserAvailable` describes native DSLSH;
other hosts retain their own parser status. Wasm invalidates its worker snapshot
after complete publication and refreshes through its existing source lifecycle.

AUTOSAVE runs only after the whole group is published. Recovery checkpoints use
the certified logical pending record without posting it. Undo rewrites a group
checkpoint with the restored bytes, preventing `.aus` from retaining undone
content. Checkpoint failure leaves the edit/undo successful and is reported in
`autosaveRc`; it does not roll back the in-memory group. An inactive buffer whose
checkpoint needs a current view reports -3 instead of borrowing another view.
Parser inactivity and checkpoint status are inspectable separately from commit
success. Ordinary explicit SAVE/reopen remains the persistence check.

## C owner API

[`editgroup.h`](../src/editgroup.h) exposes `the_edit_group_token`,
`the_edit_group_prepare`, `the_edit_group_commit`, `the_edit_group_abort`,
`the_edit_group_undo`, and copied info/receipt accessors. Requests use explicit
byte lengths. A successful commit consumes its plan; a refused commit leaves
the plan abortable. These functions run on the editor owner and never from a
renderer callback. [`editorgroup.c`](../src/editorgroup.c) supplies the editor
provider; drivers do not own replacement buffers or a second document model.

Refusals include `stale-revision`/`document-closed`/`record-unavailable`/modal or
activation refusal (RC -3), read-only policy (12), invalid operands (5), and
allocation/retention limits (94). Receipts own their code text. Regina's normal
negative-command condition behavior remains in force for EDIT; a macro may
handle its ERROR condition and inspect the workbench result. Read-only
WORKBENCH QUERY/EXTRACT keep their existing exact numeric-result bridge.
