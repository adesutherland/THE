# SET TABLINE
**set position and status of tab line on screen**

## Syntax
```text
[SET] TABLine ON|OFF [M[+n|-n]|[+|-]n]
```

## Description
The SET TABLINE command sets the position and status of the tab line for the current view.
The first form of parameters is:
M[+n|-n]
this sets the tab line to be relative to the middle of
the screen. A positive value adds to the middle line number,
a negative subtracts from it.
e.g. M+3 on a 24 line screen will be line 15
M-5 on a 24 line screen will be line 7
The second form of parameters is:
[+|-]n
this sets the tab line to be relative to the top of the
screen (if positive or no sign) or relative to the bottom
of the screen if negative.
e.g. +3 or 3 will set current line to line 3
-3 on a 24 line screen will be line 21
If the resulting line is outside the bounds of the screen the position of the current line will become the
middle line on the screen.
It is an error to try to position the TABL line on the same line as SET CURLINE .

## Compatibility
XEDIT: Compatible.
KEDIT: Compatible.

## Default
OFF -3

## Status
Complete.
