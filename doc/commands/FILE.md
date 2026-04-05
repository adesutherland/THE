# FILE
**write the current file to disk and remove from ring**

## Syntax
```text
FILE [filename]
```

## Description
The FILE command writes the current file to disk to the current file name or to the supplied filename .
Unlike the FFILE command, if the optional filename exists, this command will not overwrite the file.

## Compatibility
XEDIT: Compatible.
KEDIT: Compatible.

## Default

With no parameters, the current file is written.

## See Also
FFILE, SAVE, SSAVE

## Status
Complete
