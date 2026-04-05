# SORT
**sort selected lines in a file**

## Syntax
```text
SORT target [[sort field 1] [...] [sort field 1000]]
```

## Description
The SORT command sorts a portion of a file based on the sort field specifications.
A sort field specification consists of:
- order flag - [Ascending|Descending]
- left column - left column of field to sort on
- right column - right column of field to sort on
If the 'order flag' is omitted for a sort field , the 'order flag' for the previous sort field is used. If no
'order flag' is specified, all sort fields are sorted in acending order. Therefore SORT * D 1 1 2 2 A 3 3
4 4 is the same as SORT * D 1 1 D 2 2 A 3 3 A 4 4
The right column MUST be >= left column.
1000 sort fields are allowed.
target can be any valid target including ALL, *, -*, and BLOCK.

## Compatibility
XEDIT: XEDIT only allows ordering flag for all fields
KEDIT: Compatible.

## Status
Complete.
