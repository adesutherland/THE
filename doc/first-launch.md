# First launch in CRXTHE 0.9.0

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
| Ctrl-S | Save and keep editing |
| Ctrl-O | Prepare `edit `; enter a filename on the command line |
| Ctrl-F | Prepare `locate /`; enter search text and a closing `/` |
| Ctrl-G | Prepare `:`; enter the file line number |
| Ctrl-L | Move to the command line |
| Ctrl-W / Ctrl-Q | Close this file; unsaved changes are protected |
| Ctrl-Z | Restore the current line, rather than multi-level undo |
| F1 | Help, starting with this key guide |
| F2 | Next open buffer |
| F3 | Save and close the current file |
| F4 | Prefix area beside the current record |
| F5 / Insert | Toggle insert and overwrite mode |
| F6 / Shift-Tab | Toggle command/file focus |

On macOS, function keys may require Fn. Use Terminal's ordinary Command-C/V
for its clipboard. Windows PDCurses additionally binds Ctrl-C/X/V to the
editor's marked-block clipboard commands. Terminal key and clipboard support
still depends on the host frontend; native and browser controls differ.

Optional syntax parsers are registered only if found on PATH. A bundled
installation supplies matching `rxc`/`rxas`; external Markdown, C, Python and
JavaScript parsers are optional. No user-specific `~/.local/bin` paths are
hard-coded in the defaults.

The independent Rexx/HLASM prefix setup macros select XEDIT attention behavior
and overwrite mode for their placeholder workflow. That explicit setup takes
precedence over the normal insert-mode default. Only one language setup should
be active in a session because the alias names overlap.

The optional `profile_crexx.the` layers development bindings over the default.
Its F8/F9 compile/run helpers have separate unresolved Mac/Windows failures
recorded in the [Windows report](windows-installation-qa.md); they are not
enabled by the normal first-launch profile.

The new identity, profiles and packaging rules are local source changes until
a separately qualified candidate is built, signed and published. Earlier
published THE prereleases retain their original names and contents.
