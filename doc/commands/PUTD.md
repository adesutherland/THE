# PUTD
**write part of a file to another and delete**

## Syntax
```text
PUTD [target] [filename]
```

## Description
The PUTD command writes a portion of the current file, defined by target to another file, either
explicit or temporary, and then deletes the lines written.
When no filename is supplied the temporary file used for PUT and GET commands is overwritten.
When a filename is supplied the portion of the file written out is appended to the specified file.
If 'CLIP:' is used in place of filename , the portion of the file specified by target is written to the
clipboard. This option only available for X11, OS/2 and Win32 ports of THE.

## Compatibility
XEDIT: Compatible.
KEDIT: Compatible.

## See Also
PUT, GET

## Status
Complete.
