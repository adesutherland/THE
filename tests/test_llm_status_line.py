#!/usr/bin/env python3
"""Check that semantic status text follows the native STATUSLINE/STATOPT owner."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


binary = Path(sys.argv[1]).resolve()

with tempfile.TemporaryDirectory(prefix='CRXTHE status line ') as temporary:
    root = Path(temporary)
    sample = root / 'sample.txt'
    sample.write_text('alpha\n', encoding='utf-8')
    commands = [
        'look full compact',
        'key right',
        'command set clock off',
        'command set hexdisplay off',
        'command set insertmode off',
        'command set statopt off *',
        'command set statopt on insertmode.1 15 2 Mode=',
        'command set statopt on nbfile.1 32 0 Files=',
        'look full compact',
        'command set insertmode on',
        'delta full compact',
        'command set statusline off',
        'look full compact',
        'command set statusline gui',
        'look full compact',
        'command set statusline top',
        'look full',
        'look full compact status=0',
        'quit',
    ]
    env = dict(os.environ, THE_HOME_DIR=str(binary.parent),
               THE_REXX_INTERPRETER='NONE')
    result = subprocess.run(
        [str(binary), '--driver', 'llm', '-n', str(sample)],
        input='\n'.join(commands) + '\n', cwd=root, env=env,
        text=True, encoding='utf-8', capture_output=True, timeout=30,
        check=True,
    )
    records = []
    remaining = result.stdout.strip()
    decoder = json.JSONDecoder()
    while remaining:
        record, consumed = decoder.raw_decode(remaining)
        records.append(record)
        remaining = remaining[consumed:].lstrip()
    snapshots = [record for record in records if 'mode' in record]
    assert len(snapshots) == 7, records
    startup, configured, changed, hidden, gui, restored, excluded = snapshots

    assert 'Copyright' in startup['status_line'], startup
    status_line = configured['status_line']
    # The global status line spans the terminal, including the prefix area.
    assert len(status_line) == 80, configured
    assert status_line.startswith('CRXTHE'), configured
    assert status_line[14:21] == 'Mode=OF', configured
    assert status_line[31:38] == 'Files=1', configured
    assert 'focus=' not in status_line and 'verify=' not in status_line, configured
    assert configured['status'].startswith('focus='), configured
    assert changed['status_line'][14:21] == 'Mode=ON', changed
    assert changed['status_changed'] == 1, changed
    assert changed['status_line'].endswith(' INS '), changed
    assert hidden['status_line'] is None, hidden
    assert gui['status_line'] is None, gui
    assert restored['status_line'] == changed['status_line'], restored
    assert 'status_line' not in excluded and 'status' not in excluded, excluded
    assert sample.read_bytes() == b'alpha\n', sample.read_bytes()

print('LLM status line follows STATOPT text, visibility, and snapshot options')
