# SET BOUNDMARK
**set bounds marker display**

## Syntax
```text
[SET] BOUNDMARK OFF|Zone|TRunc|MARgins|TABs|Verify
```

## Description
The BOUNDMARK command indicates if boundary markers are to be displayed and if so, where.
Boundary markers are vertical lines drawn before or after certain columns within the filearea . This
command only has a visible effect on GUI platforms, currently only the X11 port.
OFF turns off the display of boundary markers.
ZONE turns on the display of boundary markers, before the zone start column and after the zone end
column.
TRUNC turns on the display of boundary markers, after the truncation column. Not supported.
MARGINS turns on the display of boundary markers, before the left margin and after the right margin.
TABS turns on the display of boundary markers, before each tab column.
VERIFY turns on the display of boundary markers, before each verify column. Not supported.

## Compatibility
XEDIT: N/A
KEDIT: Compatible, but no support for TRUNC or VERIFY option.

## Default
Zone

## Status

Incomplete
