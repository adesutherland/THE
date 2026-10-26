#!/usr/bin/env python3
"""Exercise release replacement failures without credentials or external writes."""
import contextlib
import copy
import hashlib
import importlib.util
import io
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

path = Path(__file__).resolve().parent.parent / 'scripts/sign-windows-release.py'
spec = importlib.util.spec_from_file_location('sign_windows_release', path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class FakeGitHub:
    def __init__(self):
        self.data = {}
        self.state = dict(id=12, tag_name='v4.0.0-test.1', draft=False, prerelease=True, assets=[])
        self.commit = 'a' * 40
        self.deletes = []
        self.fail_promotion = self.corrupt_upload = False
        self.add('THE-4.0.0-test.1-windows-x86_64-setup.exe', b'original installer')
        self.add('THE-4.0.0-test.1-windows-x86_64.zip', b'unchanged zip')
        self.add('THE-4.0.0-test.1-windows-x86_64-SHA256SUMS.txt', b'original checksums')
        self.add('THE-4.0.0-test.1-darwin-arm64.pkg', b'unchanged mac pkg')

    def add(self, name, data):
        item_id = max(self.data, default=0) + 1
        self.data[item_id] = data
        self.state['assets'].append(dict(id=item_id, name=name, size=len(data),
                                        digest='sha256:' + hashlib.sha256(data).hexdigest(), updated_at='initial'))

    def api(self, endpoint, *args):
        if '/git/ref/' in endpoint:
            return dict(object=dict(type='commit', sha=self.commit))
        if '/releases?per_page=' in endpoint:
            return [copy.deepcopy(self.state)]
        if endpoint.endswith('/releases/' + str(self.state['id'])):
            return copy.deepcopy(self.state)
        if '/releases/tags/' in endpoint:
            if self.state['draft']:
                raise RuntimeError('Published-release endpoint does not expose drafts')
            return copy.deepcopy(self.state)
        item_id = int(endpoint.rsplit('/', 1)[1])
        item = next(a for a in self.state['assets'] if a['id'] == item_id)
        if args[1] == 'DELETE':
            self.deletes.append(item_id)
            self.state['assets'].remove(item)
            return None
        name = args[3].removeprefix('name=')
        if self.fail_promotion and '.sign-' in item['name'] and '.sign-' not in name:
            raise RuntimeError('simulated rename failure')
        item.update(name=name, updated_at=item['updated_at'] + '+rename')
        return copy.deepcopy(item)

    def gh(self, *args):
        assert args[:2] == ('release', 'upload'), args
        uploaded = Path(args[3])
        data = uploaded.read_bytes()
        self.add(uploaded.name, data + b'corrupt' if self.corrupt_upload else data)
        return ''


class SigningTests(unittest.TestCase):
    def setUp(self):
        self.remote = FakeGitHub()
        self.stack = contextlib.ExitStack()
        self.addCleanup(self.stack.close)
        self.stack.enter_context(patch.object(module, 'api', self.remote.api))
        self.stack.enter_context(patch.object(module, 'gh', self.remote.gh))
        self.stack.enter_context(patch.object(module.Release, 'download',
            lambda release, asset, path: path.write_bytes(self.remote.data[asset['id']])))
        self.stack.enter_context(contextlib.redirect_stderr(io.StringIO()))
        self.tmp = Path(self.stack.enter_context(tempfile.TemporaryDirectory(prefix='THE signing tests ')))
        self.release = module.Release('owner/THE', 'v4.0.0-test.1')
        self.installer = self.tmp / self.remote.state['assets'][0]['name']
        self.installer.write_bytes(b'signed installer')
        self.checksums = self.tmp / self.remote.state['assets'][2]['name']
        self.checksums.write_bytes(b'updated checksums')

    def test_publish_preserves_other_platforms_and_zip(self):
        with patch.object(module, 'verify_signature') as verify:
            self.release.publish([self.installer, self.checksums])
        verify.assert_called_once()
        self.assertEqual(len(self.remote.state['assets']), 4)
        self.assertEqual(self.remote.deletes, [1, 3])
        for item_id in (2, 4):
            self.assertEqual(next(a for a in self.remote.state['assets'] if a['id'] == item_id),
                             self.release.original['assets'][item_id - 1])
        for path in (self.installer, self.checksums):
            self.assertEqual(module.asset_named(self.remote.state, path.name)['digest'], module.digest(path))

    def test_draft_requires_explicit_opt_in(self):
        self.remote.state['draft'] = True
        with self.assertRaises(RuntimeError):
            module.Release('owner/THE', 'v4.0.0-test.1')
        draft = module.Release('owner/THE', 'v4.0.0-test.1', allow_draft=True)
        self.assertTrue(draft.check()['draft'])
        self.remote.state['draft'] = False
        with self.assertRaises(RuntimeError):
            draft.check()

    def test_draft_opt_in_does_not_allow_immutable_release(self):
        self.remote.state.update(draft=True, immutable=True)
        with self.assertRaises(RuntimeError):
            module.Release('owner/THE', 'v4.0.0-test.1', allow_draft=True)

    def test_draft_discovery_pins_id_without_published_tag_endpoint(self):
        self.remote.state['draft'] = True
        with patch.object(module, 'api', wraps=self.remote.api) as requests:
            draft = module.Release('owner/THE', 'v4.0.0-test.1', allow_draft=True)
            draft.check()
        paths = [call.args[0] for call in requests.call_args_list]
        self.assertEqual(draft.release_id, 12)
        self.assertIn('repos/owner/THE/releases?per_page=100&page=1', paths)
        self.assertIn('repos/owner/THE/releases/12', paths)
        self.assertFalse(any('/releases/tags/' in path for path in paths))

    def test_product_asset_stem_retains_historical_support(self):
        self.assertEqual(module.release_stem(self.release,'windows','x86_64'),
                         'THE-4.0.0-test.1-windows-x86_64')
        for asset in self.release.original['assets']:
            asset['name']=asset['name'].replace('THE-','CRXTHE-',1)
        self.assertEqual(module.release_stem(self.release,'windows','x86_64'),
                         'CRXTHE-4.0.0-test.1-windows-x86_64')
        self.release.original['assets'].append({'name':'THE-4.0.0-test.1-windows-x86_64-SHA256SUMS.txt'})
        with self.assertRaisesRegex(RuntimeError,'unambiguous'):
            module.release_stem(self.release,'windows','x86_64')

    def test_moving_tag_aborts_before_upload(self):
        self.remote.commit = 'b' * 40
        with self.assertRaisesRegex(RuntimeError, 'changed'):
            self.release.publish([self.installer, self.checksums])
        self.assertEqual(len(self.remote.state['assets']), 4)
        self.assertEqual(self.remote.deletes, [])

    def test_changed_asset_aborts_before_upload(self):
        self.remote.state['assets'][0]['updated_at'] = 'someone replaced it'
        with self.assertRaisesRegex(RuntimeError, 'changed'):
            self.release.publish([self.installer, self.checksums])
        self.assertEqual(self.remote.deletes, [])

    def test_corrupt_upload_never_removes_original(self):
        self.remote.corrupt_upload = True
        with self.assertRaisesRegex(RuntimeError, 'digest mismatch'):
            self.release.publish([self.installer, self.checksums])
        self.assertEqual(module.asset_named(self.remote.state, self.installer.name)['id'], 1)
        self.assertEqual(self.remote.deletes, [])

    def test_promotion_failure_retains_recoverable_original(self):
        self.remote.fail_promotion = True
        with self.assertRaisesRegex(RuntimeError, 'rename failure'):
            self.release.publish([self.installer, self.checksums])
        self.assertEqual(self.remote.deletes, [])
        original = next(a for a in self.remote.state['assets'] if a['id'] == 1)
        self.assertIn('.backup-', original['name'])
        self.assertEqual(self.remote.data[1], b'original installer')

    def test_readback_signature_failure_retains_originals(self):
        with patch.object(module, 'verify_signature', side_effect=RuntimeError('signature failed')):
            with self.assertRaisesRegex(RuntimeError, 'signature failed'):
                self.release.publish([self.installer, self.checksums])
        self.assertEqual(self.remote.deletes, [])
        self.assertTrue(all('.backup-' in a['name'] for a in self.remote.state['assets'] if a['id'] in (1, 3)))

    def write_original_checksums(self):
        self.checksums.write_text(''.join(a['digest'][7:] + '  ' + a['name'] + '\n'
                                       for a in self.release.original['assets'][:2]))

    def test_only_installer_checksum_changes(self):
        self.write_original_checksums()
        module.update_checksums(self.checksums, self.installer, self.release.original)
        entries = module.parse_checksums(self.checksums.read_text())
        self.assertEqual(entries[self.installer.name], module.digest(self.installer)[7:])
        self.assertEqual(entries[self.remote.state['assets'][1]['name']], self.remote.state['assets'][1]['digest'][7:])

    def test_validation_preserves_original_windows_newlines(self):
        self.write_original_checksums()
        original = self.checksums.read_text().replace('\n', '\r\n').encode()
        self.checksums.write_bytes(original)
        module.validate_checksums(self.checksums, self.installer.name, self.release.original)
        self.assertEqual(self.checksums.read_bytes(), original)

    def test_mismatched_original_checksum_rejected(self):
        self.write_original_checksums()
        self.checksums.write_text(self.checksums.read_text().replace(
            self.remote.state['assets'][0]['digest'][7:], '0' * 64))
        with self.assertRaisesRegex(RuntimeError, 'disagree'):
            module.update_checksums(self.checksums, self.installer, self.release.original)

    def test_duplicate_checksums_rejected(self):
        entry = '0' * 64 + '  setup.exe\n'
        with self.assertRaisesRegex(RuntimeError, 'duplicate'):
            module.parse_checksums(entry + entry)

    def test_signature_requires_verified_timestamp(self):
        with patch.object(module.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, 'Signature verification: ok')):
            with self.assertRaisesRegex(RuntimeError, 'timestamp'):
                module.verify_signature(self.installer)
        with patch.object(module.subprocess, 'run', return_value=subprocess.CompletedProcess(
                [], 0, 'Timestamp Server Signature verification: ok\nSignature verification: ok')):
            module.verify_signature(self.installer)


if __name__ == '__main__':
    unittest.main()
