# ?
**retrieve - return the next/prior command on the command line**

## Syntax
```text
?[+|?...]
```

## Description
The ? command returns the next or prior command from the command line ring and displays it on the
command line.
With the [ + ] argument, the next command in the command ring is retrieved.
With no arguments, the previous command entered on the command line is retrieved.
With multiple, concatenated ?s as argument, the previous command entered on the command line is
retrieved corresponding to the number of ?s entered.
For Example: The command; ????? will retrieve the fifth last command entered.

## Compatibility
XEDIT: Compatible. Support for +.
KEDIT: See below..
This command is bound to the up and down arrows when on the
command line depending on the setting of SET CMDARROWS .

## See Also
SET CMDARROWS

## Status
Complete.
