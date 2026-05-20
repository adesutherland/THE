# UTF-8 Enablement Handover

Last updated: 2026-05-20.

## Current State

THE's UTF-8 work is split into two models:

- Logical editor model: grapheme-cluster based, shared across platforms, using
  `TextPos` and utf8proc.
- Physical terminal model: terminal-specific cell widths, cursor widths, output
  methods, cursor repaint strategies, and replacement repaint strategies.

Do not fix terminal paint problems by changing logical cluster segmentation.
The logical model stays stable; terminal quirks belong in terminal profiles.

THE now has a shared UTF-8 repair planner in `src/utfrepair.c`. Cursor
movement and replacement/full-line redraw ask this planner how far left the
physical repair must start, then `show.c` executes the plan. Strategy meaning
should be added or changed there first, not duplicated in renderer branches.
All feature classes, including ASCII, can use the same strategy machinery; the
ASCII fast path is only an optimization when the active ASCII profile is the
native one-cell `cells` default.

The probe is expected to mirror that model. Its calibration UI presents output
and strategy choices consistently across classes; ZWJ working and keycaps
failing should be treated as profile/strategy evidence, not as permission to add
class-specific renderer behavior.

## Important Artifacts

- `doc/utf-design.md`: detailed design, findings, and historical log.
- `tools/utf_terminal_probe.c`: interactive terminal calibration/probe tool.
- `src/utfterm_defaults.h`: shared THE/probe coded default physical terminal
  table.
- `system-osx.the`: macOS system UTF-8 profile consumed by THE and generated
  by the probe.
- `tests/fixtures/utf-render.txt`: manual editor fixture for UTF-8 rendering.
- `tests/test_utfrepair.c`, `tests/test_utfterm.c`, `tests/test_utf_fixture.c`,
  and `tests/test_textpos.c`: repair planning, terminal-profile, fixture, and
  text-position regression coverage.

## macOS Apple Terminal Baseline

The current macOS baseline is `system-osx.the`. It is a complete system
profile, not a defaults-plus-overrides pair. Important observed choices include:

- `regional-flag`: default `L3 C3`, cursor `cells`, replacement
  `suffix`.
- `keycap`: `L2 C2`, cursor `first`, replacement
  `first`.
- `modifier`: `L4 C4`.
- ZWJ grouped display: use `substitute` for `short-zwj`, `heart-zwj`, and
  `family-zwj`.
- ZWJ component display: `short-zwj` uses `native L4 C4`; `heart-zwj` uses
  `expanded L6 C6`; `family-zwj` uses `expanded L8 C8`.

## Probe Usage

Build the probe:

```sh
cmake --build cmake-build-debug --target utf_terminal_probe -j2
```

Open interactive calibration:

```sh
./cmake-build-debug/utf_terminal_probe calibrate all \
  --profile-dir ./cmake-build-debug/release
```

On macOS this reads and writes `system-osx.the` in the selected directory. THE
loads that system profile before the user profile; `-n` suppresses only the
user profile. The install rule copies the build release copy, so calibrating
`cmake-build-debug/release/system-osx.the` before install carries that generated
profile into the installed `share/the` directory.

Validate the saved macOS profile non-visually:

```sh
./cmake-build-debug/utf_terminal_probe calibrate all --no-visual \
  --profile system-osx.the
```

Add `--write-profile` to make a scripted non-visual run rewrite the profile
after validation.

## Next Work

1. Continue manual keycap investigation against the shared repair planner:
   verify cursor movement, scroll redraw, and replacement separately.
2. If a strategy is wrong, fix or extend the generic planner/profile vocabulary
   rather than adding keycap-specific renderer branches.
3. Keep replacement old-line hints covered; replacing a troublesome cluster with
   plain ASCII can still require the old cluster's repair boundary.
4. Add platform probes and baselines for other terminal stacks only after the
   macOS profile path is proven in THE.

## Sequencing Advice

Finish the macOS Apple Terminal path in THE first, then probe Linux and Windows.
The macOS baseline is currently the best-evidenced terminal profile, and it has
a known manual fixture that reproduces the hard keycap/ZWJ behavior. Proving the
complete loop once is more valuable than collecting more platform data before
THE can consume a profile.

The macOS pass must still be profile-driven. Do not hard-code Apple Terminal
rules directly into rendering paths. Treat macOS as the first proof of the
configuration architecture:

1. Load coded defaults.
2. Apply the macOS override fragment.
3. Render from the resulting physical profile table.
4. Validate view, cursor walking, and replacement separately.
5. Only then add Linux, Windows Terminal, iTerm2, or other terminal baselines.

## Known Cautions

- The baseline is visual and terminal-specific. It should not be generalized to
  Linux, Windows Terminal, iTerm2, or other curses stacks without calibration.
- Cursor movement success does not prove replacement safety. Replacement has a
  separate strategy field for that reason.
- `substitute` is a physical display choice for any class/display. The file
  bytes and logical grapheme cluster remain unchanged.
