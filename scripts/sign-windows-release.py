#!/usr/bin/env python3
"""Sign an existing THE Windows installer; optionally replace its release asset.

An explicit tag is mandatory. No rebuild, inner-payload signing, or ZIP changes.
The default signs locally; --upload also replaces the installer and checksums.
Private draft assets require --allow-draft and are never published by this tool.
--dry-run only reads release metadata. Signing credentials stay in local config.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import uuid


def gh(*args):
    return subprocess.check_output(['gh', *map(str, args)], text=True, encoding='utf-8')


def api(path, *args):
    value = gh('api', path, *args)
    return json.loads(value) if value.strip() else None


def digest(path):
    return 'sha256:' + hashlib.sha256(Path(path).read_bytes()).hexdigest()


def fingerprint(asset):
    return {key: asset[key] for key in ('id', 'name', 'size', 'digest', 'updated_at')}


def asset_named(info, name):
    matches = [item for item in info['assets'] if item['name'] == name]
    if len(matches) != 1:
        raise RuntimeError(f'Expected exactly one release asset: {name}')
    item = matches[0]
    if not re.fullmatch(r'sha256:[0-9a-f]{64}', item.get('digest') or ''):
        raise RuntimeError(f'GitHub SHA-256 digest unavailable: {name}')
    return item


def release_stem(release, platform, architecture):
    """Accept historical THE assets and CRXTHE without guessing a release."""
    candidates = [f'{product}-{release.tag[1:]}-{platform}-{architecture}'
                  for product in ('CRXTHE', 'THE')]
    names = {asset['name'] for asset in release.original['assets']}
    matches = [stem for stem in candidates if stem + '-SHA256SUMS.txt' in names]
    if len(matches) != 1:
        raise RuntimeError('Expected one unambiguous CRXTHE or historical THE asset set')
    return matches[0]


class Release:
    def __init__(self, repo, tag, allow_draft=False):
        self.repo, self.tag = repo, tag
        self.base = f'repos/{repo}'
        self.original = self.info()
        if self.original.get('immutable') or (self.original.get('draft') and not allow_draft):
            raise RuntimeError('Select a mutable published release, or explicitly allow a draft')
        self.commit = self.tag_commit()
        self.expected = {a['id']: fingerprint(a) for a in self.original['assets']}

    def info(self):
        return api(f'{self.base}/releases/tags/{self.tag}')

    def tag_commit(self):
        obj = api(f'{self.base}/git/ref/tags/{self.tag}')['object']
        for _ in range(10):
            if obj['type'] == 'commit':
                return obj['sha']
            if obj['type'] != 'tag':
                break
            obj = api(f'{self.base}/git/tags/{obj["sha"]}')['object']
        raise RuntimeError('Release tag does not resolve to a commit')

    def check(self):
        info = self.info()
        if (any(info.get(k) != self.original.get(k) for k in
                ('id', 'tag_name', 'draft', 'prerelease', 'immutable')) or
                self.tag_commit() != self.commit or
                {a['id']: fingerprint(a) for a in info['assets']} != self.expected):
            raise RuntimeError('Release/tag/assets changed during signing; start again')
        return info

    def download(self, asset, path):
        with path.open('wb') as stream:
            subprocess.run(['gh', 'api', f'{self.base}/releases/assets/{asset["id"]}',
                            '-H', 'Accept: application/octet-stream'], stdout=stream, check=True)
        if digest(path) != asset['digest']:
            raise RuntimeError(f'Download digest mismatch: {asset["name"]}')

    def rename(self, asset_id, name):
        self.check()
        item = api(f'{self.base}/releases/assets/{asset_id}', '--method', 'PATCH', '-f', f'name={name}')
        previous = self.expected[asset_id]
        if any(item[k] != previous[k] for k in ('id', 'size', 'digest')) or item['name'] != name:
            raise RuntimeError('Renamed asset did not preserve its contents')
        self.expected[asset_id] = fingerprint(item)

    def remove(self, asset_id):
        self.check()
        api(f'{self.base}/releases/assets/{asset_id}', '--method', 'DELETE')
        del self.expected[asset_id]

    def publish(self, paths, verifier=None):
        """Upload and verify first; retain original assets until final readback.

        GitHub has no atomic multi-asset replacement. Renamed backups permit
        recovery if promotion is interrupted. Never delete backups on failure.
        """
        self.check()
        nonce = uuid.uuid4().hex
        originals = [asset_named(self.original, path.name) for path in paths]
        staged = []
        try:
            with tempfile.TemporaryDirectory(prefix='the-sign-upload-') as tmp:
                for path in paths:
                    temporary = Path(tmp) / f'{path.name}.sign-{nonce}'
                    shutil.copyfile(path, temporary)
                    gh('release', 'upload', self.tag, temporary, '--repo', self.repo)
                    item = asset_named(self.info(), temporary.name)
                    if item['digest'] != digest(path):
                        raise RuntimeError(f'Staged upload digest mismatch: {path.name}')
                    self.expected[item['id']] = fingerprint(item)
                    staged.append(item)
                    self.check()
                for old, new, path in zip(originals, staged, paths):
                    self.rename(old['id'], f'{path.name}.backup-{nonce}')
                    self.rename(new['id'], path.name)
                info = self.check()
                for path in paths:
                    item = asset_named(info, path.name)
                    downloaded = Path(tmp) / path.name
                    self.download(item, downloaded)
                    if digest(downloaded) != digest(path):
                        raise RuntimeError(f'Published readback differs: {path.name}')
                    if verifier:
                        verifier(downloaded)
                    elif path.suffix == '.exe':
                        verify_signature(downloaded)
        except BaseException:
            # Includes KeyboardInterrupt. A lost network response can leave a
            # rename applied remotely: retain both versions rather than guessing.
            print(f'Publication interrupted. Original assets and any verified uploads are retained; '
                  f'inspect .backup-{nonce} / .sign-{nonce} assets on {self.repo}@{self.tag}.', file=sys.stderr)
            raise
        for item in originals:
            self.remove(item['id'])
        self.check()


def parse_checksums(text):
    result = {}
    for line in text.splitlines():
        match = re.fullmatch(r'([0-9a-f]{64})  ([A-Za-z0-9_.+-]+)', line)
        if not match or match[2] in result:
            raise RuntimeError('Malformed or duplicate checksum entry')
        result[match[2]] = match[1]
    return result


def validate_checksums(path, installer_name, original_info):
    entries = parse_checksums(path.read_text())
    if installer_name not in entries:
        raise RuntimeError('Installer missing from checksum file')
    for name, checksum in entries.items():
        if asset_named(original_info, name)['digest'] != 'sha256:' + checksum:
            raise RuntimeError(f'Original checksums disagree with GitHub: {name}')
    return entries


def update_checksums(path, installer, original_info):
    entries = validate_checksums(path, installer.name, original_info)
    entries[installer.name] = digest(installer).removeprefix('sha256:')
    path.write_text(''.join(f'{value}  {name}\n' for name, value in entries.items()))


def verify_signature(path):
    result = subprocess.run(['osslsigncode', 'verify', '-in', str(path)],
                            capture_output=True, text=True)
    if (result.returncode or
            'Timestamp Server Signature verification: ok' not in result.stdout):
        raise RuntimeError('Installer signature or trusted timestamp verification failed')


def sign(path, signer):
    if signer:
        command = [str(signer.resolve()), str(path)]
    else:
        provider, alias = os.environ.get('PROVIDER'), os.environ.get('CERTUM_ALIAS')
        if not provider or not alias or not Path(provider).is_file():
            raise RuntimeError('Set PROVIDER and CERTUM_ALIAS for jsign, or pass --signer /path/to/sign-windows-file.sh')
        command = ['jsign', '--storetype', 'PKCS11', '--keystore', provider, '--alias', alias,
                   '--alg', 'SHA-256', '--tsaurl', os.environ.get('TSA_URL', 'http://time.certum.pl'), str(path)]
    # Avoid retaining provider/certificate details or credentials in logs/errors.
    result = subprocess.run(command, capture_output=True)
    if result.returncode:
        raise RuntimeError('Signing failed; check the local signer login and configuration')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', default='adesutherland/THE')
    parser.add_argument('--tag', required=True, help='Exact published tag, e.g. v4.0.0-panels.1')
    parser.add_argument('--signer', type=Path, help='Existing local single-file signing helper (optional)')
    parser.add_argument('--output-dir', type=Path, default=Path('build/sign-windows'))
    parser.add_argument('--allow-draft', action='store_true',
                        help='Explicitly permit signing assets of a private draft release')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--upload', action='store_true', help='Also replace the installer and Windows checksum assets')
    mode.add_argument('--dry-run', action='store_true', help='Read metadata only; do not download, sign or upload')
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', args.repo):
        parser.error('Use OWNER/REPO')
    if not re.fullmatch(r'v\d+\.\d+\.\d+(?:[-+][A-Za-z0-9.-]+)?', args.tag):
        parser.error('Use an explicit version tag, not latest or a moving snapshot')
    release = Release(args.repo, args.tag, allow_draft=args.allow_draft)
    stem = release_stem(release, 'windows', 'x86_64')
    installer_asset = asset_named(release.original, stem + '-setup.exe')
    checksum_asset = asset_named(release.original, stem + '-SHA256SUMS.txt')
    print(f'{args.repo}@{args.tag}: source {release.commit}')
    print(f'Installer: {installer_asset["name"]} ({installer_asset["digest"]})')
    if args.dry_run:
        release.check()
        print('Dry run: installer + Windows checksums would be replaced; ZIP and macOS assets preserved.')
        return
    if not shutil.which('osslsigncode'):
        raise RuntimeError('Install osslsigncode before signing')
    output = args.output_dir.resolve() / args.tag
    output.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='attempt-', dir=output))
    original_dir, signed_dir = work / 'original', work / 'signed'
    original_dir.mkdir()
    signed_dir.mkdir()
    (work / 'input.json').write_text(json.dumps({
        'repo': args.repo, 'tag': args.tag, 'source_commit': release.commit,
        'release_id': release.original['id'], 'assets': list(release.expected.values()),
    }, indent=2) + '\n')
    installer = original_dir / installer_asset['name']
    checksum = original_dir / checksum_asset['name']
    release.download(installer_asset, installer)
    release.download(checksum_asset, checksum)
    # Validate the original checksum file before invoking any signer.
    validate_checksums(checksum, installer.name, release.original)
    release.check()
    signed = signed_dir / installer.name
    shutil.copyfile(installer, signed)
    sign(signed, args.signer)
    verify_signature(signed)
    updated = signed_dir / checksum.name
    shutil.copyfile(checksum, updated)
    update_checksums(updated, signed, release.original)
    release.check()
    receipt = {
        'repo': args.repo, 'tag': args.tag, 'source_commit': release.commit,
        'release_id': release.original['id'], 'uploaded': False,
        'original_installer': installer_asset['digest'],
        'signed_installer': digest(signed), 'windows_checksums': digest(updated),
        'signature_and_timestamp_verified': True,
        'scope': 'outer installer only; ZIP and inner payload unchanged',
    }
    receipt_path = work / 'receipt.json'
    receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
    if args.upload:
        release.publish([signed, updated])
        receipt['uploaded'] = True
        receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'{"Published and verified" if args.upload else "Signed locally; no upload"}: {signed}')


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        print(f'error: {error}', file=sys.stderr)
        sys.exit(1)
