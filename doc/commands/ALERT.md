# ALERT
**display a user configurable dialog box with notification**

## Syntax
```text
ALERT /prompt/ [EDITfield [/val/]] [TITLE /title/] [OK|OKCANCEL|YESNO|YESNOCANCEL]
[DEFBUTTON n]
```

## Description
The ALERT command is identical to the DIALOG command except that if SET BEEP is on, a beep is
played.
On exit from the ALERT command, the following Rexx variables are set:
ALERT.0 - 2
ALERT.1 - value of 'EDITfield'
ALERT.2 - button selected as specified in the call to the command.
The colours for the alert box are the same as for a dialog box, except the prompt area which uses the
colour set by SET COLOR ALERT.

## Compatibility
XEDIT: N/A
KEDIT: Compatible. Does not support bitmap icons or font options.

## See Also
POPUP, DIALOG, READV, SET COLOR

## Status
Complete.
