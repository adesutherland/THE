# JOIN
**join a line with the line following**

## Syntax
```text
Join [ALigned] [Column|CURSOR]
```

## Description
The JOIN command makes one line out of the focus line and the line following.
If Aligned is specified, any leading spaces in the following line are ignored. If Aligned is not
specified, all characters, including spaces are added.
If Column (the default) is specified, the current line is joined at the current column location.
If CURSOR is specified, the focus line is joined at the cursor position.

## Compatibility
XEDIT: Compatible.
Does not support Colno option
KEDIT: Compatible.

## See Also
SPLIT, SPLTJOIN

## Status
Complete.
