# SET PRINTER
**define printer spooler name**

## Syntax
```text
[SET] PRINTER spooler|[OPTION options]
```

## Description
The SET PRINTER command sets up the print spooler name to determine where output from the
PRINT command goes.
The options can be one of the following: CPI n (characters per inch) LPI n (lines per inch)
ORIENTation Portrait|Landscape FONT fontname (name of fixed width font)
No checking is done for printer options. i.e. You may specify a font that THE doesn 't know about,
and the printing process may not work after that.'
The defaults for page layout for Win32 are: CPI 16 LPI 8 ORIENTation Portrait FONT LinePrinter
BM
options are only valid for Win32 platform. Printer output for the Win32 platform ALWAYS goes to
the default printer. Therefore, the spooler option is invalid on this platform.

## Compatibility
XEDIT: N/A
KEDIT: Compatible. THE adds more functionality.

## Default
- LPT1 - DOS/OS2, lpr - Unix, default - Win32

## See Also
PRINT

## Status
Complete.
