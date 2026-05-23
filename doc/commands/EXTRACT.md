# EXTRACT
**obtain various internal information about THE**

## Syntax
```text
EXTract /item/[...]
```

## Description
The EXTRACT command is used to relay information about settings within THE from within a Rexx
macro. EXTRACT is only valid within a Rexx macro.
The '/' in the syntax clause represents any delimiter character.
For a complete list of 'item's that can be extracted, see the section; QUERY, EXTRACT and STATUS
.'

`EXTRACT /MESSAGES/` returns the current remembered message list for macros:
`messages.0` is the number of messages and `messages.n` is the nth message,
oldest first. `EXTRACT /MESSAGES n/` returns the latest `n` messages.

## Compatibility
XEDIT: Compatible.
KEDIT: Compatible.

## Status
Complete.
