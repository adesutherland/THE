# SET PREFIX
**set prefix area attributes**

## Syntax
```text
[SET] PREfix ON [Left|Right] [m [n]]
[SET] PREfix Nulls [Left|Right] [m [n]]
[SET] PREfix OFF
[SET] PREfix Synonym newname oldname
[SET] PREfix GAP n [LINE]
```

## Description
The first form of the SET PREFIX command allows the user to display the prefix area and optionally
to select the position where the prefix should be displayed.
The second form of the SET PREFIX command is functionally the same as the first form. The
difference is that when the prefix area is displayed with SET NUMBER ON, numbers are displayed
with leading spaces rather than zeros; with SET NUMBER OFF, blanks are displayed instead of equal
signs.
The first and second forms of the SET PREFIX command allows the user to specify the width of the
prefix area and optionally a gap between the prefix area and the filearea. m can be specified as an

unsigned number between 2 and 20 inclusive. n can be specified as an unsigned number between 0
and 18, but less than the number specified in m .
The third form, turns the display of the prefix area off. Executed from within the profile, the only
effect is that the defaults for all files is changed. Executed from the command line, the SET PREFIX
command changes the current window displays to reflect the required options.
The fourth form of the SET PREFIX command allows the user to specify a synonym for a prefix
command or Rexx prefix macro. The newname is the command entered in the prefix area and
oldname corresponds to an existing prefix command or a Rexx macro file in the MACROPATH
ending in .the or whatever the value of SET MACROEXT is at the time the prefix command is
executed. The oldname can also be the fully qualified filename of a Rexx macro. To turn off a prefix
synonym, call SET PREFIX newname oldname where newname and oldname are the same and
newname is an existing prefix command or macro.
The fifth form of the SET PREFIX command allows the user to specify the width of the gap between
the prefix area and the file area and whether a vertical line should be displayed in that gap. n can be
specified as an unsigned number between 0 and 18, but less than the current prefix width.

## Compatibility
XEDIT: Compatible.
KEDIT: Compatible.
Specification of prefix width ang gap is a THE-only option. Kedit has
SET PREFIXWIDTH to set prefix width, but not gap.

## Default
ON Left 6 0

## Status
Complete.
