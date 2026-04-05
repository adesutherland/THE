# SOS BLOCKSTART
**move cursor to start of marked block**

## Syntax
```text
SOS BLOCKStart
```

## Description
The SOS BLOCKSTART command moves the cursor to the starting line and column of the marked
block. If the cursor is on the command line, the first line of the marked block becomes the current
line.
If no marked block is in the current file, an error is displayed.

## Compatibility

XEDIT: N/A
KEDIT: Compatible.

## See Also
SOS BLOCKEND

## Status
Complete.
