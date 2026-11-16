#!/usr/bin/env python3
"""Independent form values through the real editor and Rexx inspection bridge."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
engine = sys.argv[2] if len(sys.argv) > 2 else 'CREXX'
with tempfile.TemporaryDirectory(prefix='THE forms ') as directory, tempfile.TemporaryFile() as stderr_capture:
    root = Path(directory)
    sample = root / 'sample.txt'
    original = b'alpha\nbeta\n'
    sample.write_bytes(original)
    env = dict(os.environ, THE_REXX_INTERPRETER=engine, THE_CREXX_CACHE_DIR=str(root / 'cache'),
               THE_TOOL_CONFIG_DIR=str(root / 'tools'))
    process = subprocess.Popen([str(binary), '--driver', 'llm', '-n', str(sample)],
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=stderr_capture,
                               text=True, encoding='utf-8', env=env)

    def send(text):
        process.stdin.write(text + '\n')
        process.stdin.flush()
        while True:
            line = process.stdout.readline()
            assert line, (text, process.poll())
            if line.startswith('{'):
                return json.loads(line)

    def command(text, expected=0):
        result = send('command ' + text)
        assert result.get('rc') == expected, (text, result)
        return result

    def query(text):
        return json.loads(command('query workbench ' + text)['last_message'])

    def state():
        return next(record for record in query('forms')['records'] if record['id'] == 'public.person')

    def quote(value):
        return '"' + value.replace('"', '""') + '"'

    def set_value(value, field='name', expected=0):
        s = state()
        command(f'workbench field set public.person {field} value {quote(value)} expect {s["draftRevision"]} generation {s["generation"]}', expected)

    def operation(name):
        s = state()
        command(f'workbench form {name} public.person expect {s["draftRevision"]} generation {s["generation"]}')

    try:
        command('cursor goto 2 3')
        before = send('look full compact')
        command('workbench begin owner public.forms')
        command('workbench panel put public.pane kind fields form public.person')
        command('workbench form put public.person')
        command('workbench field put public.person name label "Name < & >" value "Guest" access edit maxcells 1024')
        command('workbench field put public.person fixed label "Protected" value "Fixed" access readonly maxcells 20')
        command('workbench commit')
        initial = state()
        definition_revision = query('summary')['revision']
        assert initial['fieldCount'] == 2 and not initial['dirty'] and not initial['submitted'], initial
        fields = query('fields public.person')['records']
        assert fields[0]['value'] == 'Guest' and fields[1]['access'] == 'readonly', fields

        # READ owns synchronous input, but every typed request still completes.
        # Refusing a form SET must preserve the modal draft as well as the form.
        modal = send('command read nochange tag')['panel']
        modal = send('text SAFE_MODAL')['panel']
        typed = (f'form set public.person {initial["generation"]} '
                 f'{initial["draftRevision"]} name {"REFUSED".encode().hex()}')
        refusal = send(typed)
        assert refusal['operation'] == 'form-receipt' and refusal['rc'] == -3 and refusal['code'] == 'busy-modal', refusal
        assert refusal['draftRevision'] == initial['draftRevision'], refusal
        busy_request_id = refusal['requestId']
        assert send('look')['panel'] == modal, 'typed SET changed the active READ model'
        # A suffix beginning at the next fgets boundary used to become a new
        # modal text input. Drain it and return exactly one malformed receipt.
        refusal = send(typed.ljust(8191) + 'text POISON')
        assert refusal['operation'] == 'form-receipt' and refusal['rc'] == 5 and refusal['code'] == 'invalid-operand', refusal
        assert send('look')['panel'] == modal, 'oversized form tail became modal input'
        assert send('key esc')['rc'] == 4
        assert state() == initial and query('fields public.person')['records'] == fields
        admitted = send(typed)
        assert admitted['operation'] == 'form-receipt' and admitted['rc'] == 0 and admitted['code'] == 'ok', admitted
        assert int(admitted['requestId']) > int(busy_request_id), admitted
        assert query('fields public.person')['records'][0]['value'] == 'REFUSED'
        operation('cancel')
        initial = state()
        poison = '中é " <tag> / # OS input POISON'
        set_value(poison)
        changed = state()
        assert changed['dirty'] and changed['generation'] == initial['generation'], changed
        assert query('summary')['revision'] == definition_revision
        assert query('fields public.person')['records'][0]['value'] == poison
        command(f'workbench form submit public.person expect {initial["draftRevision"]}', -3)
        assert query('summary')['lastResult']['code'] == 'stale-revision'
        set_value('No', 'fixed', 12)
        assert query('summary')['lastResult']['code'] == 'field-protected'
        operation('submit')
        assert state()['submitted'] and not state()['dirty']
        assert query('form public.person result')['records'][0]['value'] == poison
        set_value('Discard')
        operation('cancel')
        assert query('fields public.person')['records'][0]['value'] == poison
        set_value('x' * 1024)
        assert query('fields public.person')['records'][0]['length'] == 1024
        set_value('x' * 1025, expected=94)
        operation('cancel')
        set_value('Staged')
        command('workbench begin owner public.forms')
        command('workbench field put public.person name label "Replacement" value "New" access edit maxcells 40')
        command('workbench commit', -3)
        assert query('summary')['lastResult']['code'] == 'draft-in-use'
        assert query('fields public.person')['records'][0]['value'] == 'Staged'
        command('workbench abort')
        operation('cancel')

        # A local field name is independent in another owner's form.
        command('workbench begin owner other.forms')
        command('workbench form put other.person')
        command('workbench field put other.person name label "Other" value "Other" access edit maxcells 40')
        command('workbench commit')
        assert query('fields other.person')['records'][0]['value'] == 'Other'
        assert query('fields public.person')['records'][0]['value'] == poison

        probe = root / 'form-extract.the'
        if engine == 'CREXX':
            source = 'options levelb\nworkbench=.string[]\n'
            expose, value = ' expose workbench[]', 'workbench[1]'
        else:
            source, expose, value = '/* Rexx */\n', '', 'workbench.1'
        source += "address the 'extract /workbench form public.person result/'" + expose + ';\n'
        source += 'saved=' + value + '\n'
        source += "address the 'extract /workbench fields public.person bytes 1/'" + expose + ';\n'
        source += 'if rc <> 94 then return 81\nif ' + value + ' <> saved then return 82\n'
        source += "address the 'msg FORM_EXTRACT_OK';\nreturn 0\n"
        probe.write_text(source)
        result = command(f'macro "{probe}"')
        assert result.get('last_message') == 'FORM_EXTRACT_OK', result
        after = send('look full compact')
        assert before['focus'] == after['focus'] and before['buffer'] == after['buffer'], (before, after)
        assert not after['buffer']['dirty'] and sample.read_bytes() == original
        command('workbench begin owner public.forms')
        command('workbench form remove public.person')
        command('workbench commit', -3)
        assert query('summary')['lastResult']['code'] == 'object-in-use'
        command('workbench abort')
        command('workbench begin owner public.forms')
        command('workbench panel remove public.pane')
        command('workbench form remove public.person')
        command('workbench commit')
        assert query('forms')['total'] == 1
        send('quit')
        process.wait(timeout=10)
        assert process.returncode == 0, process.returncode
    finally:
        failed = sys.exc_info()[0] is not None
        try:
            if process.poll() is None:
                try:
                    process.stdin.write('quit\n')
                    process.stdin.flush()
                    process.wait(timeout=2)
                except (OSError, subprocess.TimeoutExpired):
                    process.kill()
                    process.wait(timeout=5)
        finally:
            # Query messages can exceed a Windows pipe's capacity. Capture to
            # disk during the dialogue, then preserve diagnostics on failure.
            if failed or sys.exc_info()[0] is not None or process.returncode:
                stderr_capture.seek(0)
                sys.stderr.write(stderr_capture.read().decode('utf-8', errors='replace'))
            for stream in (process.stdin, process.stdout):
                try:
                    stream.close()
                except OSError:
                    pass

    # Failed outer activation creates no runtime form values.
    failed = root / 'failed-form.the'
    source = 'options levelb\n' if engine == 'CREXX' else '/* Rexx */\n'
    for line in ['workbench begin owner public.profile', 'workbench form put public.failed',
                 'workbench field put public.failed name label "Name" value "Default" access edit maxcells 40',
                 'workbench commit']:
        source += "address the '" + line + "';\n"
    failed.write_text(source + 'return 7\n')
    result = subprocess.run([str(binary), '--driver', 'llm', '-p', str(failed), str(sample)],
                            input='command query workbench forms\nlook full compact\nquit\n',
                            text=True, encoding='utf-8', capture_output=True, env=env, timeout=45)
    records = [json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
    forms = [json.loads(r['last_message']) for r in records if r.get('last_message', '').startswith('{"schemaVersion":1')]
    assert result.returncode == 0 and forms and forms[-1]['total'] == 0, (result.stderr, records)
    assert sample.read_bytes() == original
print('workbench FORM real-editor checks passed:', engine)
