# SET COLORING
**enable or disable syntax highlighting**

## Syntax
```text
[SET] COLORING ON|OFF [AUTO|parser]
```

## Description
The SET COLORING command allows the user to turn on or off syntax highlighting for current file.
It also allows the parser used to be specified explicitly, or automatically determined by the file
extension or magic number .
ON turns on syntax highlighting for the current file, OFF turns it off.

AUTO determines the parser to use for the current file based on the file extension. The parser to use is
controlled by the SET AUTOCOLOR command.

## Compatibility
XEDIT: N/A
KEDIT: Compatible.

## Default
ON AUTO

## See Also
SET COLOURING, SET ECOLOUR, SET AUTOCOLOR, SET PARSER

## Status
Complete.
