# Navigation and Windows first use — 26 October 2026

The stock cREXX and Regina profiles now make F6 a command/file toggle. The
return restores the file's logical caret, including after command repaint.
From a prefix field F6 enters command; the next F6 enters the file area.
The browser handles the same toggle when the host delivers F6. Esc remains
its reliable command shortcut. Macro panel keys keep their panel behavior.

At the beginning of a text record, Left enters its last editable prefix cell.
Continuing Left off the prefix's first cell goes to the previous visible
record's end. The first record stops at its first prefix cell. Prefix-off
behavior and Shift-Left text selection retain their existing CUA behavior.

Preserving the file caret adds state to the structure shared with UI drivers.
Modules now export an ABI version, which the core checks before calling their
operations. Old modules are rejected with a matching-driver/THE_DRIVER_PATH
diagnostic. A matching module can still be found later in the normal search.
Keep the editor and its drivers from the same candidate together.

## Windows installation review

The [Windows guide](Windows-Readme.txt) covers bundled cREXX, an existing cREXX
with automatic fallback, optional Regina, first file editing, function keys,
new-terminal PATH refresh, older installations and development overrides,
portable use, diagnostics and removal. Prefer the CRXTHE shortcut or `crxthe`
command when another `the` is installed.

The installer adds a finish page with Open CRXTHE and Read Getting Started.
Its editor shortcut starts in Documents; Getting Started also has a Start
menu shortcut. Bundled packages omit the development `the-env.ps1` helper.
Intentional environment overrides remain supported. No credentials changed.

The Windows runner's installer test now checks both launcher names, shortcut
targets and the Documents working folder, installed documentation, omission
of the development helper, and removal of both shortcuts. Package smoke tests
use the public `crxthe` launcher for CRXTHE packages. These added Windows
assertions have not yet run on Windows.

## Local verification

- Full macOS editor suite: **79/79 passed**.
- Focused navigation, real curses PTY, logical cursor, rendered Chrome and
  driver-module checks: **9/9 passed** after the ABI guard was added.
- Both interpreters cover file/prefix traversal, empty and Unicode records,
  first-record boundaries, right-side and disabled prefixes, Shift-Left,
  F6 restoration and unchanged file bytes. The native PTY additionally
  types and saves at the traversed previous-line end.
- An actual published panels.2 LLM module was rejected cleanly by an isolated
  copy of the new core. Replacing it with the matching new module succeeded.
- NSIS compiled the updated installer using a disposable dummy payload.
  This verifies script syntax; it does not produce a runnable candidate.
- Generated profiles, TypeScript and whitespace checks passed.

From source commit `12e1cd20234de1b9e01f5f6061ab1a81af31773e`, fresh unsigned
macOS ZIP/PKG previews were built. The relocated ZIP passed manifest integrity,
cREXX, Regina and no-macro execution, stock profiles and serving its own
installed browser assets with build/user tool paths removed. The actual local
cREXX and Regina `.command` shortcuts also passed F6/prefix navigation and
saved-byte checks in fresh PTYs using disposable files. Adrian's existing
scratch file and ordinary installed `the` were preserved.

The refreshed isolated preview is under
`/Users/adrian/THE-macro-tests/crxthe-candidate/CRXTHE-0.9.0-darwin-arm64`.
The ZIP SHA-256 is
`493f951915adf6205142001a0e7ebd51cd35f122b07cbdb78e8cdfbcbc378d70`;
the PKG SHA-256 is
`569636963885148b4be4c9f0055508da37a58124863850d8dd7eddd103a54f01`.
These are local previews; they have not been signed or published.

## Remaining qualification

This is a local CRXTHE preview. The current published installers remain
THE v4.0.0-panels.2; they do not contain these changes. A new Windows build,
runner install/uninstall checks, interactive finish-page launch, upgrade from
the older package, signing and publication remain separate work. In particular,
an old installation may retain legacy files until upgrade cleanup is qualified.
Physical Windows keyboard/cursor appearance and the reported macOS Terminal
strip still need Adrian's visual check. No external Shmuel handoff is authorized
before Adrian's Windows smoke test. The private macro kit remains excluded.

This report supersedes the earlier F6 direct-command description in
[the prefix/key-guide report](crxthe-prefix-keys-qa.md).

## Windows candidate liaison

At Adrian's request, the source branch was pushed and the Windows chat
**Use and test installed THE version** began testing exact revision
`a939b51277eaf57132d5d02d8ae505a0b74d3191` in an isolated worktree.
[Windows CI run 40352133442](https://github.com/adesutherland/THE/actions/runs/40352133442)
built the editor, archive and installer successfully, and its installed
candidate/lifecycle checks passed. The regression step failed three tests,
so that run is not a qualified release candidate.

Both default-profile tests reached the Unicode navigation fixture and failed
when Python read its UTF-8 bytes using Windows' default cp1252 encoding. The
browser script completed, but its Python save check incorrectly required LF
although THE defaults to CRLF on Windows (`src/default.c`, `SET EOLOUT`).
The later raw-tab profile check had the same LF-only assumption.

The test correction reads UTF-8 explicitly, compares navigation fixture bytes
exactly, and asserts complete saved tab/space bytes with the platform's native
output EOL. Failures now show the actual saved bytes. It changes no editor,
profile or installer behavior. The five affected profile/browser tests pass
locally on macOS; the replacement Windows run and workstation byte check are
still pending. Windows UI smoke, final signed-installer checks and Adrian's
manual handoff gate remain separate.
