# SET BACKUP
**indicate if a backup copy of the file is to be kept**

## Syntax
```text
[SET] BACKup OFF|TEMP|KEEP|ON|INPLACE [suffix]
```

## Description
The SET BACKUP command allows the user to determine if a backup copy of the original file is to
be kept when the file being edited is saved or filed.
KEEP and ON options are the same. ON is kept for compatibility with previous versions of THE.
With OFF , the file being written to disk will replace an existing file. There is a chance that you will
end up with neither the old version of the file or the new one if problems occur while the file is being
written.
With TEMP or KEEP options, the file being written is first renamed to the filename with a .bak
extension. The file in memory is then written to disk. If TEMP is in effect, the backup file is then
deleted.
With INPLACE , the file being written is first copied to a file with a .bak extension. The file in
memory is then written to disk in place of the original. This option ensures that all operating system

file attributes are retained.
The optional suffix specifies the string to append to the file name of the backup copy including a
period if required. The maximum length of suffix is 100 characters. By default this is ".bak".

## Compatibility
XEDIT: N/A
KEDIT: Compatible.
suffix is a THE extension

## Default
KEEP

## See Also
FILE, FFILE, SAVE, SSAVE

## Status
Complete.
