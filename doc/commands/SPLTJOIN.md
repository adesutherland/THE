# SPLTJOIN
**split/join two lines**

## Syntax
```text
spltjoin
```

## Description
The SPLTJOIN command splits the focus line into two or joins the focus line with the next line
depending on the position of the cursor.
If the cursor is after the last column of a line, the JOIN command is executed, otherwise the SPLIT
command is executed.
The text in the new line is aligned with the text in the focus line .
This command can only be used by assigning it to a function key.

## Compatibility
XEDIT: Compatible.
KEDIT: Compatible.

## See Also
JOIN, SPLIT

## Status
Complete.
