# First launch in CRXTHE 0.10.dev

CRXTHE is an independently maintained fork based on Mark Hessling's THE 4.0.
Its version number describes this fork. Report CRXTHE defects to
[this fork's tracker](https://github.com/adesutherland/THE/issues).

Packaged installations provide `crxthe file.txt` (`crxthe.cmd` on Windows).
The existing `the` launcher and `THE_*` configuration names remain compatible.
A personal profile takes precedence over the shipped default; use
`-p profile.the` or `-p profile_regina.the` to try the stock profile explicitly.

The cREXX and Regina defaults share one maintained layout/theme definition:
`profiles/default-profile.json`. `scripts/generate-default-profiles.py`
produces the executable profiles for the two languages and their small
command-prompt helpers. Users should customize a personal profile rather than
edit generated shipped files. The restricted browser profiles use the same
layout commands while the frontend supplies its visual theme, retaining the browser's command and workspace policy.

## Console defaults

The top ruler shows logical columns. Line numbers and prefix commands have a
distinct shaded area; active prefixes and pending commands use separate colors.
Insert mode starts on. Enter splits a line, Home/End move within it, and
Shift-arrow/Home/End support selection where the terminal supplies those keys.
The status line and startup message expose the CRXTHE identity and key hints.

| Key | Action |
|---|---|
| Tab | Insert spaces to the next eight-column tab stop |
| Ctrl-S | Save and keep editing |
| Ctrl-O | Prepare `edit `; enter a filename on the command line |
| Ctrl-F | Prepare `locate /`; enter search text and a closing `/` |
| Ctrl-G | Prepare `:`; enter the file line number |
| F6 | Toggle between command line and file editing |
| Esc | Move to the command line (browser: Esc again returns to editing) |
| Ctrl-L | Console command-line shortcut |
| Ctrl-W / Ctrl-Q | Close this file; unsaved changes are protected |
| Ctrl-Z | Restore the current line, rather than multi-level undo |
| F1 | Help, starting with this key guide |
| F2 | Next open buffer |
| F3 | Save and close the current file |
| F4 | Prefix area beside the current record |
| F5 / Insert | Toggle insert and overwrite mode |
| Shift-Tab | Toggle console command/file focus |
| F10 | Open the complete current key-definition list |

On macOS, function keys may require Fn. Use Terminal's ordinary Command-C/V
for its clipboard. Windows PDCurses additionally binds Ctrl-C/X/V to the
editor's marked-block clipboard commands. Terminal key and clipboard support
still depends on the host frontend; native and browser controls differ.

Optional syntax parsers are registered only if found on PATH. A bundled
installation supplies matching `rxc`/`rxas`; external Markdown, C, Python and
JavaScript parsers are optional. No user-specific `~/.local/bin` paths are
hard-coded in the defaults.

The optional `profile_crexx.the` layers development bindings over the default.
Its F8/F9 compile/run helpers have separate unresolved Mac/Windows failures
recorded in [known issues](known-issues.md); they are not
enabled by the normal first-launch profile.

This guide describes the profiles in this source tree. An older installed
package retains its own profiles; inspect its version/manifest when behavior
differs. See [known issues](known-issues.md) and [qualification work](roadmap.md).

## Tabs and browser sizing

In file text, Tab inserts spaces up to the next tab stop (columns 9, 17, 25,
etc.). This applies in insert and overwrite modes. `SET TABS INCR 4` selects
four-column stops instead. Prefix/command focus uses F4/F6; macro panel forms
continue to use Tab to move between fields.

An existing literal tab byte is retained and shown as `→`, occupying one
logical cell. It is not a native terminal/browser tab expansion, so cursor
and prefix alignment agree across frontends. Saving does not convert these
bytes. For expanded indentation, explicitly use `EXPAND ALL` with the chosen
`SET TABS` stops. `SET TABSIN ON 8` opts into conversion on input (and converts
the current buffer when set in a profile); it is not enabled by the stock
profile. `SET TABSOUT OFF` keeps typed spaces as spaces on save.

The browser measures its editor viewport and updates model rows and columns
when the window or sidebar changes size, within supported bounds of 12–256
model rows and 40–1000 columns. Headings/status/command rows are accounted for;
the rendered file rows fill the available editor area. A macro panel retains
its declared dimensions, with editor resizing deferred until the panel closes.
Help is prepared with numbered prefixes on its first display.

Shmuel's prefix macros, adaptations and cREXX ports are all held in a separate
private kit and are excluded from new CRXTHE packages.

## Prefix cursor and visible key guide

The browser paints the prefix caret at the actual editable prefix column,
clamps its painted position inside the field, and highlights the active prefix.
Prefix text is left-aligned to the same cell positions used for clicks and
caret movement. The caret is hidden while the browser command field has focus.

A reserved footer row lists F1–F6, F10 and Esc. F10 opens THE's `SHOWKEY ALL`
view with default and profile key definitions. This key guide is inherited by
new buffers, including help. The console file-tab strip above the filename
uses explicit dark colors; with one file it remains an empty strip by design.
F6 toggles between command and file editing, restoring the file caret. From
the prefix field, F6 enters command; the next F6 returns to file editing.
Esc goes directly to the console command line, with Shift-Tab to return.
Browser Esc focuses its command input and Esc again returns to editing.
F6 is also handled when the browser delivers it. Ctrl-L remains a console
shortcut because browsers generally reserve it for their address bar.

Left at the first text column enters the current record's last editable prefix
column when the prefix is enabled. Left from the prefix's first column moves
to the previous visible record's end, including empty and Unicode records.
At the first visible record's first prefix column it stays put. With the
prefix off, normal CUA previous-record movement remains. Shift-Left remains
text selection rather than entering a prefix field.

## Delete, Backspace and saving

In file text, deletion updates the working record and its in-memory line before
refreshing. Delete removes the character under the caret; Backspace removes
the preceding character. UTF file editing removes a whole grapheme cluster,
including its combining marks. Ctrl-S or `SAVE` writes the edited bytes.
`QQUIT` discards unsaved buffer edits;
ordinary quit protects them.

`SOS CUADELCHAR` joins the next line when the caret is at or beyond the current
line's end. `SOS CUADELBACK` at the first column joins onto the preceding line.
The ordinary `SOS DELCHAR` and `SOS DELBACK` commands do not join lines. Key
bindings and terminal key identities can differ; inspect F10 / `SHOWKEY ALL`,
and use the explicit command when a particular boundary behavior is required.
