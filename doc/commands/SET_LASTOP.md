# SET LASTOP
**set the contents of the lastop argument**

## Syntax
```text
[SET] LASTOP operand text
```

## Description
The SET LASTOP command sets the values of the specified operand to the text supplied. This
command is most useful when run from a macro, to set the string to be passed to the next invocation
of the equivalent operand command; eg LOCATE, FIND, etc.
Because THE does not save the contents of the lastop from a command when run from a macro,
sometimes the macro is intended to set this value. This command allows that capability.

## Compatibility
XEDIT: N/A
KEDIT: Compatible.

## See Also
LOCATE, FIND, SEARCH

## Status
Complete.
