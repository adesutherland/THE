# SET FILENAME
**change the filename of the file being edited**

## Syntax
```text
[SET] FILEName filename
```

## Description
The SET FILEName command allows the user to change the filename of the file currently being
edited.
In THE, a fully qualified file name consists of a file path and a file name. THE treats all characters up
to and including the last directory separator (usually / or \) as the file's path. From the first character
after the end of the file's path, to the end of the fully qualified file name is the file name.
A file name is further broken down into a fname and fext. The fname of a file consists of all
characters from the start of the filename up to but not including the last period (if there is one). The
fext of a file consists of all characters from the end of the filename up to but not including the last
period. If there is no period in the filename then the fext is empty.
The fmode of a file is equivalent to the drive letter of the file's path. This is only valid under
native Windows.
Some examples.
                     Full File Name     File            File     Fname Fext      Fmode
                                        Path            Name
                     -----------------------------------------------------------------
                     /usr/local/bin/the /usr/local/bin/ the      the             N/A
                     c:\tools\the.exe   c:\tools\       the.exe the     exe      c
                     /etc/a.b.c         /etc/           a.b.c    a.b    c        N/A
A limited amount of validation of the resulting file name is carried out by this command, but some
errors in the file name will not be evident until the file is saved.
A leading "=" indicates that the fname portion of the current file name is be retained. This is
equivalent to the command SET FEXT . A trailing "=" indicates that the fext portion of the current
file name is to be retained. This is equivalent to the command SET FNAME .
Only one "=" is allowed in the parameter.
Some examples.

                     File Name   Parameter New File Name
                     -----------------------------------------------------------------
                     a.b.c       fred.c=    fred.c.c      SET FNAME fred.c

                    a.b.c          fred.c.=      fred.c..c        SET FNAME fred.c.
                    a.b.c          =fred         a.c.fred         SET FEXT fred
                    a.b.c          =.fred        a.c..fred        SET FEXT .fred
                    a              =d            a.d              SET FEXT d
                    a.b.c          =             a.b.c            does nothing
It is not possible to use this command on pseudo files.

## Compatibility
XEDIT: N/A
KEDIT: Compatible.

## See Also
SET FPATH, SET FNAME, SET FEXT, SET FMODE, SET EQUIVCHAR

## Status
Complete.
