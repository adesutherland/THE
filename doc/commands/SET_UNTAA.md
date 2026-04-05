# SET UNTAA
**specifies if "Unsigned Numerical Targets Are Absolute"**

## Syntax
```text
[SET] UNTAA ON|OFF
```

## Description
The SET UNTAA command allows the user to turn on or off the behaviour of unsigned numerical
targets.
Numerical targets have the form [:|;|+|-]nn. By default, if the optional portion of the target is not
supplied, then a '+' is assumed. WIth SET UNTAA set to ON, if the optional portion of the target is
not supplied, then a ':' is assumed.
Caution: This SET command affects all numerical targets, not just targets in the LOCATE command.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## Default
OFF

## Status
Complete.
