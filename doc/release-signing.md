# Signing CRXTHE release installers

## Tag-driven releases

Push a version tag matching the source to run
[Build and release CRXTHE](../.github/workflows/build-release.yml). It calls
the normal macOS/Windows build jobs, including installers and regressions,
then automatically verifies the packages, creates its draft, signs/notarizes
macOS, tests installed assets and publishes. Plain version tags are full
releases; tags containing a hyphen are prereleases. Main/PR platform builds
also create installer artifacts. No candidate workflow dispatch is needed.

This follows cREXX-RAG. Windows SimplySign remains local because its provider
and login are on the maintainer's signing machine. To sign and replace the
Windows installer after the automatic release:

```sh
python3 scripts/sign-windows-release.py --tag v0.9.1 \
  --signer /path/to/CREXX/scripts/sign-windows-file.sh --upload
gh workflow run sign-release-installers.yml --repo adesutherland/THE \
  --ref v0.9.1 -f tag=v0.9.1 -f windows_only=true
```

The second command validates the signed Windows installation on its runner;
it does not rebuild anything or re-sign macOS. The signing tool preserves
the Windows ZIP, other platforms and earlier releases. See the
[0.9.1 delivery plan](release-0.9.1-plan.md).

## Earlier controlled draft procedure

The following records the earlier manually coordinated prerelease process.
New tag releases use the automatic workflow above.

The [0.9.0 release plan](release-0.9.0-plan.md) records the source-build and
packaged-install gates. Both `macos-candidate.yml` and `windows-candidate.yml`
must pass at the same frozen commit before creating its release tag. Download
their exact artifacts and verify each clean manifest/source SHA and all hashes.
The Mac workflow uses a SHA-256-pinned cREXX beta.3 SDK and ABI header, preserves
the SDK's executable modes/symlink, builds Release for macOS 15+, runs CTest,
packages, then tests the relocated public launcher with an isolated environment.
Development-build tests alone do not qualify the release package.

Create a **draft pre-release** with those assets. Sign its outer Windows
installer with `sign-windows-release.py --allow-draft --upload`, then dispatch
`sign-release-installers.yml` at the frozen source ref with the exact tag,
`architecture=arm64` and `allow_draft=true`. Draft access is explicit; signing
tools still reject drafts by default and always reject immutable releases.
Tag, release identity and asset digests remain pinned during signing.
Draft discovery searches accessible releases for the explicit tag and pins its
release ID. Subsequent checks use that ID because GitHub's published-tag REST
endpoint does not expose drafts. The actual source tag is checked separately.

Keep the release a draft until native Windows signed-installation checks and
Mac Developer ID signing, notarization, stapling, Gatekeeper, installation and
runtime-choice checks all pass. Read back final assets/checksums, set accurate
release notes, then publish the draft as a pre-release. Its tag remains at the
qualified source commit. Adrian's physical Windows smoke test still gates any
Shmuel handoff. No private compatibility material belongs in release assets.

These tools operate on the payload of an explicit published tag. They do not
build the source at the tip of `main`, move tags or merge the experimental
fork. All original asset digests and the tag commit are pinned and checked
throughout signing. The private bundled cREXX installation stays separate
from any standalone cREXX installation. Bundled cREXX remains the default;
panels.2 offers an installed-runtime preference with automatic bundled fallback.

## Runtime-choice delivery: panels.2

[v4.0.0-panels.2](https://github.com/adesutherland/THE/releases/tag/v4.0.0-panels.2)
uses source tag `091aef7b08a2674f6ae3dfa4235ee96e7a19a09c`. Source QA passed
84/84 macOS tests and Windows [run 40223307643](https://github.com/adesutherland/THE/actions/runs/40223307643)
(67 passed, 13 platform/tool skips). Both 121-workflow macro suites and exact
parity passed on the installed Windows payload.

[Signed-installer run 40224957673](https://github.com/adesutherland/THE/actions/runs/40224957673)
passed on both systems: Authenticode/timestamp and Windows installation;
Developer ID signatures, Apple notarization, PKG stapling, Gatekeeper and
macOS installation. Both runtime preferences, explicit overrides, cache
miss/hit/toolchain changes, and absent/incompatible fallback were tested.
Native macOS Installer choice application and a differently signed external
cREXX copy passed. Host/launcher entitlements allow the explicit external
runtime while retaining hardened runtime and notarization. The Mac payload
contains ten signed code files and 130 manifest entries; Windows has 157
entries. The Windows ZIP was unchanged by signing. Manual Windows interaction
and the handoff to Shmuel remain pending.

See [runtime/cache configuration](https://github.com/adesutherland/THE/blob/v4.0.0-panels.2/doc/rexx-interpreters.md).

## Qualified delivery on 25 October 2026

The existing [panel prerelease](https://github.com/adesutherland/THE/releases/tag/v4.0.0-panels.1)
was signed and its assets replaced without moving source tag
`42d7135728e6b1fd6501f74ecd8312d3b564991b`.
[Hosted run 40219171572](https://github.com/adesutherland/THE/actions/runs/40219171572)
passed both native jobs and all 20 signing-tool regressions (11 Windows,
9 macOS). Windows verified Authenticode and timestamp, all installed hashes,
CREXX/NONE modes, browser assets, reinstall/uninstall, PATH and user-file
retention. The outer installer changed; its ZIP and inner payload stayed intact.

Apple accepted both ARM64 assets for notarization. The PKG passed signature,
staple and Gatekeeper checks, installation, payload hashes, CREXX/NONE macros
and browser checks as a normal user. All eight Mach-O files were signed.
Two `.DS_Store` files were excluded because `pkgbuild` omits them; the updated
manifest contains 128 payload hashes, with all other non-code files unchanged.
This post-signing smoke supplements the earlier full macro/panel qualification;
manual Windows interaction and any external handoff remain pending.

| Published asset | SHA-256 |
| --- | --- |
| Windows x64 installer | `711ac3c7642f0945dda142ae4a1e5d1d4720bc8d63035046b42bc645f0ec76b6` |
| macOS ARM64 PKG | `9c0cd9b5d07ab861d590a66b30f8fa86ddf4edb4fc2a2de1e314424de13edb75` |
| macOS ARM64 ZIP | `75d9dc71237e458d662b9d2947947e1ce4a28aa24cd8a2ce46e3a54b4aa838d0` |

## Windows

```sh
python3 scripts/sign-windows-release.py --tag v4.0.0-panels.2 --dry-run
python3 scripts/sign-windows-release.py --tag v4.0.0-panels.2 \
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

The macOS job uses the nine Apple secrets with the same names as cREXX-RAG.
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
  --ref main -f tag=v4.0.0-panels.2 -f architecture=arm64
```

The selected architecture must exist on that release; the panel candidate has
ARM64 macOS assets, not Intel assets. Manual Windows interactive testing and
the handoff to Shmuel remain separate gates.

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
