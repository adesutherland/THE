# UTF-8 Enablement Handover

Last updated: 2026-05-14.

## Current State

THE's UTF-8 work is split into two models:

- Logical editor model: grapheme-cluster based, shared across platforms, using
  `TextPos` and utf8proc.
- Physical terminal model: terminal-specific cell widths, cursor widths, output
  methods, cursor repaint strategies, and replacement repaint strategies.

Do not fix terminal paint problems by changing logical cluster segmentation.
The logical model stays stable; terminal quirks belong in terminal profiles.

## Important Artifacts

- `doc/utf8-design.md`: detailed design, findings, and historical log.
- `tools/utf8_terminal_probe.c`: interactive terminal calibration/probe tool.
- `tools/utf8_terminal_profiles/defaults-poc34.the`: definitive default
  physical terminal table for probe version `2026-05-13-poc34`.
- `tools/utf8_terminal_profiles/macos-apple-terminal-poc34-overrides.the`:
  captured macOS Apple Terminal override baseline.
- `tests/fixtures/utf8-render.txt`: manual editor fixture for UTF-8 rendering.
- `tests/test_utf8_fixture.c` and `tests/test_textpos.c`: UTF-8 fixture and
  text-position regression coverage.

## macOS Apple Terminal Baseline

The current macOS baseline was captured visually with
`utf8_terminal_probe 2026-05-13-poc34` on Apple Terminal
(`TERM=xterm-256color`, `TERM_PROGRAM=Apple_Terminal`).

The saved baseline is an override fragment. It should be applied on top of
`tools/utf8_terminal_profiles/defaults-poc34.the`. Important observed choices
include:

- `regional-flag`: default `L3 C3`, cursor `changed_cells`, replacement
  `clear_changed_suffix_fast`.
- `keycap`: `L2 C2`, cursor `clear_from_first_cluster_fast`, replacement
  `clear_from_first_cluster_fast`.
- `modifier`: `L4 C4`.
- ZWJ grouped intent: use `substitute` for `short-zwj`, `heart-zwj`, and
  `family-zwj`.
- ZWJ component intent: `short-zwj` uses `native L4 C4`; `heart-zwj` uses
  `expanded L6 C6`; `family-zwj` uses `expanded L8 C8`.

## Probe Usage

Build the probe:

```sh
cmake --build cmake-build-debug --target utf8_terminal_probe -j2
```

Open interactive calibration:

```sh
./cmake-build-debug/utf8_terminal_probe calibrate all \
  --profile /tmp/the-utf8-terminal-profile.the
```

Validate the saved macOS profile non-visually:

```sh
./cmake-build-debug/utf8_terminal_probe calibrate all --no-visual \
  --profile tools/utf8_terminal_profiles/macos-apple-terminal-poc34-overrides.the \
  --report /tmp/the-utf8-macos-profile-check.txt
```

Validate the definitive coded-default profile non-visually:

```sh
./cmake-build-debug/utf8_terminal_probe calibrate all --no-visual \
  --profile tools/utf8_terminal_profiles/defaults-poc34.the \
  --report /tmp/the-utf8-default-profile-check.txt
```

## Next Work

1. Implement THE-side loading/defaulting for the proposed
   `SET UTF8 TERMINAL CLASS ...` instruction shape.
2. Wire the macOS Apple Terminal baseline into the physical renderer without
   changing logical grapheme behavior.
3. Validate view, cursor movement, and replacement independently against
   `tests/fixtures/utf8-render.txt`.
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
- `substitute` is a physical display choice for grouped ZWJ intent. The file
  bytes and logical grapheme cluster remain unchanged.
