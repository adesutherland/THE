# SET STATOPT
**set display options on statusline**

## Syntax
```text
[SET] STATOPT ON option column [length [prompt] ]
[SET] STATOPT OFF option|*
```

## Description

The SET STATOPT command allows the user to specify which internal settings of THE are to be
displayed on the status line .
The option argument is any value returned by the EXTRACT command. eg NBFILE.1.
The syntax of the ON option, displays the specified value, at the position in the status line specified
by column . If supplied, length specifies the number of characters, beginning at the first character of
the returned value, to display. A value of 0 indicates that the full value if to be displayed. The optional
prompt argument, allows the user to specify a string to display immediately before the returned value.
OFF, removes the specified option from displaying. If * is specified, all displayed options will be
removed.
column is relative to the start of the status line . The value of column must be > 9, so that the version
of THE is not obscured.
Options will be displayed in the order in which they are set.
If SET CLOCK or SET HEX are ON, these will take precedence over options specified with this
command.
The more values you display the longer it will take THE to display the status line . Also, some values
that are available via EXTRACT are not really suitable for use here. eg CURLINE.3.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## Default
There are no default options displayed. The `profile.the` script typically turns them off.

## Examples
To display the parser message dynamically on the status line, use:
`SET STATOPT ON pmsg.1 23 45 Msg=`

## Status
Complete.
