# UTF-8 Terminal Profiles

This directory stores terminal-specific UTF-8 physical display baselines
captured by `tools/utf8_terminal_probe.c`.

These files are REXX/THE profile fragments. Comments use `/* ... */`, and
the actual settings are quoted THE instructions so the files can be run as
profile scripts or copied into an existing THE profile. Substitute output
settings should include the per-class codepoint, for example
`OUTPUT substitute U+0040`.

- `defaults-poc34.the`: complete default table for
  `utf8_terminal_probe 2026-05-13-poc34`, mirrored from
  `calibration_defaults[]`.
- `macos-apple-terminal-poc34-overrides.the`: macOS Apple Terminal baseline
  captured from visual calibration on 2026-05-14. It is an override fragment
  to apply on top of `defaults-poc34.the`.

When adding another platform or terminal, keep the file name specific to the
terminal family and probe version, and record `TERM`, terminal program, probe
version, and whether the file is a complete table or an override fragment.
