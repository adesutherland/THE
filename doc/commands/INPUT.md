# INPUT
**insert the command line contents into the file**

## Syntax
```text
Input [string]
```

## Description
The INPUT command inserts the string specified on the command line into the current file after the
current line .
If SET INPUTMODE FULL is in effect, and the INPUT command is entered on the command line
with no arguments, THE is put into full input mode. If the prefix area is on, it is turned off, the cursor
moved to the filearea and blank lines inserted into the file from the current line to the end of the
screen.
To get out of full input mode, press the key assigned to the CURSOR HOME [SAVE] command.

## Compatibility
XEDIT: Does not provide full input mode option.
KEDIT: Does not provide full input mode option.

## Status
Complete. Except for full input mode capability.
