# CLIPBOARD
**manipulate system clipboard**

## Syntax
```text
CLIPBOARD COPY|CUT|PASTE|CLEAR
```

## Description
The CLIPBOARD COPY command copies the text in the marked block into the system clipboard.
The CLIPBOARD CUT command copies the text in the marked block into the system clipboard and
then deletes the marked block. The CLIPBOARD PASTE command copies the text in the system
clipboard into the current file at the cursor position. The CLIPBOARD CLEAR command clears the
contents of the system clipboard.
Only text objects in the system clipboard can be manipulated.

## Compatibility
XEDIT: N/A.
KEDIT: Compatible. Does not support APPEND or PUT options.

## Status
Complete.
