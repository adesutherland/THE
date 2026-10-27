# CRXTHE 0.9.1 release

> Historical record recovered with the original commits. For current development
> status, use [known issues](known-issues.md), [testing](testing.md) and
> [release instructions](release-signing.md). The current source is `0.10.dev`.

Adrian approved full publication after his Windows smoke test. The integrated
product was also qualified with both engines on macOS and Windows, including
F7, retained tabs, Unicode status repainting and five syntax categories.
The final product candidate `6b8ce24` passed local 83/83 tests, hosted macOS
79 passes/four optional skips and Windows 69 passes/12 optional skips, plus
installed packages and the Windows agent's native/RGB checks.

Adrian then clarified that releases must follow cREXX-RAG's tag-driven pattern.
The workflow-only successor adds `.github/workflows/build-release.yml` and
reuses the same tested platform jobs; it adds no editor behaviour.

To release a version matching the source:

```sh
git tag -a v0.9.1 -m 'CRXTHE 0.9.1'
git push origin v0.9.1
```

The tag automatically builds both installers/ZIPs, runs normal regressions
once, verifies clean source manifests/checksums, creates its draft, signs and
notarizes macOS, tests installed packages, verifies final assets and publishes.
A tag containing a hyphen is a prerelease; v0.9.1 is a full/latest release.
No manual candidate dispatch or separate publication command is required.

As in cREXX-RAG, Windows SimplySign uses the maintainer's local authenticated
provider. It is a separate signing/replacement pass; Apple secrets stay in
GitHub. The publication notes report whether Windows is signed. For this
release, complete the local Windows signing pass and native signed-installer
verification too. Preserve the v0.9.0 tag/assets and send nothing to Shmuel.

The UI regressions are reusable checks for reported defects. Diagnostic
repeat runs are not release steps. Readiness barriers wait for a completed
terminal paint, completed browser navigation and an explicit successful
parser-profile marker; all original colour/cursor assertions remain.
