# SET TRAILING
**specify how to treat trailing blanks on lines**

## Syntax
```text
[SET] TRAILING ON|OFF|REMOVE|SINGLE|EMPTY
```

## Description
The SET TRAILING set command determines how trailing blanks on lines are handled when written
to disk. TRAILING ON means that THE will not treat trailing blanks any differently from any other
characters in the file. Trailing blanks are left on the line while reading in and editing, and retained
when the file is written to disk. With TRAILING OFF, THE will remove trailing blanks when a file is
read, remove them during an edit session, and not write any trailing blanks to the file. With
TRAILING REMOVE, THE will leave trailing blanks on the end of a line when the file is read in and
during editing, but will remove trailing blanks when a file is written to disk. TRAILING SINGLE is
the same as TRAILING OFF, except that a single blank character is appended to the end of every line
when the file is written. TRAILING EMPTY is the same as TRAILING OFF, except that otherwise
empty lines will be written with a single trailing blank.
Note that the default for this under THE is ON. This is because of the way that THE processes profile

files. If the default was OFF, and you had TRAILING ON in your profile, then there would be no way
to retain the original trailing blanks.
If THE is started with -u switch, then any change to TRAILING is ignored; the file will be handled as
though TRAILING ON is in effect.

## Compatibility
XEDIT: N/A
KEDIT: Compatible. REMOVE is an THE extension.

## Default
ON

## Status
Complete. Some trailing blank behaviour while editing files incomplete.
