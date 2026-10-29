# SET SYNONYM
**define and remove command aliases**

## Syntax
```text
[SET] SYNonym ON|OFF
[SET] SYNonym [LINEND char] newname [n] definition
[SET] SYNonym newname
```

## Description
`SET SYNONYM` is the supported way to add named commands. Names contain letters only.
The optional minimum abbreviation length `n` is between 1 and the name length; it
defaults to the full length. An exact name wins over other aliases' abbreviations.
Otherwise an abbreviation must identify one alias. Ambiguous names and extra
letters are rejected. A new definition replaces the same full name. A name without
a definition removes it, even when processing is off.

`ON` and `OFF` control processing for the current view without deleting definitions.
Definitions belong to the editor session. Fixed definition operands precede the
invocation operands, which are appended to each command in a chain. Only one level
of alias processing occurs. An alias may shadow a built-in; use `COMMAND` to bypass
aliases. `COMMAND` itself cannot be an alias name. `ON`, `OFF` and `LINEND` are also
reserved names.

For example:

```text
SET SYNONYM finish 3 BOTTOM
fin
SET SYNONYM put INPUT fixed
put additional text
SET SYNONYM finish
```

On native THE, aliases can wrap explicit macros and OS commands:

```text
SET SYNONYM build OSNOWAIT make
SET SYNONYM check MACRO check.the
SET SYNONYM result REXX return 7
SET SYNONYM LINEND # compact SET NUMBER OFF#SET PREFIX OFF
```

A leading `REXX` treats the remaining definition as an inline Rexx macro. Rexx forms
require an available interpreter. `LINEND char` separates up to ten built-in
commands. Without an explicit delimiter, the view's enabled LINEND setting is used.
Execution stops on failure and returns that command's return code; reaching a file
boundary follows normal command-line sequencing. Removing or redefining an
executing alias affects its next invocation.

`QUERY SYNONYM` reports `ON` or `OFF`. `QUERY SYNONYM name` reports a complete
definition, and `QUERY SYNONYM *` lists definitions. `EXTRACT /SYNONYM name/` returns
the name, minimum abbreviation length, complete command definition and delimiter
(an empty value when absent). `EXTRACT /SYNONYM */` returns one complete definition
per alias, including all commands in a chain. Complete definitions can be replayed
after `SET SYNONYM`. An unknown queried name is returned as an identity definition.

Browser sessions accept one browser-allowed editor built-in per alias. Fixed
operands and invocation operands are rechecked against browser policy. Rexx,
`MACRO`, OS commands, `RX` tool launches, command chains, `LINEND` and prefix-macro
definitions are unavailable in browser aliases. A rejected or ambiguous alias does
not fall back to a built-in of the same name. Bundled profile shortcuts retain
their separately trusted execution path. Safe browser examples include
`SET SYNONYM finish 3 BOTTOM` and `SET SYNONYM columns SET VERIFY 1`, followed by
`columns 80`.

With cREXX, inline source should declare its language, for example
`SET SYNONYM result REXX options levelb; return 7`. Browser sessions reserve
the alias name `SET` so registration and removal remain accessible.

## Compatibility
XEDIT: Does not support parameter reordering.
KEDIT: Compatible.

## Default
ON

## Status
Complete.
