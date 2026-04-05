# SET MSGLINE
**set position and size of message line**

## Syntax
```text
[SET] MSGLine ON M[+n|-n]|[+|-]n [lines] [Overlay]
[SET] MSGLine CLEAR
```

## Description
The SET MSGLINE set command specifies the position of the message line and the size of the
message line window.
The first form of positional parameters is:
- M[+n|-n]
this sets the first line to be relative to the middle of
the screen. A positive value adds to the middle line number,
a negative subtracts from it.
e.g. M+3 on a 24 line screen will be line 15
M-5 on a 24 line screen will be line 7
The second form of positional parameters is:
- [+|-]n

this sets the first line to be relative to the top of the
screen (if positive or no sign) or relative to the bottom
of the screen if negative.
e.g. +3 or 3 will set first line to line 3
-3 on a 24 line screen will set first line to line 21
If the resulting line is outside the bounds of the screen the position of the message line will become
the middle line on the screen.
The lines argument specifies the maximum number of lines of error messages to display at the one
time. If this value is specified as a whole number it must be less than or equal to the number of lines
that could fit on the screen from the starting row. '*' can be specified to indicate that as many lines as
possible should be displayed.
All options can be specified as the current EQUIVCHAR to retain the existing value.
The second format of the command clears the messages being displayed. This is useful in macros
where you need to display an error message but also want to be able to clear it.

## Compatibility
XEDIT: Compatible.
The OVERLAY option is the default but ignored.
The second format is not supported.
KEDIT: Compatible
The OVERLAY option is the default but ignored.
The second format is not supported.

## Default
ON 2 5 Overlay

## See Also
SET EQUIVCHAR

## Status
Complete
