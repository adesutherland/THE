# v4.0.0-panels.2 runtime and installer update

This candidate retains the panel and macro features of panels.1 and adds a
private per-user THE cREXX cache with explicit overrides. Both native
installers offer bundled cREXX (default) or a compatible installed cREXX, with
automatic fallback. The bundle remains available in every installation.

Selection checks a complete tool cohort, required hosting API and an actual
compile/run probe in a separate process. It does not rewrite or upgrade a
standalone installation. Mac hosting binaries allow user-selected runtimes
with different signing identities while retaining hardened runtime and
Developer ID signing. The bundled VM executable is staged as a real file,
avoiding a symlink to a build-only target.

Local macOS source qualification passed 84/84 tests. Runtime/cache checks
cover the default, detection by PATH and explicit home, saved preference,
environment overrides, real macro cache reuse, toolchain changes, absent and
incompatible fallback, and NONE/REGINA isolation.

Windows [run 40223307643](https://github.com/adesutherland/THE/actions/runs/40223307643)
passed 67 tests with 13 unavailable-tool/platform skips (80 total). Installed
checks passed both 121-workflow macro suites, exact parity and both runtime
preferences, alongside cache tests, reinstall/uninstall and PATH restoration.

[Signed-installer run 40224957673](https://github.com/adesutherland/THE/actions/runs/40224957673)
passed Windows Authenticode/timestamp and installation checks. Both ARM64
macOS assets were accepted for notarization; the PKG passed staple, Gatekeeper,
installation and native Installer choice tests. The signed host and launcher
were tested with a separately signed cREXX copy. The Library Validation
entitlement enables that explicit external-runtime choice. All ten Mac code
files are signed, with 130 manifest entries; Windows has 157 entries and its
portable ZIP was unchanged by installer signing. Checksums and the immutable
source tag `091aef7b08a2674f6ae3dfa4235ee96e7a19a09c` were verified.

The [published prerelease](https://github.com/adesutherland/THE/releases/tag/v4.0.0-panels.2)
contains the signed installers. Manual Windows interaction and the Shmuel
handoff remain pending.
