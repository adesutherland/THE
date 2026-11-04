# THE UI acceptance catalogue

The [versioned scenario data](../../tests/ui-acceptance/catalogue.json) specifies
acceptance against the agreed guided workbench. It is a **scenario
specification, not an executable runner**. Version 1.0.1 retains 18
requirements and 29 stable scenarios. Its eight decision records now contain
six resolved dispositions and two open scope items: D02 general Redo and D03
full source transfer/Save as. Every full-script initial execution and all four
verdicts remain `NotRun`.

The [consolidated alignment ledger](alignment-review.md) separately records
targeted browser, GTK and console implementation-review gates for the changed
mandatory scope. Those gates have passed with their documented candidate/cohort
and manual coverage limits. Console aesthetics use actual terminal cells and
colours; native Terminal fonts and output selection remain unqualified. Targeted checkpoint acceptance does not execute all 29 scripts,
qualify other OS/device cohorts or establish installed/release qualification.

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
  prototype: response above THE command, followed by explicit `AFTERCMDLINE`
  key-guide bands then status. Resolved D04 preserves inline/custom rows,
  two-operand undocking, command TOP/OFF and status OFF behavior.
- Browser and GTK pane buttons toggle their target: show, hide, show again.
  Switching destinations retains job, draft, output, reader and editor state.
  Resolved D08 returns to the editor when the active target is hidden, without
  automatically revealing a retained latent sibling. Explicit sibling activation
  restores it. Intentional simultaneous Program/named-LOG presentation remains.
- Resolved D05 supplies compact Expand/Return routes with state preservation.
  Resolved D06 records the user-authorized, implemented persistent GTK Files
  view, alongside the separate native Open chooser.
- Native executable programs belong to desktop adapters. Browser execution
  uses its admitted Wasm/RXBIN adapters and never pretends to run a native tool.
- `.profile.the` means Build profile/Apply profile; ordinary `.the` means Build
  macro/Run macro; compiled profile artifacts end in `.profile.rxbin`.
- Aesthetic quality requires an explicit judgement, including the user's
  concerns about small GTK margins and large buttons. Resolved D07 records
  accepted platform-appropriate proportions in the targeted UI review.

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

An open decision cannot turn an observed mandatory failure into
`DecisionNeeded`. Pane toggling, state preservation, stock reference geometry,
profile authority and aesthetic quality remain assessable under the resolved
contracts. Only D02/D03 scope details remain open; targeted gate passes cannot
fill an unexecuted full-script verdict.

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

Resolved D07 accepts the reviewed proportions in their platform context: GTK
12-unit insets and 8-unit gaps around ordinary controls of about 34 units, and
browser coarse-pointer controls at least 44 CSS px high with 13 px labels.
These relationships are not a universal fixed pixel golden or a requirement for
touch-sized desktop GTK buttons. Actual theme, font, scale and input-mode
comfort still require evidence. Concrete findings about clipped labels, crowded
margins, disproportionate controls or poor hierarchy remain valid; any new
universal numeric target needs an explicit proposal.

## Custom footer fixture guidance

F08 explicitly opts guide rows into bands with
`SET FOOTER name position AFTERCMDLINE`. Inline named/unnamed rows remain
inline; the two-operand form undocks a row, and `FOOTER name OFF` removes its
name without deleting reserved text. Custom text, colours, count and declared
ordering remain profile-owned. The bottom bands remain at the bottom for
command TOP/OFF; status OFF remains supported.

Native `SET MSGLINE` retains screen-relative overlay coordinates. With three
bottom guide bands and response on the last inline reserved row, choose
`SET MSGLINE ON -5 1` for a bottom command line, or
`SET MSGLINE ON -4 1` for command TOP/OFF. Explicit legacy overlay collisions
may temporarily cover a row without deleting its text or colour. Collision-free
coordinates are the profile author's responsibility; no automatic relocation
is required or claimed.

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
standard target and return to the editor without automatically revealing a
retained latent sibling. Explicit activation restores that sibling's state.
Resolved D08 preserves intentional simultaneous Program/named-LOG presentation;
target Hide is not an indiscriminate close-all auxiliary operation. Program,
Tools and Files retain their data across hiding, compact Expand/Return and
explicit cross-switching. Resolved D06 provides the persistent GTK Files view;
native Open remains a separate chooser route.

## Dispositions, design work and changes

The eight records are not eight user-permission questions. Each has a
`decision_authority` and `recommended_resolution`. D01 remains traceable as
`Resolved`; approved implemented D04–D08 are now resolved too. Resolved IDs
remain traceable through `resolved_decision_ids` and are absent from every
`open_decisions` list. D02/D03 remain open baseline scope/sequencing items.

| ID | Status / authority | Recommended handling |
| --- | --- | --- |
| D01 | Resolved / reference interpretation | Accept quiet idle; meaningful responses remain profile-owned |
| D02 | Open / scope owner | Include real Redo and target semantics in one scope/sequencing disposition |
| D03 | Open / scope owner | Retain baseline bounded transfer; disposition Save as and transfer sequencing together |
| D04 | Resolved / implementation design | Explicit AFTERCMDLINE bands; stock geometry and inline/custom/TOP/OFF preserved |
| D05 | Resolved / implementation design | Compact Expand/Return with retained state |
| D06 | Resolved / scope owner | User-authorized persistent GTK Files view implemented beside native Open |
| D07 | Resolved / implementation design | Reviewed GTK/browser proportions accepted in platform context |
| D08 | Resolved / implementation design | Hide returns editor; latent siblings require explicit activation; Program/LOG retained |

D04–D08 record approved implemented contracts and the targeted alignment review,
rather than inventing new requirements or adding full-script pass results.
The frozen prototype remains unchanged. Custom profile text, colours, row count
and explicit settings retain their authority; the accepted stock geometry does
not forcibly relocate arbitrary custom rows or legacy message overlays.

D03 retains the conflict explicitly: the prototype shows Save as, the fixed
baseline requires bounded source import/export, and implementation guides
describe their absence. An absence is a reported gap/scope conflict, not an
automatic deferral. General Redo and full source transfer/Save as remain baseline
work. Observed mandatory gaps remain findings; `DecisionNeeded` covers only a
genuinely unsettled D02/D03 checkpoint.

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
