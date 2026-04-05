# SPLIT
**split a line into two lines**

## Syntax
```text
SPlit [ALigned] [Column|CURSOR]
```

## Description
The SPLIT command splits the focus line into two lines.
If Aligned is specified, the first non-blank character of the new line is positioned under the first
non-blank character of the focus line .
If Aligned is not specified, the text of the new line starts in column 1.
If Column (the default) is specified, the current line is split at the current column location.
If CURSOR is specified, the focus line is split at the cursor position.

## Compatibility
XEDIT: Compatible.
Does not support Before/After/Colno options
KEDIT: Compatible.

## See Also
JOIN, SPLTJOIN

## Status
Complete.
