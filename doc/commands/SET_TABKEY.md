# SET TABKEY
**set characteristics of the SOS TABF command**

## Syntax
```text
[SET] TABKey Tab|Character Tab|Character
```

## Description
The SET TABKEY sets the action to be taken when the SOS TABF command is executed.
Depending on the insert mode, the SOS TABF command will either display a raw tab character or
will move to the next tab column.

The first operand refers to the behaviour of the SOS TABF command when SET INSERTMODE is
OFF.
The second operand specifies the behaviour when the SOS TABF command is executed when SET
INSERTMODE is ON.
All options can be specified as the current EQUIVCHAR to retain the existing value.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## Default
Tab Character

## See Also
SET EQUIVCHAR

## Status
Complete
