# SET INPUTMODE
**set input mode behaviour**

## Syntax
```text
[SET] INPUTMode OFF|FUll|LIne
```

## Description
The SET INPUTMODE command changes the way THE handles input.
When INPUTMODE LINE is in effect, pressing the ENTER key while in the filearea will result in a
new line being added.
When INPUTMODE OFF is in effect, pressing the ENTER key while in the filearea will result in the
cursor moving to the beginning of the next line; scrolling the screen if necessary.
When INPUTMODE FULL is in effect, pressing the ENTER key while in the filearea will result in
the cursor moving to the beginning of the next line; scrolling the screen if necessary.

## Compatibility
XEDIT: N/A
KEDIT: Compatible.

## Default
LINE

## See Also
INPUT

## Status
Incomplete. No support for FULL option.
