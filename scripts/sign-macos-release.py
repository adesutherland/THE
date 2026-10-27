#!/usr/bin/env python3
"""Sign and notarize the existing macOS payload of an explicit THE release.

No compiler/build is used. All input manifest hashes and the source tag are
verified. --upload replaces only this architecture's PKG, ZIP and checksums.
Credentials are read from the same nine environment settings as cREXX-RAG.
"""
import argparse
import base64
from contextlib import contextmanager
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import secrets
import shlex
import shutil
import stat
import subprocess
import sys
import tempfile
import time
import zipfile

spec = importlib.util.spec_from_file_location('release_tools', Path(__file__).with_name('sign-windows-release.py'))
release_tools = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release_tools)
installer_spec = importlib.util.spec_from_file_location('macos_installer', Path(__file__).with_name('macos_installer.py'))
installer_builder = importlib.util.module_from_spec(installer_spec)
installer_spec.loader.exec_module(installer_builder)

SECRET_NAMES = (
    'APPLE_DEVELOPER_ID_CERTIFICATE_BASE64', 'APPLE_DEVELOPER_ID_CERTIFICATE_PASSWORD',
    'APPLE_DEVELOPER_ID_IDENTITY', 'APPLE_DEVELOPER_ID_INSTALLER_CERTIFICATE_BASE64',
    'APPLE_DEVELOPER_ID_INSTALLER_CERTIFICATE_PASSWORD', 'APPLE_DEVELOPER_ID_INSTALLER_IDENTITY',
    'APPLE_ID', 'APPLE_APP_SPECIFIC_PASSWORD', 'APPLE_TEAM_ID',
)
MACHO = {bytes.fromhex(value) for value in ('feedface', 'cefaedfe', 'feedfacf', 'cffaedfe',
                                          'cafebabe', 'bebafeca', 'cafebabf', 'bfbafeca')}


def run(*command):
    # Never include command arguments, captured output or credentials in errors.
    result = subprocess.run(list(map(str, command)), capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f'{Path(command[0]).name} failed (exit {result.returncode})')
    return result.stdout


def file_hash(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def macho_files(payload):
    found = []
    for path in sorted(payload.rglob('*')):
        if path.is_file():
            with path.open('rb') as stream:
                if stream.read(4) in MACHO:
                    found.append(path)
    return found


def remove_packaging_metadata(payload):
    # pkgbuild excludes Finder metadata, so it must not appear in the manifest
    # shared by the portable ZIP and installed PKG. Only touch the staged copy.
    removed = []
    for path in sorted(payload.rglob('.DS_Store')):
        if path.is_file():
            removed.append(path.relative_to(payload).as_posix())
            path.unlink()
    return removed


def unpack(archive, destination, root_name):
    seen = set()
    with zipfile.ZipFile(archive) as source:
        for entry in source.infolist():
            name = PurePosixPath(entry.filename)
            mode = entry.external_attr >> 16
            if (not name.parts or name.parts[0] != root_name or name.is_absolute() or
                    '..' in name.parts or '\\' in entry.filename or
                    name in seen or stat.S_ISLNK(mode) or
                    stat.S_IFMT(mode) not in (0, stat.S_IFREG, stat.S_IFDIR)):
                raise RuntimeError('Unsafe or duplicate ZIP entry')
            seen.add(name)
            target = destination.joinpath(*name.parts)
            if entry.is_dir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(source.read(entry))
                target.chmod((mode & 0o777) or 0o644)
    return destination / root_name


def validate_payload(payload, version, arch, commit):
    manifest = json.loads((payload / 'manifest.json').read_text())
    expected = dict(version=version, platform='darwin', architecture=arch, source_commit=commit)
    if any(manifest.get(k) != value for k, value in expected.items()):
        raise RuntimeError('Payload identity does not match release tag/architecture')
    files = {p.relative_to(payload).as_posix(): file_hash(p) for p in payload.rglob('*')
             if p.is_file() and p != payload / 'manifest.json'}
    if files != manifest['files_sha256']:
        raise RuntimeError('Payload file inventory/hash mismatch')
    if not macho_files(payload):
        raise RuntimeError('No executable code found in payload')
    return manifest


@contextmanager
def signing_keychain():
    with tempfile.TemporaryDirectory(prefix='the-private-signing-') as tmp:
        work = Path(tmp)
        keychain = work / 'signing.keychain-db'
        password = secrets.token_hex(24)
        created = False
        search_list = shlex.split(run('security', 'list-keychains', '-d', 'user'))
        try:
            run('security', 'create-keychain', '-p', password, keychain)
            created = True
            run('security', 'set-keychain-settings', '-lut', '21600', keychain)
            run('security', 'unlock-keychain', '-p', password, keychain)
            run('security', 'list-keychains', '-d', 'user', '-s', keychain, *search_list)
            for kind in ('', 'INSTALLER_'):
                certificate = work / f'{kind}certificate.p12'
                data = ''.join(os.environ[f'APPLE_DEVELOPER_ID_{kind}CERTIFICATE_BASE64'].split())
                certificate.write_bytes(base64.b64decode(data, validate=True))
                certificate.chmod(0o600)
                run('security', 'import', certificate, '-k', keychain,
                    '-P', os.environ[f'APPLE_DEVELOPER_ID_{kind}CERTIFICATE_PASSWORD'],
                    '-T', '/usr/bin/codesign', '-T', '/usr/bin/productbuild')
                certificate.unlink()
            run('security', 'set-key-partition-list', '-S', 'apple-tool:,apple:', '-s', '-k', password, keychain)
            yield keychain
        finally:
            if created:
                try:
                    run('security', 'list-keychains', '-d', 'user', '-s', *search_list)
                finally:
                    run('security', 'delete-keychain', keychain)


def notarize(path):
    reply = json.loads(run('xcrun', 'notarytool', 'submit', path,
        '--apple-id', os.environ['APPLE_ID'], '--password', os.environ['APPLE_APP_SPECIFIC_PASSWORD'],
        '--team-id', os.environ['APPLE_TEAM_ID'], '--wait', '--timeout', '25m', '--output-format', 'json'))
    if reply.get('status') != 'Accepted':
        raise RuntimeError(f'Notarization was not accepted: {path.name}; submission {reply.get("id")}')
    print(f'Notarization accepted: {path.name}; submission {reply["id"]}', flush=True)
    return reply['id']


def staple(path):
    for attempt in range(3):
        result = subprocess.run(['xcrun', 'stapler', 'staple', str(path)], capture_output=True)
        if result.returncode == 0:
            run('xcrun', 'stapler', 'validate', path)
            return
        if result.returncode != 68 or attempt == 2:
            raise RuntimeError(f'Stapling failed (exit {result.returncode})')
        time.sleep(10)


def verify_code(payload):
    for path in macho_files(payload):
        run('codesign', '--verify', '--strict', path)


def smoke(payload, read_only=False):
    script = Path(__file__).resolve().parent.parent / 'tests/test_installed_package.py'
    # The test removes user/build tool paths and runs real cREXX macros,
    # NONE mode, and the installed browser assets using the package launcher.
    command = [sys.executable, str(script), str(payload)]
    if read_only:
        command.append('--read-only-prefix')
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode:
        print(result.stdout, file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        raise RuntimeError('Signed installed-payload smoke test failed')
    print(result.stdout, end='', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', default='adesutherland/THE')
    parser.add_argument('--tag', required=True)
    parser.add_argument('--arch', choices=('arm64', 'x86_64'), default='arm64')
    parser.add_argument('--output-dir', type=Path, default=Path('build/sign-macos'))
    parser.add_argument('--test-install', action='store_true',
                        help='Install and smoke-test on a disposable GitHub-hosted runner before upload')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--dry-run', action='store_true')
    mode.add_argument('--upload', action='store_true')
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', args.repo):
        parser.error('Use OWNER/REPO')
    if not re.fullmatch(r'v\d+\.\d+\.\d+(?:[-+][A-Za-z0-9.-]+)?', args.tag):
        parser.error('Use an explicit published version tag')
    release = release_tools.Release(args.repo, args.tag)
    name = release_tools.release_stem(release, 'darwin', args.arch)
    inputs = {suffix: release_tools.asset_named(release.original, name + suffix)
              for suffix in ('.zip', '.pkg', '-SHA256SUMS.txt')}
    print(f'{args.repo}@{args.tag}: source {release.commit}, architecture {args.arch}', flush=True)
    if args.dry_run:
        release.check()
        print('Dry run: sign/notarize published payload; replace only its macOS ZIP, PKG and checksums.')
        return
    if platform.system() != 'Darwin' or platform.machine() != args.arch:
        raise RuntimeError('Use a native macOS runner matching the payload architecture')
    if args.test_install and os.environ.get('RUNNER_ENVIRONMENT') != 'github-hosted':
        raise RuntimeError('--test-install requires a disposable GitHub-hosted runner')
    missing = [name for name in SECRET_NAMES if not os.environ.get(name, '').strip()]
    if missing:
        raise RuntimeError('Missing Apple settings: ' + ', '.join(missing))
    output = args.output_dir.resolve() / args.tag
    output.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='attempt-', dir=output))
    original, signed = work / 'original', work / 'signed'
    original.mkdir()
    signed.mkdir()
    (work / 'input.json').write_text(json.dumps(dict(repo=args.repo, tag=args.tag,
        source_commit=release.commit, release_id=release.original['id'],
        assets=list(release.expected.values())), indent=2) + '\n')
    for suffix, asset in inputs.items():
        release.download(asset, original / (name + suffix))
    release_tools.validate_checksums(original / (name + '-SHA256SUMS.txt'), name + '.pkg', release.original)
    payload = unpack(original / (name + '.zip'), work / 'payload', name)
    manifest = validate_payload(payload, args.tag[1:], args.arch, release.commit)
    old_hashes = manifest['files_sha256'].copy()
    removed_metadata = remove_packaging_metadata(payload)
    binaries = macho_files(payload)
    changed_names = [p.relative_to(payload).as_posix() for p in binaries]
    print(f'Original manifest verified: {len(old_hashes)} files; signing {len(binaries)} Mach-O files', flush=True)
    release.check()
    with signing_keychain() as keychain:
        for path in binaries:
            entitlements = []
            if path.relative_to(payload).as_posix() in ('libexec/the/the', 'libexec/the/the-launcher'):
                entitlements = ['--entitlements', Path(__file__).resolve().parent.parent /
                                'packaging/macos/host-entitlements.plist']
            run('codesign', '--force', '--timestamp', '--options', 'runtime', '--keychain', keychain,
                '--sign', os.environ['APPLE_DEVELOPER_ID_IDENTITY'], *entitlements, path)
        verify_code(payload)
        manifest['files_sha256'] = {p.relative_to(payload).as_posix(): file_hash(p) for p in payload.rglob('*')
                                    if p.is_file() and p != payload / 'manifest.json'}
        if (set(manifest['files_sha256']) != set(old_hashes) - set(removed_metadata) or
                any(old_hashes[n] != h for n, h in manifest['files_sha256'].items() if n not in changed_names)):
            raise RuntimeError('Signing changed a non-executable payload file')
        manifest['signing'] = dict(type='Developer ID', source_archive_sha256=inputs['.zip']['digest'],
                                  removed_packaging_metadata=removed_metadata)
        (payload / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        validate_payload(payload, args.tag[1:], args.arch, release.commit)
        smoke(payload)
        if (payload / 'libexec/the/the-launcher').exists():
            run(sys.executable, Path(__file__).resolve().parent.parent / 'tests/test_runtime_selection.py',
                '--prefix', payload)
            print('Signed runtime selection and cache tests passed, including a differently signed installed cREXX', flush=True)
        archive = Path(shutil.make_archive(str(signed / name), 'zip', payload.parent, name))
        installer = signed / (name + '.pkg')
        installer_builder.build_installer(payload, installer, args.tag[1:], work, run=run,
            identity=os.environ['APPLE_DEVELOPER_ID_INSTALLER_IDENTITY'], keychain=keychain)
        run('pkgutil', '--check-signature', installer)
        submissions = {archive.name: notarize(archive), installer.name: notarize(installer)}
        staple(installer)
        run('spctl', '--assess', '--type', 'install', '--verbose=2', installer)
    if args.test_install:
        run('sudo', 'installer', '-pkg', installer, '-target', '/')
        installed = Path('/usr/local/the-crexx')
        if Path('/usr/local/bin/the').resolve() != installed / 'bin/the':
            raise RuntimeError('Installer did not create the expected THE launcher symlink')
        smoke(installed, read_only=True)
        if (installed / 'libexec/the/the-launcher').exists():
            run(sys.executable, Path(__file__).resolve().parent.parent / 'tests/test_macos_runtime_installer.py',
                '--installer', installer)
            print('macOS Installer default, installed-runtime choice and automatic fallback passed', flush=True)
        print('Signed PKG installed and tested on the disposable runner', flush=True)
    checksums = signed / (name + '-SHA256SUMS.txt')
    checksums.write_text(''.join(f'{file_hash(p)}  {p.name}\n' for p in (archive, installer)))
    receipt = dict(repo=args.repo, tag=args.tag, source_commit=release.commit, architecture=args.arch,
        source_dirty=manifest['source_dirty'], files_signed=changed_names, notarization=submissions,
        removed_packaging_metadata=removed_metadata,
        uploaded=False, files_sha256={p.name: file_hash(p) for p in (archive, installer, checksums)})
    receipt_path = work / 'receipt.json'
    receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')

    def verify_download(path):
        if path.suffix == '.pkg':
            run('pkgutil', '--check-signature', path)
            run('xcrun', 'stapler', 'validate', path)
            run('spctl', '--assess', '--type', 'install', path)
        elif path.suffix == '.zip':
            with tempfile.TemporaryDirectory(prefix='the-signed-readback-') as tmp:
                tree = unpack(path, Path(tmp), name)
                validate_payload(tree, args.tag[1:], args.arch, release.commit)
                verify_code(tree)

    release.check()
    if args.upload:
        release.publish([archive, installer, checksums], verifier=verify_download)
        receipt['uploaded'] = True
        receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'{"Published and verified" if args.upload else "Signed and notarized locally"}: {signed}', flush=True)


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f'error: {error}', file=sys.stderr)
        sys.exit(1)
