# Console candidate02 focused regressions

`test_curses_reader_colors` **passes**, exit **0**, in **86.12 seconds** on the
frozen native candidate. The exact [receipt](receipt.json) and [CTest log](focused-ctest.log)
record this single targeted check; the older cohort was not rerun.

- **AC01:** 140×40 and 80×24 live PULL retains its viewport through completion
  and preserves the Unicode draft/caret across hide/reopen. It announces unseen
  output and offers End: Latest. End,
  new jobs and Clear reset the cue. Program and Build readers retain independent
  positions. The 32×20 six-row checkpoint keeps status0 and input visible beside
  the compact header cue.
- **AC02:** CREXX and Regina profiles exercise all eight named foreground and
  background colours, the two custom Unicode guide bands and named Console
  roles. ANSI output and PTY cells agree. A logical blue palette write targets
  native slot4; allocated RGB body colours remain unchanged.

The configured matching native cREXX cohort is `43807b`; its exact runtime,
compiler, assembler and library hashes are in the receipt. Reviewed SHA-256:

- Core: `b2e9ce309428dc2b5666f40ea298bbd49e9c2dce7acba3cd2f70917c75fdf76d`
- Curses module: `fa2adfb71b724e1b93b23d0824f66797a7b8213a7c45f3d14d2838a86dfe1a58`
- Curses source: `4a04d100ed1699b0142eefcb7a612a4ca936099f69119e20175512678ed86e9c`
- Focused test: `803b65a0bdaaed6a39cf33971ddf266591eba0a243d68073de26f03fd3b5a4ef`
- Log: `fc1b535b6629f1724813fd8b6438f45d82746ca9d662cb31bb92d38d0e0b0a4e`

This is real PTY/ANSI evidence, not native terminal pixels, font/glyph appearance
or physical-device input qualification. Independent UI review and the final
122-test source gate remain separately owned by the coordinator and UI reviewer.
No Console UI-gate, hosted CI, publication or release verdict is asserted here.
