# SET AUTOCOLOR
**specifies which parser to use for syntax highlighting**

## Syntax
```text
[SET] AUTOCOLOR mask parser [MAGIC]
```

## Description
The SET AUTOCOLOR command allows the user to specify which syntax highlighting parser is to
be used for which file masks.
The parser argument specifies a syntax highlighting parser that already exists, either as a default
parser , or added by the user with SET PARSER . The special parser name of '*NULL' can be
specified; this will effectively remove the association between the parser and the file mask.
The mask argument specifies the file mask (or magic number ) to associate with the specified parser.
The mask can be any valid file mask for the operating system. eg *.c fred.* joe.?
If the magic option is specified, the mask argument refers to the last element of the magic number that
is specified in the first line of a Unix shell script comment. eg if the first line of a shell script contains:
#!/usr/local/bin/rexx then the file mask argument would be specified as "rexx".

## Compatibility
XEDIT: N/A
KEDIT: Similar. KEDIT does not have MAGIC option.

## Default
See QUERY AUTOCOLOR

## See Also
SET COLORING, SET ECOLOUR, SET PARSER

## Status
Complete.
