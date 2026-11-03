# Known issues and limitations

This page describes current limitations of the source tree. Qualification work
and proposed enhancements belong in the [roadmap](roadmap.md).

## Editing and macros

| Area | Limitation | Current route |
| --- | --- | --- |
| Optional F8/F9 development helpers | `profile_crexx.the` compile/run helpers have unresolved reports: a filename ending in `hello.crexxmsg` on Windows, and standalone bundled `crexx` exit 127 on macOS. These reports do not establish a single root cause or failure in every runtime cohort. | Stock profiles leave these helpers unbound. Use the matching standalone `rxc`, `rxas` and `rxvm` tools; inspect compiler status and output, not only THE's exit code. |
| Key identity | Normalized key names and terminal byte sequences follow THE's key tables and profile definitions. In the Unix LLM route, `key delete` uses the ordinary delete-character binding; at end of line it can differ from explicit CUA deletion. | Use `SHOWKEY ALL` to inspect bindings. Use `command sos cuadelchar` to request next-line joining explicitly, or `command sos cuadelback` for previous-line joining. |
| Macro dialects | Classic Rexx, cREXX Level B and Level G sources are not interchangeable. | Select the engine before startup and use a matching profile. Terminate consecutive cREXX addressed command literals with semicolons. See [cREXX hosting](crexx.md). |
| External prefix workflows | Shmuel's original macros, adaptations and ports are outside the public package. Upstream THE and live XEDIT equivalence are not established by this fork's tests. | Use the separately held [private kit](private-compatibility.md) under its own instructions. |

Delete and Backspace within a file record commit the changed working record
before cursor movement or refresh. `SAVE` and the stock Ctrl-S binding persist
those changes. ASCII and UTF grapheme deletion have dedicated persistence
coverage in [the test guide](testing.md); this is separate from Tab insertion.

## Unicode and console display

Terminal fonts and composition can disagree with terminal cell accounting,
especially for keycaps, flags, modifiers and ZWJ sequences. Software-cursor
and saved-byte tests do not prove the physical appearance of every terminal.
Use the supplied conservative system profile and [UTF display controls](utf-design.md).
Display substitution does not change file bytes or grapheme identity.

The non-utf8proc fallback does not provide complete Unicode grapheme behavior.
Supported UTF builds should retain `USE_UTF8=ON` and `USE_UTF8PROC=ON`.
Prefix and command fields retain their fixed-cell compatibility model unless
the particular mutation path decodes UTF text.

## Workbench interaction

| Surface | Current limitation | Current route |
| --- | --- | --- |
| Terminal Program console | At 80×24 the bottom dock can show only three output rows at once. | Focus the Program console and use PageUp/PageDown or Home/End for scrollback; expand the terminal for more visible output. |
| Terminal file opening | Open/create prepares an `EDIT` command but has no file picker or path completion. | Type a path at the prompt, or use `DIRECTORY` to inspect files. |
| GTK desktop | The unpackaged macOS executable appears as `the` in the system menu bar. App-bundle identity and native installer checks are separate from the GTK workflow. | Use the native Open/New chooser; build and profile actions are available in the contextual action row. |
| GTK menus and setup | The default Workbench menu contains 18 flat entries, including duplicates of File/Edit actions. The permanent action row includes irrelevant disabled controls; there is no consolidated Tools & Profiles view matching the browser. | Existing current-file actions work. The [graphical usability proposal](roadmap.md#graphical-workbench-usability-proposal) reorganizes their presentation. |
| GTK macOS application menu | About and Settings/Preferences are not registered application actions, so GTK's native application-menu entries are disabled. F10 opens a separate GTK popup, not the exported system menu bar. | Use the existing File/Edit/Workbench menus. Application identity and F10 behavior require distinct macOS acceptance. |
| GTK typography | The editor uses a hardcoded `Monospace 12`; there is no user font/size preference or unified remeasurement of editor, output and field typography. | Font preferences are a GTK presentation capability; browser zoom and terminal-emulator fonts remain host settings. |
| GTK accessibility | Native review could inspect menus through macOS accessibility, but most custom-drawn editor content required screenshot/coordinate interaction. Platform accessibility integration remains unimplemented. | Keyboard/core command access exists; a screen-reader and native accessibility acceptance pass is still required. |
| Graphical editor clipboard | System Copy/Cut is not connected to core editor selection. GTK's Paste toolbar button targets the retained editor insertion zone even when Program input or a form field has focus. Browser editor input rejects text bursts above 128 codepoints, with a separate Wasm message-size bound. | Native text controls and read-only output have some normal clipboard support. A target-aware clipboard path and atomic multiline insertion are required before editor clipboard support can be claimed. |
| Browser New/edit/paste | Live review of an empty newly created file produced `Improper cursor position` and text in the command field. Selected-text paste followed by Undo also failed to restore the original displayed line. | These are observed journeys requiring controlled byte/cursor/undo reproduction; their precise causes remain unconfirmed. Do not treat the simple typing/Undo smoke as clipboard acceptance. |
| Browser compact layout | At 820×720, the open Tools & Profiles pane left roughly 68 pixels of usable body, with duplicate pane switches. Profiles/import controls precede tool associations in a long form. | Enlarge the viewport for the current setup form. The proposed sectioned setup view and minimum pane sizes need browser and GTK review. |
| Browser parser diagnostics | Inline error colours and cursor-local status messages require a configured parser service. The Classic interpreter alone does not provide parser diagnostics. | Check the build's highlighter capability and the Syntax state at the top of the workbench. |

## Panels and browser

The [panel contract](panels.md) lists unsupported CMS/3270 facilities, field
limits and the cREXX queue adaptation. Panels keep their declared dimensions;
a narrow browser can require horizontal scrolling. Resizing the underlying
editor is deferred while a panel is open.

The [browser driver](web-driver-poc.md) is a proof of concept with one
controlling connection per editor process. It does not implement accounts,
TLS, multi-user routing, or a complete IDE. External syntax parsers must be
installed separately. The browser may intercept F6 or Ctrl-L; Esc provides
command-field focus. The wrapping key guide and fixed-column forms have
different sizing behavior.

The common RXBIN tool host is opt-in and requires its prepared dependency
bundle. It is locally qualified on macOS and Chromium/Wasm; the native Windows
helper, installed graphical distribution and other browser engines require
qualification. Native pipes do not signal a waiting-for-input state. Profile
PULL uses legacy stdin; use ADDRESS THE READ for editor prompts. See
[tool capabilities and limits](runtime-tools.md).

## Installation

An older executable, driver directory or development environment override can
take precedence over an installed package. Use the full `crxthe` launcher path
and inspect its runtime information as described in the [Windows guide](Windows-Readme.txt).
Keep the editor and driver modules from the same build together.

Legacy in-place upgrades can retain obsolete macro files; cleanup has not been
qualified against every older installation. Explicit runtime, profile and
cache overrides remain supported. Refresh existing terminals and IDEs after
changing persistent PATH or environment settings.

## Documentation tools

Concurrent invocations of the same batch renderer/toolchain have produced an
unisolated driver status `255`. Serialize calls per installed tool copy, as
the CTest documentation-tool group does. Isolate tool/runtime state and prove
stability before adding parallel workers. See the
[source-highlighting integration guide](syntax-highlighting.md).
