# Windows installation findings and next-release checks

Recorded 26 October 2026 from the Windows chat **Use and test installed THE
version**, at Adrian's explicit request to feed back installation improvements
and other findings. These are that chat's workstation results, not Windows
tests rerun on macOS. Recording them does not mark the proposed improvements
implemented or authorize publication or a Shmuel handoff.

The tested installation is signed `v4.0.0-panels.2`, source
`091aef7b08a2674f6ae3dfa4235ee96e7a19a09c`. The initial installation was actually
`4.0.0-regina.1` / `99e43c2e`, despite Adrian believing it was current.

## Triage

| Item | Finding | Next-release acceptance |
|---|---|---|
| WIN-01: command resolution | An older `.local/bin/the.exe` preceded installed `bin/the.cmd`; appending the new directory did not make the new editor the default. | Check effective resolution in PowerShell and cmd, including aliases/shims and EXE/CMD precedence. Identify conflicting THE commands and offer a reversible choice to prefer the installed launcher. Preserve unrelated `.local/bin` tools. |
| WIN-02: stale overrides | Persistent THE resource/runtime paths redirected the installed launcher to the old development installation. | Diagnose effective resource and runtime paths, identify stale overrides separately from intentional settings, and offer backed-up cleanup or a clean installed-launch option. Investigate the crash correlation below without assuming its cause. |
| WIN-03: installation identity | The apparent upgrade was an older downloaded/installed package. | Show version/source, prefix, actual launcher/executable, selected drivers/profile, runtime library/tool cohort and cache in a post-install self-check. Verify from a neutral working directory. |
| WIN-04: optional Regina | Regina was initially absent; the original-macro path could not be tested until it was installed explicitly. | Detect a same-architecture Regina, report absence clearly, and provide setup guidance or an explicit library-path choice. Keep bundled cREXX as the default and Regina separately installed. |
| WIN-05: F8/F9 | The helpers failed even though THE exited successfully. | Repair the observed command/path corruption and test reported macro status, compilation artifacts and actual program output, rather than only THE's exit status. |
| WIN-06: corrected panel cursor | Cursor correction `b3263df` was made after the tested release. | Build the correction on Windows; rerun both engines' macro/panel/native-input tests and explicitly assess cursor visibility, Unicode/field boundaries, clicks and restored ordinary editing. |

WIN-05 and WIN-06 need resolution/qualification before claiming that the next
candidate fixes the reported compile/run and cursor problems. WIN-01 through
WIN-03 are the requested installer improvement work; a PATH-only change is
insufficient. WIN-04 is a setup improvement, not a request to bundle Regina.

## Environment and recovery details

The confirmed stale user settings were `THE_HOME_DIR`, `THE_DRIVER_PATH`,
`THE_HELP_FILE`, `THE_MACRO_PATH`, `THE_BIN`, and `THE_CREXX_RXC`,
`THE_CREXX_RXAS`, `THE_CREXX_IMPORT_DIR`, `THE_CREXX_LOCATION` and
`THE_CREXX_LIBRARY_RXBIN`. An audit should also consider `THE_PROFILE_FILE`
and `THE_CREXX_LIBRARY`. Preserve intentional explicit library and cache
settings, including the documented `CREXXSAA_CACHE_DIR` precedence.

One invocation inheriting the stale environment exited `0xC0000005`; clean
installed-launcher tests passed. This is a correlation requiring investigation,
not an isolated crash root cause.

The local repair prepended only installed THE's `bin`, removed only confirmed
stale THE overrides, and retained old binaries, standalone CREXX/DSLSH settings,
the original environment and prior installed package for rollback. An installer
should broadcast environment changes and explain that already running
terminals, CLion and Codex need their environment refreshed. It must not claim
that a persistent PATH update changes existing processes. An absolute installed
launcher with a clean THE environment can provide an immediate test session.
Launching inside a checkout can still select its local `profile.the`.

## F8/F9 reproduction and workaround

With installed `profile_crexx.the` and a valid `hello.crexx`, F8/F9 invoked
`crexxcompile` / `crexxrun`, but `rxc` reported an input filename ending in
`hello.crexxmsg`. Messages included `CREXXCOMPILE rc -1` and `CREXXRUN rc -1`.
THE itself exited zero. No root cause has been established.

On Windows the bundled `crexx.exe -noexec` succeeded, and direct bundled
`rxc` → `rxas` → `rxvm` produced the expected greeting. The workstation has a
tested `compile-and-run.cmd` workaround. This differs from the Mac finding,
where the bundled standalone driver itself exits 127. The packages were not
modified to mask either failure.

## Regina installation and download validation

On Adrian's explicit request the Windows chat installed official Regina 3.9.7
x64 from its manual-install ZIP under `%LOCALAPPDATA%/Programs/ReginaREXX`,
appended its directory to user PATH, and set `THE_REGINA_LIBRARY` to the full
DLL path. Standalone version/script checks and the hosted tests passed.
Separate test shortcuts select Regina; THE still defaults to bundled cREXX.

Reported SHA-256 identifiers:

- `Regina397w64.zip`:
  `7cc7993bcbcabea57e38f17468ad94081503b4d1b1685964891567b093860add`
- `regina.dll`:
  `b3727061f8fe19934c9e2a04c1eb4f4372d147097374022a6e42cdd0f05e2f5b`

PowerShell download attempts returned SourceForge HTML instead of the ZIP.
`curl.exe -L` through the official master download mirror succeeded. Future
setup tooling must validate the expected archive/hash and reject HTML or invalid
archives before extracting or running anything. The manual-install procedure
is documented in the distribution's `READMEW64.txt`.

## Completed workstation qualification

- Installer signature and timestamp verified; all 157 installed payload hashes
  matched. Executable and loaded-module paths confirmed the installed editor,
  its drivers and selected runtime, rather than relying on the command name.
- Isolated-PATH installed checks passed for CREXX, REGINA, NONE and installed
  web assets. Runtime/cache checks passed bundled/installed selection, saved
  and environment overrides, missing/incompatible fallback, private cache
  miss/hit and toolchain changes.
- Actual patched originals with Regina 3.9.7 and bundled Level G cREXX each
  passed 121 workflows/all 57 aliases, with exact text, cursor, pending-state
  and error-marker parity.
- Both engines passed panel fields/key results, ALL/NOCHANGE, cancellation,
  Unicode, protection and full forms. Availability/invalid-library handling,
  MASK/FLSCREEN and cursor-priority checks passed.
- Default-profile UTF-8 editing, saving and reopening matched expected bytes.
  Native ConPTY tests passed cREXX and Regina DO/body entry, continuation
  cleanup and saved-text assertions. cREXX panel submission/cancellation
  followed by ordinary editing also matched expected saved bytes.

Evidence is retained on that workstation under
`%USERPROFILE%/THE-macro-tests`: `README.txt`,
`results/qualification.json`, `results/qualification.txt`,
`windows-features.json`, `native-conpty.json` and `regina-installation.json`.
`windows-features.json` records F8/F9 as failed despite THE's zero exit status.
Seven test shortcuts and the direct compile/run workaround are available.

These tests do not complete Adrian's visual cursor assessment, qualify
`b3263df` on Windows, or establish upstream THE/live XEDIT compatibility.
No lifecycle uninstall/reinstall test was run against his live installation;
the hosted lifecycle checks remain separate. The Windows checkout and published
packages were not changed, and no external handoff occurred.
