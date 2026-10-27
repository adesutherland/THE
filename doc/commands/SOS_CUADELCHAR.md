# SOS CUADELCHAR
**delete character under cursor**

## Syntax
```text
SOS CUADELChar
```

## Description
The SOS CUADELCHAR command deletes the character under the cursor. Text to the right is shifted
to the left. It differs from SOS DELCHAR in the case when the cursor is after the last character of the
line and in the FILEAREA. Then, the next line is joined with the current line.

UTF file editing deletes a complete grapheme cluster. The edit is committed before
refresh and is written to disk by SAVE.

## Compatibility

XEDIT: N/A
KEDIT: N/A

## See Also
SOS CUADELBACK, SOS DELCHAR

## Status
Complete
