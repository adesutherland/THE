# MARK
**mark a portion of text**

## Syntax
```text
MARK Box [line1 col1 line2 col2]
MARK Line [line1 line2]
MARK Stream [line1 col1 line2 col2]

MARK Column [col1 col2]
MARK Word [line1 col1]
MARK CUA [LEFT|RIGHT|UP|DOWN|START|END|FOrward|BAckward|TOP|Bottom|MOUSE]
```

## Description
The MARK command marks a portion of text for later processing by a COPY , MOVE or DELETE
command. This marked area is known as a block .
When the MARK command is executed with the optional line/column arguments, these values are
used to specify the position of the marked block . Without the optional arguments, the position of the
cursor is used to determine which portion of text is marked.
line1 and line2 specify the first or last line of the marked block.
col1 and col2 specify the first or last column of the marked block.
Any currently marked block will be unmarked or extended depending on the arguments supplied.
When marking a word block , line1 and col1 refer to any position within the word.

## Compatibility
XEDIT: N/A
KEDIT: Adds CUA, WORD, and COLUMN options and position specifiers.

## Status
Complete.
