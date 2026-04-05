# SET UNDOING
**turn on or off undo facility for the current file**

## Syntax
```text
[SET] UNDOING ON|OFF
```

## Description
The SET UNDOING command allows the user to turn on or off the undo facility for the current file.
At this stage in the development of THE, setting UNDOING to OFF stops THE from saving changes
made to lines in a file, and prevents those lines from being able to be RECOVERed.
Setting UNDOING to OFF will increase the speed at which THE can execute CHANGE and
DELETE commands.

## Compatibility
XEDIT: N/A
KEDIT: Does not support optional arguments.

## Default
ON

## Status
Complete.
