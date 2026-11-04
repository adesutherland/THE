# Shared footer core verification

The D04 shared footer implementation passed the focused native checks recorded
in [receipt.json](receipt.json). This evidence applies to shared source/profile
hash `79dfd8a41b2441d29fe2984bf4a10dcdb6626d68f5331be96a1817371ed4a051`,
driver module ABI **4**, and the Debug runtime configured in
`build/gtk-focus-review` with cREXX cohort
`43807b4368cb3dcda49d67c0ac15599cf1e22594`. The receipt lists all 21 source
inputs, their hash algorithm and the configured SDK paths.

| Focused check | Result | Seconds |
| --- | --- | ---: |
| `test_default_profile_CREXX` | PASS | 20.62 |
| `test_default_profile_REGINA` | PASS | 1.01 |
| `test_headlessdriver` | PASS | 0.26 |
| `test_named_footer` | PASS | 0.12 |

[ctest-last-test.log](ctest-last-test.log) is the original CTest log copied
before subsequent platform checks could replace it. The four tests passed in
22.02 seconds. Their exact build/CTest commands are in the receipt.

The bounded code audit identified two defects, both fixed before these checks:

- UTF-8 footer text used a byte count as a glyph count. The dedicated band now
  decodes within the explicit byte length, clips whole clusters by physical
  cells, reads colours by byte position, pads safely and scrolls by logical
  VERIFY columns. The production painter tests use exact-length Ω/界 buffers,
  narrow widths, guard cells and VERIFY at and inside a wide glyph.
- Page stepping subtracted docked rows after geometry had already removed
  them from FILEAREA. It now subtracts inline reserved rows only. A disposable
  300-line file proves forward/backward distances and round trips with two
  docked and two inline rows, clean dirty state and byte-identical saved bytes.
  Other reserved-list loops were inspected for analogous accounting.

The same headless target passed AddressSanitizer and UBSan. Its previously
executed command is preserved in
[asan-ubsan-command.txt](asan-ubsan-command.txt); stdout is preserved in
[asan-ubsan-result.log](asan-ubsan-result.log). The command used the matching
CMake inputs copied as [headless-flags.make.txt](headless-flags.make.txt) and
[headless-link.txt](headless-link.txt). The sanitizer binary was disposable.
These files preserve the existing run; writing this receipt reran no checks.

This is focused local core/native evidence. The separate browser, GTK and
console visual gates retain their own inputs and results. It does not qualify
hosted CI, installation, publication or a release. Original test binaries were
not retained or hashed as immutable candidates.
