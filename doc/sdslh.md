# THE + DSL-Syntax-Highlighter Integration Guide

This document describes how the DSL Syntax Highlighter (SDSLH) tokens map to THE's internal `ECOLOUR` system for syntax highlighting.

## Token Mapping Table

| DSLSH Token (`LEXER_*`)      | THE `ECOLOUR_*`           | Profile Command Area Name | Example Usage                           |
| :--------------------------- | :------------------------ | :------------------------ | :-------------------------------------- |
| `COMMENT`                    | `ECOLOUR_COMMENTS`        | `comment`                 | `/* ... */`, `// ...`                   |
| `STRING_LITERAL`             | `ECOLOUR_STRINGS`         | `string`                  | `"hello"`                               |
| `NUMBER_LITERAL`             | `ECOLOUR_NUMBERS`         | `number`                  | `123`, `0xFF`                           |
| `KEYWORD`                    | `ECOLOUR_KEYWORDS`        | `keyword`                 | `if`, `else`, `return`                  |
| `PREPROCESSOR`               | `ECOLOUR_PREDIR`          | `preprocessor` / `macro`  | `#include`, `#define`, `import`         |
| `TYPE_IDENTIFIER`            | `ECOLOUR_TYPES`           | `type`                    | `char`, `size_t`, `MyStruct`            |
| `FUNCTION_IDENTIFIER`        | `ECOLOUR_FUNCTIONS`       | `function`                | `printf`, `main`                        |
| `CONSTANT_IDENTIFIER`        | `ECOLOUR_CONSTANTS`       | `constant`                | `MAX_SIZE`, `NULL`                      |
| `IDENTIFIER`                 | `ECOLOUR_LABEL`           | `identifier` / `label`    | Local variables, fields                 |
| `OPERATOR_*`                 | `ECOLOUR_OPERATOR`        | `operator`                | `+`, `-`, `=`, `==`                     |
| `SEPARATOR`, `STATEMENT_SEP` | `ECOLOUR_PUNCTUATION`     | `punctuation`             | `,`, `;`                                |
| `LH_*/RH_*` (Blocks, Exprs)  | `ECOLOUR_PAREN`           | `paren`                   | `(`, `)`, `{`, `}`, `[`, `]`            |
| N/A (Dynamic Matching)       | `ECOLOUR_MATCH`           | `match`                   | Dynamic cursor matching of `()` or `{}` |

## Modern Profile Theme Configuration

By default, `profile.the` maps these areas to a modern dark theme inspired by VSCode/modern editors. You can customize them using the standard `SET ECOLOR` command:

```rexx
/* Examples from default profile.the */
'set ecolor comment #6A9955 on #1E1E1E'
'set ecolor string #CE9178 on #1E1E1E'
'set ecolor keyword #569CD6 on #1E1E1E'
'set ecolor preprocessor #C586C0 on #1E1E1E'
'set ecolor type #4EC9B0 on #1E1E1E'
'set ecolor function #DCDCAA on #1E1E1E'
'set ecolor constant #4FC1FF on #1E1E1E'
'set ecolor identifier #9CDCFE on #1E1E1E'
'set ecolor number #B5CEA8 on #1E1E1E'
'set ecolor operator #D4D4D4 on #1E1E1E'
'set ecolor punctuation #D4D4D4 on #1E1E1E'
'set ecolor paren #D4D4D4 on #1E1E1E'

/* Bracket Matching (Highlighting the matching brace under cursor) */
'set ecolor match black on #4D4D4D'
```

## Note on Bracket Matching (`ECOLOUR_MATCH`)
Unlike other tokens which are determined directly by the external parser, `ECOLOUR_MATCH` is applied dynamically by THE's rendering engine when the user's cursor is positioned directly over a structural parenthesis, brace, or bracket. It replaces the classic `A_REVERSE` logic to allow full 24-bit RGB customization of the match indicator.
