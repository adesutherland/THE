# Macro and panel delivery stages

Authorized 25 October 2026. Work stays on `temp/shmuel-macros`.
After each stage: local QA, commit/push, Windows Release and installed-package
QA, then publish a tagged GitHub pre-release with checksums and qualification
notes. Do not send anything to Shmuel. His handoff and interactive Windows
qualification wait for Adrian's manual Windows test.

1. Regina: repair dependencies and provide reproducible patches for the
   downloaded PFXREXX/PFXHLASM files, with behavioral coverage of their aliases,
   operands, continuations, errors and cursor state. Preserve original hashes.
2. Level G: independent ports of both workflows, comparing text, pending work,
   errors and cursor results with the corrected Regina baseline. Keep Regina.
3. Panels: protected/editable fields, navigation, submission keys and tagged
   READ ALL / READ NOCHANGE results in shared state, with curses, browser and
   LLM presentation and both macro engines. Exercise normal editing regressions.

Only a successful exact-commit CI candidate can become a pre-release.
Production release, upstream THE equivalence and live XEDIT equivalence are
separate from this staged qualification.

Adrian selected a minimal compatibility patch set against unchanged originals.
Keep downloads outside the repository and verify original hashes before patching.

Stage 1 published as [v4.0.0-regina.1](https://github.com/adesutherland/THE/releases/tag/v4.0.0-regina.1),
commit `99e43c2`, after 67/67 macOS tests and Windows run `40177173979`
(54 passed, 11 unavailable-tool skips). Both installed payloads passed the
86-workflow patched-original suite; original source remains unchanged.

Stage 2 published as [v4.0.0-levelg.1](https://github.com/adesutherland/THE/releases/tag/v4.0.0-levelg.1),
commit `c8eae43`, after 70/70 macOS tests and Windows run `40180543859`
(56 passed, 11 unavailable-tool skips). Both installed payloads passed 121
patched-original Regina workflows and 121 Level G workflows, with exact text,
cursor, pending-command and error-marker parity.
