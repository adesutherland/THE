#!/usr/bin/env python3
"""Clipboard transactions through the real editor owner, including save/reopen."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

BINARY = Path(sys.argv[1]).resolve()
ENGINE = sys.argv[2] if len(sys.argv) > 2 else 'CREXX'


class Session:
    def __init__(self, root, content):
        self.root = root
        self.path = root / 'clipboard.txt'
        self.path.write_bytes(content)
        self.process = subprocess.Popen(
            [str(BINARY), '--driver', 'llm', '-n', str(self.path)], cwd=root,
            env=dict(os.environ, THE_REXX_INTERPRETER=ENGINE,
                     THE_CREXX_CACHE_DIR=str(root / 'cache'), THE_TOOL_CONFIG_DIR=str(root / 'tools')),
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, encoding='utf-8')
        self.command('set eolout lf')
        self.command('set interface cua')
        self.command('define c-z sos undo')
        self.command('workbench begin owner public.clipboard')
        self.command('workbench panel put public.doc kind document buffer active access readonly')
        self.command('workbench commit')
        self.send('focus filearea')

    def send(self, line):
        self.process.stdin.write(line + '\n')
        self.process.stdin.flush()
        while True:
            result = self.process.stdout.readline()
            assert result, (line[:100], self.process.poll(), self.process.stderr.read())
            if result.startswith('{'):
                return json.loads(result)

    def command(self, command, rc=0):
        result = self.send('command ' + command)
        assert result['rc'] == rc, (command, result)
        return result

    def clip(self, command, rc=0):
        result = self.send('clipboard ' + command)
        assert result['rc'] == rc, (command[:100], result)
        return result

    def paste(self, text, token=None, rc=0):
        token = token or self.clip('capture')['token']
        encoded = text.encode('utf-8') if isinstance(text, str) else text
        if len(encoded) < 1800 and encoded:
            return self.clip(f'paste {token} {encoded.hex()}', rc)
        self.clip(f'begin {token}')
        for start in range(0, len(encoded), 1500):
            self.clip(f'chunk {token} {encoded[start:start + 1500].hex()}')
        return self.clip(f'commit {token}', rc)

    def view(self):
        return self.send('look full compact')

    def lines(self):
        page = json.loads(self.command('query workbench document public.doc limit 8 bytes 8192')['last_message'])
        return [bytes.fromhex(record['bytesHex']).decode('utf-8') for record in page['records']]

    def close(self):
        if self.process.poll() is None:
            self.send('quit')
            self.process.wait(timeout=10)
        assert self.process.returncode == 0, self.process.stderr.read()


with tempfile.TemporaryDirectory(prefix='THE clipboard ') as temporary:
    root = Path(temporary)
    s = Session(root, b'')
    try:
        before = s.view()
        assert not before['buffer']['dirty']
        s.paste('first\r\nλ界\nlast')
        assert s.lines() == ['first', 'λ界', 'last']
        after = s.view()
        assert after['buffer']['dirty'] and after['focus']['line'] == 3, after['focus']
        assert after['history'] == {'undo': 1, 'redo': 0}
        assert s.path.read_bytes() == b''
        s.clip('undo')
        assert s.lines() == [] and not s.view()['buffer']['dirty']
        assert s.view()['history'] == {'undo': 0, 'redo': 0}
        s.send('text typed')
        assert s.lines() == ['typed'] and s.view()['buffer']['dirty']
        s.send('key c-z')
        assert s.lines() == [] and not s.view()['buffer']['dirty']
        s.paste('saved λ')
        s.command('save')
        assert s.path.read_bytes() == 'saved λ\n'.encode()
        other = root / 'other.txt'
        other.write_bytes(b'keep session open\n')
        s.command(f'edit "{other}"')
        s.command(f'edit "{s.path}"')
        s.command('qquit')
        s.command(f'edit "{s.path}"')
        s.command('workbench begin owner public.clipboard')
        s.command('workbench panel put public.doc kind document buffer active access readonly')
        s.command('workbench commit')
        assert s.lines() == ['saved λ'] and not s.view()['buffer']['dirty']
    finally:
        s.close()

    s = Session(root, b'')
    try:
        s.command('set case upper')
        s.send('text cased')
        assert s.lines() == ['CASED']
        s.clip('undo')
        assert s.lines() == [] and not s.view()['buffer']['dirty']
        s.send('text cased')
        s.command('save')
        assert s.path.read_bytes() == b'CASED\n'
        s.clip('selectAll')
        s.paste('MiXeD')
        assert s.lines() == ['MiXeD'], 'clipboard paste must remain literal'
        s.clip('undo')
        assert s.lines() == ['CASED']
    finally:
        s.close()

    s = Session(root, b'return0\nother\n')
    try:
        s.command('cursor goto 1 4')
        s.command('mark cua')
        s.command('mark cua right')
        s.command('mark cua right')
        before = s.view()
        copied = s.clip('copy')
        assert copied['text'] == 'ur', copied
        s.paste('XYZ', copied['token'])
        assert s.lines() == ['retXYZn0', 'other']
        # Selection and copy are presentation/read operations: neither may
        # discard the complete replacement transaction waiting for Undo.
        s.clip('selectAll')
        exported = s.clip('copy')
        assert exported['text'] == 'retXYZn0\nother'
        assert s.view()['history']['undo']
        s.clip('undo')
        assert s.lines() == ['return0', 'other']
        after = s.view()
        assert after['focus'] == before['focus']
        assert after['buffer']['dirty'] == before['buffer']['dirty']
        # A caller that cannot write the OS clipboard simply sends no Cut.
        s.clip('selectAll')
        copied = s.clip('copy')
        assert s.lines() == ['return0', 'other']
        # Movement between copy and OS completion makes Cut stale.
        s.send('key right')
        s.clip('cut ' + copied['token'], -3)
        assert s.lines() == ['return0', 'other']
        s.command('reset block')
        s.command('cursor goto 1 1')
        # This command leaves the current record pending in the real core.
        s.command('text PENDING')
        s.clip('selectAll')
        copied = s.clip('copy')
        assert copied['text'] == 'PENDING\nother', copied
        before = s.view()
        s.paste('α\r\nβ\nγ', copied['token'])
        assert s.lines() == ['α', 'β', 'γ']
        s.clip('undo')
        assert s.lines() == ['PENDING', 'other']
        assert s.view()['buffer']['dirty'] == before['buffer']['dirty']
        s.command('save')
        assert s.path.read_bytes() == b'PENDING\nother\n'
        s.clip('selectAll')
        copied = s.clip('copy')
        s.clip('cut ' + copied['token'])
        assert s.lines() == []
        s.clip('undo')
        assert s.lines() == ['PENDING', 'other']
        s.command('set readonly on file')
        s.clip('selectAll')
        copied = s.clip('copy')
        assert copied['text'] == 'PENDING\nother' and copied['readOnly']
        s.clip('cut ' + copied['token'], 12)
        s.paste('no', copied['token'], 12)
        assert s.lines() == ['PENDING', 'other']
    finally:
        s.close()

    s = Session(root, b'source remains\n')
    try:
        s.send('focus command')
        before = s.view()
        s.paste('query version')
        after = s.view()
        assert after['command'] == 'query version', after
        assert not after['buffer']['dirty']
        s.paste('do not\nexecute', rc=5)
        assert s.view()['command'] == 'query version'
        copied = s.clip('copy')
        assert copied['text'] == 'query version' and copied['selectionScope'] == 'field'
        s.clip('cut ' + copied['token'])
        assert s.view()['command'] == ''
        s.clip('undo')
        assert s.view()['command'] == 'query version'
        assert s.path.read_bytes() == b'source remains\n'
        assert not s.view()['buffer']['dirty']
        s.send('focus prefix')
        before = s.view()
        s.paste('dd')
        assert not s.view()['buffer']['dirty']
        copied = s.clip('copy')
        assert copied['text'] == 'dd' and copied['selectionScope'] == 'field'
        s.paste('x\ny', rc=5)
        s.clip('cut ' + s.clip('copy')['token'])
        assert s.clip('copy')['text'] == ''
        s.clip('undo')
        assert s.clip('copy')['text'] == 'dd'
        assert s.path.read_bytes() == b'source remains\n'
        assert not s.view()['buffer']['dirty']
    finally:
        s.close()

    s = Session(root, b'keep\n')
    try:
        s.command('cursor goto 1 1')
        s.clip('selectAll')
        before = s.view()
        s.paste('')
        assert s.lines() == ['keep'] and s.view()['buffer']['dirty'] == before['buffer']['dirty']
        s.clip('selectAll')
        for invalid in [b'a\x00b', b'\xc0\xaf', b'\xed\xa0\x80', b'\xf4\x90\x80\x80', b'a\x1bb']:
            s.paste(invalid, rc=5)
            assert s.lines() == ['keep'] and s.view()['buffer']['dirty'] == before['buffer']['dirty']
        s.command('set trunc 3')
        s.paste('refuse long line', rc=94)
        assert s.lines() == ['keep']
        s.command('set trunc *')
        # 192 KiB is far larger than the legacy 128-event/1024-byte paths.
        s.clip('selectAll')
        large = ('λ' * 96 + '\r\n') * 1024
        s.paste(large)
        s.command('save')
        normalized = large.replace('\r\n', '\n') + '\n'
        assert s.path.read_bytes() == normalized.encode()
        assert s.view()['focus']['line'] == 1025
        # A line limit refusal must not insert even its valid first prefix.
        s.clip('selectAll')
        s.paste('ok\n' + 'X' * 40000, rc=94)
        assert s.path.read_bytes() == normalized.encode()
        assert not s.view()['buffer']['dirty']
        # Overflow poisons a staged transfer: a later Commit cannot publish a
        # previously accepted prefix after the transport already refused it.
        token = s.clip('capture')['token']
        s.clip(f'begin {token}')
        payload = b'x' * 1500
        for _ in range(699):
            s.clip(f'chunk {token} {payload.hex()}')
        s.clip(f'chunk {token} {payload.hex()}', 94)
        s.clip(f'commit {token}', 94)
        assert s.path.read_bytes() == normalized.encode()
        assert not s.view()['buffer']['dirty']
        token = s.clip('capture')['token']
        s.clip(f'begin {token}')
        s.clip(f'chunk {token} 41')
        s.clip(f'chunk {token} ' + '42' * 3000, 5)
        s.clip(f'commit {token}', 5)
        assert s.path.read_bytes() == normalized.encode()
        assert not s.view()['buffer']['dirty']
    finally:
        s.close()

    for hex_text, expected in [("X'4142'", b'AB\n'), ("X'0041'", None)]:
        if expected is None:
            baseline = Session(root, b'\n')
            try:
                baseline.command('cursor goto 1 1')
                baseline.command('set hex on')
                baseline.command('text ' + hex_text)
                baseline.command('save')
                expected = baseline.path.read_bytes()
            finally:
                baseline.close()
        s = Session(root, b'')
        try:
            s.command('set hex on')
            s.command('text ' + hex_text)
            s.command('save')
            assert s.path.read_bytes() == expected
        finally:
            s.close()

    s = Session(root, b'tagged\n')
    try:
        s.command('cursor goto 1 1')
        s.command('set lineflag nochange new tag', rc=1)
        before = s.command('query lineflag')['last_message'].lower().split()
        assert 'new' in before and 'tag' in before and 'nochange' in before
        s.paste('X')
        flags = s.command('query lineflag')['last_message'].lower().split()
        assert 'new' in flags and 'tag' in flags and 'change' in flags
        s.clip('undo')
        assert s.command('query lineflag')['last_message'].lower().split() == before
        assert s.lines() == ['tagged']
    finally:
        s.close()

    s = Session(root, b'one\ntwo\nthree\n')
    try:
        s.command('cursor goto 2 1')
        s.command('mark line 2 2')
        copied = s.clip('copy')
        assert copied['text'] == 'two\n'
        s.clip('cut ' + copied['token'])
        assert s.lines() == ['one', 'three']
        s.clip('undo')
        assert s.lines() == ['one', 'two', 'three']
        s.command('mark line 2 2')
        s.paste('α\nβ\n')
        assert s.lines() == ['one', 'α', 'β', 'three']
        s.clip('undo')
        assert s.lines() == ['one', 'two', 'three']
        s.clip('selectAll')
        s.clip('cut ' + s.clip('copy')['token'])
        assert s.lines() == []
        assert s.view()['focus']['line'] == 1
        s.command('save')
        assert s.path.read_bytes() == b''
        s.send('text after-empty-cut')
        assert s.lines() == ['after-empty-cut']
        s.command('save')
        assert s.path.read_bytes() == b'after-empty-cut\n'
    finally:
        s.close()

    # Structural replacements refuse contexts whose extra line references or
    # queued prefix commands cannot be published atomically by this operation.
    s = Session(root, b'preserve\n')
    try:
        s.command('cursor goto 1 1')
        s.command('mark box 1 1 1 2')
        s.clip('capture', 5)
        assert s.lines() == ['preserve'] and not s.view()['buffer']['dirty']
        s.command('reset block')
        s.send('focus prefix')
        s.paste('dd')
        s.send('focus filearea')
        s.paste('NO', rc=-3)
        assert s.lines() == ['preserve'] and not s.view()['buffer']['dirty']
    finally:
        s.close()
    s = Session(root, b'views\n')
    try:
        s.command('set screen 2 horizontal')
        s.send('focus filearea')
        s.paste('NO', rc=-3)
        assert s.lines() == ['views'] and not s.view()['buffer']['dirty']
    finally:
        s.close()

    parser = next((path for path in [Path(os.environ.get('THE_SDSLH_TP', '/not-present')),
        Path.home() / '.local/bin/tp', Path(__file__).resolve().parents[2] /
        'DSL-Syntax-Highlighter/cmake-build-debug/toyparser/tp'] if path.is_file()), None)
    if parser and os.environ.get('THE_EDITGROUP_NO_PARSER') != '1':
        s = Session(root, b'int original = 1;\n')
        try:
            s.command(f'set sdslh tp {parser} -d')
            s.command('set autocolor *.txt tp')
            s.command('set coloring on auto')
            s.command('sdslhwait 5000')
            assert any(row.get('s') for row in s.view()['screen_rows'] if row['line'] == 1)
            s.command('cursor goto 1 1')
            s.command('text int pending_name = 999')
            s.clip('selectAll')
            copied = s.clip('copy')
            assert copied['text'] == 'int pending_name = 999'
            s.paste('int good = 42;\nint two = 2;', copied['token'])
            s.command('sdslhwait 5000')
            pasted = s.view()
            assert pasted['history']['undo'] and not pasted.get('diagnostics', [])
            assert any(row.get('s') for row in pasted['screen_rows'] if row['line'] == 1)
            s.clip('undo')
            s.command('sdslhwait 5000')
            restored = s.view()
            assert restored.get('diagnostics'), 'Undo parser ignored the restored pending record'
            assert any(row.get('s') for row in restored['screen_rows'] if row['line'] == 1)
            assert s.lines() == ['int pending_name = 999']
            assert s.path.read_bytes() == b'int original = 1;\n'
        finally:
            s.close()

print(f'clipboard owner runtime passed ({ENGINE})')
