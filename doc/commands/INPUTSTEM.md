# INPUTSTEM
**insert lines from a Rexx stem variable**

## Syntax
```text
INPUTSTEM stem.
```

## Description
The INPUTSTEM command inserts lines from a Rexx stem variable into the current file after the
current line. `stem.0` is read as the number of lines, and `stem.1` through `stem.n` are inserted
in order without trimming leading or trailing spaces.

INPUTSTEM is intended for Rexx macros that call `ADDRESS THE` with an exposed stem:

```rexx
out = .string[]
out[1] = "first line"
out[2] = "second line"
address the "inputstem out." expose out[]
```

This is useful when a macro has built or captured output in a cREXX `.string[]`
and wants to refresh a THE buffer directly.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## Status
Complete.
