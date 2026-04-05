# SET DEFSORT
**specify the order in which files appear in DIR.DIR**

## Syntax
```text
[SET] DEFSORT OFF|DIRectory|Size|Date|Time|Name [Ascending|Descending]
```

## Description
The SET DEFSORT command allows the user to determine the order in which files appear in a
DIR.DIR file.
Directory specifies that directories within the current directory are shown before other files.
Size specifies that the size of the file determines the order in which files are displayed.
Date specifies that the date of the last change to the file determines the order in which files are
displayed. If the dates are the same, the time the file was last changed is used as a secondary sort key.
Time specifies that the time of the file determines the order in which files are displayed.
Name specifies that the name of the file determines the order in which files are displayed. This is the
default. Files are sorted by name as a secondary sort key when any of the above options are specified
and two files have equal values for that sort option.
OFF indicates that no ordering of the files in the directory is performed. On directories with a large
number of files, this option results in a displayed DIR.DIR file much quicker than any sorted display.
The second parameter specifies if the sort order is ascending or descending.
If this command is issued while the DIR.DIR pseudo file is the current file, the settings are applied
immediately.

## Compatibility
XEDIT: N/A

KEDIT: Similar in functionality.

## Default
NAME ASCENDING

## Status
Complete.
