# SET POINT
**assign a name to the current line**

## Syntax
```text
[SET] Point .name [OFF]
```

## Description
The SET POINT command assigns the specified name to the focus line , or removes the name from
the line with the specified name. A valid line name must start with a '.' followed by alphanumeric
characters. e.g. .a .fred and .3AB are valid names.
When a line is moved within the same file, its line name stays with the line.

## Compatibility
XEDIT: Compatible. See below.
KEDIT: Compatible. See below.

## Status
Complete.
