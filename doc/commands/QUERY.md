# QUERY
**display various option settings**

## Syntax
```text
Query item
```

## Description

The QUERY command displays the various settings for options set by THE.
For a complete list of 'item's that can be extracted, see the section; QUERY, EXTRACT and STATUS
.'
Results of the QUERY command are displayed at the top of the display window, and ignore the
setting of SET MSGLINE .

`QUERY MESSAGES` redisplays the remembered message list. `QUERY MESSAGES n`
shows the latest `n` messages.

`QUERY PMSGS` lists SDSLH parser diagnostics for the current file. Each entry
shows the diagnostic line, column, severity, code, and message.

## Compatibility
XEDIT: Compatible functionality, but not all options.
KEDIT: Compatible functionality, but not all options.

## See Also
STATUS, MODIFY

## Status
Complete.
