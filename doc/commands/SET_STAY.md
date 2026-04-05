# SET STAY
**set condition of cursor position after CHANGE/LOCATE**

commands

## Syntax
```text
[SET] STAY ON|OFF
```

## Description

The SET STAY set command determines what line is displayed as the current line after an
unsuccessful LOCATE or successful CHANGE command.
With STAY ON, the current line remains where it currently is.
With STAY OFF, after an unsuccessful LOCATE , the current line becomes the Bottom-of-File line
(or Top-of-File line if direction is backwards).
After a successful CHANGE , the current line is the last line affected by the CHANGE command.

## Compatibility
XEDIT: Compatible.
KEDIT: Compatible.

## Default
ON

## Status
Complete
