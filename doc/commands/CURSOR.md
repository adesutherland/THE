# CURSOR
**move cursor to specified position**

## Syntax
```text
CURsor Column [Priority priority]
CURsor Screen UP|DOWN|LEFT|RIGHT [Priority priority]
CURsor Screen row [col] [Priority priority]
CURsor [Escreen] UP|DOWN [Priority priority]
CURsor [Escreen|Kedit] LEFT|RIGHT [Priority priority]
CURsor [Escreen] row [col] [Priority priority]
CURsor CUA UP|DOWN|LEFT|RIGHT [Priority priority]
CURsor CMdline [n] [Priority priority]
CURsor HOME [SAVE] [Priority priority]
CURsor File line [col] [Priority priority]
CURsor GOTO line col [Priority priority]
CURsor Mouse [Priority priority]
CURsor Prefix [Priority priority]
```

## Description
The CURSOR command allows the user to specify where the cursor is to be positioned.
CURSOR Column moves the cursor to the current column of the focus line .
CURSOR Screen UP | DOWN | LEFT | RIGHT moves the cursor in the indicated direction one line or
column. If the cursor is positioned on the first or last line of the screen, the cursor wraps to the first or
last enterable line. If the cursor is positioned on the left or right edges of the screen, the cursor moves
to the left or right edge of the screen on the same line.
CURSOR Screen row [ col ] is similar to CURSOR Escreen row [ col ], but all coordinates are
relative the the top left corner of the screen, not the top left corner of the filearea . Hence, 1,1 would
be an invalid cursor position because it would result in the cursor being moved to the idline .
Specification of row and/or col outside the boundaries of the logical window is regarded as an error.
CURSOR [ Escreen ] UP | DOWN | LEFT | RIGHT is similar to CURSOR Screen UP | DOWN | LEFT
| RIGHT , except that where scrolling of the window is possible, then scrolling will take place.
CURSOR [ Escreen ] row [ col ] moves the cursor to the specified row / col position within the
filearea . The top left corner of the filearea is 1,1. row and col may be specified as '=' , which will
default to the current row and/or column position. If row or col are greater than the maximum number
of rows or columns in the filearea , the cursor will move to the last row/column available. If the
specified row is a reserved line , scale line or tab line an error will be displayed. If the row specified is
above the Top-of-File line or below the Bottom-of-File line the cursor will be placed on the closest
one of these lines.
CURSOR Kedit LEFT | RIGHT mimics the default behaviour of CURL and CURR in KEDIT.
CURSOR CUA UP | DOWN | LEFT | RIGHT moves the cursor in the indicated direction one line or
column. The behaviour of the cursor at the the end of a line and at the start of a line is consistent with
the Common User Access (CUA) definition.
CURSOR CMdline moves the cursor to the indicated column of the command line .
CURSOR HOME moves the cursor to the first column of the command line (if not on the command
line), or to the last row/column of the filearea if on the command line . With the [ SAVE ] option, the
cursor will move to the last row/column of the filearea or prefix area (which ever was the last

position) if on the command line .
CURSOR File moves the cursor to the line and column of the file. If the line and/or column are not
currently displayed, an error message is displayed.
CURSOR GOTO moves the cursor to the specified line and column of the file, whether the row and
column are currently displayed or not. If the line and col are currently displayed, then this command
behaves just like CURSOR File . If not, then the current line will be changed to the specified line .
CURSOR Mouse moves the cursor to the position where a mouse button was last activated. This
command is specific to THE.
CURSOR PREFIX moves the cursor to the first column of the prefix area (if not in the prefix area), or
to the first column of the filearea if in the prefix area . This command has no effect if run from the
command line . This command replaces TABPRE.
The optional Priority argument is included for compatibility with XEDIT. The value of the priority
argument must be between 0 and 256, but otherwise it is ignored.

## Compatibility
XEDIT: Compatible. Priority is ignored.
KEDIT: Compatible. Added GOTO and PREFIX option.

## Status
Complete.
