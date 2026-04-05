# NFIND
**locate forwards the line which does NOT begin with the supplied**

string

## Syntax
```text
NFind [string]
```

## Description
The NFIND command attempts to locate a line towards the end of the file that does NOT begin with
string . If the optional string is not supplied the last string used in any of the family of find commands
is used.
string can contain two special characters:
- space - this will match any single character in the target line
- underscore - this will match any single space in the target line

## Compatibility
XEDIT: Compatible.
KEDIT: Compatible.

## See Also
FIND, FINDUP, NFINDUP

## Status
Complete
