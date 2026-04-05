# SET HEADER
**turn on or off syntax highlighting headers**

## Syntax
```text
[SET] HEADer section ON|OFF
```

## Description
The SET HEADER command allows fine tuning of which sections of a TLD file are to be applied for
the current view.
section refers to one of the following headers that can be specified in a TLD file: NUMBER,
COMMENT, STRING, KEYWORD, FUNCTION, HEADER, LABEL, MATCH, COLUMN,
POSTCOMPARE, MARKUP, DIRECTORY. section can also be specified as '*' , in which case all
headers are applied or not applied.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## Default
* ON

## See Also
SET PARSER, SET COLORING, SET AUTOCOLOR

## Status
Complete.
