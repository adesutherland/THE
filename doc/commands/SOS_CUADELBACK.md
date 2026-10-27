# SOS CUADELBACK
**delete the character to the left of the cursor**

## Syntax
```text
SOS CUADELBAck
```

## Description
The SOS CUADELBACK command deletes the character to the left of the current cursor position. It
differs from SOS DELBACK in the case when the cursor is in the first column of the file and in the
FILEAREA. Then, the current line is joined to the previous line, and the cursor remains at the join
boundary.

UTF file editing deletes a complete grapheme cluster. The edit is committed before
refresh and is written to disk by SAVE.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## See Also
SOS DELBACK, SOS CUADELCHAR

## Status
Complete
