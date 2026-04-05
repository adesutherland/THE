# SET COMPAT
**set compatibility mode**

## Syntax
```text
[SET] COMPat The|Xedit|Kedit|KEDITW|Ispf|= [The|Xedit|Kedit|KEDITW|Ispf|=]
[The|Xedit|Kedit|KEDITW|Ispf|=]
```

## Description
The SET COMPAT command changes some settings of THE to make it more compatible with the
look and/or feel of XEDIT, KEDIT, KEDIT for Windows, or ISPF.
This command is most useful as the first SET command in a profile file. It will change the default
settings of THE to initially look and behave like the chosen editor. You can then make any additional
changes in THE by issuing other SET commands.
It is recommended that this command NOT be executed from the command line, particularly if you
have 2 files being displayed at the same time. Although the command works, things may look and
behave strangely :-)
The first parameter affects the look of THE, the second parameter affects the feel of THE, and the
third parameter determines which default function key settings you require.
Any of the parameters can be specified as =, which will not change that aspect of THE's
compatibility.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## Default

  THE THE THE

## Status
  Complete.
