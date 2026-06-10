# TEXT
**simulate keyboard entry of characters**

## Syntax
```text
TEXT text
```

## Description
The TEXT command simulates the entry of text from the keyboard. This command is
actually called when you enter text from the keyboard, so it writes to the
currently focused command, prefix, or file area.

In UTF-8 builds, `TEXT` accepts literal UTF-8 text and inserts complete decoded
code points when the file area is focused. Use `UTFTEXT` when a profile, test,
or LLM client needs deterministic code-point entry without embedding literal
UTF-8 characters in the command stream.

## See Also
[`UTFTEXT`](UTFTEXT.md)

## Compatibility
XEDIT: N/A
KEDIT: Compatible.
Does not allow trailing spaces in text.

## Status
Complete.
