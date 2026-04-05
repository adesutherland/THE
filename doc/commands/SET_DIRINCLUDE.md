# SET DIRINCLUDE
**set the file mask for directory command**

## Syntax
```text
[SET] DIRInclude *
[SET] DIRInclude [Normal] [Readonly] [System] [Hidden] [Directory]
```

## Description
The DIRINCLUDE command sets the file mask for files that will be displayed on subsequent
DIRECTORY commands. The operand "*" will set the mask to all files, the other options will set the
mask to include those options specified together with "normal" files e.g.
DIRINCLUDE R S
will display readonly and system files together with "normal" files the next time the DIRECTORY
command is issued.
The effects of DIRINCLUDE are ignored in the Unix version.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## Default
*

## See Also
DIRECTORY, LS

## Status
Complete.
