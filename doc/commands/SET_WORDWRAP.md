# SET WORDWRAP
**set wordwrap feature on or off**

## Syntax
```text
[SET] WORDWrap ON|OFF
```

## Description
The SET WORDWRAP set command determines whether wordwrap occurs when the cursor moves
past the right margin (as set by the SET MARGINS command).
With WORDWRAP ON, the line, from the beginning of the word that exceeds the right margin, is
wrapped onto the next line. The cursor position stays in the same position relative to the current word.
With WORDWRAP OFF, no word wrap occurs.

## Compatibility

XEDIT: N/A
KEDIT: Compatible.

## Default
OFF

## See Also
SET MARGINS

## Status
Complete.
