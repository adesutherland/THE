# Release signing

These tools operate on the payload of an explicit published tag. They do not
build the source at the tip of `main`, move tags or merge the experimental
fork. All original asset digests and the tag commit are pinned and checked
throughout signing. The private bundled cREXX installation stays separate
from any standalone cREXX installation. Bundled cREXX remains the default;
installers offer an installed-runtime preference with automatic bundled fallback.

## Windows

```sh
release_tag=vX.Y.Z  # replace with the explicitly selected published tag
python3 scripts/sign-windows-release.py --tag "$release_tag" --dry-run
python3 scripts/sign-windows-release.py --tag "$release_tag" \
  --signer ../CREXX/scripts/sign-windows-file.sh --upload
```

The helper signs the existing outer NSIS installer, verifies its signature and
timestamp, updates Windows checksums and optionally replaces those two release
assets. The ZIP and internal EXE/DLL signatures are unchanged. Alternatively,
set local `PROVIDER`, `CERTUM_ALIAS` and optional `TSA_URL` for `jsign`.
Credentials and provider files are never committed.

## macOS

The manually dispatched `sign-release-installers.yml` workflow first verifies
the published Windows installer's Authenticode signature and timestamp, then
tests installation, payload hashes, CREXX/NONE modes, browser assets,
reinstallation, uninstallation, PATH restoration and user-file preservation.
The Windows installer must already have been signed locally with SimplySign.

The macOS job uses the Apple secrets listed in [packaging](packaging.md).
It validates the existing release ZIP and every input manifest hash; signs
all nested Mach-O executables and libraries with Developer ID Application;
removes staged Finder `.DS_Store` metadata (which `pkgbuild` excludes),
refreshes manifest hashes; and smoke-tests the signed payload. It builds a
Developer ID Installer PKG from that exact signed tree, submits both ZIP and
PKG to Apple, requires `Accepted`, staples and validates the PKG, and checks
Gatekeeper. It installs and tests the signed PKG on its disposable runner
before replacing the release's three macOS assets. ZIP files cannot carry
stapled tickets; the ZIP's notarization is checked by Apple's online service.

```sh
gh workflow run sign-release-installers.yml --repo adesutherland/THE \
  --ref main -f tag="$release_tag" -f architecture=arm64
```

The selected architecture must exist on the selected release. Native visual
checks and any external package handoff are separate activities.

Signing uses a temporary private keychain and restores the user's keychain
search list. Certificate files and the keychain are removed on exit. Public
input snapshots and credential-free receipts live under `build/sign-macos/`
and `build/sign-windows/`. Only receipts are uploaded as CI artifacts.

Uploads are staged and verified before canonical names change. Original
assets are retained under temporary `.backup-<id>` names until final readback
verification succeeds, then removed. GitHub has no atomic multi-asset
replacement: an interruption can leave a temporary missing name or checksum
mismatch. Retain the reported `.backup-<id>` / `.sign-<id>` assets and local
originals for recovery. Release tags and other platforms are preserved.
