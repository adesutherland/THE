# VALIDTARGET
**validate a target and return parsed range details**

## Syntax
```text
VALIDTarget [SPARE] target
```

## Description
The VALIDTARGET command validates a THE target using the same parser used by
commands such as CHANGE, COPY, DELETE, and MOVE.

VALIDTARGET is intended for macros, including CREXX-hosted macros, that need the
old `valid_target()` helper capability without using a Rexx external function.

The command sets the following Rexx variables:

```text
VALIDTARGET.0 - 1
VALIDTARGET.1 - ERROR | NOTFOUND | "line count [spare]"
```

`ERROR` means the target syntax is invalid. `NOTFOUND` means the target syntax is
valid, but no matching line was found.

When `SPARE` is supplied, the target may be followed by additional text. That
additional text is returned after the parsed line and count.

## Examples
```rexx
validtarget = .string[]
address the "validtarget 1" expose validtarget[]
```

```rexx
validtarget = .string[]
address the "validtarget spare :7 /fred/" expose validtarget[]
```

## Compatibility
XEDIT: N/A
KEDIT: N/A

## Status
Complete.
