# SET READONLY
**allow/disallow changes to a file if it is readonly**

## Syntax
```text
[SET] READONLY ON|OFF|FORCE [File]
```

## Description
The SET READONLY command allows the user to disallow changes to files if they are readonly.
Normally, if a file is readonly, THE allows the user to make changes to the file contents while in the
editing session, but does not allow the file to be saved.
With READONLY ON, THE disallows any changes to be made to the contents of the file in memory,
in much the same way that THE disallows changes to be made to any files, if THE is started with the
-r command line switch.
With READONLY FORCE, THE disallows any changes to be made to the contents of the file in
memory, in the same way that THE disallows changes to be made to any files, if THE is started with
the -r command line switch.
While the -r command line switch disallows changes to be made to any files, SET READONLY ON,
only disallows changes to be made to readonly files. SET READONLY FORCE disallows changes to
be made to the any files irrespective of whether they are readonly on disk.
With the [File] option, SET READONLY ON and SET READONLY FORCE will result in the
current file being readonly. SET READONLY OFF will allow changes to be made to the current file,
provided the global READONLY status is OFF.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## Default
OFF

## Status
Complete.
