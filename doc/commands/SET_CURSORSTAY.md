# SET CURSORSTAY
**set on or off the behaviour of the cursor on a scroll**

## Syntax
```text
[SET] CURSORSTay ON|OFF
```

## Description
The SETCURSORSTAY command allows the user to set the behaviour of the cursor when the file is
scrolled with a FORWARD or BACKWARD command.
Before this command was introduced, the position of the cursor after the file was scrolled depended
on SET COMPAT ; for THE, the cursor moved to the current line, for XEDIT and KEDIT modes the
cursor stayed on the same screen line.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## Default
ON

## Status
Complete.
