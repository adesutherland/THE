# SET ARBCHAR
**set arbitrary character(s) for targets**

## Syntax
```text
[SET] ARBchar ON|OFF [char1] [char2]
```

## Description
Set the character to use as an 'arbitrary character' in string targets. The first arbitrary character
matches a group of zero or more characters, the second will match exactly one character.
All options can be specified as the current EQUIVCHAR to retain the existing value.

## Compatibility
XEDIT: Compatible.
Single arbitrary character not supported.
KEDIT: Compatible.
Arbitrary character not supported in CHANGE or SCHANGE commands.

## Default
Off $ ?

## See Also
SET EQUIVCHAR

## Status
Complete.
