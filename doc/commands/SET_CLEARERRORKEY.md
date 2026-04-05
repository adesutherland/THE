# SET CLEARERRORKEY
**specify which key clears the message line**

## Syntax
```text
[SET] CLEARErrorkey *|keyname
```

## Description
The SET CLEARERRORKEY command allows the user to specify which key clears the message
line. By default, any key pressed will cause the message line to be cleared. The keyname specified is
the name returned via the SHOWKEY command.

As the QUERY command also uses the same mechanism for displaying its results as errors, then this
command affects when results from the QUERY command are cleared.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## Default
*

## Status
Complete
