# SET STATUSLINE
**set position of status line**

## Syntax
```text
[SET] STATUSLine Top|Bottom|Off|GUI
```

## Description
The SET STATUSLINE command determines the position of the status line for the editing session.
TOP will place the status line on the first line of the screen; BOTTOM will place the status line on the
last line of the screen; OFF turns off the display of the status line.
The GUI option is only meaningful for those platforms that support a separate status line window. If
specified for non-GUI ports, the GUI option is equivalent to OFF.

## Compatibility
XEDIT: N/A
KEDIT: Compatible.
Added GUI option for THEdit port.

## Default
Bottom

## Status
Complete
