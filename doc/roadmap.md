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
separate from these source-development stages. The browser has its own web
presentation, and an optional first GTK 3 desktop frontend now exists.

### Cross-platform workbench review

The [extensible workbench staged design](workbench-extension-design.md) freezes
the phase-0 command, ownership, limit and result contracts. [Shared actions and
menus](workbench-actions-menus.md) are implemented in the development source.
The remaining order is named log/read-only document panes, forms/editable
fields, then qualified file IO and ooRexx adapters. Each increment retains its
own implementation and surface-qualification gate; later contracts do not
claim available features.

Continue the shared browser, terminal and GTK review through real file,
editing, run and console workflows. The guided workbench now has visible
Menu and Run routes, profile-defined footer rows, keyboard access to controls,
and separate Program input/output. Remaining work includes terminal file
discovery beyond the `EDIT` prompt and explicit diagnostics capability states
across frontends. Keep editor and tool-session state in the C core through the
driver boundary.

GTK implements the browser reference workflow using native widgets and the
shared session model:

1. Program input submits raw entry text once. The GTK journey covers successive
   input records, including Unicode and an explicitly empty line, while retaining
   input focus and saved source bytes.
2. A typed native snapshot exposes C current-file action/artifact/result state;
   job completion advances in the owner update path rather than JSON formatting.
   Contextual Build/Run/Apply presents the same enabled states, stale-source
   refusals and profile replacement outcomes as the browser, using the existing
   native compiler and profile adapters.
3. Program output/input is separate from build/profile results. Reader scroll
   position and selection survive output updates, with an unread/Latest control.
   The GTK journey exercises open/edit/build/run/profile actions using the
   actual widgets and callbacks.

The acceptance boundary is a real GTK journey through the same selected-file
actions and outcomes, preserving cursor, dirty state and exact saved/reopened
bytes. OS integration and installer qualification remain separate. Terminal
alignment follows the same contracts after this GTK slice.

### Graphical workbench usability proposal

The browser remains the reference for the guided workbench. GTK now shares
its file-action and runtime contracts, but its navigation, setup surface and
visual hierarchy are not yet the same usability baseline. The browser also
needs the clipboard, New-file and compact-layout corrections recorded in
[known issues](known-issues.md#workbench-interaction). The following is a design
proposal for feedback, not implemented UI behavior.

**Review basis.** A single lead UI reviewer inspected the hosted browser
reference (`e80730a`) and GTK source `88e448e`. The browser was reviewed in a
separate in-app-browser tab using distinct review filenames; storage isolation
from previous in-app-browser sessions was not established. GTK was operated
through a disposable macOS app wrapper around the current executable with
separate files and configuration;
the wrapper does not qualify the application's distributable identity.
Classic Run/input/completion, compiled-source Build/Run and profile
Edit/Build/Apply were exercised, alongside navigation, narrow layouts and
clipboard attempts. Source audits separately checked clipboard limits,
native menu projection and typography. These are expert-review findings,
not a measured user study or Linux/Windows qualification.

GTK New was inspected but not reliably completed through computer use, so
this review makes no native New defect claim. Detailed native clipboard and
keyboard coverage beyond the walked journeys remains an acceptance task.

**Shared structure.** Retain the guided workbench and THE's prefix, command
line, logical cursor and profile-owned footer beneath the editor. Use one
application navigation model with File, Edit, View, Tools and Help groups.
Remove the default 18-entry catch-all Workbench group and duplicate actions.
The existing registry already supports multiple flat groups; regrouping does
not require a new menu grammar. Browser and GTK must project equivalent
actions, availability and refusal reasons from shared identities. Keep
profile-defined extensions and profile-owned key bindings.

- File: New/Open, Save/Save as, Close and Next file.
- Edit: Undo/Redo, Cut/Copy/Paste/Select all, Find and Go to line, once their
  shared action contracts are complete.
- View: Files, Program console, Tools & Profiles and focus/navigation routes.
- Tools: current-file actions and Tools & Profiles. Unassociated prose files
  have role Text, with no forced Configure runner action.
- Help: command help, key definitions and About, with native placement below.

Keep one compact toolbar for frequent operations and one current-file task
row. Show Build/Run for applicable source, Build/Apply for profile source,
and Run for runnable macros or images; do not expose profile administration
as permanent unrelated buttons beside every file. Prefer a clear next action
and an accessible prerequisite over a row of disabled controls. Preserve
source/revision/generation checks when menus or controls queue an action.

**Tools & Profiles.** Provide the same discoverable setup destination in both
frontends, separate from Program input/output. Split the long browser form
into Profiles, File types & tools, and Build results sections. Show the active
profile, editable source when available, compiled artifact and startup
persistence state. Edit source opens it in the normal editor; Build and Apply
then use current-file actions. Compiled-only imports explicitly have no source
to edit. Tool associations and permitted runners must be visible without
scrolling past an unrelated profile-import form. Program console remains
program stdin/stdout/stderr and lifecycle controls. Retain build/program
history, focus, drafts, selection and unread state across setup work.

Use one set of pane switches. At wide sizes, retain a useful editor and right
auxiliary pane. At narrow/short sizes, offer a properly sized lower or expanded
auxiliary view, with explicit return to the editor; do not compress setup into
an unusable sliver. Do not add permanent diagnostic/debug information to the
main toolbar. Version, capabilities and licences belong in About/details.

**Platform-native placement and Settings.** On macOS, application About,
Settings and Quit belong in the system application menu; register the missing
`app.about` and `app.preferences` actions in the existing GtkApplication
integration. Keep Services/Hide and checked Quit behavior. Other GTK desktops
use their in-window menus, with Preferences and Help/About routes to the same
actions. Browser uses Menu/Settings and its existing About route. macOS F10
currently opens a GTK popup; retain a consistent grouped fallback unless
native system-menu keyboard focus is demonstrated through supported APIs.
Do not describe that popup as operation of the macOS menu bar. Proper app
bundle naming is separate from implementing About and Settings.

Settings provides familiar access while retaining configuration authority:

| Section | Scope and owner |
| --- | --- |
| Appearance | GTK-only monospace family and point size, applied consistently to editor, output and file fields. Keep themed UI control fonts. Persist driver preferences separately from editor profiles. Browser text size uses browser zoom; terminal font belongs to the emulator. |
| Editor | Show current core values and their file/profile scope. Apply live options through existing commands; offer Edit profile defaults for startup behavior. Do not silently maintain a second set of editor preferences or rewrite arbitrary Rexx source. |
| Tools & Profiles | Navigate to the shared setup view; do not duplicate runner/profile forms in Settings. Profiles/macros remain the source of IDE behavior. |
| About | Product/development version, credits/licences and capability details; no editable IDE or runtime setup. |

Only show profile provenance when it is known. Arbitrary macro side effects
can make attribution incomplete; an explicit unknown/profile-defined state is
better than claiming a particular source line supplied a setting.

GTK font application needs a single driver path for chooser, startup and
display/style changes: validate installed monospace fonts, remeasure cells,
update readers/field CSS and pane minimums, then issue the normal owner resize.
Preserve cursor, dirty state and bytes. Save only explicit preference changes,
with a visible session-only result if persistence fails. Use logical GTK pixel
metrics rather than multiplying point size for Retina displays.

**Clipboard and editing acceptance come first.** Resolve clipboard actions
from the focused target, preserving native entry behavior and read-only copy.
The editor core owns selected bytes, replacement, structural multiline
insertion, read-only checks and one complete Undo transaction. Copy includes
pending edits; Cut writes the clipboard successfully before deleting; Paste
validates and either commits completely or leaves bytes unchanged. A transport
must not silently discard a large paste or leave a partially inserted prefix.
Browser clipboard permissions/user activation are frontend concerns; keyboard
copy/paste events and menu actions require appropriate supported paths and a
clear denial result. Multiline Program input must never cause accidental Send.

The bounded implementation order, subject to this proposal's approval, is:

1. Reproduce and repair New/first-input and selection-replacement Undo; provide
   shared target-aware clipboard actions and bounded atomic paste transport.
2. Regroup default menus, add missing About/Settings actions and make the
   browser/GTK task row and Tools & Profiles setup coherent. Remove redundant
   permanent buttons and correct ordinary-text file classification.
3. Add GTK font preferences with consistent metrics and persistence; complete
   narrow/short pane sizing and visual spacing, contrast and focus polish.
4. Repeat the same human task review on the updated browser and GTK, including
   both pointer and keyboard journeys. Repair findings before declaring parity.
   Terminal presentation follows the agreed shared actions after GUI review.

Acceptance includes New→type/paste→Undo→Save/reopen, Unicode/multiline and
large selection replacement, clipboard denial without mutation, copy from
read-only output, correct Program/form targets, Classic and compiled-source
run, profile Edit/Build/Apply/import, menu cancellation with retained caret,
visible profile footer, and wide/narrow/short layouts. Confirm source bytes,
dirty state, complete Undo and persisted settings as well as visible pixels.
Font/DPI and system-menu behavior require native macOS checks; Linux/Windows
and other browser engines remain separate qualification.

Platform references: [Apple menu-bar guidance](https://developer.apple.com/design/human-interface-guidelines/the-menu-bar),
[GTK application-menu behavior](https://docs.gtk.org/gtk3/class.ApplicationWindow.html),
[GTK font chooser filtering](https://docs.gtk.org/gtk3/method.FontChooser.set_filter_func.html)
and the [W3C clipboard contract](https://www.w3.org/TR/clipboard-apis/).

### Other retained engineering work

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
