# SCHANGE
**selectively change strings**

## Syntax
```text
SCHange /string1/string2/ [target] [n] [m]
```

## Description
The SCHANGE command changes one string of text to another only after confirming each individual
change with the user.
The first parameter to the change command is the old and new string values, separated by delimiters.
The allowable delimiters are '/' '\' and '@' .
The second parameter is the target ; how many lines are to be searched for occurrences of string1 to
be changed.
n determines how many occurrences of string1 are to be changed to string2 on each line.
m determines from which occurrence of string1 on the line changes are to commence.

## Compatibility
XEDIT: Functionally compatible, but syntax different.
KEDIT: Compatible.

## Default

## See Also
CHANGE

## Status
Complete.
