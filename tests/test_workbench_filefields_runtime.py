#!/usr/bin/env python3
"""CTLCHAR projections through the real editor: bytes, drafts, pinning and undo."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
engine = sys.argv[2] if len(sys.argv) > 2 else 'CREXX'


def quote(value):
    return '"' + value.replace('"', '""') + '"'


class Session:
    def __init__(self, root, original, profile=None):
        self.sample = root / 'fields.txt'
        self.sample.write_bytes(original)
        arguments = [str(binary), '--driver', 'llm'] + (['-p', str(profile)] if profile else ['-n']) + [str(self.sample)]
        self.stderr_capture = tempfile.TemporaryFile()
        try:
            self.process = subprocess.Popen(arguments,
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.stderr_capture, text=True,
                encoding='utf-8', cwd=root, env=dict(os.environ, THE_REXX_INTERPRETER=engine,
                    THE_CREXX_CACHE_DIR=str(root / 'cache'), THE_TOOL_CONFIG_DIR=str(root / 'tools')))
        except BaseException:
            self.stderr_capture.close()
            raise

    def send(self, text):
        self.process.stdin.write(text + '\n')
        self.process.stdin.flush()
        while True:
            line = self.process.stdout.readline()
            assert line, (text, self.process.poll())
            if line.startswith('{'):
                return json.loads(line)

    def command(self, text, rc=0):
        result = self.send('command ' + text)
        assert result.get('rc') == rc, (text, result)
        return result

    def query(self, text):
        return json.loads(self.command('query workbench ' + text)['last_message'])

    def fields(self):
        return self.query('filefields public.fields')

    def typed(self, operation, field=None, value=None, rc=0, state=None):
        state = state or self.fields()
        command = f'filefield {operation} public.fields {state["panelGeneration"]} {state["draftRevision"]}'
        if field:
            command += ' ' + field + ' ' + (value.encode().hex() or '-')
        result = self.send(command)
        assert result.get('operation') == 'filefield-receipt' and result.get('rc') == rc, (command, result)
        return result

    def setup(self):
        for command in ['set eolout lf', 'set linend off', 'set ctlchar ! escape', 'set ctlchar p protect white',
                        'set ctlchar u noprotect green', 'set filectlchar on']:
            self.command(command)
        self.command('workbench begin owner public.fields')
        self.command('workbench panel put public.doc kind document buffer active access readonly')
        self.command('workbench panel put public.fields kind fields buffer active markup ctlchar access staged')
        self.command('workbench commit')

    def lines(self):
        page = self.query('document public.doc limit 8 bytes 8192')
        return [bytes.fromhex(record['bytesHex']).decode() for record in page['records']]

    def close(self):
        failed = sys.exc_info()[0] is not None
        try:
            if self.process.poll() is None:
                try:
                    self.process.stdin.write('quit\n')
                    self.process.stdin.flush()
                    self.process.wait(timeout=2 if failed else 10)
                except (OSError, subprocess.TimeoutExpired):
                    self.process.kill()
                    self.process.wait(timeout=5)
                    if not failed:
                        raise
            if not failed:
                assert self.process.returncode == 0, self.process.returncode
        finally:
            # Read only after the editor stops, avoiding both pipe backpressure
            # and seeking the inherited file descriptor while it is writing.
            if failed or sys.exc_info()[0] is not None or self.process.returncode:
                self.stderr_capture.seek(0)
                sys.stderr.write(self.stderr_capture.read().decode('utf-8', errors='replace'))
            for stream in (self.process.stdin, self.process.stdout, self.stderr_capture):
                try:
                    stream.close()
                except OSError:
                    pass


with tempfile.TemporaryDirectory(prefix='THE file fields ') as directory:
    root = Path(directory)
    original = ('!pName: !uGuest     !p protected' + 'Z' * 140 + '\n!pAge: !u42 !p END\n').encode()
    s = Session(root, original)
    try:
        s.setup()
        initial = s.fields()
        assert initial['active'] and not initial['dirty'] and initial['total'] == 2
        assert initial['records'][0]['spans'][2]['value'] == ' protected' + 'Z' * 140
        modal = s.send('command read nochange tag')['panel']
        blocked = s.typed('set', 's2', 'REFUSED', rc=-3, state=initial)
        assert blocked['code'] == 'busy-modal'
        assert s.send('look')['panel'] == modal
        malformed = s.send(('filefield set public.fields ' + initial['panelGeneration'] + ' ' + initial['draftRevision'] + ' s2 41').ljust(8191) + 'text POISON')
        assert malformed['rc'] == 5 and malformed['code'] == 'invalid-operand'
        assert s.send('look')['panel'] == modal
        assert s.send('key esc')['rc'] == 4
        assert s.fields() == initial
        s.typed('reload')  # Modal READ admission conservatively stales write tokens.
        s.command('workbench panel show public.fields')
        s.command('workbench panel focus public.fields')
        before = s.send('look full compact')
        s.typed('set', 's2', '中é "')
        draft = s.fields()
        assert draft['dirty'] and draft['records'][0]['spans'][1]['value'] == '中é "     '
        assert s.sample.read_bytes() == original and not s.send('look full compact')['buffer']['dirty']
        s.typed('set', 's1', 'NO', rc=12)
        s.typed('set', 's2', '!pPOISON', rc=5)
        assert s.fields() == draft, 'refusal changed staged values'
        s.typed('apply')
        applied = s.query('write')
        assert applied['undoAvailable']
        expected = original.decode().replace('Guest     ', '中é "     ')
        assert s.lines() == expected.splitlines()
        assert s.sample.read_bytes() == original
        assert s.send('look full compact')['focus'] == before['focus']
        s.command(f'workbench edit undo {applied["group"]} document {applied["documentId"]} expect {applied["writeRevision"]}')
        assert s.lines() == original.decode().splitlines()
        # Stale projection retains its accepted baseline and draft until Reload.
        s.typed('set', 's2', 'Draft')
        stale = s.fields()
        s.typed('apply', rc=-3)
        assert s.fields() == stale
        s.typed('reload')
        assert s.fields()['records'][0]['spans'][1]['value'] == 'Guest     '
        # Pending rec is captured in full without a post/save. A refused close
        # must not publish it or change dirty state, caret, prefix or disk.
        pending = '!pName: !uPending   !p protected' + 'Z' * 140
        s.command('text ' + pending)
        pending_before = s.send('look full compact')
        s.typed('reload')
        assert s.fields()['records'][0]['spans'][1]['value'] == 'Pending   '
        for command in ['qquit', 'quit', 'file', 'ffile', 'cancel', 'ccancel', 'redit']:
            s.command(command, -3)
            after = s.send('look full compact')
            assert after['focus'] == pending_before['focus'] and after['buffer'] == pending_before['buffer'], (command, after)
            assert s.sample.read_bytes() == original
        s.typed('reload')
        s.typed('set', 's2', 'Saved')
        s.typed('set', 's5', '99')
        s.typed('apply')
        s.command('save')
        saved = s.sample.read_bytes()
        assert saved == original.replace(b'Guest     ', b'Saved     ').replace(b'42 ', b'99 ')
        s.typed('set', 's2', 'Discard')
        s.typed('cancel')
        cancelled = s.fields()
        assert not cancelled['active'] and not cancelled['dirty']
        assert cancelled['records'][0]['spans'][1]['value'] == 'Saved     '
        s.typed('set', 's2', 'NO', rc=-3)
        other = root / 'other.txt'
        other.write_text('keep ring alive\n')
        s.command(f'edit {quote(str(other))}')
        s.command(f'edit {quote(str(s.sample))}')
        s.command('qquit')
        s.command(f'edit {quote(str(s.sample))}')
        assert s.sample.read_bytes() == saved and s.query('write')['documentId'] != initial['documentId']
    finally:
        s.close()

    # Every configuration/policy refusal retains the entire existing draft.
    s = Session(root, b'!pName: !uGuest     !p END\n')
    try:
        s.setup()
        s.typed('set', 's2', 'Draft')
        draft = s.fields()
        s.command('set readonly on file')
        s.typed('apply', rc=12)
        assert s.fields() == draft
        s.command('set readonly off file')
        s.command('set ctlchar p noprotect white')
        s.typed('apply', rc=-3)
        assert s.fields() == draft
        s.command('set ctlchar p protect white')
        s.typed('reload')
        s.command('set trunc 5')
        s.typed('apply', rc=-3)
        assert not s.send('look full compact')['buffer']['dirty']
        s.typed('cancel')
    finally:
        s.close()

    # A trusted native profile may prepare copied captures only at its final
    # successful publication. APPLY still uses ordinary activation write gates.
    def profile(name, result=0):
        path = root / name
        commands = ['set linend off', 'set ctlchar ! escape', 'set ctlchar p protect white',
                    'set ctlchar u noprotect green', 'set filectlchar on',
                    'workbench begin owner public.profile',
                    'workbench form put public.form',
                    'workbench field put public.form value label "Value" value "baseline" access edit maxcells 20',
                    'workbench panel put public.log kind log model public.activationlog',
                    'workbench panel put public.doc kind document buffer active access readonly',
                    'workbench panel put public.fields kind fields buffer active markup ctlchar access staged',
                    'workbench commit']
        source = ('options levelb\nimport rxfnsb\n' if engine == 'CREXX' else '/* Rexx */\n') + 'address THE\n'
        source += ''.join("'" + command.replace("'", "''") + "';\n" for command in commands)
        path.write_text(source + f'return {result}\n')
        return path

    original = b'!pName: !uGuest     !p END\n'
    s = Session(root, original, profile('successful-fields.the'))
    try:
        summary, captured = s.query('summary'), s.fields()
        assert summary['panels'] == 3 and summary['forms'] == 1 and summary['logs'] == 1, summary
        assert captured['active'] and not captured['dirty']
        assert captured['writeRevision'] == s.query('write')['writeRevision'], 'profile completion staled its capture'
        assert not s.send('look full compact')['buffer']['dirty'] and s.sample.read_bytes() == original
        s.typed('set', 's2', 'Profile')
        s.typed('apply')  # No Reload or mutation-token bypass is needed after publication.
        assert s.lines() == ['!pName: !uProfile   !p END'] and s.sample.read_bytes() == original
        s.typed('cancel')
    finally:
        s.close()

    for name, result, source in [('failed-fields.the', 7, original),
                                 ('invalid-capture-fields.the', 0, b'!pNo editable field here\n')]:
        s = Session(root, source, profile(name, result))
        try:
            summary = s.query('summary')
            assert summary['panels'] == summary['forms'] == summary['fields'] == summary['logs'] == 0, summary
            assert summary['lastResult']['code'] == 'activation-failed', summary
            assert not summary['activationActive'] and not summary['transactionActive']
            assert s.sample.read_bytes() == source and not s.send('look full compact')['buffer']['dirty']
        finally:
            s.close()

print('workbench file-field runtime tests passed')
