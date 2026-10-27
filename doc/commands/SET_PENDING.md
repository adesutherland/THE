# SET PENDING
**set status of pending prefix commands**

## Syntax
```text
[SET] PENDing ON string
[SET] PENDing OFF
[SET] PENDing BLOCK string
[SET] PENDing ERROR string
```

## Description
The SET PENDING command allows the user to insert or remove commands from the pending prefix
list.
ON string, simulates the user typing string in the prefix area of the focus line .
OFF, removes any pending prefix command from the focus line.
BLOCK string, simulates the user typing string in the PREFIX area of the focus line and identifies the
prefix command to be a BLOCK command.
ERROR string displays `?string` in the prefix area, removes the executable
pending entry, and clears the error marker on the next prefix-processing pass.
OFF clears both the pending entry and its visible prefix text.

In XEDIT compatibility mode, entries queued or replaced during a prefix macro
wait until the next prefix-processing pass. A nonzero macro result stops the
current pass. Native THE compatibility retains its existing same-pass timing.
An explicit `SOS DOPREFIX` starts a pass; a nested call during a pass is ignored.

## Compatibility
XEDIT: ON, OFF, BLOCK and ERROR are supported in this fork. This does not
imply full XEDIT macro compatibility.
KEDIT: N/A

## Status

Complete.
