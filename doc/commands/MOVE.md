# MOVE
**move a portion of text**

## Syntax
```text
MOVE target1 target2
MOVE BLOCK [RESET]
```

## Description
The MOVE command copies the contents of a portion of the file to the same or a different file, and
deletes the marked portion from the original file.
The first form of the MOVE command, moves the portion of the file specified by target1 to the line
specified by target2 in the same file.
The second form of the MOVE command moves the contents of the marked block to the current
cursor position. If the optional [ RESET ] argument is supplied, the marked block is reset as though a
RESET BLOCK command had been issued.

## Compatibility
XEDIT: N/A
KEDIT: Adds extra functionality with [RESET] option.

## Status
  Incomplete. First form is not supported.
