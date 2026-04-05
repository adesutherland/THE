# SEARCH
**locate a string**

## Syntax
```text
SEArch string target
```

## Description
The SEARCH command searches for the next or previous occurrence of the specified string target .
If no parameter is supplied, SEARCH uses the the last target specified. If no prior target has been
specified, an error message is displayed.
The SEARCH command is similar to the LOCATE command, but it only locates strings or regular
expressions. The advantage of SEARCH over LOCATE is that targets are searched from the current
focus line and focus column and if the found target is not currently in view, it will change the view to
make the target visible. This behaviour is more compatible with other editors than the behaviour of
LOCATE .
When searching backwards, not only is the search done from the focus line to the end of file, but the
searching within a line is done from right to left.
string target can also be specified as a regular expression. The syntax of this is "Regexp /re/". eg
SEARCH RE /[0-9].*$/

## Compatibility
XEDIT: N/A
KEDIT: N/A

## See Also
LOCATE

## Status
Complete.
