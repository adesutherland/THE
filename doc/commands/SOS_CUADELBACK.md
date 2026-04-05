# SOS CUADELBACK
**delete the character to the left of the cursor**

## Syntax
```text
SOS CUADELBAck
```

## Description
The SOS CUADELBACK command deletes the character to the right of the current cursor position. It
differs from SOS DELBACK in the case when the cursor is in the first column of the file and in the
FILEAREA. Then, the cursor first moves to the last character of the previous line, and deletes this
character.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## See Also
SOS DELBACK, SOS CUADELCHAR

## Status
Complete
