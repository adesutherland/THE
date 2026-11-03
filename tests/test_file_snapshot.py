#!/usr/bin/env python3
"""Typed file actions and JSON share one projection; owner poll admits results."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary, module = (str(Path(value).resolve()) for value in sys.argv[1:3])
with tempfile.TemporaryDirectory(prefix='THE file snapshot ') as temporary:
    root = Path(temporary).resolve()
    source = root / 'source.probe'
    original = 'alpha\nZoë😀\n'.encode('utf-8')
    source.write_bytes(original)
    env = dict(os.environ, THE_REXX_INTERPRETER='NONE', THE_DRIVER_PATH=module,
               THE_FILE_SNAPSHOT_SOURCE=str(source), THE_TOOL_ROOT=str(root / 'missing-helper'),
               THE_TOOL_CONFIG_DIR=str(root / 'config'))
    result = subprocess.run([binary, '--driver', 'curses', '-n', str(source)],
                            cwd=root, env=env, stdin=subprocess.DEVNULL,
                            capture_output=True, text=True, timeout=20)
    assert result.returncode == 0, (result.stdout, result.stderr)
    assert 'FILE_SNAPSHOT_OWNER_POLL_AND_PROJECTION_PASS' in result.stdout, result.stdout
    typed = next(line.split()[1:] for line in result.stdout.splitlines()
                 if line.startswith('TYPED_FILE_SNAPSHOT '))
    source_id, revision, generation, serial, rc = map(int, typed)
    state = next(json.loads(line) for line in result.stdout.splitlines()
                 if line.startswith('{') and json.loads(line).get('type') == 'tools')
    assert state['currentFile'] == {
        'sourceId': source_id, 'revision': revision, 'path': str(source), 'role': 'program',
        'actions': [
            {'id': 'build', 'label': 'Snapshot Build', 'enabled': False,
             'reason': 'capability-unavailable', 'tool': 'public.snapshot-build',
             'target': 'current-file', 'generation': str(generation)},
            {'id': 'run', 'label': 'Run', 'enabled': True, 'reason': '',
             'tool': 'snapshot-runner', 'target': 'current-file', 'generation': '0'},
            {'id': 'activate', 'label': 'Apply profile', 'enabled': False,
             'reason': 'This file has no profile activation role', 'tool': '',
             'target': 'current-file', 'generation': '0'}]}, state
    assert state['fileResult'] == {
        'serial': serial, 'sourceId': source_id, 'revision': revision, 'rc': rc,
        'action': 'run', 'code': 'completed', 'message': 'Completed · status 0'}, state
    assert source.read_bytes() == original
    print('Typed actions/source/generation match JSON; owner completion advances once; cursor, dirty state and bytes retained')
