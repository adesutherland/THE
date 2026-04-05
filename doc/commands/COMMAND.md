# COMMAND
**execute a command without translation**

## Syntax
```text
COMMAND command [options]
```

## Description
The COMMAND command executes the specified command without synonym or macro translation.
THE does not attempt to execute the command as a macro even if SET IMPMACRO is ON. The
command will be passed to the operating system if SET IMPOS is ON.

## Compatibility
XEDIT: Compatible.
KEDIT: Compatible.

## Status
Complete.
