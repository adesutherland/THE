# SOS TABB
**move cursor to previous tab stop**

## Syntax
```text
SOS TABB
```

## Description
The SOS TABB command causes the cursor to move to the previous tab column as set by the SET
TABS command. If the resulting column is beyond the left hand edge of the main window, the
window will scroll half a window.

## Compatibility
XEDIT: Does not allow arguments.
KEDIT: Compatible. See below.
Does not line tab to next line if before the left hand tab column.

## See Also
SET TABS, SOS TABF

## Status
Complete.
