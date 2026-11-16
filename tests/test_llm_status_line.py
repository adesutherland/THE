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
    sample.write_bytes(b'alpha\n')
    commands = [
        'capabilities',
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
    if not next(record['utf'] for record in records if 'utf' in record):
        print('LLM status line follows STATOPT text, visibility, and snapshot options (UTF disabled)')
        raise SystemExit(0)

    # HEXDISPLAY's native painter overlays glyphs onto blank cells. Semantic
    # status text must contain the same preview, including safe tab/keycap and
    # combining-mark presentation, while retaining profile-selected modes.
    previews = [('a', '[a]', 'U+61'), ('\t', '[→]', 'U+9'),
                ('é', '[é]', 'U+E9'), ('界', '[界]', 'U+754C'),
                ('e\u0301', '[e \u0301]', 'U+65+301'),
                ('😀', '[😀]', 'U+1F600'), ('1\ufe0f\u20e3', '[1 □]', 'U+31+FE0F+20E3')]
    status_sample = root / 'status.txt'
    status_bytes = ('\n'.join(item[0] for item in previews) + '\n').encode('utf-8')
    status_sample.write_bytes(status_bytes)
    commands = ['command set statusline bottom', 'command set statopt off *',
                'command set clock off', 'command set hexdisplay both', 'focus filearea']
    for line in range(1, len(previews) + 1):
        commands += [f'command cursor goto {line} 1', 'look full compact']
    commands += ['command cursor goto 3 1', 'command set hexdisplay chars', 'look full compact',
                 'command set hexdisplay codes', 'look full compact',
                 'command set hexdisplay off', 'look full compact',
                 'command set statusline off', 'look full compact', 'quit']
    result = subprocess.run([str(binary), '--driver', 'llm', '-n', str(status_sample)],
                            input='\n'.join(commands) + '\n', cwd=root, env=env,
                            text=True, encoding='utf-8', capture_output=True, timeout=30, check=True)
    status_snapshots = [record for output in result.stdout.splitlines() if output.startswith('{')
                        for record in [json.loads(output)] if 'screen_rows' in record]
    assert len(status_snapshots) == len(previews) + 4, result.stdout
    for line, ((_, glyph, code), snapshot) in enumerate(zip(previews, status_snapshots), 1):
        assert glyph in snapshot['status_line'], (glyph, snapshot)
        assert code in snapshot['status_line'], (code, snapshot)
        assert snapshot['focus']['line'] == line and snapshot['focus']['cell'] == 0, snapshot
        assert snapshot['buffer']['dirty'] == 0, snapshot
    chars, codes, off, hidden = status_snapshots[-4:]
    assert '[é]' in chars['status_line'] and 'U+' not in chars['status_line'], chars
    assert 'U+E9' in codes['status_line'] and '[' not in codes['status_line'], codes
    assert 'U+' not in off['status_line'] and '[' not in off['status_line'], off
    assert hidden['status_line'] is None, hidden
    assert status_sample.read_bytes() == status_bytes, 'status previews changed saved bytes'

print('LLM status line follows profile visibility, formatting and Unicode previews')
