# SOS
**execute various sos commands**

## Syntax
```text
SOS sos_command [sos_command ...]
```

## Description
The SOS command is a front end to existing SOS commands. It treats each parameter it receives as a
command and executes it.
The SOS command will execute each command until the list of commands has been exhausted, or
until one of the commands returns a non-zero return code.

## Compatibility

  XEDIT: XEDIT only permits 1 command
  KEDIT: Compatible.

## Status
  Complete.
