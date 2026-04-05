# SET SLK
**set Soft Label Key definitions**

## Syntax
```text
[SET] SLK n|ON|OFF [text]
```

## Description
The SET SLK command allows the user to specify a short text description to be displayed on the
bottom of the screen, using the terminal's built-in Soft Label Keys, on the last line of the screen.
The n argument of the command represents the label number from left to right, with the first label
numbered 1.
OFF turns off display of the Soft Label Keys.
ON restores the display of the Soft Label Keys.
The main use for this command is to describe the function assigned to a function key, in place of a
reserved line .
On those platforms that support a pointing device, clicking the left mouse button on the Soft Label
Key, is equivalent to pressing the associated function key.
See COMMAND LINE SWITCHES in the THE manual for details on the number and format of Soft
Label Keys.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## Default
- ON - if support for Soft Label Keys is available

## See Also
SET COLOUR

## Status
Complete.
