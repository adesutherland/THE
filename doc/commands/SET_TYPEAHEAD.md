# SET TYPEAHEAD
**set behaviour of screen redraw**

## Syntax
```text
[SET] TYPEAhead ON|OFF
```

## Description
The SET TYPEAHEAD set command determines whether or not THE uses the curses screen display
optimization techniques.
With TYPEAHEAD ON, curses will abort screen display if a keystroke is pending.
With TYPEAHEAD OFF, curses will not abort screen display if a keystroke is pending.
For BSD based curses, this function has no effect.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## Default
OFF

## Status
Complete.
