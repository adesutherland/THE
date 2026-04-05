# FINDUP
**locate backwards the line which begins with the supplied**

string

## Syntax
```text
FINDUp [string]
```

## Description
The FINDUP command attempts to locate a line towards the start of the file that begins with string . If
the optional string is not supplied the last string used in any of the family of find commands is used.
string can contain two special characters:
- space - this will match any single character in the target line
- underscore - this will match any single space in the target line

## Compatibility
XEDIT: Compatible.
KEDIT: Compatible.

## See Also
FIND, NFIND, NFINDUP, FUP

## Status
Complete
