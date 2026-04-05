# MSG
**display message on error line**

## Syntax
```text
MSG [message]
```

## Description
The MSG command displays a message on the message line . This command is usually issued from a
macro file. This is similar to EMSG , but MSG does not sound the bell if SET BEEP is on.
If the number of messages displayed on the message line exceeds the number of lines defined in the
message line as set by SET MSGLINE , a prompt will be displayed. If a macro is being executed, the
prompt will indicate that the user may terminate the macro by pressing the SPACE bar or any other
key to continue execution of the macro.

## Compatibility
XEDIT: Compatible.
KEDIT: Compatible.

## See Also
CMSG, EMSG, SET MSGLINE

## Status
Complete.
