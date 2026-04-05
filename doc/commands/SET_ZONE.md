# SET ZONE
**set column limits for editing**

## Syntax
```text
[SET] Zone first [last]
```

## Description
The SET ZONE command sets the column limits for various other editor commands, such as
LOCATE and CHANGE . It effectively restricts to the specified columns those parts of the file which
can be acted upon.
If no last option is specified '*' is assumed.
All options can be specified as the current EQUIVCHAR to retain the existing value.

## Compatibility
XEDIT: Compatible.
KEDIT: Compatible.

## Default
1*

## See Also
SET VERIFY, SET EQUIVCHAR

## Status
Complete.

The Hessling Editor is Copyright © Mark Hessling, 1990-2022 <mark@rexx.org>
