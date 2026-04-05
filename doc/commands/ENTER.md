# ENTER
**execute a command**

## Syntax
```text
ENTER [CUA]
```

## Description
If the cursor is currently on the command line , the ENTER command executes the command
currently displayed.
If the cursor is in the filearea , the ENTER command results in a new line being added after the focus
line , and the cursor placed on the next line depending on the value of SET NEWLINES . If SET
READONLY is ON, then no new lines is added and the cursor is moved to the first column of the
next line.
If the cursor is in the prefix area , any pending prefix commands will be executed.
With the optional CUA argument, when in the filearea , the enter command acts like the SPLIT .

## Compatibility
XEDIT: N/A
KEDIT: N/A

## See Also
SOS EXECUTE, SET NEWLINES, SET READONLY

## Status
Complete.
