# PUT
**write part of a file to another**

## Syntax
```text
PUT [target] [filename]
```

## Description
The PUT command writes a portion of the current file, defined by target to another file, either
explicit or temporary.
When no filename is supplied the temporary file used for PUT and GET commands is overwritten.
When a filename is supplied the portion of the file written out is appended to the specified file.
If 'CLIP:' is used in place of filename , the portion of the file specified by target is written to the
clipboard. This option is only available for X11 and native Windows ports of THE.

## Compatibility
XEDIT: Compatible.
KEDIT: Compatible.

## See Also
PUTD, GET

## Status
Complete.
