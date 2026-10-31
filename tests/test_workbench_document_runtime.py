#!/usr/bin/env python3
"""DOCUMENT inspection through the real editor; no renderer scraping."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
engine = sys.argv[2] if len(sys.argv) > 2 else 'CREXX'
with tempfile.TemporaryDirectory(prefix='THE document ') as directory:
    root = Path(directory)
    sample, anchor = root / 'sample.txt', root / 'anchor.txt'
    original = b'alpha\nbeta\n'
    sample.write_bytes(original)
    anchor.write_bytes(b'anchor\n')
    env = dict(os.environ, THE_REXX_INTERPRETER=engine,
               THE_CREXX_CACHE_DIR=str(root / 'cache'), THE_TOOL_CONFIG_DIR=str(root / 'tools'))
    process = subprocess.Popen([str(binary), '--driver', 'llm', '-n', str(sample)],
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               text=True, encoding='utf-8', env=env)

    def send(command):
        process.stdin.write(command + '\n')
        process.stdin.flush()
        while True:
            line = process.stdout.readline()
            assert line, (command, process.poll(), process.stderr.read())
            if line.startswith('{'):
                return json.loads(line)

    def command(text, rc=0):
        result = send('command ' + text)
        assert result.get('rc') == rc, (text, result)
        return result

    def query(selector):
        result = command('query workbench ' + selector)
        return json.loads(result['last_message'])

    def define(panel):
        command('workbench begin owner public.docs')
        command(f'workbench panel put {panel} kind document buffer active access readonly')
        command('workbench commit')

    try:
        define('public.first')
        define('public.second')
        panels = query('panels')['records']
        first, second = panels
        assert first['documentAvailable'] and first['readonly'] and first['lineCount'] == 2, first
        assert first['documentId'] == second['documentId'], panels
        assert isinstance(first['documentRevision'], str) and isinstance(first['documentId'], str), first
        before = send('look full compact')
        revision = query('panels')['records'][0]['documentRevision']
        page = query(f'document public.first expect {revision} limit 1 bytes 3')
        assert page['records'][0]['text'] == 'alp' and page['records'][0]['bytesHex'] == '616c70', page
        assert page['nextLine'] == 1 and page['nextOffset'] == 3 and page['more'], page
        continuation = query(f'document public.second expect {revision} line 1 offset 3')
        assert continuation['records'][0]['text'] == 'ha', continuation
        assert query('panels')['records'][0]['documentRevision'] == revision
        after = send('look full compact')
        assert before['focus'] == after['focus'] and before['buffer'] == after['buffer'], (before, after)
        assert sample.read_bytes() == original and not after['buffer']['dirty']

        command('cursor goto 1 2')
        command('set insertmode on')
        command('text PENDING')
        pending_before = send('look full compact')
        current = query('panels')['records'][0]
        assert current['dirty'] and current['documentAvailable'], current
        command(f'query workbench document public.first expect {revision}', -3)
        assert query('summary')['lastResult']['code'] == 'stale-revision'
        page = query('document public.first')
        assert 'PENDING' in page['records'][0]['text'], page
        assert page['documentId'] == first['documentId']
        pending_after = send('look full compact')
        assert pending_before['focus'] == pending_after['focus'] and pending_before['buffer'] == pending_after['buffer']
        assert sample.read_bytes() == original

        # Exercise the physical command-line Enter path, including the outer
        # process_key guard that direct LLM command dispatch does not enter.
        command('define enter enter cua')
        send('focus command')
        send('text query workbench document public.first')
        entered = send('key ENTER')
        entered_page = json.loads(entered['last_message'])
        assert entered_page['section'] == 'document' and 'PENDING' in entered_page['records'][0]['text'], entered

        probe = root / 'inspection.the'
        if engine == 'CREXX':
            source = 'options levelb\nworkbench=.string[]\n'
            expose, value = ' expose workbench[]', 'workbench[1]'
        else:
            source = '/* Rexx */\nsignal on error name failed\n'
            expose, value = '', 'workbench.1'
        source += "address the 'extract /workbench panels/'" + expose + ';\n'
        source += 'saved=' + value + '\n'
        source += "address the 'extract /workbench document public.first/'" + expose + ';\n'
        source += 'if rc <> -3 then return 81\nif ' + value + ' <> saved then return 82\n'
        source += "address the 'msg DOCUMENT_BUSY_INSPECTION_OK';\nreturn 0\n"
        if engine == 'REGINA':
            source += 'failed:\nreturn 83\n'
        probe.write_text(source)
        result = command(f'macro "{probe}"')
        assert result.get('last_message') == 'DOCUMENT_BUSY_INSPECTION_OK', result
        assert query('summary')['lastResult']['code'] == 'busy-document'
        assert query('document public.first')['records'][0]['text'] == page['records'][0]['text']

        send('focus filearea')
        command('set synonym query text SHADOWED')
        shadow_before = json.loads(command('q workbench panels')['last_message'])['records'][0]
        command('query workbench document public.first')
        shadow_after = json.loads(command('q workbench panels')['last_message'])['records'][0]
        assert int(shadow_after['documentRevision']) > int(shadow_before['documentRevision']), (shadow_before, shadow_after)
        assert shadow_after['documentId'] == shadow_before['documentId']
        command('set synonym query')
        assert query('document public.first')['documentId'] == first['documentId']
        assert sample.read_bytes() == original

        # Keep one buffer alive so close/reopen runs in the same editor session.
        command(f'edit "{anchor}"')
        command(f'edit "{sample}"')
        command('qquit')
        closed = query('panels')['records'][0]
        assert not closed['documentAvailable'] and closed['documentReason'] == 'document-closed', closed
        command('query workbench document public.first', -3)
        assert query('summary')['lastResult']['code'] == 'document-closed'
        command(f'edit "{sample}"')
        assert query('panels')['records'][0]['documentId'] == first['documentId']
        define('public.reopened')
        reopened = next(p for p in query('panels')['records'] if p['id'] == 'public.reopened')
        assert reopened['documentId'] != first['documentId'] and reopened['documentAvailable'], reopened
        assert query('document public.reopened')['records'][0]['text'] == 'alpha'
        assert sample.read_bytes() == original
        command('workbench begin owner public.docs')
        command('workbench panel put public.file kind document file "missing.txt" access readonly', -3)
        assert query('summary')['lastResult']['code'] == 'unsupported-feature'
        command('workbench abort')
        send('quit')
        process.wait(timeout=10)
        assert process.returncode == 0, process.stderr.read()
    finally:
        if process.poll() is None:
            try:
                process.stdin.write('quit\n')
                process.stdin.flush()
                process.wait(timeout=2)
            except (BrokenPipeError, subprocess.TimeoutExpired):
                process.kill()
                process.wait(timeout=5)
print('workbench DOCUMENT real-editor checks passed:', engine)
