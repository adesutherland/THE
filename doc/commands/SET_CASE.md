# SET CASE
**set case sensitivity parameters**

## Syntax
```text
[SET] CASE Mixed|Lower|Upper [Respect|Ignore] [Respect|Ignore] [Respect|Ignore]
[Mixed|Lower|Upper] [Mixed|Lower|Upper]
```

## Description
The CASE command sets the editor's handling of the case of text.
The first option (which is mandatory) controls how text is entered by the user in the filearea . When
LOWER or UPPER are in effect, the shift or caps lock keys have no effect on the text being entered.
When MIXED is in effect, text is entered in the case set by the use of the shift and caps lock keys.
The second option determines how the editor determines if a string target matches text in the file
when the target is used in a LOCATE command. With IGNORE in effect, a match is found
irrespective of the case of the target or the found text. The following strings are treated as equivalent:
the THE The ThE... With RESPECT in effect, the target and text must be the same case. Therefore a
target of 'The' only matches text containing 'The' , not 'THE' or 'ThE' etc.
The third option determines how the editor determines if a string target matches text in the file when
the target is used in a CHANGE command. With IGNORE in effect, a match is found irrespective of
the case of the target or the found text. The following strings are treated as equivalent: the THE The
ThE... With RESPECT in effect, the target and text must be the same case. Therefore a target of 'The'
only matches text containing 'The' , not 'THE' or 'ThE' etc.
The fourth option determines how the editor determines the sort order of upper and lower case with
the SORT command. With IGNORE in effect, upper and lower case letters are treated as equivalent.
With RESPECT in effect, upper and lower case letters are treated as different values and uppercase
characters will sort before lowercase characters.
The fifth option controls how text is entered by the user on the command line . The allowed values
and behaviour are the same as for the first option.
The sixth option controls how text is entered by the user in the prefix area . The allowed values and
behaviour are the same as for the first option.
All options can be specified as the current EQUIVCHAR to retain the existing value.

## Compatibility
XEDIT: Adds support for case significance in CHANGE commands.
KEDIT: Adds support for LOWER option.
Both: Adds support for case significance in SORT command.

## Default
Mixed Ignore Respect Respect

## See Also
SET EQUIVCHAR

## Status
Complete
