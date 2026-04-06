# SET SDSLH
**specifies an external SDSLH parser for syntax highlighting**

## Syntax
```text
[SET] SDSLH parser executable_path
```

## Description
The SET SDSLH command registers an external executable as a named parser capable of communicating over the SDSLH stdio protocol. This allows integrating external syntax highlight parsers dynamically.

The `<parser>` name can then be used with `<SET AUTOCOLOR>`.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## See Also
SET PARSER, SET COLORING, SET AUTOCOLOR
