# SET FNAME
**change the filename of the file being edited**

## Syntax
```text
[SET] FName filename
```

## Description
The SET FNAME command allows the user to change the fname of the file currently being edited.
See SET FILENAME for a full explanation of THE's definitions of fpath, filename, fname, fext and
fmode.
A limited amount of validation of the resulting file name is carried out by this command, but some
errors in the file name will not be evident until the file is saved.
It is not possible to use this command on pseudo files.

## Compatibility
XEDIT: N/A
KEDIT: Compatible.

## See Also
SET FPATH, SET FILENAME, SET FEXT, SET FMODE

## Status
Complete.
