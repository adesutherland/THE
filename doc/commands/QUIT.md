# QUIT
**exit from the current file if no changes made**

## Syntax
```text
QUIT
```

## Description
The QUIT command exits the user from the current file, provided that any changes made to the file
have been saved, otherwise an error message is displayed.
The previous file in the ring then becomes the current file.
If the current file is the only file in the ring , THE terminates.

## Compatibility
XEDIT: Does not support return code option.
KEDIT: Compatible.

## See Also
QQUIT

## Status
Complete
