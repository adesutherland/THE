# Prefix cursor, top strip and key guide — 26 October 2026

The later [navigation report](crxthe-navigation-windows-qa.md) supersedes this
report's F6 behavior: F6 now toggles command/file focus.

Adrian reported the missing browser prefix caret, an orange console strip above
the filename on shortcut launch, and a need for visible function-key definitions
and a command-line shortcut.

The browser now paints a caret in the active prefix field. Prefix content,
clicks and the caret use the same left-aligned cells. The backend supplies the
editable prefix width and gap; the painted caret is clamped inside the field,
including at its full length. Prefix/file carets are hidden while the command
input owns browser focus.

The stock cREXX and Regina profiles now reserve a bottom key-guide row:
F1 Help, F2 Next, F3 Save/Close, F4 Prefix, F5 Insert, F6 Command, F10 Keys,
and Esc Command. F10 opens SHOWKEY ALL with default and profile definitions.
F6 and Esc go directly to the console command line; Ctrl-L remains available.
In the browser, Esc focuses its command field and a second Esc returns to
editing. F6 is handled when delivered by the host browser. Panel Esc still
cancels the form and panel function keys retain their panel behavior.

The reported strip's position identifies the console file-tab window. It is
blank with a single file and was not covered by the new profile's theme.
FILETABS/FILETABSDIV now have explicit dark backgrounds, alongside a separately
styled key-guide row. This is a targeted theme repair; the original orange
appearance was not captured from Adrian's physical Terminal window.

## Verification

The profile checks cover both engines: the visible key guide, F6/Esc/Ctrl-L
command focus, F10 key definitions and the file-tab RGB background. A real
80×24 PTY checks ordinary editing and saving through the native F6 sequence.
The actual local .command shortcuts were additionally invoked in fresh PTYs,
using a disposable file via their new --file option; both engines passed.
No existing scratch file was overwritten by that launch test.

Rendered Chrome checks cover prefix clicking, caret alignment, character entry,
backspace, the full prefix boundary, Esc in/out of the command field, command
execution, F10 and the footer guide. An independent LLM-controlled Chrome
walkthrough confirmed the visible caret, key guide, Esc focus and F10 viewer.

Full macOS editor regression passed **79/79**. Profile generation, TypeScript
and diff checks passed. Fresh shortcut tests passed under both engines.
Local package checks are separate from this source result. The changes are local previews; no new Windows qualification,
signing, publication or external macro handoff is claimed. Physical macOS
Terminal appearance still needs Adrian's visual recheck after restarting the
preview. The cREXX runtime remains bundled; Shmuel's prefix package remains
private and excluded.
