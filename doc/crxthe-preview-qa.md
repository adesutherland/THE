# CRXTHE 0.9.0 local preview — 26 October 2026

Subsequent scope correction: all Shmuel-related prefix implementations and
ports have moved to the private kit too. The bundled-port and public 121-test
statements below describe this earlier preview, not the current package.
See [private compatibility](private-compatibility.md).

Adrian selected **CRXTHE 0.9.0**, credited as based on Mark Hessling's THE 4.0.
This is the independent fork's version. The command banner, VERSION query,
console identity, browser title and package metadata now distinguish it from
upstream THE. CRXTHE defects are directed to this fork's issue tracker.
Packaged `crxthe` / `crxthe.cmd` launchers accompany the compatible `the` names.
Existing `THE_*` configuration names, runtime cache and installation IDs remain.

Shmuel's unchanged originals, compatibility patches and preparation script are
held outside the repository in a private review ZIP. New package staging
rejects private patch material. The independent Regina adaptation and full
cREXX Level G ports remain bundled. Historical releases and Git history are
unchanged. See [private compatibility](private-compatibility.md).

The [first-launch guide](first-launch.md) documents the common cREXX/Regina
layout, colors, top ruler, numbered prefixes, insert mode, CUA editing and
save/find/open/line/focus shortcuts. Optional parsers are registered only when
available. A shared JSON definition generates the profiles and prompt helpers.
The browser keeps its own safe command/workspace policy and toolbar shortcuts;
its visual theme is supplied by the frontend CSS rather than Rexx colors.

## Repairs discovered in first-launch QA

- CMake now enables the raw/cbreak functions provided by supported curses
  libraries. The old compatibility fallbacks left Ctrl-S/Q subject to terminal
  flow control. A real PTY regression verifies insert, Ctrl-S, End/Enter,
  command focus and save/close under both engines.
- Native Unix Shift-Up/Down definitions and semantic Ctrl-letter/Shift-key
  events now reach the configured editor bindings.
- New empty buffers avoid an invalid startup cursor target.
- The semantic/browser ruler uses the console's shared formatter, including
  horizontal column offsets and tabs.
- A browser client's first snapshot prepares presentation state so numbered
  prefixes are visible before the first edit.

## Local validation

macOS ARM64 source regression: **87/87 passed**. After the final first-snapshot
repair, **10/10 focused profile, ruler, web runtime/policy/highlighting and
rendered Chrome checks passed**. Matched profile generation and TypeScript
checking passed. Windows/macOS signing-tool unit checks passed.

An unsigned CRXTHE ZIP and macOS PKG were built. The relocated ZIP's manifest
hashes, private-kit exclusion, compatible launchers and isolated-PATH runtime
checks passed with CREXX, NONE and separately installed Regina. The public
package runs all **121 independent cREXX prefix workflows** without downloading
the private kit. Stock installed cREXX and Regina profiles are also checked.
The build/package uses the configured sibling Debug cREXX SDK; this is local
candidate evidence, not a signed release qualification.

The private ZIP's originals were hash-verified, patch context/EOF tests passed,
and ZIP CRCs were checked. Original-Regina and independent-port source suites
passed with exact parity in the 87-test run.

## Remaining release checks

Nothing from this preview has been pushed, signed, published or sent to
Shmuel. The published panels.2 release and the normal local installation remain
unchanged. Local preview shortcuts use the development editor with the
published runtime cohort to make testing convenient.

The renamed Windows installer needs a runner build and install/reinstall/
uninstall checks. Both platforms need new signed-candidate qualification,
including upgrade from the earlier 4.0-based installer version to 0.9.0 and
removal of any historical patch-kit leftovers in an existing installation.
Actual Terminal visual inspection and Windows visual cursor inspection remain
human checks. The optional F8/F9 development helpers retain the separately
recorded failures; they are not enabled by the normal profile.

The no-Shmuel-handoff condition remains until Adrian finishes the agreed
Windows testing. Upstream THE and live XEDIT compatibility remain untested.
