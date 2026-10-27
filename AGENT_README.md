# Developing CRXTHE

CRXTHE is an independent C99 fork of THE 4.0 for macOS, Linux/POSIX and
native Windows. Start with the [documentation index](doc/README.md),
[architecture](doc/architecture.md) and [build/test guide](doc/testing.md).
Use `main` for development and integration; [AGENTS.md](AGENTS.md) defines
branch preservation, publication and release-source requirements.
Current defects belong in [known issues](doc/known-issues.md); future work and
qualification belong in the [roadmap](doc/roadmap.md).

## Engineering contracts

- The editor core owns file bytes, logical cursor/focus, commands, selection,
  parser state and macro integration. Physical UI behavior belongs in the
  runtime-loaded driver behind `TheDriverOps`.
- Use `the --driver llm` for agent/editor tests. It runs the real editor and
  accepts normalized inputs and semantic snapshots. Do not infer editor state
  by scraping curses output.
- Mutation tests must assert text, cursor, dirty state and saved/reopened
  bytes where relevant. Use a real PTY for changes to console input handling.
- Keep parser-backed reads inside the DSLSH critical-section helpers. Wait
  with `SDSLHWAIT` before asserting asynchronous styles or diagnostics.
- cREXX is the default engine; Regina is optional and selected at startup.
  Keep compiler, assembler, shared runtime and bytecode from a matching cohort.
  Consecutive cREXX addressed command literals need explicit semicolons.
- Default profiles are generated from `profiles/default-profile.json` by
  `scripts/generate-default-profiles.py`. Update the shared definition and
  verify generated profiles with `--check`.
- Terminal-specific Unicode behavior belongs in system profiles, not hidden
  renderer branches. The Linux profile is conservative because SSH does not
  reliably identify the client's terminal.
- Shmuel's prefix workflows and their ports are private and excluded from
  public packages. Generic macro and panel tests use public probe fixtures.

## Build dependencies

CMake discovers a matching cREXX SDK from configured paths, sibling build
outputs or an installation. Debug and Release builds should select matching
SDK outputs; review the CMake cache when changing cohorts. Clear stale
`CREXXSAA_LIBRARY`, `CREXX_RXC_EXECUTABLE`, `CREXX_RXAS_EXECUTABLE` and
`CREXX_LIBRARY_RXBIN` entries if they pin an unintended build.

Use available build parallelism, then run the affected CTest selection and
complete candidate suite. Do not rebuild over running tests. Native installer,
visual UI and signed-package checks are separate from source qualification.

## Documentation

Describe implemented behavior for users or developers. Keep proposals in the
roadmap and unresolved limitations in known issues. Use Git and CI for change
history and run receipts; do not add session journals or dated progress packs.
