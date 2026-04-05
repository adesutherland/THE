# LOCATE
**search for a target**

## Syntax
```text
[Locate] target [command]
```

## Description
The LOCATE command searches for the next or previous occurrence of the specified target . If no
parameter is supplied, LOCATE uses the the last target specified. If no prior target has been specified,
an error message is displayed.
target can also be specified as a regular expression. The syntax of this is "Regexp /re/". eg LOCATE
RE /[0-9].*$/
With an optional command , this command is executed after finding the target .

## Compatibility
XEDIT: Compatible.
KEDIT: Compatible.

## Status
Complete.
