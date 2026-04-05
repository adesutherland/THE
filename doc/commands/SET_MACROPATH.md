# SET MACROPATH
**set default path for macro commands**

## Syntax
```text
[SET] MACROPath PATH|path[s]
```

## Description
The SET MACROPATH command sets up the search path from which macro command files are
executed. Each directory is separated by a colon (Unix) or semi-colon (DOS & OS/2). Only 20
directories are allowed to be specified.
When PATH is specified, the search path is set to the system PATH environment variable.

## Compatibility
XEDIT: N/A
KEDIT: Incompatible.

## Default
Path specified by env variable THE_MACRO_PATH

## See Also
MACRO, SET IMPMACRO

## Status
Complete.
