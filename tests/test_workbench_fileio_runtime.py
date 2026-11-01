#!/usr/bin/env python3
"""Native FILE route grammar and publication through the real LLM editor."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='THE native FILE ') as directory:
    root = Path(directory)
    source = root / 'program.rexx'
    input_file = root / 'input with spaces λ.bin'
    output = root / 'output with spaces λ.bin'
    source.write_text("say x2c('00FFe282ac')\nparse pull value\nsay value\nexit 0\n")
    original = source.read_bytes()
    input_file.write_bytes(b'finite input\n')
    env = dict(os.environ, THE_TOOL_CONFIG_DIR=str(root / 'config'),
               THE_CREXX_CACHE_DIR=str(root / 'cache'))
    process = subprocess.Popen([str(binary), '--driver', 'llm', '-n', str(source)],
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, text=True, encoding='utf-8',
                               env=env, cwd=root)

    def send(text):
        process.stdin.write(text + '\n')
        process.stdin.flush()
        while True:
            line = process.stdout.readline()
            assert line, (text, process.poll(), process.stderr.read())
            if line.startswith('{'):
                return json.loads(line)

    def command(text, rc=0):
        record = send('command ' + text)
        assert record.get('rc') == rc, (text, record)
        return record

    def query(selector):
        return json.loads(command('query workbench ' + selector)['last_message'])

    def route(path=output, mode='CREATE'):
        command('workbench begin owner public.io')
        command(f'workbench route put public.stdin from FILE "{input_file.name}" '
                'to tool:classic-rexx/stdin format bytes eof end')
        command(f'workbench route put public.stdout from tool:classic-rexx/stdout '
                f'to FILE "{path}" format bytes mode {mode} publish success')
        command('workbench commit')

    def run():
        command('RX RUN')
        send('toolwait 10000')
        return send('tools')

    try:
        capability = query('capabilities')
        assert capability['fileInput'] and capability['fileCreate']
        assert not capability['fileReplace'] and not capability['fileAppend']
        assert capability['fileWorkspace'] == str(root.resolve()), capability
        # Inspection is a runtime read. A refused inspection must not poison
        # an unrelated candidate definition transaction.
        command('workbench begin owner public.inspect')
        command('workbench file inspect "/tmp/outside-workspace.bin"', -3)
        assert query('summary')['lastResult']['code'] == 'policy-denied'
        command('workbench commit')
        route()
        assert not output.exists()
        routes = query('routes')['records']
        assert {r['kind'] for r in routes} == {'file-input', 'file-output'}, routes
        before = send('look full compact')
        result = run()
        assert result['job']['state'] == 'Completed · status 0', result
        # Classic x2c produces characters, and SAY encodes them as UTF-8.
        # The raw-host C test separately supplies invalid UTF-8 byte chunks.
        prefix = b'\x00\xc3\xbf\xc3\xa2\xc2\x82\xc2\xac\n'
        expected = prefix + b'finite input\n'
        assert output.read_bytes() == expected, output.read_bytes()
        receipt = query('fileio')
        assert receipt['published'] and receipt['outputBytes'] == len(expected) and receipt['token'], receipt
        assert receipt['inputBytes'] == len(input_file.read_bytes())
        inspected = query(f'FILE "{output.name}"')
        assert inspected['bytes'] == len(expected) and inspected['token'], inspected
        after = send('look full compact')
        assert before['buffer'] == after['buffer'] and before['focus'] == after['focus'], (before, after)
        assert source.read_bytes() == original

        # A later collision is a launch refusal, and never truncates bytes.
        command('RX RUN', -3)
        assert query('fileio')['code'] == 'stale-revision'
        assert output.read_bytes() == expected
        output.unlink()
        input_file.write_bytes(b'')
        result = run()
        assert result['job']['state'] == 'Completed · status 0', result
        assert output.read_bytes() == prefix + b'\n'
        output.unlink()

        # Declaring unsupported replacement modes never writes a target and
        # poisons only the candidate graph until explicitly aborted.
        command('workbench begin owner public.io')
        command(f'workbench route put public.stdout from tool:classic-rexx/stdout '
                f'to FILE "{output}" format bytes mode REPLACE expect opaque publish success', -3)
        assert query('summary')['lastResult']['code'] == 'unsupported-feature'
        command('workbench abort')
        command('workbench begin owner public.io')
        command(f'workbench route put public.stdout from tool:classic-rexx/stdout '
                f'to FILE "{output}" format bytes mode APPEND expect opaque publish success', -3)
        command('workbench abort')
        assert not output.exists()
        # Definition admission is side-effect-free but still bounds the
        # canonical workspace/parent; absent input leaves remain valid.
        command('workbench begin owner public.outside')
        command('workbench route put public.outside from FILE "/tmp/not-in-workspace.bin" '
                'to tool:classic-rexx/stdin format bytes eof end')
        command('workbench commit', -3)
        assert query('summary')['lastResult']['code'] == 'policy-denied'
        command('workbench abort')
        command('workbench begin owner public.io')
        command('workbench route put public.future from FILE "future-input.bin" '
                'to tool:classic-rexx/stdin format bytes eof end')
        command('workbench route remove public.stdin')
        command('workbench commit')
        future = next(r for r in query('routes')['records'] if r['id'] == 'public.future')
        assert not future['available'] and future['reason'] == 'missing-object', future
        command('workbench begin owner public.io')
        command('workbench route remove public.future')
        command('workbench commit')
        route()

        # A clean, unsaved open buffer is protected too; the increment does
        # not silently leave it stale after external output publication.
        command(f'edit "{output}"')
        command(f'edit "{source}"')
        command('RX RUN', -3)
        assert query('fileio')['code'] == 'policy-denied'
        assert not output.exists()
        command(f'edit "{output}"')
        command('qquit')
        command('set readonly force')
        command('RX RUN', -3)
        assert query('fileio')['code'] == 'policy-denied'
        command('set readonly off')

        # Output errors remain separate from successful interpreter exits.
        blocked = root / 'blocked'
        blocked.mkdir()
        route(blocked / 'result')
        blocked.chmod(0o500)
        command('RX RUN', -3)
        assert query('fileio')['code'] == 'policy-denied'
        blocked.chmod(0o700)
        assert not (blocked / 'result').exists()
        assert not list(root.rglob('.the-stage-*'))
        send('quit')
        process.wait(timeout=10)
        assert process.returncode == 0, process.stderr.read()
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=10)
print('Native FILE input, exact binary CREATE, finite EOF and editor protection passed')
