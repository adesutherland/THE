# Prebuilt THE candidates

The bundled install contains THE, its curses and LLM drivers, profiles and
help, the Classic PFXREXX adaptation, and a private matching cREXX tool set.
Regina is optional, loaded only when selected, and is never bundled.
See [interpreter setup](../doc/rexx-interpreters.md).

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
  --version 4.0.0-dev --installer
```

The version above is a local candidate label, not a release declaration.
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
  `/usr/local/the-crexx` with `/usr/local/bin/the` pointing to its launcher.
- Windows x64: NSIS `makensis`, per-user under
  `%LOCALAPPDATA%\Programs\THE-cREXX`. The Start menu shortcut launches THE;
  new terminals can run `the`. The PATH helper follows cREXX-RAG's ownership
  pattern, preserves long registry PATH values and removes only its own entry.
- Linux: use the portable ZIP (omit `--installer`).

Every payload includes a file-hash manifest, source commit and dirty-state
flag. Uninstall on Windows removes only the packaged files, leaving files the
user adds. macOS has no uninstaller; remove the package directory and its
command symlink only when they still belong to this installation.

The script stages and packages only. It does not install, sign, notarize or
publish. A production release still needs dependency/license inventory,
platform-native install/upgrade/uninstall tests, signing and macOS notarization
as appropriate. These recipes do not establish that published installers or
Windows/Intel qualification already exist.

The Windows candidate workflow uses a GitHub Windows 2025 x64 runner. It pins
the signed cREXX 1.0.0-beta.3 SDK and matching ABI header by SHA-256, rebuilds
its parser-enabled compiler from the same release source, and downloads
Regina 3.9.6 separately for testing, runs CTest, builds NSIS, and tests the
installed payload with a restricted PATH. It also tests reinstall, uninstall,
PATH restoration and retention of a user-created file. Its artifact is a
candidate for manual Windows testing; the workflow does not publish a release.
