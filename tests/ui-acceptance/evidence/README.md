# Design review evidence

These captures support the [consolidated gap review](../../../doc/ui-acceptance/gap-review.md)
and its [structured assessment](../../../doc/ui-acceptance/assessment.json).
They are observations from bounded checkpoints, not a completed catalogue run.

- `browser-*` and `gtk-*` are current-UI screenshots; `reference-*` captures
  the unchanged prototype. They are not approved pixel goldens. The review
  records viewport, theme and native provenance limitations.
- `console-*.txt` are parsed real-PTY character grids. Trailing spaces retain
  captured columns. They are not native Terminal screenshots; DEC ACS
  approximations do not establish glyph defects.
- `console-records.zip` retains full cell attributes, cursor metadata, raw
  ANSI, independent fixture hashes and a labelled HTML projection. Its SHA-256
  is recorded in the assessment.
- `goal-first-browser.txt` is the independent agent's narrative route log.
  Raw CUA trace/screenshots from that probe are not retained in this pack.

No capture establishes saved-byte mutation correctness, platform accessibility
or aesthetic acceptance beyond the explicitly exercised checkpoints.
