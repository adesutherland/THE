# RIGHT
**scroll the screen to the right**

## Syntax
```text
RIght [n|HALF|FULL]
```

## Description
The RIGHT command scrolls the screen to the right.
If n is supplied, the screen scrolls by that many columns.
RIGHT 0 is equivalent to SET VERIFY 1

If HALF is specified the screen is scrolled by half the number of columns in the filearea .
If FULL is specified the screen is scrolled by the number of columns in the filearea .
If no parameter is supplied, the screen is scrolled by one column.

## Compatibility
XEDIT: Compatible.
KEDIT: Compatible.

## See Also
LEFT, RGTLEFT, SET VERIFY

## Status
Complete.
