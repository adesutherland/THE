# ALL
**select and display restricted set of lines**

## Syntax
```text
ALL [rtarget]
```

## Description
The ALL command allows for the selective display, and editing (subject to SET SCOPE ) of lines that
match the specified target. This target consists of any number of individual targets separated by '&'
(logical and) or '|' (logical or).
For example, to display all lines in a file that contain the strings 'ball' and 'cat' on the same line or the
named lines .fred or .bill, use the following command
ALL /ball/ & /cat/ | .fred | .bill
Logical operators act left to right, with no precedence for &.
rtarget can also be specified as a regular expression. The syntax of this is "Regexp /re/". eg ALL R
/[0-9].*$/
ALL without any arguments, is the equivalent of setting the selection level of all lines in your file to 0
and running SET DISPLAY 0 0.

## Compatibility
XEDIT: Compatible.
KEDIT: Compatible.

## See Also
SET SCOPE, SET DISPLAY, SET SELECT

## Status
Complete.
