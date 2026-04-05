# SET HEX
**set how hexadecimal strings are treated in string operands**

## Syntax
```text
[SET] HEX ON|OFF
```

## Description
The SET HEX set command determines whether hexadecimal strings are treated as such in string
operands.
With the ON option, any string operand of the form /x '31 32 33' / or /d '49 50 51' / will be converted
to /123/ before the command is executed.
With the OFF option, no conversion is done.
This conversion should work wherever a string operand is used in any command.

## Compatibility
XEDIT: Adds support for decimal representation. See below.
KEDIT: Compatible. See below.
Spaces must separate each character representation.

## Default
OFF

## Status
Complete.
