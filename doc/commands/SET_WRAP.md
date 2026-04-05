# SET WRAP
**enable/disable string locates around the end of the file**

## Syntax
```text
[SET] WRap ON|OFF
```

## Description
The SET WRAP set command determines whether THE will look for a string target off the ends of
the file.
With WRAP OFF, THE will attempt to locate a string target from the current line to the end of file (or
top of file if the locate is a backwards search).
With WRAP ON, THE will attempt to locate a string target from the current line to the end of file (or
top of file if the locate is a backwars search) and wrap around the end of the file and continue
searching until the current line is reached.
If the string target is located after wrapping around the end of the file, the message 'Wrapped...' is
displayed.
Commands affected by SET WRAP are; LOCATE , FIND , NFIND , FINDUP and NFINDUP .

## Compatibility
XEDIT: N/A
KEDIT: Compatible.

## Default
OFF

## See Also
LOCATE, FIND, NFIND, FINDUP, NFINDUP

## Status
Complete.
