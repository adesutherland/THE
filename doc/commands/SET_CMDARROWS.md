# SET CMDARROWS
**sets the behaviour of the up and down arrow keys**

## Syntax
```text
[SET] CMDArrows Retrieve|Tab
```

## Description
The SET CMDARROWS command determines the action that occurs when the up and down arrows
keys are hit while on the command line .
RETRIEVE will set the up and down arrows to retrieve the last or next command entered on the
command line .
TAB will set the up arrow to move to the last line of the main window. The down arrow and shift-down arrow will retrieve the older and newer command respectively from the command line. In addition, when at the bottom of the file area, the down arrow jumps to the command line.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## Default
RETRIEVE

## See Also
CURSOR, ?

## Status
Complete.
