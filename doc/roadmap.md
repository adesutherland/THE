# Roadmap

The source tree is a `0.10.dev` development build towards version 0.10.0.
This page contains outstanding work, not a release promise. Implemented
behavior belongs in the [user and developer guides](README.md); defects and
workarounds belong in [known issues](known-issues.md).

## Editing and runtime integration

### Common RXBIN host implementation stages

The [common architecture](architecture.md#common-desktop-and-browser-application)
is implemented for native desktop (graphical and terminal) and browser. Keep
each stage usable and validate its owning contracts before proceeding. The
source implementation covers the five stages below; platform and release
qualification remains separately listed after the table. See the
[implemented commands and build inputs](runtime-tools.md).

| Stage | Deliverable | Acceptance |
| --- | --- | --- |
| 1. Session contracts | Shared C source identity, tool registration, job/console model and normalized actions. | Pending edits, job identity, bounded streams, EOF, completion and stale-event rejection are independent of UI/host. |
| 2. Native execution | Native RXBIN/source runner adapter with pipes, termination and the graphical/terminal split console. | Actual native bytecode, interactive stdin, errors and Stop leave the editor responsive; terminal keys and resize use a real PTY. |
| 3. Browser execution | Existing Wasm runner routed through the common THE session; renderer becomes a presentation client. | Same run/stop/input/EOF behavior, unsaved source and persistent workspace tests; direct RXBIN selection works. |
| 4. Profiles and tools | Common registration, profile edit/build/validate/activate and language-service selection/restart. | Both hosts execute a newly built addressed profile; failed build retains working selection; a registered highlighter observes revision admission. |
| 5. Integration | Installed assets, dependency provenance, user guides and common host/driver regression checks. | Native graphical/terminal and browser exercise the same workflows; unavailable platforms/capabilities are reported explicitly. |

Desktop-window packaging and broader browser/OS release qualification remain
separate from these source-development stages. The existing native web driver
supplies the graphical desktop presentation without selecting a new shell framework.

- Diagnose and repair the optional F8/F9 helpers. Reproduce each reported
  failure with its matching runtime, then assert compilation artifacts,
  runtime output and failure status through the hosted macro path.
- Reconcile normalized Delete/Backspace names with profile key identities
  across Unix and Windows. Cover both deletion within a record and CUA
  joining at record boundaries without changing unrelated key definitions.

## Installation and release qualification

- Qualify each intended candidate on native Windows, macOS and Linux with
  its matching dependencies. Keep executed passes, skips, optional parser
  availability and installed-copy checks distinct.
- Exercise legacy in-place upgrade, including obsolete private macro-file
  cleanup, effective command resolution, user-file retention and uninstall.
- Improve setup diagnostics for conflicting commands, stale development
  overrides, discovered installed cREXX compatibility/fallback, and absent
  optional Regina. Preserve intentional customizations.
- Complete physical terminal checks for cursor visibility, Unicode display
  and host shortcuts. Automated ConPTY/PTY input and saved bytes supplement
  these checks; they do not prove visible pixels or fonts.
- Build, sign and qualify a matching installer before publishing a candidate.
  Signing and publication require their own authorization; a source change
  or green source test run does not update an existing installer.

## Unicode and driver maintenance

- Consider exact-sequence or structured UTF selectors when measured cases
  require more precision than the implemented class-and-mode table.
- Extend canonical UTF profile extraction/replay coverage when a new rule is
  introduced, using fresh sessions and resolved LLM metadata.
- Measure terminal-specific rendering before relaxing conservative Linux or
  Apple rules. Keep physical corrections in profiles and drivers.
- Decide whether to retain the incomplete non-utf8proc fallback as an
  explicitly limited build option or remove it.
- Review legacy column arithmetic when changing an affected command. Add a
  shared per-line layout cache only if measured repeated scans justify it.
- Contract remaining opaque physical compatibility operations in cohesive
  slices with behavior coverage. Keep one public driver vtable; consider a
  separate Windows driver only if demonstrated divergence requires it.

## Browser and document tooling

- Extend the [WebAssembly browser preview](browser-wasm.md) beyond its implemented
  editor/storage, precompiled cREXX profiles/READ and optional Level L Classic
  highlighter and Classic interpreter/split console. [Remaining acceptance](wasm-browser-plan.md) covers incremental
  parser transport, import/export, interruption/recovery, publication of the
  local highlighter cohort and broader browser qualification.
- Broaden browser acceptance across browsers, viewport sizes and intended
  deployment environments. Authentication, TLS and routing remain host/proxy
  responsibilities.
- Isolate the batch-tool concurrency failure before adding parallel service
  workers. Assert stable output/status with private tool/runtime state.
- Add further parser/language fixtures where the optional executables are
  available. Report missing optional tools as skips, not passes.
- Integrate source-highlighting fragments into an external manual pipeline
  only in that pipeline's owning repository, preserving its fallback when
  fragments or dependencies are absent. The THE fragment generator already
  exists; an external integration is separate work.
