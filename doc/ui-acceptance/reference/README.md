# Agreed workbench reference

These files preserve the design reference used by the acceptance catalogue.
They are prototypes, rather than implementations or test results. Their source
bytes are retained unchanged; `viewer.html` only selects which platform variant
to display in an iframe.

| File | Purpose | SHA-256 |
| --- | --- | --- |
| [workbench-polish.html](workbench-polish.html) | Shared guided workbench: browser, GTK on macOS, GTK on Linux or Windows | `04fc03feeb50a672d9c0f3800572aa53b2bdf05c71eb80dd2ae15b8380092f2b` |
| [ui-directions.html](ui-directions.html) | Earlier direction choice; Guided workbench was selected | `6385dd1bf504b11b23d7cbee7e4430e45c16bbc2ed17d27e4617917e8035e545` |

Open the [viewing harness](viewer.html) through a local static server. It is a
review aid and is not shipped as the editor UI. The prototype's own sample
actions simulate states; executing them does not qualify an editor operation.

## Authority and interpretation

Explicit user decisions take precedence over the prototype. The prototype is
the reference for structure, terminology, hierarchy and depicted states.
Current implementation and test receipts describe what works; they do not
approve a departure from the design.

The browser is the first reference experience. GTK preserves the same concepts
with native controls, including macOS system-menu placement and GTK-only font
preferences. No final console mockup accompanies this prototype: console
translation rules must be distinguished from an approved console design.
Linux/Windows GTK reference variants do not establish execution on those OSes.

The following clarifications govern the catalogue:

- Standard pane buttons toggle a visible active pane off and restore it on
  the next activation. Choosing another pane activates that destination.
  Jobs, drafts, output, reader position and editor state survive these changes.
  This explicit user requirement supersedes the prototype's selection-only
  Program and Tools demonstration handlers.
- Profiles own editor footer text, names, count and colours. Default layout
  comparison must also test an intentional custom profile. The mock's literal
  `Ready` is sample content, not a requirement for a permanent idle message.
- GTK needs an explicit aesthetics assessment, including content margins,
  spacing, button proportions, typography and visual density. Prototype pixel
  values are evidence of intended relationships; precise platform tolerances
  require a decision rather than an invented universal pixel limit.
- A persistent GTK file list is a proposed shared feature. A native Open
  chooser supports opening files but does not establish file-list parity.
- Redo and Save as appear in the mock. Their scope and capability disposition
  must remain visible in the review; an implementation omission is not an
  accepted exception merely because existing tests pass.
- Short-window Expand/Return behavior is an implementation adaptation. Review
  its visibility, state preservation and discoverability, and identify any
  design decision still needed. The prototype is not itself a complete
  short-window or phone acceptance implementation.

Changes to this reference or to mandatory expectations require a recorded
design decision. A defect repair or test-fixture adjustment must not silently
redefine the expected outcome.
