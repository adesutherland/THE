# THE UI acceptance catalogue

The [versioned scenario data](../../tests/ui-acceptance/catalogue.json) specifies
acceptance against the agreed guided workbench. It is a **scenario
specification, not an executable runner**. Version 1.0.0 contains 18
requirements, 29 stable scenarios and eight decision records: one resolved
reference annotation and seven open dispositions/design tasks. Every initial
execution and verdict remains `NotRun`; creating this catalogue qualifies no
implementation.

Review the browser experience first, then record GTK macOS, other GTK desktops
and console adaptations separately. Return one consolidated findings report
before further UI implementation changes. Publication and release qualification
remain separate from this review.

## Authority and the reference

The frozen [approved prototype](reference/workbench-polish.html) has SHA-256
`04fc03feeb50a672d9c0f3800572aa53b2bdf05c71eb80dd2ae15b8380092f2b`.
The [reference viewer](reference/viewer.html) exposes its browser, GTK macOS
and other GTK variants. Catalogue references identify the unchanged source
file, root anchor `the-ui-polish` and numbered code regions. The original
prototype is retained as reference evidence, rather than edited to match the
current product.

Use explicit newer user decisions first, followed by agreed design and scope
and subsequently resolved decisions. Current implementation, regression tests,
screenshots and earlier `PASS` reports do not approve a design. The prototype
shows intended hierarchy and interactions; its illustrative handlers do not
prove that Save as, Redo, saving, input or execution were implemented.

The accepted decisions include:

- Browser/WebAssembly is the first reference experience. GTK and console adapt
  the shared C-owned operations, results and refusals to their native controls.
- Profiles own status/key-guide text, colours and row count. Frontends render
  the published information; they must not impose a fixed stock guide on a
  customised profile.
- The prototype's literal `Ready` is sample content. D01 is resolved: quiet idle
  is acceptable, while meaningful responses remain visible and profile-owned.
  For the stock/reference profile, default geometry follows the agreed
  prototype: response above THE command, followed by key guide then status.
  Current stock documentation does not approve a geometric departure.
- Browser and GTK pane buttons toggle their target: show, hide, show again.
  Switching destinations retains job, draft, output, reader and editor state.
  This newer user decision overrides the prototype's selection-only handlers.
- Native executable programs belong to desktop adapters. Browser execution
  uses its admitted Wasm/RXBIN adapters and never pretends to run a native tool.
- `.profile.the` means Build profile/Apply profile; ordinary `.the` means Build
  macro/Run macro; compiled profile artifacts end in `.profile.rxbin`.
- Aesthetic quality requires an explicit judgement, including the user's
  concerns about small GTK margins and large buttons.

The [fixed scope baseline](../ide-scope-baseline.md) supplies required journeys
and host boundaries. [First launch](../first-launch.md), the
[GTK guide](../gtk-driver.md), [runtime tools](../runtime-tools.md) and
[testing guide](../testing.md) supply implemented routes and coverage leads.
Their statements about *current implementation* may age. Check source and the
actual candidate before relying on them. Where scope, prototype and current
guides conflict, retain the conflict and its named disposition. Separate scope
sequencing from implementation design; these records are not individual user
permission requests.

## Requirements

Each `R` record contains its basis (`user_decision`, `prototype`, `derived`),
mandatory or decision-needed obligation, canonical reference regions, principle
and observable checkpoints. An open detail does not suspend unrelated mandatory
checkpoints.

| ID | Acceptance principle |
| --- | --- |
| R01 | Recognisable editor; ordinary Text needs no tool setup |
| R02 | File lifecycle, cancellation and durable byte preservation |
| R03 | Pane controls toggle their target and retain state |
| R04 | THE editing, Unicode, prefix and undo invariants |
| R05 | Command response history, grouped menus and keyboard authority |
| R06 | File-purpose actions and matching build/source identity |
| R07 | Explicit language and RXBIN program execution |
| R08 | Separate Program input, completion, Stop and recovery |
| R09 | Editable/compiled profiles, activation and startup persistence |
| R10 | Profile composition and explicit macros/prefixes/synonyms |
| R11 | Profile-owned status, footer and console colours |
| R12 | Clipboard operations follow the focused target |
| R13 | Responsive layout preserves access and state |
| R14 | Stable readers and named LOG/DOCUMENT/forms/file fields |
| R15 | Driver Appearance versus current-file Editor settings |
| R16 | Native conventions and truthful host boundaries |
| R17 | Scope dispositions and coherent implementation design tasks |
| R18 | Aesthetic comfort, spacing, hierarchy and control scale |

## Goal first, then repeatable replay

Give a fresh or reset independent participant only a scenario's
`goal_first_prompt` and its ordinary task materials. Do not reveal replay steps,
menu labels, selectors or the preferred route beforehand. Record the route,
wrong turns, self-correction, requested/given hints and outcome. A successful
developer replay is not a first-user discoverability result.

Then reset the isolated fixtures and perform the numbered `replay_steps`.
These name actual visible task operations and expected transitions without
depending on DOM selectors, GTK widget IDs or screenshot coordinates. A future
runner may bind those operations to platform controls, but it must retain the
same acceptance assertions. Setup through a profile or test harness is not
evidence that the user can discover the operation.

Fixtures live as recipes and exact data in the catalogue. Use disposable browser
origins/profiles, temporary native workspaces and isolated configuration. Record
the exact fixture source and image hashes before execution. Preserve personal
profiles, unrelated files and private macro kits. Each subvariant starts from a
clean oracle; a complicated sequence of accumulated edits is not a substitute
for an isolated reproducible case.

For mutations, assert text, cursor, selection, pending record and dirty state,
then independently compare saved and reopened bytes. Use the LLM driver for
authoritative semantic state where available, alongside the actual browser,
GTK or real PTY journey. Do not infer semantic state from curses scraping.
Parser observations wait for the current result; use `SDSLHWAIT` where required.
Neither headless semantic success nor a screenshot alone qualifies physical UI.

## Independent verdicts

Every platform/sample/variant has four verdicts:

| Axis | Question |
| --- | --- |
| `functional` | Did the operation produce the correct state, data and refusal/recovery behavior? |
| `design_fidelity` | Does its structure and interaction follow the agreed reference and decisions? |
| `aesthetics` | Is the presentation readable, comfortable and coherent, with proportionate controls and deliberate spacing? |
| `discoverability` | Could an independent first user find and understand the route without replay hints? |

Do not average these into one `PASS`. A working action can fail design or
aesthetics; a visually faithful screen can fail function or discovery.

| Verdict | Strict meaning |
| --- | --- |
| `Pass` | All applicable assertions on this axis were observed with evidence. |
| `Fail` | At least one applicable assertion failed; record expected versus actual. |
| `Partial` | A named subset was exercised or evidence is incomplete; list what remains. This is not a weaker Pass. |
| `NotRun` | The axis was not exercised/observed. Old test success does not fill it. |
| `NotApplicable` | A narrowly allowed platform/optional variant does not apply; cite its reason and authority. |
| `DecisionNeeded` | An unresolved named decision prevents judging particular checkpoints. Assess the independent checkpoints anyway. |

Execution status is separate: `NotRun`, `InProgress`, `Completed` or `Blocked`.
If a display, fixture, tool or participant is unavailable, name that blocker and
leave unobserved axes `NotRun` or `Partial`. A missing mandatory UI/adapter, a
skipped GUI test or unimplemented behavior is not `NotApplicable`. Expected
browser host refusal can pass its refusal assertion; unexpected refusal of a
required operation remains a gap.

An open design record cannot turn an observed mandatory failure into
`DecisionNeeded`. Pane toggling, state preservation, stock reference geometry,
profile authority and aesthetic quality remain assessable while a coherent
implementation design is prepared.

Existing tests are recorded as **candidate partial regression evidence**, with
execution `NotRun` and explicit limitations. Inspect their assertions and exact
cohort before reusing a result. They do not cover aesthetic comfort or independent
discovery merely because they produce screenshots or invoke a control.

## Aesthetic review

S29 makes aesthetic acceptance explicit rather than hiding it inside functional
checks. At actual GTK toolbar, Program/Tools panes, all setup sections, Settings
and About, record:

- Container insets and gaps, including text/controls crowded against boundaries.
- Spacing rhythm within related groups and separation between unrelated groups.
- Button/field bounds relative to label and font size, and usable editor/reader
  area relative to control density.
- Readability, hierarchy, focus-ring clearance, label clipping and control access
  at stock and targeted larger text sizes.
- Native theme/font context and an observer's comfort judgement, separately from
  objective measurements.

Compare the prototype's hierarchy and balance with the corresponding native
platform at a comparable text scale. Native styling need not copy browser pixels.
The prototype's 44 px coarse-pointer rule applies to the targeted browser touch
sample; it does not require desktop GTK buttons to be touch-sized.

No exact 8 px inset or other numeric golden has been approved. Proposed numeric
tolerances belong to the D07 implementation design task, alongside footer
geometry, compact navigation and pane state. Prepare one coherent proposal;
do not ask the user to approve each pixel or state transition. Concrete findings
about clipped labels, crowded margins, disproportionate buttons or poor hierarchy
remain valid. Record the observation, screenshot and measurement before proposing
a target.

## Targeted sample matrix

Run the canonical wide browser journeys first. Each scenario lists additional
risk-targeted sample IDs; do not expand these into every possible combination
of size, input mode, theme and platform. A listed sample is planned coverage,
not an executed result.

| Sample | Surface and size | Input/purpose |
| --- | --- | --- |
| B-W | Browser 1440 × 900 CSS px | Keyboard/pointer, canonical core journeys |
| B-K | Browser 1280 × 900 | Keyboard only, menus/focus/editing |
| B-N | Browser 820 × 720 | Narrow panes/setup/readers |
| B-S | Browser 820 × 500 | Short-height controls/dialogs |
| B-T | Browser 390 × 844 | Targeted touch journeys |
| G-M | GTK macOS 1440 × 900 logical px | Native keyboard/pointer and application menu |
| G-O | Other GTK 1440 × 900 | Record Linux and Windows as separate cohorts |
| G-N | GTK 820 × 720 | Targeted measured native placement |
| G-S | GTK 820 × 500 | Short window/dialog reachability |
| C-W | Console 140 × 40 cells | Real PTY plus semantic evidence |
| C-C | Console 80 × 24 cells | Compact navigation and pane adaptation |

Record actual size, DPI, font, zoom, theme and input device; touch emulation has
an explicit evidence limit. GTK's measured breakpoint need not equal a browser
CSS breakpoint. Console adapts operation, hierarchy and readability in cells,
with emulator-controlled font/clipboard, rather than reproducing GTK pixels.

## Scenario index

Detailed prompts, fixtures, numbered steps, four assertion groups, transitions,
captures, platform exceptions and regression leads are in the JSON.

| ID | Journey |
| --- | --- |
| S01 | First launch and ordinary Text role |
| S02 | Files/Open/New/Close/Save and cancellation |
| S03 | Unicode/prefix/selection/Undo and byte round trip |
| S04 | Current-file error/response history, clear and focus |
| S05 | Menu structure and keyboard-only route |
| S06 | Classic Rexx unsaved run and input |
| S07 | cREXX Level B/G Build then Run |
| S08 | MiniBASIC usable immediately |
| S09 | Explicit RXBIN program and invalid-image recovery |
| S10 | Stale/failed builds and repair |
| S11 | Runtime error, Stop and recovery |
| S12 | Edit/Build/Apply profile and restart |
| S13 | Compiled-only profile import and immutability |
| S14 | Explicit macro, prefix and synonym routes |
| S15 | Profile composition, footer and colour authority |
| S16 | Clipboard target matrix and denial |
| S17 | Wide/narrow/short/touch transitions |
| S18 | Repeated pane toggles and cross-pane restoration |
| S19 | Reader scroll/selection/unread/Latest |
| S20 | LOG/DOCUMENT/forms and optional file fields |
| S21 | Appearance/font/zoom/emulator ownership |
| S22 | Editor Settings current-file scope |
| S23 | About and native application placement |
| S24 | Failed profile, host refusal and recovery |
| S25 | Workspace persistence/save failure/origin boundary |
| S26 | Syntax/diagnostics/unavailable-service recovery |
| S27 | Save as and source import/export decision |
| S28 | Desktop native source adapter and optional real ooRexx |
| S29 | Aesthetics: margins/gaps/hierarchy/control scale |

For pane testing, record **logically open**, **selected** and **actually
rendered** states separately. Repeated activation must hide the clicked visible
target. Another already-open pane appearing afterward does not itself fail
that hide and does not mean the entire auxiliary area must disappear. D08 owns
the unresolved sibling-selection policy. Program and Tools retain job, input
draft, reader and source state across hiding and cross-switching. A native GTK
Open chooser does not establish the proposed persistent GTK Files list.

## Dispositions, design work and changes

The eight records are not eight user-permission questions. Each has a
`decision_authority` and `recommended_resolution`. D01 remains traceable as
`Resolved` and is absent from all `open_decisions` lists. The other seven are
open: three scope/sequencing dispositions and four related design tasks.

| ID | Status / authority | Recommended handling |
| --- | --- | --- |
| D01 | Resolved / reference interpretation | Accept quiet idle; meaningful responses remain profile-owned |
| D02 | Open / scope owner | Include real Redo and target semantics in one scope/sequencing disposition |
| D03 | Open / scope owner | Retain baseline bounded transfer; disposition Save as and transfer sequencing together |
| D04 | Open / implementation design | Follow prototype stock geometry and preserve custom-profile rendering |
| D05 | Open / implementation design | Propose coherent compact navigation and any needed Expand/Return route |
| D06 | Open / scope owner | Recommend sequencing for the proposed persistent GTK Files view |
| D07 | Open / implementation design | Derive native spacing/control proportions from measured aesthetic evidence |
| D08 | Open / implementation design | Specify predictable sibling selection and latent-open pane state |

D04, D05, D07 and D08 belong in one coherent design proposal with screenshots,
state transitions and measurable acceptance. The coordinator should make those
design recommendations, rather than escalating each row position, breakpoint,
gap or sibling transition into a separate user question. D04 does not reopen
the agreed stock geometry: the prototype is its reference, and descriptions of
the current implementation do not approve a departure. Custom profile text,
colours, row count and explicit settings retain their authority.

D03 retains the conflict explicitly: the prototype shows Save as, the fixed
baseline requires bounded source import/export, and implementation guides
describe their absence. An absence is a reported gap/scope conflict, not an
automatic deferral. D06 is a user proposal and must not silently become a
mandatory new GTK feature. D01/D04 do not weaken existing profile ownership.
Observed mandatory gaps remain findings while these dispositions/design tasks
are open; `DecisionNeeded` covers only a genuinely unsettled checkpoint.

Turn new user feedback into a traceable observation with original wording,
candidate/state evidence and affected requirement/scenario IDs. Extend an
existing case or add the next stable ID with a minimal isolated fixture and
observable assertion. Never renumber/reuse IDs. Route findings and proposed
changes to the coordinator's consolidated report before implementation.

Changing an agreed principle or mandatory assertion needs an explicit user
decision and a catalogue version change. Record affected IDs and preserve old
evidence; an implementation fix changes the product/evidence, not the oracle.
Do not convert failure to N/A, shrink coverage or edit the reference to obtain
a pass. Keep actual UI evidence distinct from development-source tests, hosted
deployment, installation, other platform cohorts, signing and release approval.
