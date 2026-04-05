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

## Compatibility

XEDIT: N/A
KEDIT: N/A

## See Also
SOS CURDELBACK, SOS DELCHAR

## Status
Complete
