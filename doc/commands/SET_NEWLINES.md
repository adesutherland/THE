# SET NEWLINES
**set position of cursor after adding blank line**

## Syntax
```text
[SET] NEWLines Aligned|Left
```

## Description
The SET NEWLINES set command determines where the cursor displays after a new line is added to
the file.

With ALIGNED , the cursor will display in the column of the new line immediately underneath the
first non-blank character in the line above. With LEFT , the cursor will display in the first column of
the new line.

## Compatibility
XEDIT: N/A
KEDIT: Same command, different functionality.

## Default
Aligned

## Status
Complete
