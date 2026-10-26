# Controlled CRXTHE 0.9.0 pre-release

Base: `1b345d801ff9de5ab79ca863ac520ee9826066d2`.
Adrian requested a fresh, controlled release after clean Mac packaging exposed
gaps in the earlier development-build qualification. The final tag will include
the release repairs below, rather than relabel the failing package as qualified.
No tag or release was created during the abandoned attempt. Its locally signed
Windows installer is not the final candidate.

## Intended outcome

Publish one GitHub pre-release containing signed Windows x64 and signed,
notarized macOS ARM64 installers, portable archives and matching checksums.
Build both from the same frozen source commit with recorded dependency inputs.
Keep the release private as a draft until signatures and native installed tests
pass. Preserve existing installations and all unrelated work. Do not contact or
hand material to Shmuel before Adrian's own Windows smoke test.

## Acceptance criteria

- AC-01: Reproduce and repair the optimized Mac relocated-payload failure.
  The owned debugger trace currently shows stack exhaustion during snapshot
  rendering. Add a functional regression with a normal bounded stack; verify
  macro execution, snapshots, delta and subsequent editing.
- AC-02: Encode the Mac Release build, dependency SHA-256 checks, SDK symlink
  handling, package manifest and relocated-payload tests in a checked-in hosted
  workflow. Use the released cREXX beta.3 cohort and pinned DSLSH commit, without
  dependencies on Adrian's installed tools or developer environment.
- AC-03: The same frozen commit passes both hosted candidate workflows, full
  applicable regression suites and native installed/lifecycle checks. Report
  executed passes and skips separately. Have the Windows agent retest the new
  artifact before tagging.
- AC-04: Explicit draft support in signing tools preserves default rejection of
  drafts and immutable-release rejection. Tests cover draft opt-in and refusal
  when release identity/assets change. Keep the release a draft until complete.
- AC-05: Tag only the frozen green commit. Verify candidate provenance/hashes,
  sign Windows with a trusted timestamp, verify the signed installer on a runner;
  sign every Mac code file, notarize ZIP/PKG, staple/validate PKG, check Gatekeeper
  and install/runtime-choice tests on a disposable runner. Verify promoted assets
  and checksums by downloading them before publishing the draft.
- AC-06: Publish accurate release notes with source, platform/architecture,
  signing/test results and remaining manual Windows qualification. Private
  compatibility macros remain excluded. Shmuel handoff remains held.

## Steps and status

1. STEP-01 (AC-01): Confirm the minimal stack reproducer, move oversized protocol
   snapshots off the call stack, and run the focused regression. LOCAL PASS:
   old Release binary fails with SIGSEGV; repaired snapshots/delta/edit/save
   pass with a 4 MiB stack. Full Release suite passes 80/80 and relocated
   CREXX/NONE/Regina/browser/runtime-choice/cache checks pass.
2. STEP-02 (AC-02, AC-04): Add the reproducible Mac candidate workflow and tested
   draft-signing support. IMPLEMENTED: workflows parse; 14 Windows-signing and
   9 Mac-signing tool regressions pass. Hosted confirmation remains open.
3. STEP-03 (AC-01..04): Run local affected checks, commit and push the repairs;
   freeze that SHA and require successful Mac/Windows candidate runs. Windows
   agent confirms the matching artifact. OPEN. First hosted Mac run at
   `c55b396` passed the stack regression but failed two qualification checks:
   the optional parser was absent from PATH, and the native save poll read
   during destination replacement. The gate remains closed while SDK PATH
   setup and polling readiness are corrected; assertions are retained.
   Optional web-profile parser tests now check the same PATH availability as
   the profile, instead of assuming that files in a developer's `~/.local/bin`
   are active. Missing optional Markdown/Python parsers remain explicit skips;
   the provided cREXX parser must run and pass.
   Local hosted-environment replay passes native keys, provided cREXX styles
   and rendered browser checks; the absent optional Markdown/Python combination
   is correctly skipped. Await both replacement hosted runs before tagging.
4. STEP-04 (AC-05): Create the exact source tag and private draft, sign staged
   Windows assets, dispatch native signed-install verification and Apple
   signing/notarization against the draft. OPEN.
5. STEP-05 (AC-05, AC-06): Read back signatures, manifest/source and checksums;
   finalize notes, publish the pre-release and record the release URL and test
   limits. No Shmuel communication. OPEN.

The new packaged-build checks are required because the first attempt reproduced
a failure absent from development-mode tests. Reuse valid evidence for unchanged
inputs; do not repeat broad suites merely for documentation or promotion steps.
This source plan records the gates before freezing the candidate. The eventual
release notes and signing receipts record the frozen SHA, hosted outcomes and
final publication without moving its tag for a documentation-only update.
