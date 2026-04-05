# DIALOG
**display a user configurable dialog box**

## Syntax
```text
DIALOG /prompt/ [EDITfield [/val/]] [TITLE /title/] [OK|OKCANCEL|YESNO|YESNOCANCEL]
[DEFBUTTON n]
```

## Description
The DIALOG command displays a dialog box in the middle of the screen with user-configurable
settings.
The mandatory prompt parameter is the text of a prompt displayed near the top of the dialog window.
Up to 100 lines can be displayed by separating lines with a character (decimal 10).
EDITfield creates a user enterable field, with a default value of val , if supplied. While the cursor is in
the editfield, "normal" edit keys are in effect. See READV for more details on keys that are useable in
the editfield. The same keys that exit from the READV command also exit the editfield. On exit from
the editfield, the first button becomes active.
title specifies optional text to be displayed on the border of the dialog box.
The type of button combination can be specifed as one of the following:
OK - just an OK button is displayed
OKCANCEL - an OK and a CANCEL button are displayed
YESNO - a YES and a NO button are displayed
YESNOCANCEL - a YES, a NO and a CANCEL button are displayed
If no button combination is selected, an OK button is displayed.
If DEFBUTTON is specified, it indicates which of the buttons is to be set as the active button. This is
a number between 1 and the number of buttons displayed. By default, button 1 is active. If EDITfield
is specified, no active button is set.
The active button can be selected by pressing the TAB key; to exit from the DIALOG, press the
RETURN or ENTER key, or click the first mouse button on the required button.
On exit from the DIALOG command, the following Rexx variables are set:
DIALOG.0 - 2
DIALOG.1 - value of 'EDITfield'
DIALOG.2 - button selected as specified in the call to the command.
The colours used for the dialog box are:
Border - SET COLOR DIALOGBORDER
Prompt area - SET COLOR DIALOG
Editfield - SET COLOR DIALOGEDITFIELD

Inactive button - SET COLOR DIALOGBUTTON
Active button - SET COLOR DIALOGABUTTON

## Compatibility
XEDIT: N/A
KEDIT: Compatible. Does not support bitmap icons or font options.

## See Also
POPUP, ALERT, READV, SET COLOR

## Status
Complete.
