# SET TARGETSAVE
**set type(s) of targets to save for subsequent**

LOCATEs

## Syntax
```text
[SET] TARGETSAVE ALL|NONE| STRING REGEXP ABSOLUTE RELATIVE POINT BLANK
```

## Description
The SET TARGETSAVE command allows you to specify which target types are saved for
subsequent calls to the LOCATE command without any parameters.
By default; SET TARGETSAVE ALL, the LOCATE command without any parameters, locates the
last target irrespective of the type of target.
SET TARGETSAVE NONE turns off saving of targets, but does not delete any already saved target.
Any combination of the target types, STRING, REGEXP, ABSOLUTE, RELATIVE, POINT, or
BLANK can be supplied. e.g. SET TARGETSAVE STRING POINT.
As an example, having SET TARGETTYPE STRING then the only target saved will be one that has a
string target component. i.e. if you executed LOCATE /fred/ then LOCATE :3 then LOCATE, the
final LOCATE will look for /fred/ NOT :3

## Compatibility
XEDIT: N/A
KEDIT: N/A

## Default
ALL

## See Also
LOCATE

## Status
Complete.
