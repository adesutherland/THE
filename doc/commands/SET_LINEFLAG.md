# SET LINEFLAG
**set the line characteristics of lines**

## Syntax
```text
[SET] LINEFLAG CHAnge|NOCHange NEW|NONEW TAG|NOTAG [target]
```

## Description
The SET LINEFLAGS command controls the line characteristics of lines in a file.
Each line in a file has certain characteristics associated with it depending on how the line has been
modified. On reading a file from disk, all lines in the file are set to their default values.
Once a line is modified, or tagged, the characteristics of the line are set appropriately. A line that is
added, is set to NEW; a line that is changed is set to CHANGE, and a line that is tagged with the TAG
command, is set to TAG. All three characteristics can be on at the one time.

## Compatibility
XEDIT: N/A
KEDIT: Compatible.

## Default
NOCHANGE NONEW NOTAG

## See Also
TAG, SET HIGHLIGHT

## Status
Complete.
