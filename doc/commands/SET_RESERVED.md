# SET RESERVED
**display a reserved line**

## Syntax
```text
[SET] RESERved [AUTOSCroll] *|+|-n [colour] [text|OFF]
```

## Description
The SET RESERVED command reserves a line for the display of arbitrary text by the user. The
position is determined by +|-n. This number, if positive, specifies the line relative from the top of the
display. A negative number is relative from the bottom of the display.
By specifying a line, say +3, then the third line from the top will be reserved, with the supplied text
being displayed in that line.

The idline of a file will always be displayed after any reserved lines.
The status line is not considered part of the displayable area, so any positioning specifications ignore
that line.
A reserved line can only be turned off by identifying it in the same way that it was defined. If a
reserved line was added with the position specification of -1, it cannot be turned off with a position
specification of 23, even though both position specifiers result in the same display line.
All reserved lines may be turned of by specifying * as the number of lines.
By default, reserved lines are fixed; that is the left portion of the reserved line that can be displayed
stays displayed irrespective of whether the file contents have been scrolled left or right. With the
AUTOSCroll option, the text of the reserved line is scrolled with the file contents using the same
settings as SET AUTOSCROLL .
The colour option specifies the colours to use to display the reserved line. The format of this colour
specifier is the same as for SET COLOUR . If no colour is specified, the colour of the reserved line
will be the colour set by any SET COLOUR RESERVED command for the view or white on black by
default.
The text of reserved lines can also included embedded control characters to control the colour of
portions of the text. Assume the following SET CTLCHAR commands have been issued:
SET CTLCHAR ! ESCAPE
SET CTLCHAR @ PROTECT BOLD RED ON WHITE
SET CTLCHAR % PROTECT GREEN ON BLACK
Then to display a reserved line using the specified colours:
SET RESERVED -1 normal!@bold red on white!%green on black
It is an error to try to reserve a line which is the same line as SET CURLINE .

## Compatibility
XEDIT: Compatible.
KEDIT: Compatible.
AUTOSCroll option is a THE extension.

## See Also
SET COLOUR, SET CTLCHAR

## Status
Complete.
