# BACKWARD
**scroll backward by number of screens or lines**

## Syntax
```text
BAckward [n|*|HALF] [Lines]
```

## Description
The BACKWARD command scrolls the file contents backwards n screens or n lines if the optional
Lines argument is specified.
If * is specified, the Top-File line becomes the current line .
If HALF is specified, the file contents are scrolled one half of a screen.
If 0 is specified as the number of lines or screens to scroll, the last line of the file becomes the current
line .
If the BACKWARD command is issued while the current line is the Top-of-File line and SET
PAGEWRAP is ON, the last line of the file becomes the current line .

## Compatibility
XEDIT: Compatible.
KEDIT: Compatible

## Default

## See Also
FORWARD, TOP, SET PAGEWRAP

## Status
Complete
