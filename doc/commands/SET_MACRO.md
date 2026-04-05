# SET MACRO
**indicate if macros executed before commands**

## Syntax
```text
SET MACRO ON|OFF
```

## Description
The SET MACRO command allows the user to determine if macros are executed before a built-in
command of the same name.
This command MUST be prefixed with SET to distinguish it from the MACRO command.
A macro with the same name as a built-in command will only be executed before the built-in
command if SET IMPMACRO is ON, SET MACRO is ON, and the command was NOT executed
with the COMMAND command.

## Compatibility
XEDIT: Compatible.
KEDIT: N/A

## Default
OFF

## See Also
MACRO, SET IMPMACRO, COMMAND

## Status
Complete.
