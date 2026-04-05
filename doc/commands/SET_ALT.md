# SET ALT
**change alteration counts**

## Syntax
```text
[SET] ALT [n] [m]
```

## Description
The SET ALT command allows the user to change the alteration counts. This command is usually
called from within a macro.
The first number; n sets the number of changes since the last AUTOSAVE was issued.
The second number; m sets the number of changes since the last SAVE or SSAVE command was
issued.
All options can be specified as the current EQUIVCHAR to retain the existing value.

## Compatibility
XEDIT: Compatible.
KEDIT: Compatible.

## Default
OFF

## See Also
SET EQUIVCHAR

## Status
Complete.
