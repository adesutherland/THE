#!/usr/bin/env python3
"""Input-integrity and private-keychain regressions; no live credentials."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import stat
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import zipfile

source = Path(__file__).resolve().parent.parent / 'scripts/sign-macos-release.py'
spec = importlib.util.spec_from_file_location('sign_macos', source)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class MacSigningTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='THE Apple signing tests ')
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.name = 'THE-4.0.0-test.1-darwin-arm64'
        self.payload = self.root / self.name
        self.payload.mkdir()
        binary = self.payload / 'libexec/the/crexx/rxc'
        binary.parent.mkdir(parents=True)
        binary.write_bytes(bytes.fromhex('cffaedfe') + b'compiler fixture')
        binary.chmod(0o755)
        self.binary = binary
        self.manifest = dict(version='4.0.0-test.1', platform='darwin', architecture='arm64',
            source_commit='a' * 40, source_dirty=True, regina_bundled=False,
            files_sha256={binary.relative_to(self.payload).as_posix(): hashlib.sha256(binary.read_bytes()).hexdigest()})
        (self.payload / 'manifest.json').write_text(json.dumps(self.manifest))

    def validate(self):
        return module.validate_payload(self.payload, '4.0.0-test.1', 'arm64', 'a' * 40)

    def test_manifest_and_nested_code(self):
        self.assertEqual(self.validate(), self.manifest)
        self.assertEqual(module.macho_files(self.payload), [self.binary])

    def test_modified_file_rejected(self):
        self.binary.write_bytes(self.binary.read_bytes() + b'changed')
        with self.assertRaisesRegex(RuntimeError, 'hash mismatch'):
            self.validate()

    def test_unlisted_file_rejected(self):
        (self.payload / 'unlisted').write_text('unexpected')
        with self.assertRaisesRegex(RuntimeError, 'inventory'):
            self.validate()

    def test_wrong_source_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'identity'):
            module.validate_payload(self.payload, '4.0.0-test.1', 'arm64', 'b' * 40)

    def test_archive_preserves_executable_mode(self):
        archive = self.root / 'input.zip'
        with zipfile.ZipFile(archive, 'w') as output:
            for path in self.payload.rglob('*'):
                output.write(path, path.relative_to(self.root))
        extracted = module.unpack(archive, self.root / 'unpacked', self.name)
        self.assertTrue((extracted / self.binary.relative_to(self.payload)).stat().st_mode & stat.S_IXUSR)
        module.validate_payload(extracted, '4.0.0-test.1', 'arm64', 'a' * 40)

    def test_traversal_and_symlink_rejected(self):
        archive = self.root / 'bad.zip'
        for name, mode in ((self.name + '/../escape', 0o100644), (self.name + '/link', 0o120777)):
            with self.subTest(name=name), zipfile.ZipFile(archive, 'w') as output:
                item = zipfile.ZipInfo(name)
                item.external_attr = mode << 16
                output.writestr(item, 'target')
            with self.assertRaisesRegex(RuntimeError, 'Unsafe'):
                module.unpack(archive, self.root / 'bad', self.name)

    def test_native_failure_does_not_expose_credentials(self):
        with patch.object(module.subprocess, 'run', return_value=subprocess.CompletedProcess([], 1, '', 'private-secret')):
            with self.assertRaises(RuntimeError) as error:
                module.run('security', 'import', '-P', 'private-password')
        self.assertNotIn('private', str(error.exception))

    def test_keychain_search_list_restored_after_error(self):
        calls = []

        def fake_run(*command):
            calls.append(command)
            return '"/Users/test/login.keychain-db"' if command == ('security', 'list-keychains', '-d', 'user') else ''

        env = {name: ('ZHVtbXk=' if name.endswith('_BASE64') else 'dummy') for name in module.SECRET_NAMES}
        with patch.dict(os.environ, env), patch.object(module, 'run', side_effect=fake_run):
            with self.assertRaisesRegex(RuntimeError, 'synthetic failure'):
                with module.signing_keychain():
                    raise RuntimeError('synthetic failure')
        self.assertEqual(calls[-2], ('security', 'list-keychains', '-d', 'user', '-s', '/Users/test/login.keychain-db'))
        self.assertEqual(calls[-1][:2], ('security', 'delete-keychain'))
        self.assertFalse(Path(calls[-1][2]).parent.exists())


if __name__ == '__main__':
    unittest.main()
