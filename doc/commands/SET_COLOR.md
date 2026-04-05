# SET COLOR
**set colors for display**

## Syntax
```text
[SET] COLOR area [modifier[...]] [foreground] [ON] [background]
[SET] COLOR area [modifier[...]] ON|OFF
[SET] COLOUR color red blue green
[SET] COLOUR BOLD FONT|BRIGHT
```

## Description
The SET COLOR command changes the colors or display attributes of various display areas in THE.
Valid values for area :
- ALERT - alert boxes; see ALERT
- Arrow - command line prompt
- Block - marked block
- BOUNDmarker - bound markers (GUI platforms only)
- CBlock - current line if in marked block
- CHIghlight - highlighted line if the same as current line
- Cmdline - command line
- CTofeof - as for TOfeof if the same as current line
- CUrline - the current line
- CURSORline - the line in filearea that the cursor is or was on
- Divider - dividing line between vertical split screens
- Filearea - area containing file lines
- GAP - the gap between the prefix area and filearea
- CGAP - the gap between the prefix area and filearea - current

- HIghlight - highlighted line
- Idline - line containing file specific info
- Msgline - error messages
- Nondisp - Non-display characters ( SET ETMODE OFF)
- Pending - pending commands in prefix area
- PRefix - prefix area
- CPRefix - prefix area if the same as current line
- Reserved - default for reserved line
- Scale - line showing scale line
- SHadow - hidden line marker lines
- SLK - soft label keys
- STatarea - line showing status of editing session
- Tabline - line showing tab positions
- TOfeof - Top-of-File line and Bottom-of-File line
- DIALOG - background of a dialog box; see DIALOG
- DIALOGBORDER - border for a dialog box
- DIALOGEDITFIELD - edit field of a dialog box
- DIALOGBUTTON - inactive button in a dialog box
- DIALOGABUTTON - active button in a dialog box
- CBError - diagnostic error background in filearea
- CBWarn - diagnostic warning background in filearea
- CBInfo - diagnostic information background in filearea
- PMSGError - diagnostic error message on status line
- PMSGWarn - diagnostic warning message on status line
- PMSGInfo - diagnostic information message on status line
- POPUP - all non-highlighted lines in a popup; see POPUP
- POPUPBORDER - border for a popup
- POPUPCURLINE - the highlighted line in a popup
- POPUPDIVIDER - dividing line in a popup
- * - All areas (second format only)
Valid values for foreground , background and color :
- BLAck
- BLUe
- Brown
- Green
- GRAy
- GREy
- Cyan
- RED
- Magenta
- Pink
- Turquoise
- Yellow
- White
Valid values for modifier :
- NORmal
- BLInk
- BOld
- BRIght
- High
- REVerse
- Underline
- DARK
- Italic - only available on X11 port with valid Italic font, on
Windows with "GUI" PDcurses, and the SDL2 port.
The second format of this command allows the user to turn on or off any of the valid modifiers.

The third format of this command allows the user to change the intensity of specified colors on
platforms that support changing the content of a color (X11, SDL2, Windows GUI). The specified
color can be changed by supplying the intensity of red, green and blue. These are numeric values
between 0 and 1000 inclusive. eg To change red to blue : SET COLOR RED 0 0 1000. All characters
being displayed as red will be displayed with the specified intensities. Note that this behaviour is not
consistent across platforms, and should be considered experimental at this stage.
The fourth format of this command allows the BOLD modifier to be displayed as an actual bold font,
or as a brighter colour than the normal, non-bold colour. This format is only supported on platforms
that support different fonts. (SDL2)
It is an error to attempt to set a colour on a mono display.

## Compatibility
XEDIT: Functionally compatible. See below.
KEDIT: Functionally compatible. See below.
Does not implement all modifiers.

## Default
Depends on compatibility mode setting and monitor type.

## See Also
SET COMPAT, SET COLOUR, SET ECOLOUR, DIALOG, POPUP

## Status
Complete.
