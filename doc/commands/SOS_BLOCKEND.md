# SOS BLOCKEND
**move cursor to end of marked block**

## Syntax
```text
SOS BLOCKEnd
```

## Description
The SOS BLOCKEND command moves the cursor to the ending line and column of the marked
block. If the cursor is on the command line, the last line of the marked block becomes the current line.
If no marked block is in the current file, an error is displayed.

## Compatibility
XEDIT: N/A
KEDIT: Compatible.

## See Also
SOS BLOCKSTART

## Status
Complete.
