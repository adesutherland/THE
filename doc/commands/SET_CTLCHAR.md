# SET CTLCHAR
**define control character attributes**

## Syntax
```text
[SET] CTLchar OFF
[SET] CTLchar char Escape | OFF
[SET] CTLchar char Protect|Noprotect [modifier[...]] fore [ON back]
```

## Description
The SET CTLCHAR command defines control characters to be used when displaying a reserved line .
Control characters determine how parts of a reserved line are displayed.
See SET COLOUR for valid values for modifier , fore and back .
The Protect and Noprotect arguments are ignored.

## Compatibility
XEDIT: Similar, but does not support all parameters.
KEDIT: N/A.

## Default
OFF

## See Also
SET COLOUR, SET RESERVED

## Status
Complete.
