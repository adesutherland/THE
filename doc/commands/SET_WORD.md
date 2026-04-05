# SET WORD
**controls what THE considers a word to be**

## Syntax
```text
[SET] WORD NONBlank|ALPHAnum
```

## Description
The SET WORD set command determines what sequence of characters THE considers a word to be.
This is used in command such as SOS DELWORD , SOS TABWORDF and MARK WORD to
specify the boundaries of the word.
The default setting for SET WORD is NONBlank . THE treats all sequences of characters separated
by a blank (ASCII 32) as words.
With ALPHAnum THE treats a group of consecutive alphanumeric characters as a word. THE also
includes the underscore character and characters with an ASCII value > 128 as alphanumeric.

## Compatibility
XEDIT: N/A
KEDIT: Compatible.

## Default
NONBlank

## Status
Complete.
