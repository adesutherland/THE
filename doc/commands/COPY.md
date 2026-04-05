# COPY
**copies text from one position to another**

## Syntax
```text
COPY target1 target2
COPY BLOCK [RESET]
```

## Description
With the first form of the COPY command, text is copied from target1 to the line specified by target2
. Text can only be copied within the same view of the file.
The second form of the COPY command copies text within the currently marked block to the current
cursor position. The text can be in the same file or a different file.

## Compatibility
XEDIT: COPY BLOCK not available.
KEDIT: Adds extra functionality with [RESET] option.
With the cursor in the marked block this command in KEDIT
acts like DUPLICATE BLOCK.

## Status
Complete.
