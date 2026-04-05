# SET THIGHLIGHT
**specify if text highlighting is supported**

## Syntax
```text
[SET] THIGHlight ON|OFF
```

## Description
The SET THIGHLIGHT command allows the user to specify if a the result of a string LOCATE
command should be highlighted. The colour that is used to highlight the found string is set by the
THIGHLIGHT option of SET COLOUR . The found string is highlighted until a new line is added or
deleted, a command is issued from the command line, another LOCATE or CLOCATE command is
executed, a block is marked, or RESET THIGHLIGHT is executed.

## Compatibility
XEDIT: N/A
KEDIT: Compatible.

## Default
ON - THE/KEDIT/KEDITW OFF - XEDIT/ISPF

## See Also
LOCATE, SET COLOUR

## Status

Complete.
