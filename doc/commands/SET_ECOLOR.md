# SET ECOLOR
**set colors for syntax highlighting**

## Syntax
```text
[SET] ECOLOR char [modifier[...]] [foreground] [on background]

[SET] ECOLOR char [modifier[...]] ON|OFF
```

## Description
The SET ECOLOR command allows the user to specify the colors of each category of items used in
syntax highlighting.
char refers to one of the following valid values:
- A - comments
- B - strings
- C - numbers
- D - keywords
- E - labels
- F - preprocessor directives
- G - header lines
- H - extra right paren, matchable keyword (N/A)
- I - level 1 paren
- J - level 1 matchable keyword (N/A)
- K - level 1 matchable preprocessor keyword (N/A)
- L - level 2 paren, matchable keyword (N/A)
- M - level 3 paren, matchable keyword (N/A)
- N - level 4 paren, matchable keyword (N/A)
- O - level 5 paren, matchable keyword (N/A)
- P - level 6 paren, matchable keyword (N/A)
- Q - level 7 paren, matchable keyword (N/A)
- R - level 8 paren or higher, matchable keyword (N/A)
- S - incomplete string
- T - HTML markup tags
- U - HTML character/entity references
- V - Builtin functions
- W - not used
- X - not used
- Y - not used
- Z - not used
- 1 - alternate keyword color 1
- 2 - alternate keyword color 2
- 3 - alternate keyword color 3
- 4 - alternate keyword color 4
- 5 - alternate keyword color 5
- 6 - alternate keyword color 6
- 7 - alternate keyword color 7
- 8 - alternate keyword color 8
- 9 - alternate keyword color 9
N/A indicates that this capability is not yet implemented.
For valid values for modifier , foreground and background see SET COLOR .
The second format of this command allows the user to turn on or off any of the valid modifiers.
Note: The `CBERROR`, `CBWARN`, `CBINFO` colors for diagnostic highlighting are configured using `SET COLOR`, not `SET ECOLOR`.

## Compatibility
XEDIT: N/A
KEDIT: Compatible.

## Default
See QUERY ECOLOR

## See Also
SET COLORING, SET AUTOCOLOR, SET PARSER, SET COLOR

## Status

Complete.
