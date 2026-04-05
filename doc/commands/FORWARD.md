# FORWARD
**scroll forward by number of screens or lines**

## Syntax
```text
FOrward [n|*|HALF] [Lines]
```

## Description
The FORWARD command scrolls the file contents forwards n screens or n lines if the optional Lines
argument is specified.
If * is specified, the Bottom-of-File line becomes the current line .
If HALF is specified, the file contents are scrolled one half of a screen.
If 0 is specified as the number of lines or screens to scroll, the Top-of-File line becomes the current
line .
If the FORWARD command is issued while the current line is the Bottom-of-File line and SET
PAGEWRAP is ON, the Top-of-File line becomes the current line .

## Compatibility
XEDIT: Compatible.
KEDIT: Compatible.

## Default

## See Also
BACKWARD, TOP, SET PAGEWRAP

## Status
Complete
