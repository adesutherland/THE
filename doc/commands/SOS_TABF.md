# SOS TABF
**move cursor to next tab stop**

## Syntax
```text
SOS TABf
```

## Description
The SOS TABF command causes the cursor to move to the next tab column as set by the SET TABS
command. If the resulting column is beyond the right hand edge of the main window, the window will
scroll half a window.

## Compatibility
XEDIT: Does not allow arguments.
KEDIT: Compatible. See below.
Does not line tab to next line if after the right hand tab column.

## See Also
SET TABS, SOS TABB

## Status
Complete.
