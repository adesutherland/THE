#!/usr/bin/env python3
"""Deletion must survive refresh, save and reopen through the real editor."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
engine = sys.argv[2] if len(sys.argv) > 2 else 'CREXX'
repo = Path(__file__).resolve().parent.parent
profile = repo / ('profile_regina.the' if engine == 'REGINA' else 'profile.the')

with tempfile.TemporaryDirectory(prefix='CRXTHE deletion ') as temporary:
    root = Path(temporary)
    target = root / 'sample.txt'
    env = dict(os.environ, THE_REXX_INTERPRETER=engine,
               THE_CREXX_CACHE_DIR=str(root / 'cache'))

    def run(commands):
        result = subprocess.run(
            [str(binary), '--driver', 'llm', '-p', str(profile), str(target)],
            input='command set eolout lf\n' + commands + '\nquit\n',
            env=env, text=True, encoding='utf-8', capture_output=True,
            timeout=45, check=True)
        assert not any(word in result.stderr for word in
                       ['Error ', 'Unable to', 'not found', 'not recognized']), result.stderr
        return [json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]

    def snapshot(records):
        return next(r for r in reversed(records) if 'screen_rows' in r)

    def line_text(record):
        return [r['t'] for r in record['screen_rows'] if r['role'] == 'file']

    # Positive control: positioning, inspection and save alone preserve bytes.
    target.write_bytes(b'alpha\nsecond\n')
    run('command cursor goto 1 3\nkey right\nlook filearea compact\nkey ctrl-s')
    assert target.read_bytes() == b'alpha\nsecond\n'
    caps = next(r for r in run('capabilities') if 'macro_available' in r)
    assert caps['macro_available'] and caps['macro_interpreter'] == engine, caps

    # Plain deletion at a record boundary must remain a clean no-op.
    for position, action in [(6, 'command sos delchar'), (1, 'command sos delback')]:
        records = run(f'command cursor goto 1 {position}\n{action}\nlook filearea compact\ncommand save')
        assert snapshot(records)['buffer']['dirty'] == 0, records
        assert target.read_bytes() == b'alpha\nsecond\n'

    cases = [
        ('delete-key', 'alpha\nsecond\n', 1, 3, 'key delete', 'alha\nsecond\n', 2),
        ('cuadelchar', 'alpha\nsecond\n', 1, 3, 'command sos cuadelchar', 'alha\nsecond\n', 2),
        ('delchar', 'alpha\nsecond\n', 1, 3, 'command sos delchar', 'alha\nsecond\n', 2),
        ('backspace-key', 'alpha\nsecond\n', 1, 3, 'key backspace', 'apha\nsecond\n', 1),
        ('cuadelback', 'alpha\nsecond\n', 1, 3, 'command sos cuadelback', 'apha\nsecond\n', 1),
        ('delback', 'alpha\nsecond\n', 1, 3, 'command sos delback', 'apha\nsecond\n', 1),
        ('delete-first', 'alpha\n', 1, 1, 'key delete', 'lpha\n', 0),
        ('delete-last', 'alpha\n', 1, 5, 'key delete', 'alph\n', 4),
        ('delete-only', 'a\n', 1, 1, 'key delete', '\n', 0),
        ('join-next', 'alpha\nsecond\n', 1, 6, 'command sos cuadelchar', 'alphasecond\n', 5),
        ('join-previous', 'alpha\nsecond\n', 2, 1, 'command sos cuadelback', 'alphasecond\n', 5),
        ('repeat-delete', 'alpha\n', 1, 2, 'key delete\nkey delete', 'aha\n', 1),
        ('erase-end', 'alpha\n', 1, 3, 'command sos delend', 'al\n', 2),
    ]
    if caps.get('utf'):
        cases.extend([
            ('delete-wide', 'A界B\n', 1, 2, 'key delete', 'AB\n', 1),
            ('delete-combining', 'Ae\u0301B\n', 1, 2, 'command sos cuadelchar', 'AB\n', 1),
            ('backspace-wide', 'A界B\n', 1, 4, 'key backspace', 'AB\n', 1),
            ('backspace-combining', 'Ae\u0301B\n', 1, 3, 'command sos cuadelback', 'AB\n', 1),
        ])

    for name, original, line, column, action, expected, cell in cases:
        target.write_bytes(original.encode('utf-8'))
        records = run(f'command cursor goto {line} {column}\n{action}\nlook filearea compact')
        edited = snapshot(records)
        assert line_text(edited) == expected.splitlines(), (name, edited)
        assert edited['buffer']['dirty'] == 1, (name, edited)
        assert edited['focus']['cell'] == cell, (name, edited['focus'])
        assert target.read_bytes() == original.encode('utf-8'), (name, 'saved without SAVE')
        # Repeat from the original: test both save routes independently.
        for save in ['key ctrl-s', 'command save']:
            target.write_bytes(original.encode('utf-8'))
            run(f'command cursor goto {line} {column}\n{action}\n{save}')
            assert target.read_bytes() == expected.encode('utf-8'), (name, save, target.read_bytes())
            reopened = snapshot(run('look filearea compact'))
            assert line_text(reopened) == expected.splitlines(), (name, save, reopened)
            assert reopened['buffer']['dirty'] == 0, (name, save, reopened)

    # Moving the caret can itself repaint a horizontally scrolled record.
    original = b'x' * 100 + b'ABC\n'
    target.write_bytes(original)
    records = run('command cursor goto 1 102\nkey backspace\nlook filearea compact\nkey ctrl-s')
    assert snapshot(records)['focus']['cell'] == 100, records
    assert snapshot(records)['buffer']['dirty'] == 1, records
    assert target.read_bytes() == b'x' * 100 + b'BC\n', target.read_bytes()

print(engine + ': deletion, dirty state, cursor, save and reopened bytes passed')
