# SET MACROEXT
**set default macro extension value**

## Syntax
```text
[SET] MACROExt [ext]
```

## Description
The SET MACROEXT command sets the value of the file extension to be used for macro files. When
a macro file name is specified on the command line , a period '.' , then this value will be appended. If

no value is specified for ext , then THE assumes that the supplied macro file name is the fully
specified name for a macro.
The length of ext must be 10 characters or less.
The macro extension is only appended to a file if that file does not include any path specifiers.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## Default
the

## Status
Complete.
