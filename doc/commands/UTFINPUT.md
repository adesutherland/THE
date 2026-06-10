# UTFINPUT
**insert a line from Unicode code points**

## Syntax
```text
UTFINPUT U+codepoint[+codepoint...] [U+codepoint[+codepoint...]...]
```

## Description
`UTFINPUT` converts Unicode code-point notation to UTF-8 and then inserts
the resulting string after the current line through the same editor path as
`INPUT`.

Use this command for deterministic tests, profiles, and LLM clients. Literal
UTF-8 strings should use `INPUT` directly.

Examples:

```text
UTFINPUT U+0041 U+4E2D U+0042
UTFINPUT U+1F44B+1F3FD
```

The older `UTFINPUT CODES ...` spelling is accepted as a compatibility alias.

The parser accepts Unicode scalar values from `U+0020` through `U+10FFFF`.
Surrogates, malformed tokens, empty chained components, and C0/DEL control
characters are rejected. The input order is preserved and no Unicode
normalization is applied.

## See Also
[`INPUT`](INPUT.md), [`UTFTEXT`](UTFTEXT.md)

## Status
Complete.
