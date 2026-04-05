# SET AUTOSAVE
**set autosave period**

## Syntax
```text
[SET] AUtosave n|OFF
```

## Description
The SET AUTOSAVE command sets the interval between automatic saves of the file, or turns it off
altogether. The interval n refers to the number of alterations made to the file. Hence a value of 10 for
n would result in the file being automatically saved after each 10 alterations have been made to the

file.
It is not possible to set AUTOSAVE for 'pseudo' files such as the directory listing 'file' , Rexx output
'file' and the key definitions 'file'

## Compatibility
XEDIT: Does not support [mode] option.
KEDIT: Compatible.

## Default
OFF

## Status
Complete.
