# Independent workbench forms

Phase 3A adds named forms whose definitions, staged values and submitted results
are C-owned session state. Forms have no implicit editor-buffer binding.
Editing, Submit and Cancel do not change file bytes, dirty state or file undo.
Submit publishes a structured result version; it does not invoke a command,
macro, program or shell. Staged [CTLCHAR file-field projections](workbench-file-fields.md)
bind live buffers separately and use the
[core edit-group and write-token service](workbench-edit-groups.md) for Apply.

## Define a form

```text
WORKBENCH BEGIN OWNER user.forms
WORKBENCH FORM PUT user.person
WORKBENCH FIELD PUT user.person name LABEL "Name" VALUE "Guest" ACCESS EDIT MAXCELLS 40
WORKBENCH FIELD PUT user.person status LABEL "Status" VALUE "New" ACCESS READONLY MAXCELLS 20
WORKBENCH PANEL PUT user.personpane KIND FIELDS FORM user.person
WORKBENCH PANEL PLACE user.personpane RIGHT FALLBACK BELOW,TAB MINCOLS 32
WORKBENCH COMMIT
WORKBENCH PANEL SHOW user.personpane
```

Form, field and pane definitions use the existing
[owner-bound transactions](workbench-actions-menus.md). A failed profile activation
publishes no candidate form values. Fields belong to their form: `(form-id,
field-id)` is the identity, so two forms may each contain a field named `name`.
IDs remain case-sensitive ASCII with the existing 63-byte bound. A FIELDS pane
may reference its owner's form; cross-owner definition changes and references
remain refused.

Definitions never display or focus a pane. Existing SHOW/HIDE/FOCUS and
generation-checked RETURN operations control presentation. Hiding, returning
from or removing a pane retains the form's staged values. Definition replacement,
field removal, form removal or owner reset that would replace a dirty draft
returns `draft-in-use` RC -3. Submit or Cancel first. Removing a referenced form
requires removing or rebinding its panes in the same transaction.

Clean definition replacement starts a new form generation and initializes values
from its declarations. It clears the previous submission. Any field definition
change also changes the form generation; queued edits for the old generation
refuse. A focused pane whose form generation changes requests a return to its
saved editor/Program origin, retaining visibility.

## Edit and publish values

Inspect FORMS to obtain the form generation and current draft revision:

```text
QUERY WORKBENCH FORMS
QUERY WORKBENCH FIELDS user.person
WORKBENCH FIELD SET user.person name VALUE "Adrian" EXPECT 1
WORKBENCH FORM SUBMIT user.person EXPECT 2
QUERY WORKBENCH FORM user.person RESULT
```

The numbers above illustrate a fresh form; callers must use the actual returned
tokens. Runtime SET and SUBMIT require EXPECT. GENERATION is optional in command
syntax and selects the current generation if omitted; typed frontend requests
require both generation and draft revision. Cancel restores the latest submitted
values, or declaration defaults before the first Submit. It never changes a
buffer. RETURN retains unsent values rather than cancelling them.

The implemented grammar is:

```text
WORKBENCH FORM PUT form-id
WORKBENCH FORM REMOVE form-id
WORKBENCH FIELD PUT form-id field-id LABEL "label" VALUE "value" ACCESS EDIT|READONLY MAXCELLS integer
WORKBENCH FIELD REMOVE form-id field-id
WORKBENCH PANEL PUT panel-id KIND FIELDS FORM form-id

WORKBENCH FIELD SET form-id field-id VALUE "value" EXPECT draft-revision [GENERATION form-generation]
WORKBENCH FORM SUBMIT form-id EXPECT draft-revision [GENERATION form-generation]
WORKBENCH FORM CANCEL form-id [EXPECT draft-revision] [GENERATION form-generation]

QUERY WORKBENCH FORMS [OFFSET integer] [LIMIT integer]
QUERY WORKBENCH FIELDS form-id [OFFSET integer] [LIMIT integer] [BYTES integer]
QUERY WORKBENCH FORM form-id RESULT [OFFSET integer] [LIMIT integer] [BYTES integer]
```

EXTRACT accepts the same inspection selectors and returns one counted JSON record
through the existing WORKBENCH stem. Failed inspection retains the previous stem.
Form runtime errors do not poison an open definition transaction. Runtime requests
during profile activation return `activation-pending`; a modal READ retains input
ownership and returns `busy-modal`.

Form generation identifies the definition lifetime. Draft revision identifies
the staged values and changes after an admitted value change, Submit or Cancel.
An unchanged SET may succeed without advancing it. Result revision changes only
on Submit; editing the next draft retains the previous result for inspection.
All generations and revisions are decimal JSON strings. Stale definitions return
`stale-generation` RC -3; stale drafts return `stale-revision` RC -3; edits to
READONLY fields return `field-protected` RC 12.

## Bounds and frontend integration

This checkpoint qualifies 32 forms, 128 fields per form and 512 fields per
session. Values are single-line valid UTF-8, at most **1,024 bytes**. The broader
4,096-byte design ceiling is not implemented. MAXCELLS accepts 1–1,024 logical
cells, and both byte and cell bounds apply. Labels retain the 255-byte limit.
Control characters, NUL, invalid UTF-8 and edits splitting a text cluster refuse
without changing the draft. Combining and wide characters use the shared core
text-position rules.

FORMS reports generation, draft/result revisions, dirty/submitted state and field
count. FIELDS and FORM RESULT return complete fields with ID/generation, label,
access, maxcells, value/length and dirty state. Pages contain at most eight records
and 8,192 value bytes, never truncated values; continue with `nextOffset`. An
insufficient byte or JSON capacity returns RC 94 without partial JSON.

The [form API](../src/workbenchform.h) owns values and provides read access,
typed SET/cluster-edit/Submit/Cancel requests and a receipt containing request ID,
completion serial, final RC/code, resulting revisions and admitted caret cell.
Receipt codes are copied, so unrelated WORKBENCH results cannot alter a completed
receipt. Drivers enqueue the
[normalized form event](../src/inputevent.h) at the editor input boundary. The
1,024-byte value and metadata fit ABI 3 without changing its structure size.
Field text is never lowered to a generated command.

Frontends serialize mutations per form. A later keystroke or Submit uses the
previous admitted receipt's revision; an external stale result requires explicit
reconciliation rather than an automatic overwrite. Browser/GTK widgets are
presentation mirrors, not owners of an independent accepted draft. Pane scroll,
field caret and input traversal remain separate from the editor cursor.

The LLM adapter supports explicit command/query usage and its qualified typed
form verbs:

```text
form set FORM GENERATION DRAFT_REVISION FIELD HEX_UTF8
form submit FORM GENERATION DRAFT_REVISION
form cancel FORM GENERATION DRAFT_REVISION
```

HEX_UTF8 contains hexadecimal UTF-8 bytes; `-` represents an empty value. The
adapter lowers these values to the same typed request and does not advertise a
general JSON input parser. Core/source tests,
physical frontend qualification, installed packages and publication remain
separate gates. [The staged design](workbench-extension-design.md) retains the
later atomic file apply and editable projection contract.
