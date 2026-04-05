# SET REXXOUTPUT
**indicate where Rexx output is to go**

## Syntax
```text
[SET] REXXOUTput File|Display n
```

## Description
The SET REXXOUTPUT command indicates where output from the Rexx interpreter is to go; either
captured to a file in the ring or displayed in a scrolling fashion on the screen.
Also specified is the maximum number of lines from the Rexx interpreter that are to be displayed or
captured. This is particularly useful when a Rexx macro gets into an infinite loop.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## Default
Display 1000

## Status
Complete.
