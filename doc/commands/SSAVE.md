# SSAVE
**force SAVE to specified file**

## Syntax
```text
SSave [filename]
```

## Description

The SSAVE command writes the current file to disk. If a filename is supplied, the current file is saved
in that file, otherwise the current name of the file is used.
If a filename is supplied and that filename already exists, the previous contents of that filename will be
replaced with the current file.
Both 'Alterations' counters on the idline are reset to zero.

## Compatibility
XEDIT: N/A
KEDIT: Compatible.

## See Also
SAVE, FILE, FFILE

## Status
Complete
