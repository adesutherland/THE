# READV
**read keystrokes and pass to macro**

## Syntax
```text
READV Cmdline [initial text]
READV KEY
```

## Description
The READV command allows a Rexx macro to interact with the user by accepting either individual
keystrokes ( KEY ) or a complete line of text ( Cmdline ).
The READV Cmdline can take optional initial text to be displayed on the command line.
The 'macro' obtains the entered information by setting Rexx variables. These are set as follows.
KEY option:
readv.0 = 4
readv.1 = name of key (empty if unknown)
readv.2 = ASCII value of key (null if not an ASCII code)
readv.3 = curses key value (or ASCII code if an ASCII code)
readv.4 = shift status (see below)
CMDLINE option:
readv.0 = 1
readv.1 = contents of command line

While editing the command in READV Cmdline , any key redefinitions you have made will be in
effect. Therefore you can use your "normal" editing keys to edit the line. THE will allow the
following commands to be executed while in READV Cmdline :
CURSOR LEFT, CURSOR RIGHT, CURSOR DOWN, CURSOR UP,
SOS FIRSTCHAR , SOS ENDCHAR , SOS STARTENDCHAR ,
SOS DELEND , SOS DELCHAR , SOS DELCHAR ,
SOS TABB , SOS TABF , SOS TABWORDB , SOS TABWORDF ,
SOS UNDO , SOS DELWORD , SET INSERTMODE , TEXT
Either of the keys, TAB, ENTER, RETURN and NUMENTER will terminate READV Cmdline ,
irrespective of what THE commands have been assigned.
The shift status of the key is an eight character string of 0 or 1; each position represented by the
following.
Position 1 1 if INSERTMODE is ON 2 always 0 3 always 0 4 always 0 5 1 if ALT key pressed 6 1 if
CTRL key pressed 7 1 if SHIFT key pressed 8 same as position 7

## Compatibility
XEDIT: Similar to READ CMDLINE option.
KEDIT: Compatible.

## Status
Complete.
