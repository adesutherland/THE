# SET INTERFACE
**set overall behaviour of THE**

## Syntax
```text
[SET] INTerface CLASSIC|CUA
```

## Description
The SET INTERFACE command changes the behaviour of several operations within THE. THE
normally operates in a block-mode manner, however many applications conform to the Common User
Access (CUA) standard developed by IBM. This command specifies that CUA behaviour should
occur on various actions during the edit session.
The major differences between CLASSIC and CUA behaviour involve keyboard and mouse actions.
Various THE commands have CUA options to allow the user to customise the behaviour individual
keys or the mouse to behave in a CUA manner.
Where behaviour is not related to particular key or mouse actions, this command provides the
mechanism for changing the behaviour. The behaviour that SET INTERFACE affects:

- entering text in the filearea with a marked CUA block will
first delete the block and reposition the cursor
- executing SOS DELCHAR or SOS DELBACK will delete the
marked CUA block
- executing any positioning command, such as CURSOR DOWN,
FORWARD or CURSOR MOUSE, will unmark the CUA block

## Compatibility
XEDIT: N/A
KEDIT: Compatible with KEDIT for Windows.

## Default
CLASSIC

## See Also
MARK, CURSOR

## Status
Complete.
