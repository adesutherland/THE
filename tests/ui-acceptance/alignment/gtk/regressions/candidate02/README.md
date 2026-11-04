# GTK candidate02 focused regression evidence

This directory preserves the existing final focused PASS from 4 November 2026,
12:27 GMT. It was copied after the run; no build, test or application was rerun
for this evidence preservation. The [PASS log](focused-gtk-runtime.log) reports
one test passed in 8.14 seconds (CTest total: 8.15 seconds), with zero failures
and zero skips. The `44/120` label is the test's index in the configured suite,
not evidence that all 120 tests ran.

[manifest.json](manifest.json) records the exact commands, source/base commit,
source and binary SHA-256 values, configured Debug SDK paths, matching cREXX
`43807b4368cb3dcda49d67c0ac15599cf1e22594` inputs, native tool bundle hashes,
and all 33 copied capture hashes. The native tool bundle's declared cohort
is also retained in [native-tool-cohort.json](native-tool-cohort.json).

## What the focused probe establishes

The test-only module compiles the production GTK driver into a real editor
process and calls its real GTK callbacks against temporary files/configuration.
The final log includes the existing source/dirty/save/reopen, native menu,
clipboard, font/persistence, Files and named-tab navigation, no-sibling toggle,
Program input/draft, reader/history/selection, profile-colour, contextual
Build/Run/Apply, forms/file fields and footer projection checks. The deliberately
invalid compiled-profile fixture emits truncated-header messages while its
refusal/preservation assertions pass.

The candidate02 repair adds two specific checks:

- `GTK_DECLARED_ASSOCIATION_GROUPS_ROLES_SECONDARY_DETAILS_PASS`: primary declared
  associations have explicit Build, Run and Syntax/highlighting roles, the raw
  ownership table is under collapsed Installed tool details, and Text has no
  invented action chain. The compiled-source journey separately checks the
  current-file `Build source → Run` chain from the real core action snapshot.
- `GTK_SETUP_FONT18_820X500_SCROLL_DISCLOSURES_REGISTER_FIXED_CONTROLS_PASS`:
  with an 18-point editor font and an actual 820×500 expanded native window,
  the vertical body adjustment scrolls to both runner/register disclosures and
  the Register button; fields fit the scroll body and heading/section controls
  remain available. Restoring the editor retains source bytes, dirty state,
  source caret and the Program draft. Core editor minimums were not changed.

The most direct repair captures are
[wide associations](captures/gtk-runtime-associations-summary.png),
[short-window disclosures](captures/gtk-runtime-associations-font18-short-disclosures.png)
and [short-window registration](captures/gtk-runtime-associations-font18-short-register.png).

## Capture and review boundary

Every PNG here is an **in-process GtkWidget capture** made with
`gtk_widget_draw(ui.window, cairo_context)` by the regression probe. These are
real GTK widgets, but they are not operating-system screenshots or a manual
production-application walkthrough. Independent UIlead production-UI evidence
lives outside this `regressions` directory. Functional probe results do not
establish aesthetics, discoverability or acceptance against every mandatory
catalogue checkpoint.

The font probe exercises the real `apply_font`, `save_font_preferences` and
`restore_font_preferences` functions, including metrics, persistence, session-only
failure handling and field/caret preservation. It opens Settings through the
native application action and closes the modal programmatically. It does **not**
claim a manual Settings Apply-button journey. The UIlead's CUA limitation in
routing physical input to that modal is a separate observation, not a product
failure or a reason to discard the functional font evidence.

This is local macOS focused regression evidence only. It does not qualify other
GTK desktops, the full candidate suite/catalogue, hosted CI, installation,
signing, publication or release readiness.
