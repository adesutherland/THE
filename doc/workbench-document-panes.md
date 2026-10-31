# Read-only workbench DOCUMENT panes

Phase 2B adds a bounded view of an existing editor buffer. A DOCUMENT pane binds
that buffer's immutable identity, including unsaved working-record bytes. Reading
does not commit a record, save a file, move the editor cursor, change dirty state
or create an execution source snapshot. FILE-path snapshots, editable fields and
forms remain later contracts in the [staged design](workbench-extension-design.md).

## Declare and inspect

```text
WORKBENCH BEGIN OWNER user.docs
WORKBENCH PANEL PUT user.reference KIND DOCUMENT BUFFER ACTIVE ACCESS READONLY
WORKBENCH PANEL PLACE user.reference RIGHT FALLBACK BELOW,TAB MINCOLS 32
WORKBENCH COMMIT
WORKBENCH PANEL SHOW user.reference
QUERY WORKBENCH DOCUMENT user.reference LIMIT 8 BYTES 4096
```

`BUFFER ACTIVE` binds the currently active buffer when PUT runs. Changing focus
later does not retarget the pane. Several panes, including panes owned by different
profiles, can view the same document without copying it into a second editable
model. Definition transactions and profile rollback retain their existing
[action/menu](workbench-actions-menus.md) rules. Declaring a pane neither shows
nor focuses it. SHOW/HIDE/FOCUS, placement and RETURN use the
[same runtime state](workbench-log-panes.md) as LOG panes; a frontend must provide
a DOCUMENT renderer before the semantic visibility request has a physical view.

Closing the buffer invalidates its identity before names or lines are freed.
The pane remains defined with `documentAvailable: false` and
`documentReason: "document-closed"`. Opening the same path, or reusing an allocation,
creates a different document ID. Repeating PUT is an explicit new binding; there
is no automatic path lookup or rebinding. Existing closed panes do not prevent
unrelated definition transactions from committing.

The implemented grammar is:

```text
WORKBENCH PANEL PUT panel-id KIND DOCUMENT BUFFER ACTIVE ACCESS READONLY
QUERY WORKBENCH DOCUMENT panel-id [EXPECT revision] [LINE integer] [OFFSET integer] [LIMIT integer] [BYTES integer]
EXTRACT /WORKBENCH DOCUMENT panel-id [EXPECT revision] [LINE integer] [OFFSET integer] [LIMIT integer] [BYTES integer]/
```

Other PANEL operations retain their existing grammar. `KIND DOCUMENT FILE "path"`
returns `unsupported-feature`; it does not open a buffer. Access modes other than
READONLY and field/form operations are unsupported.

## Tokens, pending records and refusals

PANELS records for DOCUMENT include `readonly: true`, `documentId`,
`documentRevision`, `documentAvailable`, `documentReason`, `lineCount` and `dirty`,
alongside common placement, generation and presentation state. Document IDs,
document revisions and panel generations are decimal JSON strings. Preserve
them without JavaScript Number conversion.

The revision is a conservative session edit epoch. Commands, editor events and
macro execution can invalidate it even if they ultimately change no bytes.
An old EXPECT returns `stale-revision` RC -3. Query PANELS or omit EXPECT on a
fresh owner-thread read to obtain the current revision. Inspection itself does
not advance that epoch. A top-level admitted QUERY, including physical command-line
Enter, can inspect the document; a synonym or implicit macro shadowing QUERY
retains ordinary guarded dispatch.

The pending record is included only with an explicit file/line binding whose
captured LINE base still matches. A certified TOF/EOF sentinel is safely excluded
from content. Unknown scratch ownership or a direct base rewrite returns
`record-unavailable` RC -3 until normal editor processing rebinds the record.
Inspection never reloads or discards scratch bytes to manufacture a result.

An active macro, modal panel or unsafe/reentrant edit returns `busy-document`
RC -3. In particular, DOCUMENT content EXTRACT inside an executing macro is
recognized but refused; metadata EXTRACT remains available. Finish execution
before fetching content through QUERY or a frontend's read-only page request.
A failed EXTRACT retains its previous WORKBENCH stem values. Closed documents
return `document-closed` RC -3; invalid ranges return RC 5; insufficient capacity
or a byte budget smaller than the next UTF-8 scalar returns RC 94.

## Bounded logical-line pages

LINE is one-based and defaults to 1; OFFSET is a byte offset within that logical
line and defaults to zero. LIMIT accepts 1–8 records, default 8. BYTES accepts
1–8,192, default 4,096. Long lines continue through `nextLine` and `nextOffset`;
`more` indicates remaining content. Valid UTF-8 scalars are never split, and an
offset inside one is refused. Malformed octets remain individually addressable.
Empty logical lines have zero-length records; editor TOF/EOF sentinels are absent.

Each result echoes the panel ID/generation and document ID/revision. Records
carry `line`, `offset`, `length`, `originalLength`, display `text` and exact
`bytesHex`. NUL and controls are escaped; malformed UTF-8 displays replacement
characters while bytesHex preserves the original octets. Render text as data,
without interpreting terminal controls or HTML. JSON may return less than the
requested byte budget to reserve room for both representations in the existing
65,536-byte bridge buffer. Failure never returns truncated JSON.

The [C service](../src/workbenchdocument.h) copies at most eight slices and
8,192 bytes into caller-owned storage. No LINE, working-record or file pointer
escapes. The [editor adapter](../src/editordocument.c) binds actual record
ownership and forgets closed files. Its single-record base guard is capped at
1 MiB; larger unqualified working records refuse inspection. Drivers keep their
own pane viewport and use generation-checked focus events; scroll positions do
not move the editor's logical cursor.

For LLM sessions, prefix QUERY with `command`, for example
`command query workbench document user.reference limit 8 bytes 4096`.
Core tests, physical frontend qualification, installed packages, hosted CI and
release publication remain separate checks.
