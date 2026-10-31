# Staged CTLCHAR file fields

A file-field pane projects the complete logical records of one live editor
buffer. The core interprets CTLCHAR markers into protected text and fixed-cell
editable spans. Staging changes only the pane's draft; Apply commits every
changed record through one [atomic edit group](workbench-edit-groups.md).
Save remains an explicit editor operation.

```text
SET CTLCHAR ! ESCAPE
SET CTLCHAR p PROTECT WHITE
SET CTLCHAR u NOPROTECT GREEN
SET FILECTLCHAR ON
WORKBENCH BEGIN OWNER public.settings
WORKBENCH PANEL PUT public.settings KIND FIELDS BUFFER ACTIVE MARKUP CTLCHAR ACCESS STAGED
WORKBENCH COMMIT
WORKBENCH PANEL SHOW public.settings
WORKBENCH PANEL FOCUS public.settings
QUERY WORKBENCH FILEFIELDS public.settings
```

For example, `!pName: !uGuest     !p` defines protected `Name: ` and an
editable ten-cell field. The existing record determines its capacity,
including trailing spaces. A final editable span ends at the logical record's
end; viewport width never supplies additional file bytes or capacity.
The capture includes offscreen records and complete tails, together with a
certified pending working record. It does not post that record, save the file,
move the caret, or capture prefix drafts as file data.

Every row is unwrapped. Frontends may scroll the bounded pane; they consume
core-produced protected and editable spans without interpreting markup or
converting display offsets into file offsets. Protect flags apply to this
projection and modal READ, while ordinary editor commands retain their existing
behavior.

## Draft operations

```text
WORKBENCH PANEL SET public.settings FIELD s2 VALUE "Alice" EXPECT draft-revision [GENERATION panel-generation]
WORKBENCH PANEL APPLY public.settings EXPECT draft-revision [GENERATION panel-generation]
WORKBENCH PANEL RELOAD public.settings [EXPECT draft-revision] [GENERATION panel-generation]
WORKBENCH PANEL CANCEL public.settings [EXPECT draft-revision] [GENERATION panel-generation]
QUERY WORKBENCH FILEFIELDS public.settings [OFFSET 0] [LIMIT 8] [BYTES 8192]
```

Use the returned decimal-string `panelGeneration` and `draftRevision` tokens.
Span IDs (`s1`, `s2`, and so on) include protected spans and belong to that
capture. SET treats text as data, validates UTF-8 and cell width, and pads a
shorter value with spaces to its captured capacity. A value cannot contain the
CTLCHAR escape. Individual key edits use the shared core overwrite, cluster
Backspace/Delete and space-padding rules. Browser UTF-16 offsets are validated
and mapped on the editor owner; an offset inside a grapheme is refused.

Apply validates the separate captured `writeRevision`, the complete logical
before-images, CTLCHAR policy, WIDTH, TRUNC, buffer identity and read-only
policy. Rebuilt records must retain the same marker partition and fixed cell
capacities. All required result storage is prepared before publication. A
refusal preserves both the source and the complete draft; it does not guess
new offsets or rebase. A successful Apply refreshes the baseline and write
token and keeps the projection active. The edit-group receipt provides the
conditional group undo; saving or ordinary editing may invalidate it.

Reload explicitly discards the draft and captures current complete logical
records and policy. A failed Reload retains the previous draft. Cancel
restores the captured baseline and releases the buffer dependency, setting
`active:false`; Reload reactivates the pane. Return restores editor focus
without applying, cancelling or releasing the draft. Hide also preserves it.
An active pane pins its buffer, including a clean draft. QQUIT, QUIT, FILE,
FFILE, CANCEL, CCANCEL and REDIT refuse that dependency before posting a
pending record or saving. Remove a clean pane, or Cancel before closing.
Definition removal/rebinding refuses a dirty draft until it is cancelled.
Session teardown frees retained capture and draft storage. A trusted native
profile prepares captures only at its final successful workbench publication;
profile failure or capture refusal publishes none of its candidate forms,
panes or log models. The read-only capture seam does not admit Apply during
activation. Restricted and Wasm profiles retain their existing untrusted
definition policy.

## Bounds and copied inspection

The initial version accepts printable valid UTF-8 source and printable ASCII
CTLCHAR escape/tag bytes, with FILECTLCHAR and at least one CTLCHAR definition
enabled. Unsupported controls, malformed UTF-8, empty editable-only sources,
non-ASCII tags and unrepresentable captures are explicitly refused.

A pane is bounded to 128 complete logical records, 128 nonempty spans and
256 KiB of retained source/value bytes. The session permits 32 panes and
8 MiB; the 512-field total is shared with independent named forms. Each
protected or editable span retains at most 1024 UTF-8 bytes and 1024 cells.
Each complete displayed row must fit the 8192-byte inspection budget. Capture,
SET and Apply enforce these bounds before changing live state. No source row
or field value is silently truncated.

Inspection pages return at most eight complete rows and 8192 value bytes:
`schemaVersion`, `section:"file-fields"`, `panel`, `panelGeneration`,
`documentId`, `writeRevision`, `draftRevision`, `dirty`, `active`, `total`,
`offset`, `nextOffset`, and `records`. Each record has `line`, `cells`, and
`spans`; a span has `id`, `cell`, `maxcells`, `editable`, `dirty`, and `value`.
The paging offset counts logical rows. A smaller caller budget that cannot
hold one complete row returns `limit-exceeded` without partial JSON.

[`workbenchfilefields.h`](../src/workbenchfilefields.h) defines the copied
state, typed requests and owned completion receipts. Normalized input carries
SET/EDIT/APPLY/RELOAD/CANCEL/RETURN with panel generation and draft revision;
values are never interpolated into editor commands. The real LLM driver also
accepts `filefield set panel generation draft field utf8-hex` and
`filefield apply|reload|cancel|return panel generation draft`. Use `-` for an
empty SET value. Every request produces a `filefield-receipt`; the ordinary
QUERY/EXTRACT bridge exposes complete row pages.

Core refusals distinguish `stale-revision`, `stale-configuration`,
`stale-generation`, `record-unavailable`, `busy-modal`, `read-only`,
`field-protected`, `marker-injection`, `projection-released`,
`unsupported-configuration`, `unsupported-source`, and `limit-exceeded`.
