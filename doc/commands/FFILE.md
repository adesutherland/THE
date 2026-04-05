# FFILE
**force a FILE of the current file to disk**

## Syntax
```text
FFile [filename]
```

## Description
The FFILE command writes the current file to disk to the current file name or to the supplied filename
. Unlike the FILE command, if the optional filename exists, this command will overwrite the file.

## Compatibility
XEDIT: N/A
KEDIT: Compatible.

## Default
With no parameters, the current file is written.

## See Also
FILE, SAVE, SSAVE

## Status
Complete
