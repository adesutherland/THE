# SET ECOLOR / ECOLOUR
**Set colours for syntax highlighting**

## Syntax
```text
[SET] ECOLOR element|char [modifier[...]] [foreground] [on background]
[SET] ECOLOR element|char [modifier[...]] ON|OFF
```

## Description
The `SET ECOLOR` (or `SET ECOLOUR`) command allows the user to specify the colours of each category of syntax elements used in syntax highlighting.

Unlike legacy versions of THE or KEDIT which relied exclusively on opaque single-character codes (`A`, `B`, `C`, etc.), this modern implementation allows you to use **descriptive element names**. This makes your `.the` profiles much easier to read and maintain.

If an asterisk (`*`) is supplied, the modifier applies to all syntax elements simultaneously.

---

## Definitive Element Mapping

The table below describes the definitive list of acceptable descriptive syntax elements. It outlines how these descriptive names map to the legacy KEDIT/TLD character codes, as well as how they are driven internally by the **DSL Syntax Highlighter (SDSLH)** engine when it performs live AST parsing.

| Descriptive Element | Legacy Char | DSLSH Internal Mapping (LEXER_*) | Typical Usage |
|:---|:---:|:---|:---|
| **`comment`** | `A` | `LEXER_COMMENT` | Standard block or line comments |
| **`string`** | `B` | `LEXER_STRING_LITERAL` | String literals |
| **`number`** | `C` | `LEXER_NUMBER_LITERAL` | Numeric literals |
| **`keyword`** | `D` | `LEXER_KEYWORD` | Core language keywords |
| **`label`** or **`identifier`** | `E` | `LEXER_IDENTIFIER` | Variables, instances, and labels |
| **`preprocessor`** or **`macro`** | `F` | _N/A_ | Preprocessor directives |
| **`header`** | `G` | _N/A_ | Section headers |
| **`paren`** or **`operator`** or **`match`** | `I` | `LEXER_OPERATOR*` | Punctuation, math, and assignments |
| **`macro_name`** or **`macro_identifier`** | `O` | `LEXER_MACRO_IDENTIFIER` | Macro names and calls |
| **`macro_variable`** or **`macro_var`** | `P` | `LEXER_MACRO_VARIABLE` | Macro/template variables |
| **`macro_constant`** | `Q` | `LEXER_MACRO_CONSTANT` | Macro-time constants |
| **`function`** | `V` | `PARSE_TREE_FUNCTION` | Function and method names |
| **`incomplete_string`** | `S` | _N/A_ | Unclosed string literals |
| **`html_tag`** | `T` | _N/A_ | XML/HTML Tag names |
| **`html_char`** | `U` | _N/A_ | XML/HTML Entities |
| **`level1_paren`** | `I` | _N/A_ | Scope / Level 1 Bracket Matching |
| **`level2_paren`** | `L` | `LEXER_SEPARATOR` / `LEXER_*_BLOCK` | Scope / Level 2 Bracket Matching |
| **`directory`** | `W` | _N/A_ | File system directory paths |
| **`link`** | `X` | _N/A_ | File system links |
| **`executable`** | `Y` | _N/A_ | Executable targets |
| **`alt_keyword1`..`9`** | `1`..`9` | _N/A_ | Custom/Alternate Keywords |

*Note: For full backwards compatibility with legacy `.the` profiles, the legacy single-character codes (`A`, `B`, `1`, etc.) remain fully functional.*

## Example Usage

### Modern Descriptive Form
```text
set ecolor comment #6A9955 on #1E1E1E
set ecolor keyword #569CD6 on #1E1E1E
set ecolor string #CE9178 on #1E1E1E
set ecolor number #B5CEA8 on #1E1E1E
set ecolor function #DCDCAA on #1E1E1E
set ecolor identifier #9CDCFE on #1E1E1E
```

### Legacy Character Form (Equivalent)
```text
set ecolor A #6A9955 on #1E1E1E
set ecolor D #569CD6 on #1E1E1E
set ecolor B #CE9178 on #1E1E1E
set ecolor C #B5CEA8 on #1E1E1E
set ecolor V #DCDCAA on #1E1E1E
set ecolor E #9CDCFE on #1E1E1E
```

## Compatibility
XEDIT: N/A
KEDIT: Compatible (Expanded with descriptive names).

## Default
See `QUERY ECOLOR`

## See Also
`SET COLOURING`, `SET AUTOCOLOUR`, `SET PARSER`, `SET COLOUR`

## Status
Complete. Expanded.
