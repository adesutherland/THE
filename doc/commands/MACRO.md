# MACRO
**execute a macro command file**

## Syntax
```text
MACRO [?] filename [arguments ...]
```

## Description
The MACRO command executes the contents of the specified filename as command line commands.
The filename can contain either a series of THE commands, or can be a Rexx program. The filename
is considered a macro .
Rexx macros can be passed optional arguments .
With the optional ? parameter, interactive tracing of the Rexx macro is possible, but this does not set
interactive tracing on;

## Compatibility
XEDIT: Compatible.
KEDIT: Compatible.

## Status
Complete.
