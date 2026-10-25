# Shmuel macro qualification — 24 October 2026

The sections dated 24 October retain the unchanged-package observations.
The current delivery is summarized in [the staged report](macro-panel-stages.md),
with [patched-original setup](../compat/shmuel/README.md),
[compiled Level G ports](pfxrexx-levelg.md) and [macro panels](panels.md).

This report concerns the experimental THE fork. There was no live XEDIT test
and no upstream THE qualification. Installer and hosted Windows results must
be assessed separately from the local macro results below.

## Source distinction

The older scanned XEDIT listing attached to THE request 76 is different from
the current [PFXREXX THE macro](https://mason.gmu.edu/~smetz3/source/pfxrexx.the)
and [PFXHLASM THE macro](https://mason.gmu.edu/~smetz3/source/pfxhlasm.the).
The current PFXREXX already has a THE calling-convention branch and local
indentation defaults: CMS GLOBALV is not a blocker for that branch. Earlier
findings about the scanned XEDIT listing must not be applied to it wholesale.

The downloaded THE files were tested unchanged with Regina on macOS ARM64
(fork commit `389bd1e`),
using their documented prefix aliases, XEDIT compatibility, mixed-case input,
a 12-column prefix area, and a two-line scratch buffer. Download hashes:

| File | SHA-256 |
|---|---|
| pfxrexx.the | d6915e9f470cda917ac29004a402b5faab787fb73f513f1b1adc763f1b0dc654 |
| pfxhlasm.the | 95abb6790df0b5ccfc302d1792f9f282f3406e5cfc6302e9461c81df493ac4b0 |

## Unchanged package observations

| Test | Observed result |
|---|---|
| PFXREXX DO, IF, SEL, WHEN | Templates inserted without interpreter errors |
| DO WHILE | Condition template inserted |
| RI | Placeholder and comment field inserted |
| CU / CL, whole line | Case conversion worked |
| DO followed by three processing passes | 5, then 6, then 7 file lines; untouched RIC placeholders continued to grow |
| DO0 | Invalid count reached an unresolved cursor target instead of a useful error |
| PFXHLASM DO | EXTRACT rejected TRUNC; execution then failed on uninitialized Indent arithmetic |
| PFXREXX under cREXX Level B | Classic source did not compile |

These observations do not qualify every operand or HLASM command. PFXHLASM
also enables interactive tracing in its distributed source. Its initialization,
TRUNC dependency and trace setting need attention before a useful full test.
The initial DO cursor column is the one requested by the macro; whether to
move it onto the body dot is a workflow choice to discuss with Shmuel.

## Fork changes and adaptation

The shared core now fixes alias lookup through OLDNAME, defers newly queued or
replaced work in XEDIT passes, handles cancellation/reset/deletion safely, stops
a pass on macro failure, implements PENDING ERROR, and clears visible text on
PENDING OFF. Native THE pass timing is retained.

MASK, the FLSCREEN visible-range query, and cursor priority arbitration are
implemented in shared editor state. MASK handles UTF-8 cell boundaries and
PRESERVE/RESTORE. Cursor requests follow surviving line objects through edits.
The browser exposes the corresponding safe editor commands; its existing
restrictions on arbitrary macros and OS commands remain in force.

The separately authored `pfxrexx.the`/`pfxsetup.the` adaptation provides a smaller
Regina template workflow with EDITV configuration. Tests cover its actual
text-entry/Enter/save cycle in LLM and curses, plus template variants, counts,
cleanup and errors. This is not the unchanged package and does not implement
its complete command set. See [the adaptation guide](pfxrexx.md).

Both engine bridges load the selected runtime explicitly at startup. The
standard bundle provides cREXX; users install/configure Regina separately.
The native macOS archive was relocated into a directory containing spaces and
tested with an isolated PATH in CREXX, NONE and REGINA modes. A GitHub Windows
candidate workflow builds, tests, packages, installs and uninstalls separately;
its successful run and artifact identify the Windows candidate for manual QA.

## Qualification and handoff

The native macOS ARM64 regression run passed all 63 tests. The browser build
passed its four focused tests, including real Chrome interaction and web
transport checks with both engines. A Regina-only build and a no-macro build
were also exercised. The portable macOS archive was extracted with `ditto`
before its installed-payload checks; no permission repair or developer PATH
was needed.

Windows qualification uses a native Release build on GitHub's Windows 2025
x64 runner. It tests cREXX and separately supplied Regina, builds the ZIP and
NSIS installer, then installs into a directory containing spaces. The installed
test uses an isolated PATH, compiles a real cREXX macro, and runs the Regina
template adaptation. Reinstall/uninstall checks cover user-file retention,
PATH restoration and removal of the uninstall registry entry.

The [successful Windows run](https://github.com/adesutherland/THE/actions/runs/40113064730)
qualified commit `4cb285569b4eddfe904fb145d096fb30c4946962` on branch
`temp/shmuel-macros`:

| Check | Result |
|---|---|
| Native Release build | Passed |
| Windows child-process argument round trip | Passed |
| CTest | 51 passed, 11 skipped, none failed |
| Installed cREXX / NONE / Regina, isolated PATH | All passed |
| Install into a path containing spaces | Passed |
| Reinstall and uninstall | Passed |
| User file retained, PATH restored, uninstall key removed | Passed |

The skipped tests require unavailable Unix terminal tools or test-parser
executables. The macOS curses workflow test is not registered on Windows.
The hosted run therefore does not qualify interactive Windows curses behavior.

[Download the Windows candidate and results](https://github.com/adesutherland/THE/actions/runs/40113064730/artifacts/12567876348).
The artifact contains `THE-4.0.0-ci.9-windows-x86_64-setup.exe`, a portable ZIP,
SHA-256 checksums and test logs. The downloaded archive and installer checksums,
and all 140 payload manifest hashes, were verified. Its manifest records the
tested source commit, a clean source tree and `regina_bundled: false`.

The installed test exposed a cREXX beta.3 runtime bug: unquoted `_spawnvp`
arguments split paths containing spaces. The Windows package rebuilds the
matching host runtime with an explicit argument-quoting correction. Its native
child-process round-trip test includes spaces, an empty argument, quotes and
trailing backslashes. This is a pinned packaging patch, not a released cREXX
fix; see [packaging provenance](../packaging/README.md).

Adrian's later test on a Windows machine remains necessary for interactive
terminal behavior, cursor placement, prefix entry and the complete user
installation experience. The CI artifact is an unsigned candidate and has not
been published as a release. No upstream THE or live XEDIT compatibility is
claimed.

The next macro-package work to agree with Shmuel is RIC's mask initialization
and repeat behavior, count validation, and PFXHLASM's TRUNC/Indent/trace
dependencies. The smaller adaptation already has bounded count validation and
cleanup, but replacing his entire package has not been attempted. A cREXX
version would be a separate port; users can keep Regina for Classic macros.

## Stage 1 development — 25 October 2026

Adrian authorized all three delivery stages and selected minimal patches
against unchanged original downloads. The staged work and its handoff hold
are recorded in [the delivery plan](macro-panel-stages.md). Nothing is to be
sent to Shmuel until Adrian completes his Windows smoke test.

The compatibility kit now repairs the observed macro defects and supplies
the documented HLASM aliases that had no implementation. The original source
hashes above remain the required patch inputs. See the
[patch inventory and setup](../compat/shmuel/README.md).
Local QA currently passes 86 behavioral workflows against the patched files,
all 57 original aliases, the real curses text-entry/save test, and focused
TRUNC and patch-application tests. These results supersede the earlier
unchanged-package failures only when the documented patches are applied.
Stage 1 was subsequently qualified on Windows run `40177173979` (54 passed,
11 unavailable-tool skips) and published as
[v4.0.0-regina.1](https://github.com/adesutherland/THE/releases/tag/v4.0.0-regina.1).

Stage 2 was published as
[v4.0.0-levelg.1](https://github.com/adesutherland/THE/releases/tag/v4.0.0-levelg.1)
after 70/70 macOS tests and Windows run `40180543859` (56 passed, 11 skips).
The expanded suite runs 121 patched-original Regina workflows and 121 compiled
Level G workflows, with exact text, cursor, pending-command and error parity.
It covers both macro packages and all 57 original aliases. Two additional
numeric-Boolean defects found in label/CASENTRY/IF paths are included in the
minimal patch set. No original download was changed or bundled.

Stage 3 adds shared READ panel fields for curses, browser and LLM input, with
ALL/NOCHANGE, protection, navigation, submission keys and tagged results. The
local suite passes 80/80, including real curses and Chromium interaction;
the installed macOS payload passes both macro suites and 35 panel scenarios
across Regina and cREXX. Final Windows qualification and panel pre-release
publication follow separately. The no-handoff hold remains until Adrian's
manual Windows test, including cursor and ordinary editing behavior.
