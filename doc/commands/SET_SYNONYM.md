# SET SYNONYM
**define synonyms for commands (unavailable)**

## Syntax
```text
[SET] SYNonym ON|OFF
[SET] SYNonym [LINEND char] newname [n] definition
```

## Description
The SET SYNONYM command allows the user to define synonyms for commands or macros.
The first format indicates if synonym processing is to be performed.
The second format defines a command synonym.
The synonym is newname , which effectively adds a new THE command with the definition specified
by definition . The n parameter defines the minimum length of the abbreviation for the new command
An optional LINEND character can be specified prior to newname if the definition contains multiple
commands.
definition can be of the form: [REXX] command [args] [#command [args] [...]] (where # represents
the LINEND character specified prior to newname )
If the optional keyword; 'REXX' , is supplied, the remainder of the command line is treated as a Rexx
macro and is passed onto the Rexx interpreter (if you have one) for execution.
Only 1 level of synonym processing is carried out; therefore a synonym cannot be specified in the
definition .

## Compatibility
XEDIT: Compatible. Does not support format that can reorder parameters.
KEDIT: Compatible.

## Default
OFF

## Status
Incomplete.
