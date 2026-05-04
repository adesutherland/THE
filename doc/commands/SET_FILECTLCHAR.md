# SET FILECTLCHAR
**interpret CTLCHAR markup in file-area lines**

## Syntax
```text
[SET] FILECTLChar ON|OFF
```

## Description
The SET FILECTLCHAR command controls whether ordinary file-area lines in the
current file are displayed using the control character definitions from
SET CTLCHAR. When ON, CTLCHAR escape sequences are hidden and change the display
attributes of the following text.

For example:

```text
SET CTLCHAR ! ESCAPE
SET CTLCHAR R PROTECT RED
SET CTLCHAR N OFF
SET FILECTLCHAR ON
```

A line containing `normal !Rred!N normal` is displayed as `normal red normal`,
with the middle word shown using the CTLCHAR `R` attributes.

FILECTLCHAR is intended for generated output buffers and simple XEDIT-style
display panels. It affects display only. The Protect and Noprotect attributes
stored by SET CTLCHAR are still ignored by THE editing commands.
When CTLCHAR colours are applied in file-area lines, THE preserves the current
file-area background and applies the marker foreground/modifiers. This avoids
visible background patches in ordinary output buffers.

FILECTLCHAR display markup takes precedence over parser-based syntax
highlighting for the current file.

## Compatibility
XEDIT: Similar to the undocumented READ NOCHANGE file-line CTLCHAR behaviour.
KEDIT: N/A.

## Default
OFF

## See Also
SET CTLCHAR, SET RESERVED

## Status
Complete.
