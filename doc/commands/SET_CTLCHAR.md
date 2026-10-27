# SET CTLCHAR
**define control character attributes**

## Syntax
```text
[SET] CTLchar OFF
[SET] CTLchar char Escape | OFF
[SET] CTLchar char Protect|Noprotect [modifier[...]] fore [ON back]
```

## Description
The SET CTLCHAR command defines control characters to be used when displaying a reserved line or,
when SET FILECTLCHAR is ON, ordinary file-area lines.
Control characters determine how parts of the displayed line are displayed.
See SET COLOUR for valid values for modifier , fore and back .
Protect and Noprotect define protected and editable fields during READ panels.
See [macro panels](../panels.md). Outside READ these flags do not restrict normal editing.

## Compatibility
XEDIT: Similar, but does not support all parameters.
KEDIT: N/A.

## Default
OFF

## See Also
SET COLOUR, SET FILECTLCHAR, SET RESERVED

## Status
Complete.
