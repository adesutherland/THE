# SET PARSER
**associates a language definition file with a parser**

## Syntax
```text
[SET] PARSER parser file
```

## Description
The SET PARSER defines a new syntax highlighting parser ; parser based on a language definition
file; file .
The file is looked for in the directories specified by SET MACROPATH .
To specify one of the builtin parsers, prefix the filename with '*' . Therefore to define a parser called
FRED using the builtin C parser, the command would be: SET PARSER FRED *C.TLD.

## Compatibility
XEDIT: N/A
KEDIT: Compatible.

## See Also
SET COLORING, SET ECOLOUR, SET AUTOCOLOR, SET MACROPATH

## Status
Complete.
