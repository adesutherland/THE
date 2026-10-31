#!/usr/bin/env python3
"""Bounded atomic replacements through the real editor and both Rexx owners."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
engine = sys.argv[2] if len(sys.argv) > 2 else 'CREXX'


class Session:
    def __init__(self, root, text=b'alpha\nbeta\n', suffix='.txt'):
        self.root = root
        self.sample = root / ('sample' + suffix)
        self.sample.write_bytes(text)
        self.env = dict(os.environ, THE_REXX_INTERPRETER=engine,
                        THE_CREXX_CACHE_DIR=str(root / 'cache'), THE_TOOL_CONFIG_DIR=str(root / 'tools'))
        self.process = subprocess.Popen([str(binary), '--driver', 'llm', '-n', str(self.sample)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, encoding='utf-8', cwd=root, env=self.env)

    def send(self, text):
        self.process.stdin.write(text + '\n')
        self.process.stdin.flush()
        while True:
            line = self.process.stdout.readline()
            assert line, (text, self.process.poll(), self.process.stderr.read())
            if line.startswith('{'):
                return json.loads(line)

    def command(self, text, rc=0):
        result = self.send('command ' + text)
        assert result.get('rc') == rc, (text, result)
        return result

    def query(self, text):
        return json.loads(self.command('query workbench ' + text)['last_message'])

    def token(self):
        return self.query('write')

    def apply(self, token, *changes, rc=0):
        text = f'workbench edit apply document {token["documentId"]} expect {token["writeRevision"]}'
        for line, before, after in changes:
            text += f' line {line} before {quote(before)} after {quote(after)}'
        return self.command(text, rc)

    def undo(self, token, rc=0):
        return self.command(f'workbench edit undo {token["group"]} document {token["documentId"]} expect {token["writeRevision"]}', rc)

    def lines(self):
        return [bytes.fromhex(r['bytesHex']).decode('utf-8') for r in self.query('document public.doc limit 8 bytes 8192')['records']]

    def setup(self):
        self.command('cursor goto 1 1')
        self.command('workbench begin owner public.groups')
        self.command('workbench panel put public.doc kind document buffer active access readonly')
        self.command('workbench commit')

    def close(self):
        if self.process.poll() is None:
            self.send('quit')
            self.process.wait(timeout=10)
        assert self.process.returncode == 0, self.process.stderr.read()


def quote(text):
    return '"' + text.replace('"', '""') + '"'


with tempfile.TemporaryDirectory(prefix='THE edit groups ') as tmp:
    root = Path(tmp)
    s = Session(root)
    try:
        s.setup()
        s.command('set autosave 1')
        s.command('text PENDING')
        before = s.send('look full compact')
        assert s.lines() == ['PENDING', 'beta']
        token = s.token()
        assert s.token()['writeRevision'] == token['writeRevision'], 'inspection changed write token'
        s.apply(token, (1, 'PENDING', 'one'), (2, 'beta', 'two'))
        applied = s.token()
        assert applied['undoAvailable'] and applied['writeRevision'] != token['writeRevision']
        assert s.lines() == ['one', 'two'] and s.sample.read_bytes() == b'alpha\nbeta\n'
        checkpoint = Path(str(s.sample) + '.aus')
        assert checkpoint.read_bytes() == b'one\ntwo\n'
        # Read-only ALT inspection cannot invalidate the immediate group undo.
        s.command('query alt')
        assert s.token()['writeRevision'] == applied['writeRevision']
        s.undo(applied)
        assert s.lines() == ['PENDING', 'beta'] and checkpoint.read_bytes() == b'PENDING\nbeta\n'
        after = s.send('look full compact')
        assert before['focus'] == after['focus'] and before['buffer']['dirty'] == after['buffer']['dirty']
        assert s.sample.read_bytes() == b'alpha\nbeta\n'
        # Whole-line after-images remain literal, including doubled quotes.
        s.apply(s.token(), (1, 'PENDING', 'a "quote"'), (2, 'beta', ''))
        applied = s.token()
        s.apply(applied, (1, 'a "quote"', 'a "quote"'))
        assert s.token() == applied, 'no-op invalidated prior group'
        # A malformed/failing last tuple cannot partly change the first line.
        s.apply(applied, (1, 'a "quote"', 'BAD'), (2, 'wrong', 'BAD'), rc=-3)
        assert s.lines() == ['a "quote"', ''] and s.token()['undoAvailable']
        s.apply(applied, (1, 'a "quote"', 'BAD'), (1, 'a "quote"', 'BAD'), rc=5)
        assert s.token()['writeRevision'] == applied['writeRevision']
        s.undo(applied)
        s.command('save')
        assert s.sample.read_bytes() == b'PENDING\nbeta\n'
        s.apply(s.token(), (1, 'PENDING', 'saved group'), (2, 'beta', 'saved tail'))
        applied = s.token()
        s.command('save')
        assert s.sample.read_bytes() == b'saved group\nsaved tail\n'
        s.undo(applied, rc=-3)
        assert not s.send('look full compact')['buffer']['dirty'] and not s.token()['undoAvailable']
        other = root / 'other.txt'
        other.write_text('keep session alive\n')
        s.command(f'edit {quote(str(other))}')
        s.command(f'edit {quote(str(s.sample))}')
        s.command('qquit')
        s.command(f'edit {quote(str(s.sample))}')
        assert s.token()['documentId'] != token['documentId']
        assert s.sample.read_bytes() == b'saved group\nsaved tail\n'
    finally:
        s.close()

    s = Session(root, b'long original\nbeta\n')
    try:
        s.setup()
        s.command('set trunc 3')
        s.apply(s.token(), (1, 'long original', 'new'))
        s.undo(s.token())
        assert s.lines() == ['long original', 'beta'], 'undo truncated the exact before-image'
        t = s.token()
        s.apply(t, (1, 'long original', 'too long'), rc=5)
        assert s.token()['writeRevision'] == t['writeRevision']
        s.command('set readonly on file')
        s.apply(s.token(), (1, 'long original', 'new'), rc=12)
        assert s.lines() == ['long original', 'beta']
        s.command('set readonly off file')
        s.command('set trunc *')
        s.command('set alt 65535 65535')
        s.apply(s.token(), (1, 'long original', 'new'))
        assert '65535' in s.command('query alt')['last_message'] and s.send('look full compact')['buffer']['dirty']
        s.undo(s.token())
    finally:
        s.close()

    s = Session(root)
    try:
        s.setup()
        s.command('set synonym workbench text SHADOWED')
        old = s.token()
        s.apply(old, (2, 'beta', 'NO'))
        assert s.token()['writeRevision'] != old['writeRevision'] and s.lines()[1] == 'beta'
        s.command('set synonym workbench')
        s.command('sos undo')
        assert s.lines() == ['alpha', 'beta']
        s.command('set linend on #')
        old = s.token()
        combined = (f'workbench edit apply document {old["documentId"]} expect {old["writeRevision"]}'
                    ' line 1 before "alpha" after "NO"#input POISON')
        s.command(combined, rc=5)
        assert s.lines() == ['alpha', 'beta'], 'mixed LINEND admission executed an extra command'
        s.command('set linend off')
        s.command('define enter input ENTER_SIDE_EFFECT')
        s.send('focus command')
        old = s.token()
        s.send('text workbench edit apply document 1 expect 1 line 1 before "alpha" after "NO"')
        s.send('key c-m')
        current, copied = s.token(), s.lines()
        assert current['writeRevision'] != old['writeRevision'] and 'ENTER_SIDE_EFFECT' in copied, (old, current, copied)
    finally:
        s.close()

    # The same admitted command can be physically typed into cmd_rec. A direct
    # function binding to SAVE must still invalidate the group afterward.
    s = Session(root)
    try:
        s.setup()
        s.command('define enter enter cua')
        s.command('define f2 save')
        s.send('focus command')
        token = s.token()
        line = (f'workbench edit apply document {token["documentId"]} expect {token["writeRevision"]}'
                ' line 1 before "alpha" after "typed" line 2 before "beta" after "tail"')
        s.send('text ' + line)
        assert s.token()['writeRevision'] == token['writeRevision'], 'command-record insertion invalidated token'
        s.send('key enter')
        assert s.lines() == ['typed', 'tail']
        applied = s.token()
        assert applied['undoAvailable']
        s.send('key f2')
        assert s.sample.read_bytes() == b'typed\ntail\n'
        s.undo(applied, rc=-3)
        assert not s.send('look full compact')['buffer']['dirty']
    finally:
        s.close()

    # Runtime refusals cannot poison a separate definition transaction.
    s = Session(root)
    try:
        s.setup()
        s.command('workbench begin owner public.independent')
        s.command('workbench form put public.independent')
        t = s.token()
        s.apply(t, (1, 'WRONG', 'no'), rc=-3)
        s.command('workbench commit')
        assert s.query('forms')['total'] == 1 and s.lines() == ['alpha', 'beta']
        # Modal READ does not dispatch EDIT, preserving its owner and file.
        t = s.token()
        modal = s.send('command read nochange tag')['panel']
        blocked = s.send(f'command workbench edit apply document {t["documentId"]} expect {t["writeRevision"]} line 1 before "alpha" after "NO"')
        assert blocked['panel'] == modal
        s.send('key esc')
        assert s.lines() == ['alpha', 'beta']
    finally:
        s.close()

    # Native DSLSH snapshots are retired before publication. Explicit parser
    # reselection is a separate operation and must parse current complete bytes.
    parser = next((p for p in [Path(os.environ.get('THE_SDSLH_TP', '/not-present')),
        Path.home() / '.local/bin/tp', Path(__file__).resolve().parents[2] / 'DSL-Syntax-Highlighter/cmake-build-debug/toyparser/tp'] if p.is_file()), None)
    if parser and os.environ.get('THE_EDITGROUP_NO_PARSER') != '1':
        s = Session(root, b'int x = 1\n', '.toy')
        try:
            s.setup()
            s.command(f'set sdslh tp {parser} -d')
            s.command('set autocolor *.toy tp')
            s.command('set coloring on auto')
            s.command('sdslhwait 5000')
            assert s.send('look full compact')['diagnostics'], 'toy parser did not expose original diagnostic'
            s.apply(s.token(), (1, 'int x = 1', 'int x = 1;'))
            applied = s.token()
            assert not applied['parserAvailable'] and not s.send('look full compact').get('diagnostics', [])
            s.undo(applied)
            assert not s.token()['parserAvailable']
            s.command('sdslhwait 5000', rc=5)
            s.command('set coloring on auto')
            s.command('sdslhwait 5000')
            assert s.send('look full compact')['diagnostics'], 'explicit reactivation did not parse restored source'
            s.apply(s.token(), (1, 'int x = 1', 'int x = 1;'))
            s.command('set coloring on auto')
            s.command('sdslhwait 5000')
            assert not s.send('look full compact').get('diagnostics', []), 'reactivated parser saw stale source'
        finally:
            s.close()

    # The macro owns synchronous admitted commands; returning alone must not
    # stale its successful group. Nested pending TEXT must stale an old token.
    for mutate in ('none', 'text', 'navigate'):
        s = Session(root)
        try:
            s.setup()
            macro = root / 'group.the'
            if engine == 'CREXX':
                source = 'options levelb\nworkbench=.string[]\n'
                source += "address the 'extract /workbench write/' expose workbench[];\njson=workbench[1]\n"
            else:
                source = "/* Rexx */\naddress the 'extract /workbench write/';\njson=workbench.1\n"
                if mutate != 'none':
                    source = '/* Rexx */\nsignal on error name stale\n' + source
            source += "parse var json . '\"documentId\":\"' id '\"' . '\"writeRevision\":\"' rev '\"' .\n"
            if mutate == 'text':
                source += "address the 'text PENDING';\n"
            elif mutate == 'navigate':
                source += "address the 'cursor goto 2 1';\n"
            source += "address the 'workbench edit apply document ' || id || ' expect ' || rev || ' line 2 before \"beta\" after \"macro\"';\n"
            if mutate != 'none' and engine == 'REGINA':
                source += 'return 84\nstale:\nreturn 0\n'
            else:
                source += ('if rc <> -3 then return 81\n' if mutate != 'none' else 'if rc <> 0 then return 82\n')
                source += 'return 0\n'
            macro.write_text(source)
            s.command(f'macro {quote(str(macro))}')
            if mutate != 'none':
                assert s.lines() == (['PENDING', 'beta'] if mutate == 'text' else ['alpha', 'beta'])
                assert s.query('summary')['lastResult']['code'] == 'stale-revision'
            else:
                copied, metadata = s.lines(), s.token()
                assert copied == ['alpha', 'macro'] and metadata['undoAvailable'], (copied, metadata)
                s.undo(s.token())
                assert s.lines() == ['alpha', 'beta']
        finally:
            s.close()
print('atomic edit group real-editor checks passed:', engine)
