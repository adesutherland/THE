Windows end-user instructions: [install and first use](../doc/Windows-Readme.txt).
The build/signing instructions below are for maintainers.

# Prebuilt CRXTHE candidates

The bundled install contains CRXTHE, its curses and LLM drivers, profiles and
help, the Classic PFXREXX adaptation, compiled Level G Rexx/HLASM prefix
workflows and a private matching cREXX tool set. Shmuel's original macros, patches and preparation tool are excluded; their compatibility kit is held separately for private review.
Regina is optional, loaded only when selected, and is never bundled.
See [interpreter setup](../doc/rexx-interpreters.md).
The panel candidate enables the experimental browser driver as well as curses
and LLM input. Run `the --driver web file` to open its local URL. Shared macro
forms and their qualification limits are described in [panels](../doc/panels.md).

Build maintainers need CMake, the normal THE dependencies, cREXX ABI 3+
headers/shared runtime/tools, and Regina's `rexxsaa.h` for optional Regina
support. Users of the resulting installers need none of those build tools.

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release \
  -DTHE_REXX_INTERPRETER=CREXX -DTHE_BUNDLE_CREXX=ON \
  -DREGINA_INCLUDE_DIR=/path/to/regina/include
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure
python3 scripts/package-release.py --build-dir build-release \
  --version 0.9.0-preview.1 --installer
```

The version above is a local candidate label, not a release declaration.
Package names begin `CRXTHE-0.9.0-...`. The display version and manifest identify the independently maintained fork and its THE 4.0 ancestry. The stable `the` launcher, install/cache directories, registry/package identifiers and `THE_*` configuration keys are retained for compatibility; `crxthe` is the new public launcher alias. Upgrade behavior still needs the next candidate's native installer QA.

`CREXXSAA_RUNTIME_LIBRARY`, `CREXX_RXC_EXECUTABLE`, `CREXX_RXAS_EXECUTABLE`,
`CREXX_RXVM_EXECUTABLE`, `CREXX_DRIVER_EXECUTABLE`, `CREXX_LIBRARY_RXBIN` and
`CREXX_LICENSE_FILE` can override dependency discovery. Keep them from one
qualified cREXX distribution. On Windows, supply the actual DLL for
`CREXXSAA_RUNTIME_LIBRARY`, not its import library. Include any non-system DLL
dependencies of that cREXX build in its private runtime directory before
packaging. Do not mix CPU architectures.

The script creates `build-release/packages/` with a portable ZIP, SHA-256
checksums, and, with `--installer`, a native installer:

- macOS: Apple's `pkgbuild` and `productbuild`, installing under
  `/usr/local/the-crexx` with `/usr/local/bin/crxthe` and `/usr/local/bin/the`
  pointing to its launchers.
- Windows x64: NSIS `makensis`, per-user under
  `%LOCALAPPDATA%\Programs\THE-cREXX`. The Start menu CRXTHE shortcut starts in Documents;
  new terminals can run `crxthe`. Getting Started is available from the finish
  page and Start menu. The development `the-env.ps1` is not included. The PATH helper follows cREXX-RAG's ownership
  pattern, preserves long registry PATH values and removes only its own entry.
- Linux: use the portable ZIP (omit `--installer`).

Every payload includes a file-hash manifest, source commit and dirty-state
flag. Uninstall on Windows removes only the packaged files, leaving files the
user adds. macOS has no uninstaller; remove the package directory and its
command symlink only when they still belong to this installation.

## Existing cREXX installations

All THE installers and portable ZIPs carry a private, matching cREXX tool set
under `libexec/the/crexx`. The default is to use that bundle. Windows offers a
runtime-choice page; macOS offers an optional "Use an installed cREXX when
available" choice. Both retain the bundle for automatic fallback. A saved
installed-runtime preference is rechecked when THE starts, using a complete
tool cohort and a separate compilation/runtime probe. Missing or incompatible
copies fall back without a question. The selected tools are prepended to PATH
for THE's child processes only; standalone installations are never upgraded.
Windows adds THE's `bin` directory to the user's PATH, not its private cREXX
directory; macOS installs the public `crxthe` and compatible `the` symlinks. A standalone cREXX
installation stays separate. Windows detects an earlier **THE** installation
through `HKCU\Software\THE-cREXX`. Standalone cREXX detection separately uses
`CREXX_HOME`, its own installation registry, PATH and standard locations.
Advanced users can explicitly override the tool/runtime paths as described in
[interpreter setup](../doc/rexx-interpreters.md). Keep the runtime, compiler,
assembler and bytecode from one compatible distribution.

Silent Windows installations accept `/CREXX=BUNDLED` (default) or
`/CREXX=INSTALLED`, before the final `/D=...` argument. macOS's corresponding
Installer choice identifier is `installed-crexx`, initially unselected.
The preference file is installer-generated `share/the/runtime-preference`,
outside the immutable payload manifest; reinstalling with the default restores
bundled mode. Portable archives have no saved preference.

THE's cREXX cache now defaults to a THE-specific per-user directory on every
platform. Explicit `THE_CREXX_CACHE_DIR` and `CREXXSAA_CACHE_DIR` settings are
preserved, with the generic setting retaining precedence. See
[runtime and cache configuration](../doc/rexx-interpreters.md).

## Windows installer signing and replacement

`scripts/sign-windows-release.py` signs the **existing outer installer** from
an explicit published release tag. It never rebuilds THE, signs the internal
EXEs/DLLs or changes the portable ZIP. Python 3.9+, GitHub CLI, and
`osslsigncode` are required. For signing, use `jsign` with local `PROVIDER`,
`CERTUM_ALIAS` and optional `TSA_URL` settings, or pass an existing single-file
signing helper. No provider configuration or credentials belong in this repo.

```sh
# Read-only plan: no signer access or downloads.
python3 scripts/sign-windows-release.py --tag v4.0.0-panels.2 --dry-run

# Reuse the established local cREXX signer; sign locally first.
python3 scripts/sign-windows-release.py --tag v4.0.0-panels.2 \
  --signer ../CREXX/scripts/sign-windows-file.sh

# Sign and replace the installer + Windows checksum file on this exact release.
python3 scripts/sign-windows-release.py --tag v4.0.0-panels.2 \
  --signer ../CREXX/scripts/sign-windows-file.sh --upload
```

The script pins the release/tag and asset digests, checks the published
checksums, requires signature and timestamp verification, and retains original
downloads and a credential-free receipt under `build/sign-windows/`. Uploads
are staged and verified before promotion. Original remote assets are renamed
to temporary backups and removed only after signed readback succeeds. GitHub
cannot replace two assets atomically: an interrupted promotion can temporarily
leave a canonical name missing or its checksum out of sync. On failure, retain
the `.backup-<id>` / `.sign-<id>` assets for recovery, using the local originals
and reported invocation ID. Other release assets and the tag stay unchanged.
The panel candidate's outer Windows installer was signed, timestamp-verified,
and replaced on 25 October 2026. Its internal payload and portable ZIP were
preserved. Native Windows verification is part of the manually dispatched
signing workflow described below.

## Apple signing status

`scripts/package-release.py` creates an unsigned PKG and ZIP. The separate
`scripts/sign-macos-release.py` and manual `sign-release-installers.yml`
workflow sign the payload from an existing explicit release tag. They use
Developer ID Application for all nested Mach-O executables and libraries,
refresh manifest hashes, test the signed payload, sign the PKG with Developer
ID Installer, submit both assets for notarization, staple and validate the
PKG, check Gatekeeper, and test installation before replacing release assets.
See [release signing](../doc/release-signing.md) for commands and limits.

The panels.1 candidate completed this process successfully in
[run 40219171572](https://github.com/adesutherland/THE/actions/runs/40219171572),
including native signed-installation QA on both Windows and macOS. The macOS
signed manifest has 128 entries after excluding two Finder metadata files.

The current panels.2 candidate passed source Windows run `40223307643` and
[signed-installer run 40224957673](https://github.com/adesutherland/THE/actions/runs/40224957673)
on both systems. Native runtime/cache tests, macOS Installer choice selection,
fallback, signatures and notarization passed. Its Mac manifest has 130 entries
including the new launcher and resolved VM executable. See the
[runtime update](../doc/releases-panels-2.md).

All nine required Apple secrets were configured in `adesutherland/THE` on
25 October 2026. The separate Apple secret-validation workflow exercised both
certificates and Apple account credentials successfully. Required names:

```text
APPLE_DEVELOPER_ID_CERTIFICATE_BASE64
APPLE_DEVELOPER_ID_CERTIFICATE_PASSWORD
APPLE_DEVELOPER_ID_IDENTITY
APPLE_DEVELOPER_ID_INSTALLER_CERTIFICATE_BASE64
APPLE_DEVELOPER_ID_INSTALLER_CERTIFICATE_PASSWORD
APPLE_DEVELOPER_ID_INSTALLER_IDENTITY
APPLE_ID
APPLE_APP_SPECIFIC_PASSWORD
APPLE_TEAM_ID
```

`scripts/package-release.py` stages and packages only. It does not install, sign, notarize or
publish. A production release still needs dependency/license inventory,
platform-native install/upgrade/uninstall tests, signing and macOS notarization
as appropriate. These recipes do not establish that published installers or
Windows/Intel qualification already exist.

The Windows candidate workflow uses a GitHub Windows 2025 x64 runner. It pins
the signed cREXX 1.0.0-beta.3 SDK and matching ABI header by SHA-256, rebuilds
its parser-enabled compiler, assembler and host runtime from the same release source, downloads
Regina 3.9.6 separately for testing, runs CTest, builds NSIS, and tests the
installed payload with a restricted PATH. It also tests reinstall, uninstall,
PATH restoration and retention of a user-created file. Its artifact is a
candidate for manual Windows testing; the workflow does not publish a release.

The rebuilt Windows host runtime includes the narrowly pinned correction in
`scripts/prepare-crexx-windows.py` and `windows/crexxsaa-spawn.h`. The beta.3
runtime passes unquoted arguments to `_spawnvp`, which splits paths containing
spaces. The correction quotes each argument for the child CRT, and a native
round-trip test covers spaces, empty arguments, quotes and trailing backslashes.
Only the CI dependency checkout is patched; the sibling cREXX checkout is not
modified. This packaging patch still needs incorporation into a cREXX release.
