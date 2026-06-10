# UTFTEXT
**simulate keyboard entry from Unicode code points**

## Syntax
```text
UTFTEXT U+codepoint[+codepoint...] [U+codepoint[+codepoint...]...]
```

## Description
`UTFTEXT` converts Unicode code-point notation to UTF-8 and then enters
the resulting text through the same focused editor path as `TEXT`.

Like `TEXT`, this command simulates keyboard text entry. It does not move focus;
macros and tests should position the cursor in the intended command, prefix, or
file area before issuing it.

Use this command for deterministic tests, profiles, and LLM clients. Literal
UTF-8 text should use `TEXT` directly.

Examples:

```text
UTFTEXT U+0041 U+4E2D U+0042
UTFTEXT U+1F1FA+1F1F8
```

The older `UTFTEXT CODES ...` spelling is accepted as a compatibility alias.

The parser accepts Unicode scalar values from `U+0020` through `U+10FFFF`.
Surrogates, malformed tokens, empty chained components, and C0/DEL control
characters are rejected. The input order is preserved and no Unicode
normalization is applied.

## See Also
[`TEXT`](TEXT.md), [`UTFINPUT`](UTFINPUT.md)

## Status
Complete.
