# SET CURSORSTYLE
**set the cursor shape for insert and overwrite modes**

## Syntax
```text
[SET] CURSORStyle INSERT|OVERWRITE BLOCK|UNDERLINE|IBEAM [BLINK|STEADY]
```

## Description
The SET CURSORSTYLE command sets the appearance of the cursor in
INSERT and OVERWRITE (replace) modes.

NOTE: Explicit cursor shapes are only fully supported when THE is 
run under NCURSES in a modern terminal emulator. On PDCurses/Windows,
shapes fallback to block or underline based on capabilities.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## Default
INSERT IBEAM BLINK
OVERWRITE BLOCK STEADY

## Status
Complete.