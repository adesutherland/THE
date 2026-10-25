#!/usr/bin/env python3
"""Stage an existing bundled build; create a portable archive and native installer.

Uses the cREXX-RAG per-user NSIS/PATH pattern and Apple's pkgbuild/productbuild.
Does not install, sign, notarize, upload or publish anything.
"""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tempfile
from macos_installer import build_installer


def run(*args):
    subprocess.run([str(a) for a in args], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', required=True, type=Path)
    parser.add_argument('--version', required=True, help='Candidate version, e.g. 4.0.0-dev')
    parser.add_argument('--installer', action='store_true')
    args = parser.parse_args()
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:[-+][A-Za-z0-9.-]+)?', args.version):
        parser.error('Use a numeric three-part version with an optional suffix')
    build = args.build_dir.resolve()
    source = Path(__file__).resolve().parent.parent
    cache = (build / 'CMakeCache.txt').read_text()
    if 'THE_BUNDLE_CREXX:BOOL=ON' not in cache:
        parser.error('Configure this build with -DTHE_BUNDLE_CREXX=ON first')
    output = build / 'packages'
    output.mkdir(exist_ok=True)
    host = platform.system().lower()
    arch = platform.machine().lower().replace('aarch64', 'arm64').replace('amd64', 'x86_64')
    name = f'THE-{args.version}-{host}-{arch}'
    with tempfile.TemporaryDirectory(prefix='the-package-', dir=build) as tmp:
        work = Path(tmp)
        payload = work / name
        run('cmake', '--install', build, '--prefix', payload)
        if host == 'windows':
            helper = payload / 'share/the/installer'
            helper.mkdir(parents=True)
            shutil.copy2(source / 'packaging/windows/update-user-path.ps1', helper)
        # Apple's pkgbuild omits Finder metadata. Keep the portable archive
        # and installed payload manifest consistent on every platform.
        for metadata in payload.rglob('.DS_Store'):
            if metadata.is_file():
                metadata.unlink()
        commit = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
        dirty = bool(subprocess.check_output(['git', '-C', str(source), 'status', '--porcelain'], text=True))
        files = {str(p.relative_to(payload)).replace('\\', '/'): hashlib.sha256(p.read_bytes()).hexdigest()
                 for p in sorted(payload.rglob('*')) if p.is_file()}
        (payload / 'manifest.json').write_text(json.dumps({
            'version': args.version, 'platform': host, 'architecture': arch,
            'source_commit': commit, 'source_dirty': dirty,
            'regina_bundled': False, 'files_sha256': files,
        }, indent=2) + '\n')
        archive = Path(shutil.make_archive(str(output / name), 'zip', work, name))
        assets = [archive]
        if args.installer:
            numeric = re.split(r'[-+]', args.version)[0]
            if host == 'darwin':
                installer = output / f'{name}.pkg'
                build_installer(payload, installer, args.version, work)
            elif host == 'windows':
                owned = work / 'owned-files.nsh'
                entries = ['Delete "$INSTDIR\\' + str(p.relative_to(payload)).replace('/', '\\') + '"'
                           for p in sorted(payload.rglob('*')) if p.is_file()]
                entries += ['RMDir "$INSTDIR\\' + str(p.relative_to(payload)).replace('/', '\\') + '"'
                            for p in sorted(payload.rglob('*'), key=lambda p: len(p.parts), reverse=True)
                            if p.is_dir()]
                owned.write_text('\n'.join(entries) + '\n')
                installer = output / f'{name}-setup.exe'
                run('makensis', f'/DTHE_VERSION={args.version}', f'/DTHE_FILE_VERSION={numeric}.0',
                    f'/DTHE_OUTPUT={installer}', f'/DTHE_PAYLOAD={payload}',
                    f'/DTHE_UNINSTALL_FILES={owned}', source / 'packaging/windows/the.nsi')
            else:
                parser.error('Native installers are supplied for macOS and Windows; use the ZIP on Linux')
            assets.append(installer)
        checksums = output / f'{name}-SHA256SUMS.txt'
        checksums.write_text(''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in assets))
        print(f'Created {", ".join(str(p) for p in assets)}\nChecksums: {checksums}')


if __name__ == '__main__':
    main()
