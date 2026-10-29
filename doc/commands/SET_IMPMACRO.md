# SET IMPMACRO
**set implied macro command processing**

## Syntax
```text
[SET] IMPMACro ON|OFF
```

## Description
The SET IMPMACRO command controls implied macro processing from the command line. It is off by
default; use `MACRO name` to run a macro explicitly. `SET IMPMACRO ON` enables the legacy behavior
of running an unrecognized command as a matching macro file. A registered synonym such as
`SET SYNONYM name MACRO name` also provides an explicit macro command.

## Compatibility
XEDIT: N/A
KEDIT: Compatible.

## Default
OFF

## See Also
MACRO, SET MACROPATH, SET SYNONYM

## Status
Complete.
