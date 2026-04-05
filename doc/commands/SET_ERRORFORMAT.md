# SET ERRORFORMAT
**set format of error messages**

## Syntax
```text
[SET] ERRORFormat Normal|Extended
```

## Description
The ERRORFORMAT command allows the user to specify if extended information is displayed with
error messages. The Normal format is an error number, error text and option arguments following.
The Extended format prefixes the Normal format with the command being executed at the time of the
error. This assists in tracking down errors inside macros.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## Default
Normal

## Status
Complete.
