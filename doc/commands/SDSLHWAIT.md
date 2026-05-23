# SDSLHWAIT
**waits for SDSLH syntax parsing to complete**

## Syntax

```text
SDSLHWAIT [milliseconds]
```

## Description

The SDSLHWAIT command waits for the current file's SDSLH parser to complete
any active background parse. If the editor has pending text changes for SDSLH,
it first sends those changes to the parser.

This is mainly intended for macros and tests that need deterministic SDSLH
syntax state after scripted edits. Interactive editing normally schedules and
redraws SDSLH updates from the editor loop.

If `milliseconds` is omitted, SDSLHWAIT waits up to 2000 milliseconds. The
maximum accepted value is 60000 milliseconds.

## Compatibility

XEDIT: N/A

KEDIT: N/A

