# Workbench acceptance and design gaps

**Conclusion: the current UI is not yet aligned with the agreed design.**
The execution foundation works in the sampled journeys, but navigation,
information hierarchy and visual balance still differ. Correct the browser
reference first, then adapt that accepted experience to GTK and the console.
This is a consolidated gap review, not an acceptance sign-off or a request to
approve individual pixel changes. Application UI code was not changed.

The reviewed browser artifact is development source
`822d13fbcc00b7bbf064e38a3ce9d17411044429`. Native specimens use the supplied
matching cREXX/driver cohort; their exact source manifest was not recorded.
Native inputs did not change between the initial implementation commit
`5432cd8` and the later browser-only repairs through `822d13f`.

## The acceptance pack

The [catalogue](catalogue.md) defines **18 requirements and 29 scenarios**, with
goal-first prompts, numbered replay steps, fixtures, expected state transitions,
capture checkpoints, platform variations and existing regression leads.
[Machine-readable scenarios](../../tests/ui-acceptance/catalogue.json) allow
new cases to be added without replacing the review method. Four verdicts stay
separate: functionality, design fidelity, aesthetics and discoverability.

The [frozen reference](reference/README.md) preserves the original guided
workbench and all three graphical variants. Later user clarifications make
pane toggling and an explicit GTK aesthetic assessment mandatory. A persistent
GTK Files view is recorded for scope disposition; a native Open chooser does
not demonstrate that view. Eight interpretation/design records comprise one
resolved annotation and seven open items; only three concern scope sequencing.

## Priorities across the three platforms

| Priority | Consolidated finding | Evidence and closure |
| --- | --- | --- |
| 1 | **One dependable pane-navigation contract.** Browser Tools & Profiles does not toggle off. GTK lacks the persistent pane controls depicted in the design. The console launcher refocuses rather than toggles. | G04/G08/G14; S18. Repeated activation hides/restores the target and preserves source, draft, output, job and reader state. Hiding Console already works through its separate Hide control. |
| 2 | **Shared document and action hierarchy.** GTK retains a core file-ring text strip instead of the depicted named active document tabs. Console actions remain generic and lack discoverable Build/profile routes. | G09/G13/G15/G16; S01/S02/S05/S07/S12. Expose the same document identities and purpose-specific actions through appropriate controls. |
| 3 | **Simpler Profiles and tool associations.** Browser setup emphasizes technical forms and paths before the common information. GTK puts associations first but lacks the direct profile-edit link. | G02/G03/G11; S08/S12/S15. Friendly active-profile/startup summary first; associations visible on entry; advanced execution, import and diagnostic details secondary. |
| 4 | **A coherent aesthetic system.** Repeated title/navigation bands and uneven control density weaken both graphical layouts; GTK has especially little breathing room. | G06/G10/G12; S29. Assess insets, grouping, typography and control scale together, preserving native theme and profile-owned editor colours. |
| 5 | **Clear state and recovery language.** Ordinary Text is presented with irrelevant runner information; a file switch can read as an edited run source; the console compiled-source refusal is generic. | G05/G07/G12/G15/G16; S01/S07/S10/S19. Describe the selected role, actual run source and actionable prerequisite in ordinary language. |
| Design task | **One default footer expression, with profile authority.** Browser and native layouts currently arrange the same components differently. | G01/D04; S01/S15. Resolve the shared mapping against the agreed hierarchy; retain intentional custom profile text, colours, count, positions and OFF states. |

Pane state needs precise terminology. In the browser, clicking Console a second
time hides Console and reveals previously open Tools. That is not evidence that
Console failed to hide. D08 concerns which sibling appears afterward. The direct
browser toggle failure is Tools & Profiles. Standard GTK notebook tabs are
selection controls; the gap is the missing shared pane-show/toggle controls,
not an assertion that all native tabs should behave like toggle buttons.

## Browser first

Menu groups, source-specific Build/Run/Apply, separate Program and Build results,
Settings scope and a separate About destination are present. A fresh goal-only
agent, given no implementation or design instructions, completed three goals:
run ClassicDemo, open an editable profile, and hide/restore retained output.
The [retained route log](../../tests/ui-acceptance/evidence/goal-first-browser.txt)
is the reviewer’s narrative; its raw CUA trace/screenshots are not retained in
this pack. This is a small LLM discoverability probe, not a human user study or
proof of every catalogue scenario.

The main design departures are visible without changing files:

- **G02, Profiles:** Editable/Build/Active/Startup internals, long paths and an
  expanded compiled-profile workflow displace the short active-profile summary.
  [Actual](../../tests/ui-acceptance/evidence/browser-B02-source-profiles.jpg) ·
  [Reference](../../tests/ui-acceptance/evidence/reference-B02-browser-profiles.jpg).
- **G03, File types & tools:** the explicit-run/import form comes before the
  association list. At the inspected viewport, common associations begin near
  the bottom of the pane.
  [Actual](../../tests/ui-acceptance/evidence/browser-B03-tools.jpg) ·
  [Reference](../../tests/ui-acceptance/evidence/reference-B03-tools.jpg).
- **G04/G06:** Tools does not toggle off; duplicated pane headings and a mix of
  small navigation controls and larger actions consume space and dilute the
  hierarchy. Files and Console's own target toggling work in the sampled state.
- **G05/G07:** Welcome Text reports “No runner is associated with this file”.
  After opening a different file, retained output says “Buffer changed since
  run” even though this review did not edit the run source.
  [Retained result](../../tests/ui-acceptance/evidence/browser-B08c-other-file-console.jpg).

The current browser footer is **Status → key guide → response → THE command**.
The reference depicts **response → THE command → key guide → Status**. This
is a departure from the agreed default hierarchy. D04 tracks the implementation
design for a profile-compatible mapping; it does not reopen the reference
choice or require a hard-coded guide or permanent `Ready` message.

## GTK second, including aesthetics

Native Open, cREXX Build/Run and Edit profile defaults worked in the isolated
sample. Settings distinguishes driver Appearance from current-file Editor
settings; native application About/Settings routes are present. GTK's
association-first table and collapsed advanced forms are closer to the agreed
information hierarchy than the browser's current section.

However, the shared shell is incomplete:

- **G08/G09:** no persistent Files/Program console/Tools & Profiles row; pane
  destinations become menu-dependent when hidden. With multiple files open,
  the core text strip is not the depicted set of named active document tabs.
  [Actual shell](../../tests/ui-acceptance/evidence/gtk-K01c-buffer-strip.jpg) ·
  [Reference](../../tests/ui-acceptance/evidence/reference-K01-gtk-mac-shell-profiles.jpg).
- **G11/G12:** associations lack the immediate Edit associations in profile
  route, and ready contextual actions display an internal `crexx-source` label.
  [Associations](../../tests/ui-acceptance/evidence/gtk-K03-filetypes.jpg) ·
  [Built source](../../tests/ui-acceptance/evidence/gtk-K04-build-results.jpg).
- **D06:** Open uses a native chooser. A persistent file list was not observed.
  The reference depicts a Files affordance; the user's proposed common Files
  feature should receive an explicit next-batch scope disposition.

**G10 confirms the aesthetic concern, with an important distinction:** source
and screenshots show roughly **6 logical-unit insets around 34-unit controls**,
with related gaps of 4–6 units. The prototype's Build buttons are also about
34 units high. The evidence therefore supports rebalancing margins, repeated
bands and visual mass before assuming every button needs shrinking.

The native screenshot is at 2× backing scale; control height is an estimate,
while the six-unit borders are source-confirmed. Native widget rectangles
were not exposed through macOS accessibility, and actual/reference sizes and
themes were not perfectly matched. These values establish context, not an
approved numerical tolerance.

[GTK Profiles](../../tests/ui-acceptance/evidence/gtk-K02-profiles.jpg) and
[Settings](../../tests/ui-acceptance/evidence/gtk-K05-settings-appearance.jpg)
show the surfaces for the S29 spacing/rhythm assessment. Prepare one consistent
spacing and control-scale proposal. Preserve the editor grid, readable native
fonts and profile-owned colours. Settings' native tabs/font chooser can be a
deliberate platform adaptation; shape alone is not a failure.

GTK's default footer is **key guide → response → command → Status**, also
different from the reference. This is part of D04, not a separate platform fix.

## Console third

A fresh real PTY was operated at 140×40 and 80×24 cells. Classic execution,
keyboard console access, an `Ada` input draft, explicit Hide/restore, completion
and retained output worked. The console moved right-to-below at the compact
size; arrows could scroll the F10 menu to its final Help entry. External file
hashes confirmed the fixture files remained unchanged.

The important gaps concern the visible command presentation:

- **G13:** the stock F10 menu has no Build source, profile edit/build/apply or
  Tools & Profiles destination. The persistent row offers generic Run file.
- **G14:** activating the visible Console launcher returns to input instead of
  hiding the pane. A separate Hide control preserves the draft/job correctly.
- **G15/G16:** generic Run remains visible for Text. On a fresh `.crexx` source,
  Run reports “The explicit workbench action returned a nonzero THE result”,
  without a discoverable Build route or useful prerequisite explanation.

[Compiled-source menu](../../tests/ui-acceptance/evidence/console-C05b-crexx-menu.txt) ·
[Refusal](../../tests/ui-acceptance/evidence/console-C05c-crexx-run-menu-result.txt) ·
[Restored draft](../../tests/ui-acceptance/evidence/console-C04e-restored-draft.txt) ·
[80×24 layout](../../tests/ui-acceptance/evidence/console-C06-small-80x24.txt).

These findings do **not** establish an unsupported compiler backend. The
expert `RX BUILD`/`RX RUN` fallback was not attempted in this console pass.
The next terminal design must make the shared operations discoverable through
appropriate menus, controls or explicit command guidance.

There is no approved console pixel mock. Its footer uses the same native order
as GTK. Captured grids support mapping and keyboard observations; native
terminal font, glyph, cursor appearance and physical input remain unqualified.
The [compressed record pack](../../tests/ui-acceptance/evidence/console-records.zip)
contains cell attributes, raw ANSI, cursor metadata, independent file hashes and
a clearly labelled HTML projection. Its browser font and DEC ACS approximations
are not filed as native rendering defects.

## Coverage and interpretation

| Evidence | What it establishes | Boundary |
| --- | --- | --- |
| Existing `822d13f` extracted-ZIP receipt | Passed browser functional/DOM scenarios and a narrow paint-continuity check | Does not establish agreed design, aesthetics or all 29 catalogue scripts; named fields and other separately callable helpers are not automatically covered by that target |
| Fresh browser comparison | Eight principal current/reference states, contextual Build/Run, settings/profile navigation and repeated-button observations | Actual 1424×836, DPR2; no fresh compact/phone design walkthrough or matched-theme pixel comparison |
| Fresh GTK comparison | Six screen groups, two workflows, runtime module/cohort paths and aesthetic observations | macOS only; no exact native source manifest, compact rerun, live input-draft/clipboard test or font-persistence rerun |
| Fresh console PTY | Actual curses grids, menus, keyboard route and Classic input/state at two sizes | Not a terminal-app screenshot or native font/accessibility assessment |
| Goal-first browser probe | Three goals completed through ordinary UI, with observed first choices | LLM-only, small sample; no human study or full script replay |

All **full catalogue executions remain `NotRun`**. Existing regression receipts
and fresh checkpoint subsets are linked evidence, not substituted executions of
every new numbered script. The [structured assessment](assessment.json) maps
16 open findings to canonical requirements/scenarios and retained evidence.
The footer departure is open; D04 tracks how to implement the agreed mapping
while preserving custom profiles. The 29-case coverage records
preserve that boundary explicitly.

The existing [IDE scope baseline](../ide-scope-baseline.md) also records an
unfinished profile-composition contract: stock browser controls remain partly
hard-coded, and general user-macro menu actions are narrower than the intended
capability-limited profile-built IDE. The current menu-builtin/custom-Build
proofs do not close that whole journey. Keep S15/R10 open for explicit replay
and source-contract assessment. Browser source import/export remains a tracked
baseline capability gap under D03. Neither issue should be silently closed by
this visual review or by the successful language examples.

The three scope/sequencing decisions are D02 Redo, D03 Save as/source transfer,
and D06 a shared GTK Files view. Recommended dispositions are recorded in the
catalogue. D04 footer mapping, D05 compact navigation, D07 spacing and D08
post-hide selection belong in one coherent implementation design. They are
not eight separate permission requests. Quiet idle response is resolved as
acceptable; no permanent `Ready` label is required.

## Recommended bounded implementation sequence

1. **Browser reference alignment:** settle the shared shell/footer/pane-state
   specification, implement the pane toggle contract, simplify Profiles and
   associations, and correct state language and density. Recheck S01/S02/S05,
   S12/S15/S17/S18/S19/S29 with reference screenshots and the targeted sample
   matrix. Preserve the existing working execution/data contracts.
2. **GTK alignment:** render that accepted hierarchy with native document and
   pane controls; include a persistent Files view if approved. Apply one spacing
   system across toolbar, panes and dialogs, and recheck the same case IDs.
3. **Console adaptation:** expose the same contextual source/profile actions
   and visibility semantics through compact, keyboard-first presentation.
   Confirm compilation and recovery routes in a real PTY, with native terminal
   appearance assessed separately.

After each batch, an independent reviewer checks the affected cases against
the frozen design. Bring one consolidated delta report. Broader qualification
follows a stable candidate; do not rerun broad suites for each cosmetic repair
or silently rewrite expected behavior to match the implementation. Acceptance
requires observed mandatory checkpoints on each applicable axis/sample and
explicitly recorded exceptions; an earlier bounded “no blockers” statement
does not meet that stopping rule.
