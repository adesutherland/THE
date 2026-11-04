# Console candidate01 focused regressions

These are exact copies of the implementer's frozen-candidate receipt and raw
CTest log. They record **three passes and two failed fixture assertions**;
this directory makes no Console UI-gate or full-suite acceptance claim.

| Check | Recorded outcome |
| --- | --- |
| `test_workbench_curses` | Pass: both engines, custom menus/F10, chooser, pending bytes/save, carets and compact resize |
| `test_curses_alignment` | Pass: 140×40 and 80×24, contextual Build/Run and profile Build/Apply/Edit, separate lanes, target toggles, Unicode PULL draft/caret, small-window recovery and unchanged fixture bytes |
| `test_workbench_native_tools_curses` | Pass: explicit native TOOL action, source/argv/output, unchanged bytes and profile quit after leaving Program input |
| `test_tool_console_curses` | Failed at the final 15-row caret assertion: the old fixture expected y=13; accepted AFTERCMDLINE geometry uses y=12. The assertion was corrected; its rerun awaited the coordinator's full 121-test CTest. |
| `test_curses_workbench_sizes` | Failed at the F6-origin assertion after the LOG/reader/draft checks: F6 correctly toggled the saved command origin to the file; the old fixture expected command. The fixture was corrected to assert file, then use Esc for subsequent command-draft checks; the corrected remainder and 80×24 case awaited full CTest. |

The recorded focused CTest exit is **8**. The fixture corrections are preserved
as classifications, not converted into passing test results. The receipt's
five executed inputs are retained in `executed-fixtures/` and hash-verified.
As the receipt explains, the two failed inputs were reconstructed from their
single-block assertion corrections; the other three match the final tests.

The reviewed native build used the configured matching cREXX `43807b` cohort.
The receipt records exact compiler, assembler, library and runtime paths/hashes.
Frozen reviewed SHA-256 values:

- Core executable: `b2e9ce309428dc2b5666f40ea298bbd49e9c2dce7acba3cd2f70917c75fdf76d`
- Curses module: `9bd5c38afcee64456f9b7ef0cbdf58114ebc3cc186892c32e8d292055ff927d1`
- Curses source: `9dcfda006679d57d27219a69582d3fcfe642f0107bdeff68dcdb5ffcfdec4350`
- Production manifest: `d46d4dbe0e258afa64ba9cb0644ecef9c22c261d6155507886791198609ad203`

The snapshots in the failure log are actual PTY cell projections. They do not
qualify native terminal pixels, fonts, glyph appearance or physical-device
input. Independent UI acceptance is recorded separately by the UI reviewer.

Copied evidence SHA-256:

- [receipt.json](receipt.json): `549644a15b14192bcc8e4ecd819803d07ae6bd376a901b313e91e27d10b534b2`
- [focused-ctest.log](focused-ctest.log): `58421e469089983eda42f8602fa325f6149b4a42d5ea4a54d9f2009c9f47cfa5`
