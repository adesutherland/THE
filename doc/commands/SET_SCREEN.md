# SET SCREEN
**specify number of screens displayed**

## Syntax
```text
[SET] SCReen n [Horizontal|Vertical]
[SET] SCReen Size l1|* [l2|*]
[SET] SCReen Width c1|* [c2|*]
```

## Description
The SET SCREEN command specifies the number of views of file(s) to display on screen at once. If
the number of views specified is 2 and only one file is currently in the ring , two views of the same
file are displayed.
The second form of SET SCREEN allows the user to specify the number of lines that each screen
occupies. The sum of l1 and l2 must equal to lscreen.5 or lscreen.5 - 1 if the status line is displayed.
The value of l1 specifies the size of the topmost screen; l2 specifies the size of the bottommost screen.
Either l1 or l2 can be set to *, but not both. The * signifies that the screen size for the specified screen
will be the remainder of the full display window after the size of the other screen has been subtracted.
The third form of SET SCREEN allows the user to specify the number of columns that each screen
occupies. The sum of c1 and c2 must equal to lscreen.6.
The value of c1 specifies the size of the leftmost screen; c2 specifies the size of the rightmost screen.
Either c1 or c2 can be set to *, but not both. The * signifies that the screen size for the specified
screen will be the remainder of the full display window after the size of the other screen has been
subtracted.
The THE display can only be split into 1 or 2 screens.

## Compatibility
XEDIT: Does not support Define options.
KEDIT: Does not support Split option.
A maximum of 2 screens are supported.

## Default

## See Also
SET STATUSLINE

## Status
Complete.
