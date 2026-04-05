# SET PENDING
**set status of pending prefix commands**

## Syntax
```text
[SET] PENDing ON string
[SET] PENDing OFF
[SET] PENDing BLOCK string
```

## Description
The SET PENDING command allows the user to insert or remove commands from the pending prefix
list.
ON string, simulates the user typing string in the prefix area of the focus line .
OFF, removes any pending prefix command from the focus line.
BLOCK string, simulates the user typing string in the PREFIX area of the focus line and identifies the
prefix command to be a BLOCK command.

## Compatibility
XEDIT: Does not support ERROR option.
KEDIT: N/A

## Status

Complete.
