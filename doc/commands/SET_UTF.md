# SET UTF
**configures physical UTF terminal behaviour**

## Syntax
```text
[SET] UTF DISPLAY GROUPED|COMPONENTS|TOGGLE
[SET] UTF TERMINAL CLASS class [DISPLAY display] LAYOUT layout-width CURSOR cursor-width
[SET] UTF TERMINAL CLASS class [DISPLAY display] OUTPUT method [U+codepoint]
[SET] UTF TERMINAL CLASS class [DISPLAY display] CURSORSTRATEGY strategy
[SET] UTF TERMINAL CLASS class [DISPLAY display] REPLACESTRATEGY strategy
```

## Description
The SET UTF command configures the physical terminal profile used by UTF-8 rendering.
These settings describe how the terminal displays, covers, clears, and repaints UTF-8
grapheme clusters. They do not change the bytes stored in the file, the internal logical
UTF-8 text model, or how editing commands such as CHANGE, CINSERT, COVERLAY, CREPLACE,
and REPLACE select or replace text. Editing commands continue to operate on logical UTF-8
cluster positions; SET UTF only adjusts the physical terminal driver profile used to draw
those clusters.

The UTF DISPLAY form selects the global display mode THE prefers when rendering
UTF clusters. THE tries the selected display profile for each class and falls
back to that class's normal profile when no display-specific profile is
configured. It does not affect the logical text model.

- GROUPED prefers the grouped physical profile when one is configured. For a
  ZWJ sequence this usually means one composed glyph, or a configured
  substitute when the terminal cannot reliably render the grouped sequence.
  This is the default.
- COMPONENTS prefers the components physical profile when one is configured.
  For a ZWJ sequence this usually means the visible component characters, with
  joiners expanded or removed according to the configured OUTPUT method.
- TOGGLE switches between GROUPED and COMPONENTS.

For example, a ZWJ sequence such as a family emoji may have a GROUPED profile
that writes one composed sequence and a COMPONENTS profile that writes a
decomposed display. SET UTF DISPLAY selects which profile THE tries first.
Classes with only a normal profile, such as keycap, regional-flag, wide, and
combining in the current defaults, fall back to their normal class profile.

The CLASS operand identifies the UTF-8 feature class to configure. Supported classes are:

- ascii
- combining
- combining-stack
- wide
- ambiguous
- emoji
- text-variation
- emoji-variation
- modifier
- keycap
- regional-flag
- short-zwj
- heart-zwj
- family-zwj
- tag-flag
- private-use

The optional DISPLAY operand in the TERMINAL CLASS form selects which physical
profile entry is being configured for that class. The normal display is used
when DISPLAY is omitted. Display-specific entries use `grouped`, for a grouped
presentation, or `components`, for a sequence displayed as its visible parts.
The current default table provides grouped/components entries for ZWJ classes.
Legacy profile lines using ZWJDISPLAY are accepted for compatibility, but new
profiles should use DISPLAY with OUTPUT.

LAYOUT specifies the number of terminal cells occupied by the class. CURSOR specifies the
number of terminal cells the software cursor or cursor background must cover for that class.

OUTPUT specifies how the class is written to the terminal. Supported methods are:

- native - write the stored UTF-8 sequence
- expanded - write component characters for a decomposed display
- substitute - write a substitute display character

When method is substitute, an optional Unicode codepoint may follow the method.
The codepoint is scoped to that class and display. Substitute output can be used
for any class, not only ZWJ grouped profiles. For example:

```text
SET UTF TERMINAL CLASS short-zwj DISPLAY grouped OUTPUT substitute U+0040
SET UTF TERMINAL CLASS keycap OUTPUT substitute U+25A1
SET UTF TERMINAL CLASS regional-flag OUTPUT substitute U+25A1
```

Expanded output is meaningful for component-style profiles. For normal and
grouped profiles, unsupported expanded requests are treated as native output.

CURSORSTRATEGY specifies the repaint strategy used when the cursor moves across the class.
REPLACESTRATEGY specifies the repaint strategy used after text replacement or overlay.
These are separate because a terminal can handle cursor movement correctly while still
needing a stronger repaint after replacement.

Supported strategies are:

- `cells`: repaint only the changed cells.
- `line`: repaint the visible line/run without a pre-clear.
- `suffix`: clear from the changed cluster to the end, flush, then repaint.
- `prev`: clear from one cluster before the changed cluster, flush, then repaint.
- `first`: clear from the first matching cluster in the visible run, flush, then repaint.
- `whole`: clear the whole visible line/run, flush, then repaint.

At startup, THE loads the built-in UTF terminal defaults, applies any
recognised terminal identity override such as TERM_PROGRAM=Apple_Terminal, and
then applies the file named by the THE_UTF_TERMINAL_PROFILE environment
variable if it is set. During normal profile processing, THE also runs the
platform system profile before the user profile. On macOS this file is
`system-osx.the` in THE_HOME_DIR. It is the profile generated by
`utf_terminal_probe --profile-dir ...` and is the preferred place for physical
terminal calibration settings.

The user profile runs after the system profile, so user settings can override
platform calibration deliberately. The `-n` command-line switch skips only the
user profile; the system profile still runs.

A UTF terminal profile file contains the same SET UTF commands documented
here. REXX-style profile fragments are accepted, so `/* ... */` comments,
`address the`, and quoted instructions such as
`'SET UTF TERMINAL CLASS keycap LAYOUT 2 CURSOR 2'` may be used. Blank lines
and lines beginning with `*` or `#` are ignored.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## Default
SET UTF DISPLAY GROUPED.

Built-in UTF terminal defaults. Additional terminal identity overrides and the optional
THE_UTF_TERMINAL_PROFILE file are applied at startup.

## See Also
CHANGE, CINSERT, COVERLAY, CREPLACE, REPLACE

Design notes for the planned cluster classification and physical mapping
extension live in `doc/utf-cluster-mapping.md`.

## Status
Incomplete.
