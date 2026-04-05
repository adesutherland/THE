# PRINT
**send text to default printer or print spooler**

## Syntax
```text
PRint [target] [n]
PRint LINE [text]
PRint STRING [text]
PRint FORMfeed
PRint CLOSE
```

## Description
The PRINT command writes a portion of the current file to the default printer or print spooler, or text
entered on the command line.
PRINT [ target ] [ n ] Sends text from the file contents up to the target to the printer followed by a
CR/LF (DOS) or LF(UNIX) after each line. When [ n ] is specified, this sends a formfeed after [ n ]
successive lines of text. PRINT LINE [ text ] Sends the remainder of the text on the command line to
the printer followed by a LF(UNIX), CR(MAC) or CR/LF (DOS). PRINT STRING [ text ] Sends the
remainder of the text on the command line to the printer without any trailing line terminator. PRINT
FORMfeed Sends a formfeed (^L) character to the printer. PRINT CLOSE Closes the printer spooler.

## Compatibility
XEDIT: N/A
KEDIT: Compatible.

## See Also
SET PRINTER

## Status
Complete.
