# SET CURSORSTYLE
**set the cursor shape for insert and overwrite modes**

## Syntax
```text
[SET] CURSORStyle INSERT|OVERWRITE BLOCK|UNDERLINE|IBEAM [BLINK|STEADY]
[SET] CURSORStyle MAINFRAME|3270|MODERN|BLOCK|UNDERLINE
```

## Description
The SET CURSORSTYLE command sets the appearance of the cursor in
INSERT and OVERWRITE (replace) modes.

NOTE: Explicit cursor shapes are only fully supported when THE is 
run under NCURSES in a modern terminal emulator. On PDCurses/Windows,
shapes fallback to block or underline based on capabilities.

In UTF-8 editor windows THE draws a software cursor. `MAINFRAME`/`3270`
maps insert mode to block and overwrite mode to underline. `MODERN` reverses
that mapping. `BLOCK` and `UNDERLINE` apply the same shape to both modes.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## Default
MAINFRAME

## Status
Complete.
