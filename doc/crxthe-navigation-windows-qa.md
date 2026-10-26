# Navigation and Windows first use — 26 October 2026

Latest automated candidate: `90fa7fd502b0472dc146ef69f62834b83687b44f`.
Windows CI passes **66 tests, with 13 skipped and none failed**; the exact
artifact also passes both engines' workstation native keyboard/panel/save
checks. Final workstation qualification is recorded below: automated installer
lifecycle and visible browser checks pass; the exact final installer's visible
Finish-page repeat was limited by UI automation. Signing, publication,
physical Windows inspection and Adrian's Shmuel handoff gate remain open.

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
assertions were prepared during the local phase; the later Windows CI result
below now verifies them on the runner.

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

The current published installers remain THE v4.0.0-panels.2; they do not
contain these changes. A new Windows build and runner install/uninstall checks
have now passed as recorded below, alongside final workstation native input,
visible browser and automated installer lifecycle checks. The final EXE's
visible Finish-page repeat was incomplete; the earlier flow passed with the
same installer script and guide. In-place upgrade from the real older folder,
signing and publication remain separate work. An old installation may retain
legacy files until upgrade cleanup is qualified.
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
locally on macOS. The Windows agent independently confirmed exact saved bytes
with NONE, cREXX and Regina: `61 09 62 0a` becomes `61 09 62 0d 0a`, retaining
the literal tab. Typed Tab before `alpha` gives eight spaces and CRLF under
both macro engines. The old failed-run artifact was used only for this
diagnostic, not as a qualified candidate.

### Replacement run and workstation checkpoint

[Windows CI run 40354270900](https://github.com/adesutherland/THE/actions/runs/40354270900)
is green at exact source `0fabc48684f2bcf875b1e773af165ba09b2f7f76`. The macOS
coordinator independently verified its head SHA, job/step conclusions and test
log: **64 tests passed, 13 skipped, none failed** (77 registered). The skips
cover platform-restricted shell tests and unavailable optional parser checks;
they are not 77 executed passes. Hosted installation, runtime/cache selection,
reinstall and uninstall checks also passed.

The Windows agent reports a clean matching worktree/artifact manifest,
`source_dirty=false`, and all 155 payload hashes verified. Candidate SHA-256:

- ZIP: `b2299ce59121552dccf80944f15697d4056afa1c7518fd30ea3a188d3260b8c4`
- Unsigned installer: `e54d722e09575a2d8ffdb2cee4329f7343bc4bf92db0d581bcea396e5a0d43c5`

Its workstation public-launcher checks pass for cREXX, Regina and NONE,
including stock profiles, installed web assets, runtime/cache selection and
runtime loaders. The cREXX first-launch/navigation/Unicode/tab/footer checks,
semantic panels and rendered Chrome editor/panel suites pass. These are
reported workstation results, separate from the coordinator's CI readback.

The agent subsequently confirmed both engines' extended default-profile,
rendered editor and panel suites, no-filename listing, new/existing UTF-8
files, first-display help numbering and F10. An actual old panels.2 LLM DLL
was rejected cleanly; the matching new DLL succeeded. Workstation installer
lifecycle passed with the real panels.2 registry, PATH, shortcuts and payload
hashes verified restored. Evidence is under
`C:\Users\adria\CRXTHE-windows-qa\0fabc48`.

The actual visible installer was also exercised on that revision: destination
and runtime pages, bundled default, Open CRXTHE checked and Getting Started
unchecked on the finish page. Selecting both launched the correct candidate
without a filename and opened its installed Windows guide in Notepad. The
Start menu target/Documents folder and new-shell `crxthe` resolution matched
the disposable candidate; legacy `the` still selected signed panels.2.
Candidate uninstall restored the baseline registry, PATH, shortcuts and all
panels.2 hashes. Only the newly opened guide tab was closed. These are
interactive unsigned-installer results, not a signed-release qualification.

Native F6/prefix traversal/Enter/Tab and command Save/File exit passed under
both engines, producing exact `Z       alphaQ\r\nbeta\r\n` bytes. Native Ctrl-S
exposed the console-mode defect below. Final visible UI and signed-installer
checks and Adrian's manual Shmuel handoff gate remain open; no new tag,
signing, release publication or external handoff has occurred.

### Native Windows quiet-shell console mode defect

The Windows agent inspected only its owned running editor through
AttachConsole/CONIN$/GetConsoleMode. Fresh NONE/-n started at input mode
`0xC8` (processed/line/echo disabled). Running only `OSQ VER` changed the
same process to `0xCF`, enabling all three flags. Stock cREXX and Regina
profiles also left `0xCF`; both perform quiet shell checks for optional tools.
Temporarily clearing line/echo processing in a disposable diagnostic made
Ctrl-S save the exact expected bytes immediately. The prior standalone
getwch probe therefore did not rule out an editor restoration defect.

The source already calls `resume_curses()` after Windows OSQ specifically
to restore control-key handling (`src/execute.c`). The curses lifecycle's
suspend/resume implementations had been restricted to UNIX, and CMake had
not enabled PDCurses' program/shell mode capabilities. The repair enables
those capabilities on Windows and runs its existing endwin/reset_prog_mode
lifecycle there. A quiet return snapshots the current curses state before
resetting the console mode, preserving its current dimensions, mouse and caret
settings. Console changes remain inside the curses driver.

Eight focused Mac profile/native/boundary/module/browser checks pass after
the repair. A dependency-free Windows regression now creates an owned hidden
console for each engine, saves a ready marker through THE, attaches read-only
to the verified child image and asserts processed/line/echo modes are disabled
both at startup and after OSQ. It closes only its own test process. The Windows
agent ran this helper against the old `0fabc48` payload under both engines:
startup `0xC8` passed and post-OSQ `0xCF` failed exactly as intended. The helper
needs no extra dependency and hidden consoles were attachable on the
workstation. Its first corrected-build CI/native checks targeted
exact source `99ac22ccc79c8ed75cd457846aca7d2d7ad46dbd`,
[run 40360809171](https://github.com/adesutherland/THE/actions/runs/40360809171).
Mac and old-payload diagnostic results do not qualify this Windows repair.

Run 40360809171 built and packaged the repair; existing tests and installer
checks passed, but both new console tests exited 32 before their ready marker.
No mode assertion was reached. The raw-build tests had captured their CTest
environment before its PDCurses shared-library directory was added to PATH.
Their registration now follows native PATH assembly, and hidden-child startup
output is captured in failure diagnostics.

The Windows agent separately ran the original helper against a portable
extraction of that exact `99ac22c` payload for diagnosis only. Both engines
passed startup and post-OSQ checks at `0xC8`, confirming the repaired packaged
core/driver loads and restores input mode. This does not turn the failed
overall CI run into a qualified candidate. The successful replacement run and
final workstation record below supersede that diagnostic checkpoint.

### Green repaired candidate: 90fa7fd

[Run 40363219661](https://github.com/adesutherland/THE/actions/runs/40363219661)
is green at exact `90fa7fd502b0472dc146ef69f62834b83687b44f`. The coordinator
independently verified the head SHA, successful regression/installed-lifecycle
steps and **66 executed passes, 13 skips, zero failures** (79 registered).
This includes both native console-mode regressions; the raw-build DLL path
correction is now exercised on the runner.

The Windows agent verified the exact final artifact's clean manifest/source
and all 155 payload hashes. SHA-256:

- ZIP: `40409cfbf9a1905415e617fee59a90de50d50c6283098209076d85527053477d`
- Unsigned setup: `4d1fd9e443047bd3562978e5a5b45c8088fbedbf84649ef61dd3fb46a53d28e5`

On that exact payload, both engines pass actual native ConPTY immediate
Ctrl-S, remembered F6, Home/Left/prefix/previous-record-end, Tab/Enter,
OSQ return, ordinary OS/prompt/return, generic panel submit/cancel, explicit
cursor navigation and resumed editing/Ctrl-S. Expected saved bytes match and both editors exit zero. No
live console-mode adjustment or workaround was used. Results are
`native-final.json` and `native-steps.json` under
`C:\Users\adria\CRXTHE-windows-qa\90fa7fd`.

The agent also reports the final default-profile/rendered/panel/runtime suite
complete. Final no-filename/help/ABI and protected installer lifecycle checks,
the visible-installer limitation, baseline restoration and consolidated
workstation matrix are recorded below.
This is still an unsigned candidate, not a published release or completed
manual Shmuel handoff.

### Final exact-artifact workstation qualification

The Windows agent completed its report for the exact `90fa7fd` artifact above.
All 13 workstation suites passed. External optional Markdown/Python parser
semantic styles were skipped while their buffer editing was checked. These
workstation results are the Windows agent's evidence; the coordinator directly
verified the CI totals and confirms that `packaging/windows/the.nsi` and
`doc/Windows-Readme.txt` are byte-identical between `0fabc48` and `90fa7fd`.

| Area | Final result and scope |
|---|---|
| Payload and launch | Clean exact manifest/source, all 155 hashes, public launcher, stock profiles, isolated PATH; cREXX, Regina and NONE pass. |
| Runtime/cache | Installed/bundled choice, compatibility/fallback, explicit overrides, private cache and invalid-library checks pass. |
| Native console | Both engines start and return from OSQ at `0xC8`; immediate Ctrl-S, F6 remembered caret, Home/End/Left/prefix, Tab/Enter, OSQ and ordinary OS/prompt/return pass without changing live console flags. |
| Panel input and subsequent editing | Both engines submit RC0/cancel RC4; explicit cursor navigation and subsequent editing/Ctrl-S pass. Exact final bytes are `UTZ       alphaQRS\r\nbeta\r\n`, and editors exit zero. Demo focus/footer restoration remains separately listed below. |
| First use | Both engines pass no-filename directory, new/existing UTF-8 files, first-display help numbering and F10. |
| Driver matching | Actual old panels.2 driver rejected with ABI2 diagnostic; matching new driver loads. |
| Rendered and visible browser | Both engines' rendered editor/panel suites pass. Actual final90 IAB F6, Ctrl-S, full 16-character field caret, middle-cell mouse placement, Tab/Enter, Esc cancel, explicit navigation/resumed editing and F10 pass with exact UTF-8/tab/CRLF output. Wider 900/1200-pixel resize checks pass. |
| Final EXE automated lifecycle | Disposable install, runtime choice, reinstall, shortcut/Documents/guide, uninstall, retained user file and PATH restoration pass locally and on hosted CI. |
| Final EXE visible installer | Incomplete. Destination accessibility text was readable, but fresh-window activation/capture geometry was unavailable. No Install click was executed in this repeat; the owned setup process was stopped. This control limit is not a demonstrated installer defect. |
| Earlier visible installer | Complete Finish/Open CRXTHE/Getting Started/Start menu/new-shell/rollback walkthrough passed on `0fabc48`. Its installer script and guide are unchanged in the final candidate; this is supporting evidence, not an exact-final visible Finish pass. |

Native ConPTY results prove real input delivery and saved bytes, not physical
terminal pixels/fonts or host-shortcut behavior. Adrian's physical Windows
smoke test remains open. The legacy in-place upgrade was not attempted against
the real signed installation folder. A signed final installer still needs its
own verification after signing; the current EXE is unsigned.

The final baseline was reverified: signed THE v4.0.0-panels.2 remains the
default, all 157 old payload hashes match, raw user PATH/value type and product
and uninstall registry keys match the snapshot, and the old shortcut is
restored. No candidate shortcuts or processes remain. New-shell resolution
selects installed `bin\the.cmd` before `.local\bin\the.exe`. User-requested
Regina 3.9.7 x64 remains installed; standalone cREXX settings/cache and the
private macro kit are preserved. The Windows source worktree is clean at the
tested revision, with no Windows product-source changes.

Evidence and the consolidated report are at
`C:\Users\adria\CRXTHE-windows-qa\90fa7fd\QA-report.txt`, with JSON/logs in
`results`. `PREVIEW-README.txt` in that folder describes four double-click
launchers for isolated cREXX console, Regina console, browser and directory
previews. They use scratch files and do not change global defaults. The agent
closed its own IAB tab/server and native preview processes.

No tag, signing, release publication or external Shmuel message has occurred.
Shmuel's private kit is excluded from the candidate; this platform/UI matrix
does not claim upstream THE or live XEDIT equivalence.

### Separate demo and narrow-browser observations

The actual visible-browser walkthrough on `0fabc48` recorded three additional
UI/demo items for triage. These are separate from the repaired console blocker:

- `paneldemo` clears the stock reserved key footer (`SET RESERVED * OFF`);
  its RESTORE did not bring that footer back.
- That demo returned file focus at line 0/cell 1 from prior line 1/cell 1,
  requiring navigation before continuing on the original record.
- At a narrow 455-pixel IAB viewport, the long footer clips later hints,
  including F10. F10 itself works and resize checks pass at 900/1200 pixels.

No source fixes for these observations were included in the console-mode
repair. They do not establish data corruption or a general panel-input failure.

The optional F8/F9 compile/run helpers retain their prior unresolved reports
and are unbound in the stock profile. This matrix does not claim they were
repaired; see the historical [Windows findings](windows-installation-qa.md).

### Further installer improvements to consider

The current launcher validates installed-runtime compatibility and bundled
fallback, and the guide explains stale paths. These are implemented and tested.
The following are additional setup/diagnostic improvements:

- Report the effective executable/version/resource paths and warn about an old
  `.local` command or stale `THE_BIN`/developer `THE_*` overrides, preserving
  intentional customizations.
- When installed cREXX is selected, show the discovered path, compatibility
  result and effective bundled fallback. Setup currently records the preference;
  the launcher performs the validated selection.
- Make absent optional Regina and skipped checks explicit, with x64 setup and
  detection guidance.
- Retain spaced/long-PATH, reinstall/uninstall and user-file tests, and add
  legacy in-place upgrade cleanup qualification before claiming that behavior.
