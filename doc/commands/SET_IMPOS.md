# SET IMPOS
**set implied operating system command processing**

## Syntax
```text
[SET] IMPOS ON|OFF
```

## Description
The SET IMPOS command controls implied operating system command processing from the command
line. It is off by default; use `OS command` or `!command` to run a shell command explicitly.
`SET IMPOS ON` enables the legacy behavior of passing unrecognized commands to the operating
system.

## Compatibility
XEDIT: Compatible.
KEDIT: N/A

## Default
OFF

## See Also
SET IMPCMSCP

## Status
Complete.
