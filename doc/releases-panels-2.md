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
incompatible fallback, and NONE/REGINA isolation. Native Windows and signed
macOS installation qualification must be recorded after the exact candidate
finishes hosted QA. Manual Windows interaction and the Shmuel handoff remain
pending.
