# SET EOLOUT
**set end of line terminating character(s)**

## Syntax
```text
[SET] EOLout CRLF|LF|CR|NONE
```

## Description
The EOLOUT command allows the user to specify the combination of characters that terminate a line.
Lines of text in Unix/POSIX files are usually terminated with a LF. Windows-style text files usually
end with a CR and LF combination. Classic Mac-style files are usually terminated with a CR.
The NONE option can be used to specify that no end of line character is written.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## Default
- LF - Unix/POSIX
- CRLF - Windows
- NONE - if THE started with -u option

## Status
Complete.
