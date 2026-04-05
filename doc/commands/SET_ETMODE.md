# SET ETMODE
**indicate if extended display mode is possible**

## Syntax
```text
[SET] ETMODE ON|OFF [character list]
```

## Description
The SET ETMODE command allows the user to specify which characters in a character set are to be
displayed as their actual representation.

Those characters not explicitly specified to be displayed as they are represented, will be displayed as
the SET NONDISP character in the colour specified by SET COLOUR NONDISP. Characters below
32, will be displayed with an alphabetic character representing the "control" code.
e.g. character code with a value of 7, will display as "G" in the colour specified by SET COLOUR
NONDISP.
ON with no optional character list will display ALL characters as their actual representation.
OFF with no optional character list will display control characters below ASCII 32, as a "control"
character; characters greater than ASCII 126 will be displayed as the SET NONDISP characters. On
ASCII based machines, [SET] ETMODE OFF is equivalent to [SET] ETMODE ON 32-126. On
EBCDIC based machines [SET] ETMODE OFF is equivalent to [SET] ETMODE ON ??-??
The character list is a list of positive numbers between 0 and 255 (inclusive). The format of this
character list can be either a single number; e.g. 124, or a range of numbers specified; e.g. 32-126.
(The first number must be less than or equal to the second number).
As an example; ETMODE ON 32-127 160-250 would result in the characters with a decimal value
between 32 and 127 inclusive and 160 and 250 inclusive being displayed as their actual representation
(depending on the current font), and the characters between 0 and 31 inclusive, being displayed as an
equivalent "control" character; characters between 128 and 159 inclusive and 250 to 255 being
displayed with the SET NONDISP character.
Up to 20 character specifiers (single number or range) can be specified.

## Compatibility
XEDIT: Similar function but deals with Double-Byte characters
KEDIT: N/A

## Default
- ON - DOS/OS2/WIN32
- ON 32-255 - X11
- OFF - UNIX/AMIGA/QNX

## See Also
SET NONDISP, SET COLOUR

## Status
Complete.
