# SET HEXDISPLAY
**configure display of character under cursor**

## Syntax
```text
[SET] HEXDISPlay ON|OFF|CHARS|CODES|BOTH
```

## Description
The SET HEXDISPLAY command controls the status-line field for the character under the cursor.

- OFF hides the field.
- ON and BOTH show the character or UTF-8 cluster preview and its code values.
- CHARS shows only the character or UTF-8 cluster preview.
- CODES shows only the code values.

In UTF-8 builds, code values use compact status notation. The first scalar is shown as `U+nnnn`;
additional scalars are appended as `+nnnn`, for example `U+1F468+200D+1F469`.
The bracketed character preview expands stored clusters. Invisible presentation
components use status-only markers: `FE0E` is shown as `T` and `FE0F` as `E`,
except inside ZWJ cluster previews where they are omitted as internal
presentation glue. Keycap clusters are previewed as the base character followed
by a safe outline marker; a standalone keycap combining mark `20E3` is shown as
`K`. The code values remain the original Unicode values.

## Compatibility
XEDIT: N/A
KEDIT: Compatible.

## Default
ON (BOTH)

## Status
Complete
