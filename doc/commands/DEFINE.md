# DEFINE
**assign one or many commands to a key or mouse event**

## Syntax
```text
DEFine key-name [REXX] [command [args] [[#command [args]...]]]
DEFine mouse-key-definition IN window [REXX] [command [args] [[#command [args]...]]]
```

## Description
The DEFINE command allows the user to assign one or many commands and optional parameter(s) to
a key or a mouse button specification.
Commands may be abbreviated.
If multiple commands are assigned, then the LINEND setting must be ON and the LINEND character
must match the character that delimits the commands at the time that the DEFINE command is
executed. LINEND can be OFF at the time the key is pressed.
With no arguments, any existing definition for that key is removed and the key reverts back to its
default assignation (if it had any).
key-name corresponds to the key name shown with the SHOWKEY command.
If the optional keyword; REXX , is supplied, the remainder of the command line is treated as a Rexx
macro and is passed onto the Rexx interpreter (if you have one) for execution.

## Compatibility
XEDIT: N/A
KEDIT: Compatible.
KEDIT does not allow multiple commands except as KEXX
macros.

## See Also
SHOWKEY, SET LINEND

## Status
Complete.
