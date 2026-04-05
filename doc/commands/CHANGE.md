# CHANGE
**change one string to another**

## Syntax
```text
Change [/string1/string2/ [target] [n] [m]]
```

## Description
The CHANGE command changes one string of text to another.

The first parameter to the change command is the old and new string values, separated by delimiters.
The first non alphabetic character after the 'change' command is the delimiter.
target specifies how many lines are to be searched for occurrences of string1 to be changed.
n determines how many occurrences of string1 are to be changed to string2 on each line. n may be
specified as '*' which will result in all occurrences of string1 will be changed. '*' is equivalent to the
current WIDTH of the line.
m determines from which occurrence of string1 on the line changes are to commence.
If no arguments are supplied to the CHANGE command, the last change command, if any, is
re-executed.

## Compatibility
XEDIT: Compatible. ARBCHAR not supported however.
KEDIT: Compatible. ARBCHAR not supported however.

## Default

## See Also
SCHANGE

## Status
Complete.
