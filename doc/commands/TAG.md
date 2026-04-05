# TAG
**displays lines matching target in different colour**

## Syntax
```text
TAG [More|Less] [rtarget|Focus]
```

## Description
The TAG command is similar to the ALL command, in that it allows lines that match the specified
target to be displayed. Where it differs from ALL is that the lines that don 't match are still displayed,
but the lines that do match are displayed in the colour specified by SET COLOUR HIGHLIGHT. This
target consists of any number of individual targets separated by ' & ' (logical and) or ' | ' (logical or).'
For example, to display all lines in a file that contain the strings 'ball' and 'cat' on the same line or the
named lines .fred or .bill, use the following command
TAG /ball/ & /cat/ | .fred | .bill
Logical operators act left to right, with no precedence for &.
TAG without any arguments displays all lines without any highlighting.
If SET HIGHLIGHT is not set to TAGGED, then if the specified rtarget is found, SET HIGHLIGHT
is set to TAGGED.
When the optional More argument is specified, all lines that match the rtarget are highlighted in
addition to those already highlighted.
When the optional Less argument is specified, all lines that match the rtarget and are currently
highlighted have their highlighting removed.
If FOCUS is specified in place of rtarget , the focus line is tagged.

## Compatibility
XEDIT: N/A
KEDIT: Compatible.
FOCUS is a THE enhancement.

## See Also
ALL, SET HIGHLIGHT, SET COLOUR

## Status
Complete.
