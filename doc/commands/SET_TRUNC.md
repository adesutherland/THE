# SET TRUNC
**specify the truncation column**

## Syntax
```text
[SET] TRunc n|*
```

## Description
SET TRUNC sets a per-file limit for inserted, replaced and edited lines.
The limit uses display cells and never splits a UTF-8 grapheme. Setting a
limit does not immediately shorten existing lines; it applies when a line is
modified. INPUT, REPLACE and column edits report truncation with return code 3.
EXTRACT /TRUNC/ returns the effective numeric limit. EXTRACT /CURLINE/ returns
text through that limit. PRESERVE/RESTORE saves the setting.

## Compatibility
XEDIT: Basic non-spilling truncation; SET SPILL modes are not implemented.
KEDIT: Basic truncation.

## Default
*

## Status
Implemented for insertion, replacement, column edits and line commits.
