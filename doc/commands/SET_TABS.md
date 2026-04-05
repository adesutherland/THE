# SET TABS
**set tab columns or tab length**

## Syntax
```text
[SET] TABS n1 [n2 ... n32]
[SET] TABS INCR n
[SET] TABS OFF
```

## Description
The SET TABS command determines the position of tab columns in THE.
The first format of SET TABS, specifies individual tab columns. Each column must be greater than
the column to its left.
The second format specifies the tab increment to use. ie each tab column will be set at each n
columns.
The third format specifies that no tab columns are to be set.
Tab columns are used by SOS TABF , SOS TABB and SOS SETTAB commands to position the
cursor and also by the COMPRESS and EXPAND commands.

## Compatibility
XEDIT: Compatible. Does not support OFF option.
KEDIT: Compatible. Does not support OFF option.

## Default
INCR 8

## Status
Complete.
