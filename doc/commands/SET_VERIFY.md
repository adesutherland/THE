# SET VERIFY
**set column display limits**

## Syntax
```text
[SET] Verify first [last]
```

## Description
The SET VERIFY command sets the column limits for the display of the current file. first specifies
the first column to be displayed and last specifies the last column to be displayed.
If no last option is specified '*' is assumed.
All options can be specified as the current EQUIVCHAR to retain the existing value.

## Compatibility
XEDIT: Does not implement HEX display nor multiple column pairs.
KEDIT: Does not implement HEX display nor multiple column pairs.

## Default
1*

## See Also
SET ZONE, SET EQUIVCHAR

## Status

Complete.
