# HIT
**simulate hitting of the named key**

## Syntax
```text
HIT key
```

## Description
The HIT command enables the simulation of hitting the named key . This is most useful from within a
macro.
Be very careful when using the HIT command with the DEFINE command. If you assign the HIT
command to a key, DO NOT use the same key name. e.g. DEFINE F1 HIT F1 This will result in an
infinite processing loop.

## Compatibility
XEDIT: N/A
KEDIT: Similar, but more like the MACRO command.

## Status
  Complete.
