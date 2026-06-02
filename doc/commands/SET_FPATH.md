# SET FPATH
**change the path of the existing file**

## Syntax
```text
[SET] FPath path
```

## Description
The SET FPATH command allows the user to change the path of the file currently being edited.
The path parameter can be specified with or without the trailing directory separator. Under native
Windows, the drive letter is considered part of the file's path.
See SET FILENAME for a full explanation of THE's definitions of fpath, filename, fname, fext and
fmode.
It is not possible to use this command on pseudo files.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## See Also
SET FNAME, SET FILENAME, SET FEXT, SET FMODE

## Status
Complete.
