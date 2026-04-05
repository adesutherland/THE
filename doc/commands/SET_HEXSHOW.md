# SET HEXSHOW
**turn on or off hex display of current line**

## Syntax
```text
[SET] HEXShow ON|OFF [M[+n|-n]|[+|-]n]
```

## Description
The SET HEXShow command indicates if and where a hexadecimal representation of the current line
will be displayed.
The first form of parameters is:
M[+n|-n]
this sets the hexshow line to be relative to the middle of
the screen. A positive value adds to the middle line number,
a negative subtracts from it.
e.g. M+3 on a 24 line screen will be line 15
M-5 on a 24 line screen will be line 7
The second form of parameters is:
[+|-]n
this sets the hexshow line to be relative to the top of the
screen (if positive or no sign) or relative to the bottom
of the screen if negative.
e.g. +3 or 3 will set current line to line 3
-3 on a 24 line screen will be line 21
If the resulting line is outside the bounds of the screen the position of the hexshow line will become
the middle line on the screen.
The position argument specifies the position of the first line of the hexadecimal display.
It is an error to try to position the HEXSHOW lines on the same line as SET CURLINE .

## Compatibility

XEDIT: N/A
KEDIT: N/A

## Default
OFF 7

## Status
Complete
